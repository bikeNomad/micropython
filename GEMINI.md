# Instructions for AI Assistant
 - Save the history of this chat as @CHAT.md

## Writing Python Code
1. Write clear and maintainable Python code.
2. Use descriptive variable and function names to improve readability.
3. Follow PEP 8 guidelines for code formatting.
4. Use static typing with type hints to ensure type safety and improve code clarity, but limit this to simple types (e.g., `int`, `str`, `list`, `dict`) to maintain compatibility with MicroPython.
5. Add meaningful comments and docstrings to explain the purpose of functions, classes, and complex logic.
6. Ensure the code is compatible with MicroPython, rather than CPython, and optimized for constrained devices with limited resources.

## General Guidelines
1. Prioritize simplicity and readability over clever or overly complex solutions.
2. Avoid hardcoding values; use constants or configuration files where appropriate.
3. Ensure the code is modular and reusable by breaking it into smaller functions or classes.
4. Test the code thoroughly to ensure correctness and reliability.
5. Document any assumptions or limitations in the code or tests.
6. Ensure the code is distributed across different repositories in a way that promotes modularity and maintainability.
7. Optimize the code for constrained devices with limited memory and processing power.

## MicroPython and MicroPython-lib
MicroPython is a lean and efficient implementation of Python 3 designed for microcontrollers and constrained environments. It provides a subset of Python's standard library and is optimized for low memory usage and high performance. MicroPython is widely used in embedded systems, IoT devices, and other resource-constrained applications.

### Key Features of MicroPython
Micropython is checked out in this folder.

1. **Lightweight and Efficient**: Designed to run on devices with limited memory and processing power.
2. **Python Compatibility**: Implements a subset of Python 3, making it easy for Python developers to transition to embedded programming.

### Relevant References
1. **Optimizing Python Code for Speed**: Learn how to write efficient Python code for MicroPython environments. [Reference: Speed and Performance](https://docs.micropython.org/en/latest/reference/speed_python.html)
2. **MicroPython Development**: Explore the development process and best practices for working with MicroPython. [Reference: MicroPython Development](https://docs.micropython.org/en/latest/develop/index.html)
3. **Memory Management in MicroPython**: Understand how MicroPython handles memory and how to optimize memory usage in constrained environments. [Reference: Memory Management](https://docs.micropython.org/en/latest/develop/memorymgt.html)

## Code Formatting and Style

All new C/python/sh files should have a newline at the end of the file.

**Before committing code:**
```bash
# Format C code (requires uncrustify v0.72)
tools/codeformat.py

# Format specific files only
tools/codeformat.py path/to/file.c

# Check formatting without modifying
tools/codeformat.py -c

# Python code is formatted with ruff
ruff format

# Run spell check
codespell

# Run lint and formatting checks (if using pre-commit)
pre-commit run --files [files...]
```

**Use pre-commit hooks for automatic checks (recommended):**
```bash
pre-commit install --hook-type pre-commit --hook-type commit-msg
```

**Commit message format:**
```
component/subcomponent: Brief description ending with period.

Detailed explanation if needed, wrapped at 75 characters.

Signed-off-by: Your Name <your.email@example.com>
```

Example:
```
py/objstr: Add splitlines() method.

This implements the splitlines() method for str objects, compatible
with CPython behavior.

Signed-off-by: Developer Name <dev@example.com>
```

## Code Style Guidelines

**General:**
* Follow conventions in existing code.
* See `CODECONVENTIONS.md` for detailed C and Python style guides.

**Python:**
* Follow PEP 8.
* Use `ruff format` for auto-formatting (line length 99).
* Naming: `module_name`, `ClassName`, `function_name`, `CONSTANT_NAME`.

**C:**
* Use `tools/codeformat.py` for auto-formatting.
* Naming: `underscore_case`, `CAPS_WITH_UNDERSCORE` for enums/macros, `type_name_t`.
* Memory allocation: Use `m_new`, `m_renew`, `m_del`.
* Integer types: Use `mp_int_t`, `mp_uint_t` for general integers, `size_t` for sizes.

**Git Commits:**
* Prefix commit messages with the affected directory/file (e.g., `py/objstr: ...`).
* First line max 72 chars, subsequent lines max 75 chars. End sentences with `.`.
* Sign-off commits using `git commit -s`.


## Micropython networking
The code for the various MicroPython network adapters (Ethernet and Wifi) is in @extmod/modnetwork.c, @extmod/modnetwork.h, and specific adapter code is at @extmod/network_wiznet5k.c, @extmod/network_cyw43.c, etc.

## The Zephyr Micropython port
The Zephyr Micropython port lives in @ports/zephyr .
I have linked my Zephyr workspace at @lib/zephyr .

## Verifying compilation
To ensure that generated code actually compiles:
  - Change directory to @ports/zephyr .
  - Recursively remove the @ports/zephyr/build directory.
  - Run this command:
    - west build -b nucleo_f429zi

## Desired network module
  - I want to add Ethernet networking capabilities to the Zephyr Micropython port in @ports/zephyr .
    A good target board for initial work is the "nucleo_f429zi" board defined in Zephyr.
  - However, the networking module should be independent of specific boards.
  - Add a @ports/zephyr/network_zephyr.c file and other files as needed in @ports/zephyr that implement the required networking functions.
  - Do not change any code in @extmod .
  - Do not change @ports/zephyr/modsocket.c because it already works.
  - If it is impossible to proceed further without modifying code in either @extmod or @ports/zephyr/modsocket.c, stop, explain why, and wait for guidance.
  - The current @ports/zephyr/network_zephyr.c file is an earlier attempt from a Gemini run. You can change it at will.
  - Do not use the LWIP library because Zephyr has its own network stack.

## Further background on networking
It seems like we need to define MICROPY_PORT_NETWORK_INTERFACES, which requires these methods to be implemented:
```
typedef struct _mod_network_nic_protocol_t {
    // API for non-socket operations
    int (*gethostbyname)(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t *ip_out);
    void (*deinit)(void);

    // API for socket operations; return -1 on error
    int (*socket)(struct _mod_network_socket_obj_t *socket, int *_errno);
    void (*close)(struct _mod_network_socket_obj_t *socket);
    int (*bind)(struct _mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno);
    int (*listen)(struct _mod_network_socket_obj_t *socket, mp_int_t backlog, int *_errno);
    int (*accept)(struct _mod_network_socket_obj_t *socket, struct _mod_network_socket_obj_t *socket2, byte *ip, mp_uint_t *port, int *_errno);
    int (*connect)(struct _mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno);
    mp_uint_t (*send)(struct _mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno);
    mp_uint_t (*recv)(struct _mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno);
    mp_uint_t (*sendto)(struct _mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);
    mp_uint_t (*recvfrom)(struct _mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);
    int (*setsockopt)(struct _mod_network_socket_obj_t *socket, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
    int (*settimeout)(struct _mod_network_socket_obj_t *socket, mp_uint_t timeout_ms, int *_errno);
    int (*ioctl)(struct _mod_network_socket_obj_t *socket, mp_uint_t request, mp_uint_t arg, int *_errno);
} mod_network_nic_protocol_t;
```

And it requires calling mod_network_register_nic() on any network interfaces you want to use.

I think networking also requires MICROPY_PY_NETWORK_LAN to be defined.

The only support I can see for non-LWIP network interfaces seems to be for the NINAW10, ESP_HOSTED, MIMXRT, and the WIZNET5K.

The mimxrt port looks to be a good example (see @ports/mimxrt/network_lan.c). This allows an mimxrt board to have networking using one of five PHY types.


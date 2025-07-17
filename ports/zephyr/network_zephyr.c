/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ned Konz <ned@metamagix.tech>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/stream.h"
#include "extmod/modnetwork.h"

#if MICROPY_PY_NETWORK_ZEPHYR

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/socket.h>

#include "shared/netutils/netutils.h"

#include "ports/zephyr/network_zephyr.h"

// Forward-declare the protocol table.
const mod_network_nic_protocol_t mod_network_nic_protocol_zephyr;

typedef struct _network_zephyr_obj_t {
    mp_obj_base_t base;
    struct net_if *net_if;
} network_zephyr_obj_t;

const mp_obj_type_t mp_network_zephyr_type;

static const network_zephyr_obj_t network_zephyr_eth_obj = { { &mp_network_zephyr_type }, NULL };

static void network_zephyr_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<ZEPHYR ETH %s>", net_if_is_up(self->net_if) ? "UP" : "DOWN");
}

static mp_obj_t network_zephyr_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    network_zephyr_obj_t *self = (network_zephyr_obj_t *)&network_zephyr_eth_obj;
    struct net_if *net_if = net_if_get_default();
    if (net_if == NULL) {
        mp_raise_OSError(MP_ENODEV);
    }
    self->net_if = net_if;
    mod_network_register_nic(MP_OBJ_FROM_PTR(self));
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t network_zephyr_active(size_t n_args, const mp_obj_t *args) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (n_args == 1) {
        return mp_obj_new_bool(net_if_is_up(self->net_if));
    } else {
        if (mp_obj_is_true(args[1])) {
            net_if_up(self->net_if);
        } else {
            net_if_down(self->net_if);
        }
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_zephyr_active_obj, 1, 2, network_zephyr_active);

static mp_obj_t network_zephyr_ifconfig(size_t n_args, const mp_obj_t *args) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (n_args == 1) {
        // get settings
        struct net_if_config *ifconfig = net_if_get_config(self->net_if);
        if (ifconfig) {
            struct net_if_ipv4 *ipv4 = ifconfig->ip.ipv4;
            if (ipv4) {
                char ip_buf[NET_IPV4_ADDR_LEN];
                mp_obj_t tuple[4] = {
                    mp_obj_new_str(net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr, ip_buf, sizeof(ip_buf)), strlen(ip_buf)),
                    mp_obj_new_str(net_addr_ntop(AF_INET, &ipv4->unicast[0].netmask, ip_buf, sizeof(ip_buf)), strlen(ip_buf)),
                    mp_obj_new_str(net_addr_ntop(AF_INET, &ipv4->gw, ip_buf, sizeof(ip_buf)), strlen(ip_buf)),
                    mp_obj_new_str("0.0.0.0", 7),
                };
                return mp_obj_new_tuple(4, tuple);
            }
        }
        return mp_const_none;
    } else {
        // set settings
        return mp_const_none;
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_zephyr_ifconfig_obj, 1, 2, network_zephyr_ifconfig);

static mp_obj_t network_zephyr_status(mp_obj_t self_in) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (net_if_is_up(self->net_if)) {
        if (net_if_flag_is_set(self->net_if, NET_IF_RUNNING)) {
            return MP_OBJ_NEW_SMALL_INT(3); // link up, network up
        } else {
            return MP_OBJ_NEW_SMALL_INT(2); // network up, link down
        }
    } else {
        return MP_OBJ_NEW_SMALL_INT(1); // network down
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(network_zephyr_status_obj, network_zephyr_status);

static mp_obj_t network_zephyr_config(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if (kwargs->used == 0) {
        // Get config value
        if (n_args != 2) {
            mp_raise_TypeError(MP_ERROR_TEXT("must query one param"));
        }

        switch (mp_obj_str_get_qstr(args[1])) {
            case MP_QSTR_mac: {
                struct net_linkaddr *linkaddr = net_if_get_link_addr(self->net_if);
                return mp_obj_new_bytes(linkaddr->addr, linkaddr->len);
            }
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
        }
    } else {
        // Set config value(s)
        if (n_args != 1) {
            mp_raise_TypeError(MP_ERROR_TEXT("can't specify pos and kw args"));
        }

        for (size_t i = 0; i < kwargs->alloc; ++i) {
            if (MP_MAP_SLOT_IS_FILLED(kwargs, i)) {
                mp_map_elem_t *e = &kwargs->table[i];
                switch (mp_obj_str_get_qstr(e->key)) {
                    case MP_QSTR_mac: {
                        mp_buffer_info_t buf;
                        mp_get_buffer_raise(e->value, &buf, MP_BUFFER_READ);
                        if (buf.len != 6) {
                            mp_raise_ValueError(NULL);
                        }
                        net_if_set_link_addr(self->net_if, buf.buf, buf.len, NET_LINK_ETHERNET);
                        break;
                    }
                    default:
                        mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
                }
            }
        }

        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_KW(network_zephyr_config_obj, 1, network_zephyr_config);

static int network_zephyr_gethostbyname(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t *out_ip) {
    (void)nic;
    (void)len;
    struct zsock_addrinfo *res;
    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    int rc = zsock_getaddrinfo(name, NULL, &hints, &res);

    if (rc == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
        memcpy(out_ip, &addr->sin_addr, 4);
        zsock_freeaddrinfo(res);
        return 0;
    } else {
        return -2; // Corresponds to EHOSTNOTFOUND
    }
}

static int network_zephyr_socket(mod_network_socket_obj_t *socket, int *_errno) {
    int proto = (socket->type == MOD_NETWORK_SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP;
    socket->fileno = zsock_socket(socket->domain, socket->type, proto);
    if (socket->fileno < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static void network_zephyr_close(mod_network_socket_obj_t *socket) {
    if (socket->fileno >= 0) {
        zsock_close(socket->fileno);
        socket->fileno = -1;
    }
}

static int network_zephyr_bind(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(uint32_t *)ip;

    int ret = zsock_bind(socket->fileno, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int network_zephyr_listen(mod_network_socket_obj_t *socket, mp_int_t backlog, int *_errno) {
    int ret = zsock_listen(socket->fileno, backlog);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int network_zephyr_accept(mod_network_socket_obj_t *socket, mod_network_socket_obj_t *socket2, byte *ip, mp_uint_t *port, int *_errno) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    socket2->fileno = zsock_accept(socket->fileno, (struct sockaddr *)&addr, &addr_len);

    if (socket2->fileno < 0) {
        *_errno = errno;
        return -1;
    }

    memcpy(ip, &addr.sin_addr, 4);
    *port = ntohs(addr.sin_port);

    return 0;
}

static int network_zephyr_connect(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(uint32_t *)ip;

    int ret = zsock_connect(socket->fileno, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static mp_uint_t network_zephyr_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno) {
    int ret = zsock_send(socket->fileno, buf, len, 0);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return ret;
}

static mp_uint_t network_zephyr_recv(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno) {
    int ret = zsock_recv(socket->fileno, buf, len, 0);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return ret;
}

static mp_uint_t network_zephyr_sendto(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(uint32_t *)ip;

    int ret = zsock_sendto(socket->fileno, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return ret;
}

static mp_uint_t network_zephyr_recvfrom(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int ret = zsock_recvfrom(socket->fileno, buf, len, 0, (struct sockaddr *)&addr, &addr_len);

    if (ret < 0) {
        *_errno = errno;
        return -1;
    }

    memcpy(ip, &addr.sin_addr, 4);
    *port = ntohs(addr.sin_port);

    return ret;
}

static int network_zephyr_setsockopt(mod_network_socket_obj_t *socket, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    int ret = zsock_setsockopt(socket->fileno, level, opt, optval, optlen);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int network_zephyr_settimeout(mod_network_socket_obj_t *socket, mp_uint_t timeout_ms, int *_errno) {
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    int ret = zsock_setsockopt(socket->fileno, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int network_zephyr_ioctl(mod_network_socket_obj_t *socket, mp_uint_t request, mp_uint_t arg, int *_errno) {
    if (request == MP_STREAM_POLL) {
        struct zsock_pollfd fds = {
            .fd = socket->fileno,
            .events = (arg & MP_STREAM_POLL_RD ? ZSOCK_POLLIN : 0) | (arg & MP_STREAM_POLL_WR ? ZSOCK_POLLOUT : 0),
        };

        int ret = zsock_poll(&fds, 1, 0);
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
        return fds.revents;
    }

    *_errno = MP_EINVAL;
    return -1;
}

static const mp_rom_map_elem_t network_zephyr_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_active), MP_ROM_PTR(&network_zephyr_active_obj) },
    { MP_ROM_QSTR(MP_QSTR_ifconfig), MP_ROM_PTR(&network_zephyr_ifconfig_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&network_zephyr_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_config), MP_ROM_PTR(&network_zephyr_config_obj) },
};
static MP_DEFINE_CONST_DICT(network_zephyr_locals_dict, network_zephyr_locals_dict_table);

const mod_network_nic_protocol_t mod_network_nic_protocol_zephyr = {
    .gethostbyname = network_zephyr_gethostbyname,
    .socket = network_zephyr_socket,
    .close = network_zephyr_close,
    .bind = network_zephyr_bind,
    .listen = network_zephyr_listen,
    .accept = network_zephyr_accept,
    .connect = network_zephyr_connect,
    .send = network_zephyr_send,
    .recv = network_zephyr_recv,
    .sendto = network_zephyr_sendto,
    .recvfrom = network_zephyr_recvfrom,
    .setsockopt = network_zephyr_setsockopt,
    .settimeout = network_zephyr_settimeout,
    .ioctl = network_zephyr_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_network_zephyr_type,
    MP_QSTR_ZEPHYR,
    MP_TYPE_FLAG_NONE,
    make_new, network_zephyr_make_new,
    print, network_zephyr_print,
    protocol, &mod_network_nic_protocol_zephyr,
    locals_dict, &network_zephyr_locals_dict
    );

#endif // MICROPY_PY_NETWORK_ZEPHYR

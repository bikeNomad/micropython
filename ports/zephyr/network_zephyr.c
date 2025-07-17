/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Ned Low
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

#include <stdio.h>
#include <string.h>
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/mphal.h"

#if MICROPY_PY_NETWORK_ZEPHYR

#include "extmod/modnetwork.h"
#include "shared/netutils/netutils.h"

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_ip.h>

extern const mp_obj_type_t mp_network_zephyr_type;

typedef struct _network_zephyr_obj_t {
    mp_obj_base_t base;
    struct net_if *net_if;
} network_zephyr_obj_t;

static const network_zephyr_obj_t network_zephyr_eth_obj = { { &mp_network_zephyr_type }, NULL };

static void network_zephyr_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_zephyr_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<ZEPHYR ETH %s>", net_if_is_up(self->net_if) ? "UP" : "DOWN");
}

static mp_obj_t network_zephyr_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    network_zephyr_obj_t *self = (network_zephyr_obj_t *)&network_zephyr_eth_obj;
    self->net_if = net_if_get_default();
    return MP_OBJ_FROM_PTR(self);
}

/*******************************************************************************/
// network API

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
        struct net_if_addr *ifaddr;
        char ip_buf[NET_IPV6_ADDR_LEN];

        ifaddr = net_if_ipv4_addr_lookup(self->net_if, &self->net_if->config->ip.ipv4->unicast[0].address.in_addr);
        if (ifaddr) {
            net_addr_ntop(AF_INET, &ifaddr->address.in_addr, ip_buf, sizeof(ip_buf));
            mp_obj_t tuple[4] = {
                mp_obj_new_str(ip_buf, strlen(ip_buf)),
                mp_obj_new_str(net_addr_ntop(AF_INET, &self->net_if->config->ip.ipv4->netmask, ip_buf, sizeof(ip_buf)), strlen(ip_buf)),
                mp_obj_new_str(net_addr_ntop(AF_INET, &self->net_if->config->ip.ipv4->gw, ip_buf, sizeof(ip_buf)), strlen(ip_buf)),
                mp_obj_new_str("0.0.0.0", 7),
            };
            return mp_obj_new_tuple(4, tuple);
        }
        return mp_const_none;
    } else {
        // set settings
        return mp_const_none;
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_zephyr_ifconfig_obj, 1, 2, network_zephyr_ifconfig);

mp_obj_t mod_network_get_nic(mp_obj_t nic) {
    if (nic == MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(&network_zephyr_eth_obj);
    }
    return nic;
}

/*******************************************************************************/
// class bindings

static const mp_rom_map_elem_t network_zephyr_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_active), MP_ROM_PTR(&network_zephyr_active_obj) },
    { MP_ROM_QSTR(MP_QSTR_ifconfig), MP_ROM_PTR(&network_zephyr_ifconfig_obj) },
};
static MP_DEFINE_CONST_DICT(network_zephyr_locals_dict, network_zephyr_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_network_zephyr_type,
    MP_QSTR_ZEPHYR,
    MP_TYPE_FLAG_NONE,
    make_new, network_zephyr_make_new,
    print, network_zephyr_print,
    locals_dict, &network_zephyr_locals_dict
    );

#endif // MICROPY_PY_NETWORK_ZEPHYR

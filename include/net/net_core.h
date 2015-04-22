/*! @file
 @brief Network core definitions

 Definitions for networking support.
 */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <misc/printk.h>
#include <string.h>

#include <net/net_buf.h>

#ifndef __NET_CORE_H
#define __NET_CORE_H

/* Network subsystem logging helpers */

#define NET_DBG(fmt, ...) printk("net: %s: " fmt, __func__, ##__VA_ARGS__)
#define NET_ERR(fmt, ...) printk("net: %s: " fmt, __func__, ##__VA_ARGS__)
#define NET_INFO(fmt, ...) printk("net: " fmt,  ##__VA_ARGS__)
#define NET_PRINT(fmt, ...) printk(fmt, ##__VA_ARGS__)

/*!
 * @brief Initialize network stack. This is will be automatically called
 * by the OS.
 *
 * @details No harm will happen if application calls this multiple times.
 *
 * @return 0 if ok, < 0 in case of error.
 */
int net_init(void);

struct net_driver {
	/*! How much headroom is needed for net transport headers */
	size_t head_reserve;

	/*! Open the net transport */
	int (*open) (void);

	/*! Send data to net */
	int (*send) (struct net_buf *buf);
};

/*!
 * @brief Register a new network driver to the network stack.
 *
 * @details Only one network device can be registered at a time.
 *
 * @param buf Network driver.
 *
 * @return 0 if ok, < 0 in case of error.
 */
int net_register_driver(struct net_driver *drv);

/*!
 * @brief Unregister a previously registered network driver.
 *
 * @param buf Network driver.
 *
 * @details Networking will be disabled if there is no driver
 * available.
 *
 */
void net_unregister_driver(struct net_driver *drv);

#endif /* __NET_CORE_H */

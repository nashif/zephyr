/* dw_spi.c - Designware SPI driver implementation */

/*
 * Copyright (c) 2015 Intel Corporation.
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

#include <nanokernel.h>
#include <arch/cpu.h>

#include <misc/__assert.h>
#include <board.h>

#include <sys_io.h>
#include <clock_control.h>
#include <misc/util.h>

#include <spi.h>
#include <spi/dw_spi.h>
#include "dw_spi_priv.h"

#ifndef CONFIG_SPI_DEBUG
#define DBG(...) {;}
#else
#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define DBG printf
#else
#include <misc/printk.h>
#define DBG printk
#endif /* CONFIG_STDOUT_CONSOLE */
#endif /* CONFIG_SPI_DEBUG */

#define DEFINE_MM_REG_READ(__reg, __off, __sz)				\
	static inline uint32_t read_##__reg(uint32_t addr)		\
	{								\
		return sys_read##__sz(addr + __off);			\
	}
#define DEFINE_MM_REG_WRITE(__reg, __off, __sz)				\
	static inline void write_##__reg(uint32_t data, uint32_t addr)	\
	{								\
		sys_write##__sz(data, addr + __off);			\
	}

DEFINE_MM_REG_WRITE(ctrlr0, DW_SPI_REG_CTRLR0, 16)
DEFINE_MM_REG_WRITE(ser, DW_SPI_REG_SER, 8)
DEFINE_MM_REG_WRITE(baudr, DW_SPI_REG_BAUDR, 16)
DEFINE_MM_REG_WRITE(txftlr, DW_SPI_REG_TXFTLR, 32)
DEFINE_MM_REG_WRITE(rxftlr, DW_SPI_REG_RXFTLR, 32)
DEFINE_MM_REG_READ(rxflr, DW_SPI_REG_RXFLR, 32)
DEFINE_MM_REG_READ(txflr, DW_SPI_REG_TXFLR, 32)
DEFINE_MM_REG_WRITE(imr, DW_SPI_REG_IMR, 8)
DEFINE_MM_REG_READ(isr, DW_SPI_REG_ISR, 8)
DEFINE_MM_REG_READ(dr, DW_SPI_REG_DR, 16)
DEFINE_MM_REG_WRITE(dr, DW_SPI_REG_DR, 16)
DEFINE_MM_REG_READ(ssi_comp_version, DW_SPI_REG_SSI_COMP_VERSION, 32)

#define DEFINE_SET_BIT_OP(__reg_bit, __reg_off, __bit)			\
	static inline void set_bit_##__reg_bit(uint32_t addr)		\
	{								\
		sys_set_bit(addr + __reg_off, __bit);			\
	}

#define DEFINE_CLEAR_BIT_OP(__reg_bit, __reg_off, __bit)		\
	static inline void clear_bit_##__reg_bit(uint32_t addr)		\
	{								\
		sys_clear_bit(addr + __reg_off, __bit);			\
	}

#define DEFINE_TEST_BIT_OP(__reg_bit, __reg_off, __bit)			\
	static inline int test_bit_##__reg_bit(uint32_t addr)		\
	{								\
		return sys_test_bit(addr + __reg_off, __bit);		\
	}

DEFINE_SET_BIT_OP(ssienr, DW_SPI_REG_SSIENR, DW_SPI_SSIENR_SSIEN_BIT)
DEFINE_CLEAR_BIT_OP(ssienr, DW_SPI_REG_SSIENR, DW_SPI_SSIENR_SSIEN_BIT)
DEFINE_TEST_BIT_OP(sr_busy, DW_SPI_REG_SR, DW_SPI_SR_BUSY_BIT)
DEFINE_TEST_BIT_OP(sr_tfnf, DW_SPI_REG_SR, DW_SPI_SR_TFNF_BIT)
DEFINE_TEST_BIT_OP(sr_rfne, DW_SPI_REG_SR, DW_SPI_SR_RFNE_BIT)
DEFINE_TEST_BIT_OP(icr, DW_SPI_REG_ICR, DW_SPI_SR_ICR_BIT)

#ifdef CONFIG_PLATFORM_QUARK_SE_X86
#define int_unmask(__mask)						\
	sys_write32(sys_read32(__mask) & INT_UNMASK_IA, __mask)
#else
#define int_unmask(...) {;}
#endif

static void completed(struct device *dev, int error)
{
	struct spi_dw_config *info = dev->config->config_info;
	struct spi_dw_data *spi = dev->driver_data;
	enum spi_cb_type cb_type;

	if (error) {
		cb_type = SPI_CB_ERROR;
		goto out;
	}

	if (spi->t_len) {
		return;
	}

	if (spi->tx_buf && spi->tx_buf_len == 0 && !spi->rx_buf) {
		cb_type = SPI_CB_WRITE;
	} else if (spi->rx_buf && spi->rx_buf_len == 0 && !spi->tx_buf) {
		cb_type = SPI_CB_READ;
	} else if (spi->tx_buf && spi->tx_buf_len == 0 &&
			spi->rx_buf && spi->rx_buf_len == 0) {
		cb_type = SPI_CB_TRANSCEIVE;
	} else {
		return;
	}

out:
	spi->tx_buf = spi->rx_buf = NULL;
	spi->tx_buf_len = spi->rx_buf_len = 0;

	/* Disabling interrupts */
	write_imr(DW_SPI_IMR_MASK, info->regs);

	if (spi->callback) {
		spi->callback(dev, cb_type);
	}
}

static void push_data(struct device *dev)
{
	struct spi_dw_config *info = dev->config->config_info;
	struct spi_dw_data *spi = dev->driver_data;
	uint32_t cnt = 0;
	uint8_t data;

	DBG("spi: push_data\n");

	while (test_bit_sr_tfnf(info->regs)) {
		if (spi->tx_buf && spi->tx_buf_len > 0) {
			data = *(uint8_t *)(spi->tx_buf);
			spi->tx_buf++;
			spi->tx_buf_len--;
		} else if (spi->rx_buf && spi->rx_buf_len > 0) {
			/* No need to push more than necessary */
			if (spi->rx_buf_len - cnt <= 0) {
				break;
			}

			data = 0;
		} else {
			/* Nothing to push anymore */
			break;
		}

		write_dr(data, info->regs);
		cnt++;
	}

	DBG("Pushed: %d\n", cnt);
	spi->t_len += cnt;
}

static void pull_data(struct device *dev)
{
	struct spi_dw_config *info = dev->config->config_info;
	struct spi_dw_data *spi = dev->driver_data;
	uint32_t cnt = 0;
	uint8_t data;

	DBG("spi: pull_data\n");

	while(test_bit_sr_rfne(info->regs)) {
		data = read_dr(info->regs);
		cnt++;

		if (spi->rx_buf && spi->rx_buf_len > 0) {
			*(uint8_t *)(spi->rx_buf) = data;
			spi->rx_buf++;
			spi->rx_buf_len--;
		}
	}

	DBG("Pulled: %d\n", cnt);
	spi->t_len -= cnt;
}

static int spi_dw_configure(struct device *dev, struct spi_config *config)
{
	struct spi_dw_config *info = dev->config->config_info;
	struct spi_dw_data *spi = dev->driver_data;
	uint32_t flags = config->config;
	uint32_t ctrlr0 = 0;
	uint32_t mode;

	DBG("spi_dw_configure: %p (0x%x), %p\n", dev, info->regs, config);

	/* Check status */
	if (test_bit_sr_busy(info->regs)) {
		DBG("spi_dw_read: %Controller is busy\n");
		return DEV_USED;
	}

	/* Disable the controller, to be able to set it up */
	clear_bit_ssienr(info->regs);

	/* Word size */
	ctrlr0 |= DW_SPI_CTRLR0_DFS(SPI_WORD_SIZE_GET(flags));

	/* SPI mode */
	mode = SPI_MODE(flags);
	if (mode & SPI_MODE_CPOL) {
		ctrlr0 |= DW_SPI_CTRLR0_SCPOL;
	}

	if (mode & SPI_MODE_CPHA) {
		ctrlr0 |= DW_SPI_CTRLR0_SCPH;
	}

	if (mode & SPI_MODE_LOOP) {
		ctrlr0 |= DW_SPI_CTRLR0_SRL;
	}

	/* Installing the configuration */
	write_ctrlr0(ctrlr0, info->regs);

	/* Tx/Rx Threshold */
	write_txftlr(DW_SPI_TXFTLR_DFLT, info->regs);
	write_rxftlr(DW_SPI_RXFTLR_DFLT, info->regs);

	/* Configuring the rate */
	write_baudr(config->max_sys_freq, info->regs);

	spi->tx_buf = spi->rx_buf = NULL;
	spi->tx_buf_len = spi->rx_buf_len = spi->t_len = 0;
	spi->callback = config->callback;

	/* Mask SPI interrupts */
	write_imr(DW_SPI_IMR_MASK, info->regs);

	/* Enable the controller */
	set_bit_ssienr(info->regs);

	return DEV_OK;
}

static int spi_dw_slave_select(struct device *dev, uint32_t slave)
{
	struct spi_dw_data *spi = dev->driver_data;

	if (slave == 0 || slave > 4) {
		return DEV_INVALID_CONF;
	}

	spi->slave = 1 << (slave - 1);

	return DEV_OK;
}

static int spi_dw_transceive(struct device *dev,
			     uint8_t *tx_buf, uint32_t tx_buf_len,
			     uint8_t *rx_buf, uint32_t rx_buf_len)
{
	struct spi_dw_config *info = dev->config->config_info;
	struct spi_dw_data *spi = dev->driver_data;

	DBG("spi_dw_transceive: %p, %p, %u, %p, %u\n",
			dev, tx_buf, tx_buf_len, rx_buf, rx_buf_len);

	/* Check status */
	if (test_bit_sr_busy(info->regs)) {
		DBG("spi_dw_transceive: %Controller is busy\n");
		return DEV_USED;
	}

	/* Disable the controller */
	clear_bit_ssienr(info->regs);

	/* Set buffers info */
	spi->tx_buf = tx_buf;
	spi->tx_buf_len = tx_buf_len;
	spi->rx_buf = rx_buf;
	spi->rx_buf_len = rx_buf_len;

	/* Slave select */
	write_ser(spi->slave, info->regs);

	/* Enable interrupts */
	write_imr(DW_SPI_IMR_UNMASK, info->regs);

	/* Enable the controller */
	set_bit_ssienr(info->regs);

	return DEV_OK;
}

static int spi_dw_suspend(struct device *dev)
{
	struct spi_dw_config *info = dev->config->config_info;

	DBG("spi_dw_suspend: %p\n", dev);

	write_imr(DW_SPI_IMR_MASK, info->regs);
	clear_bit_ssienr(info->regs);
	irq_disable(info->irq);
#ifdef CONFIG_SPI_DW_CLOCK_GATE
	clock_control_off(info->clock, info->clock_data);
#endif
	return DEV_OK;
}

static int spi_dw_resume(struct device *dev)
{
	struct spi_dw_config *info = dev->config->config_info;

	DBG("spi_dw_resume: %p\n", dev);

#ifdef CONFIG_SPI_DW_CLOCK_GATE
	clock_control_on(info->clock, info->clock_data);
#endif
	irq_enable(info->irq);
	set_bit_ssienr(info->regs);
	write_imr(DW_SPI_IMR_UNMASK, info->regs);

	return DEV_OK;
}

void spi_dw_isr(void *arg)
{
	struct device *dev = arg;
	struct spi_dw_config *info = dev->config->config_info;
	uint32_t error = 0;
	uint32_t int_status;

	DBG("spi_dw_isr: %p\n", dev);

	int_status = read_isr(info->regs);
	test_bit_icr(info->regs);

	DBG("int_status 0x%x - (tx: %d, rx: %d)\n",
		int_status, read_txflr(info->regs), read_rxflr(info->regs));

	if (int_status & DW_SPI_ISR_ERRORS_MASK) {
		error = 1;
		goto out;
	}

	if (int_status & DW_SPI_ISR_RXFIS) {
		pull_data(dev);
	}

	if (int_status & DW_SPI_ISR_TXEIS) {
		push_data(dev);
	}

out:
	completed(dev, error);
}

static struct spi_driver_api dw_spi_api = {
	.configure = spi_dw_configure,
	.slave_select = spi_dw_slave_select,
	.transceive = spi_dw_transceive,
	.suspend = spi_dw_suspend,
	.resume = spi_dw_resume,
};

int spi_dw_init(struct device *dev)
{
	struct spi_dw_config *info = dev->config->config_info;

	if (read_ssi_comp_version(info->regs) != DW_SSI_COMP_VERSION) {
#ifdef CONFIG_SPI_DW_CLOCK_GATE
		clock_control_off(info->clock, info->clock_data);
#endif
		return DEV_NOT_CONFIG;
	}

	dev->driver_api = &dw_spi_api;

	info->config_func(dev);

	write_imr(DW_SPI_IMR_MASK, info->regs);
	clear_bit_ssienr(info->regs);

	int_unmask(info->int_mask);
	irq_enable(info->irq);

	DBG("Designware SPI driver initialized on device: %p\n", dev);

	return DEV_OK;
}

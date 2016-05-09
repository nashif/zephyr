/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>

#include <device.h>
#include <spi.h>
#include <gpio.h>
#include <board.h>

#include "qm_ss_spi.h"
#include "qm_ss_isr.h"
#include "ss_clk.h"

struct ss_pending_transfer {
	struct device *dev;
	qm_ss_spi_async_transfer_t xfer;
};

static struct ss_pending_transfer pending_transfers[2];

struct ss_spi_qmsi_config {
	qm_ss_spi_t spi;
#ifdef CONFIG_SPI_QMSI_CS_GPIO
	char *cs_port;
	uint32_t cs_pin;
#endif
};

struct ss_spi_qmsi_runtime {
#ifdef CONFIG_SPI_QMSI_CS_GPIO
	struct device *gpio_cs;
#endif
	device_sync_call_t sync;
	qm_ss_spi_config_t cfg;
	int rc;
	bool loopback;
};

static inline qm_ss_spi_bmode_t config_to_bmode(uint8_t mode)
{
	switch (mode) {
	case SPI_MODE_CPHA:
		return QM_SS_SPI_BMODE_1;
	case SPI_MODE_CPOL:
		return QM_SS_SPI_BMODE_2;
	case SPI_MODE_CPOL | SPI_MODE_CPHA:
		return QM_SS_SPI_BMODE_3;
	default:
		return QM_SS_SPI_BMODE_0;
	}
}

#ifdef CONFIG_SPI_QMSI_CS_GPIO
static void spi_control_cs(struct device *dev, bool active)
{
	struct ss_spi_qmsi_runtime *context = dev->driver_data;
	struct ss_spi_qmsi_config *config = dev->config->config_info;
	struct device *gpio = context->gpio_cs;

	if (!gpio)
		return;

	gpio_pin_write(gpio, config->cs_pin, !active);
}
#endif

static int ss_spi_qmsi_configure(struct device *dev,
				 struct spi_config *config)
{
	struct ss_spi_qmsi_runtime *context = dev->driver_data;
	qm_ss_spi_config_t *cfg = &context->cfg;

	cfg->frame_size = SPI_WORD_SIZE_GET(config->config) - 1;
	cfg->bus_mode = config_to_bmode(SPI_MODE(config->config));
	/* As loopback is implemented inside the controller,
	 * the bus mode doesn't matter.
	 */
	context->loopback = SPI_MODE(config->config) & SPI_MODE_LOOP;
	cfg->clk_divider = config->max_sys_freq;

	/* Will set the configuration before the transfer starts */
	return 0;
}

static void spi_qmsi_callback(void *data, int error, qm_ss_spi_status_t status,
			      uint16_t len)
{
	struct ss_spi_qmsi_config *spi_config =
			       ((struct device *)data)->config->config_info;
	qm_ss_spi_t spi_id = spi_config->spi;
	struct ss_pending_transfer *pending = &pending_transfers[spi_id];
	struct device *dev = pending->dev;
	struct ss_spi_qmsi_runtime *context;

	if (!dev)
		return;

	context = dev->driver_data;

#ifdef CONFIG_SPI_QMSI_CS_GPIO
	spi_control_cs(dev, false);
#endif

	pending->dev = NULL;
	context->rc = error;
	device_sync_call_complete(&context->sync);
}

static int ss_spi_qmsi_slave_select(struct device *dev, uint32_t slave)
{
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;
	qm_ss_spi_t spi_id = spi_config->spi;

	return qm_ss_spi_slave_select(spi_id, 1 << (slave - 1)) ? -EIO : 0;
}

static inline uint8_t frame_size_to_dfs(qm_ss_spi_frame_size_t frame_size)
{
	if (frame_size <= QM_SS_SPI_FRAME_SIZE_8_BIT) {
		return 1;
	}

	if (frame_size <= QM_SS_SPI_FRAME_SIZE_16_BIT) {
		return 2;
	}

	/* This should never happen, it will crash later on. */
	return 0;
}

static int ss_spi_qmsi_transceive(struct device *dev,
				  const void *tx_buf, uint32_t tx_buf_len,
				  void *rx_buf, uint32_t rx_buf_len)
{
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;
	qm_ss_spi_t spi_id = spi_config->spi;
	struct ss_spi_qmsi_runtime *context = dev->driver_data;
	qm_ss_spi_config_t *cfg = &context->cfg;
	uint8_t dfs = frame_size_to_dfs(cfg->frame_size);
	qm_ss_spi_async_transfer_t *xfer;
	int rc;

	if (pending_transfers[spi_id].dev) {
		return -EBUSY;
	}

	pending_transfers[spi_id].dev = dev;
	xfer = &pending_transfers[spi_id].xfer;

	xfer->rx = rx_buf;
	xfer->rx_len = rx_buf_len / dfs;
	xfer->tx = (uint8_t *)tx_buf;
	xfer->tx_len = tx_buf_len / dfs;
	xfer->data = dev;
	xfer->callback = spi_qmsi_callback;

	if (tx_buf_len == 0) {
		cfg->transfer_mode = QM_SS_SPI_TMOD_RX;
	} else if (rx_buf_len == 0) {
		cfg->transfer_mode = QM_SS_SPI_TMOD_TX;
	} else {
		cfg->transfer_mode = QM_SS_SPI_TMOD_TX_RX;
	}

	if (context->loopback) {
		QM_SPI[spi_id]->ctrlr0 |= BIT(11);
	}

	rc = qm_ss_spi_set_config(spi_id, cfg);
	if (rc != 0) {
		return -EINVAL;
	}

#ifdef CONFIG_SPI_QMSI_CS_GPIO
	spi_control_cs(dev, true);
#endif

	rc = qm_ss_spi_irq_transfer(spi_id, xfer);
	if (rc != 0) {
#ifdef CONFIG_SPI_QMSI_CS_GPIO
		spi_control_cs(dev, false);
#endif
		return -EIO;
	}

	device_sync_call_wait(&context->sync);

	return context->rc ? -EIO : 0;
}

static int ss_spi_qmsi_suspend(struct device *dev)
{
	/* FIXME */
	return 0;
}

static int ss_spi_qmsi_resume(struct device *dev)
{
	/* FIXME */
	return 0;
}

static struct spi_driver_api ss_spi_qmsi_api = {
	.configure = ss_spi_qmsi_configure,
	.slave_select = ss_spi_qmsi_slave_select,
	.transceive = ss_spi_qmsi_transceive,
	.suspend = ss_spi_qmsi_suspend,
	.resume = ss_spi_qmsi_resume,
};

#ifdef CONFIG_SPI_QMSI_CS_GPIO
static struct device *gpio_cs_init(struct ss_spi_qmsi_config *config)
{
	struct device *gpio;

	if (!config->cs_port)
		return NULL;

	gpio = device_get_binding(config->cs_port);
	if (!gpio)
		return NULL;

	gpio_pin_configure(gpio, config->cs_pin, GPIO_DIR_OUT);
	gpio_pin_write(gpio, config->cs_pin, 1);

	return gpio;
}
#endif

static int ss_spi_qmsi_init(struct device *dev);

#ifdef CONFIG_SPI_QMSI_PORT_0
static struct ss_spi_qmsi_config spi_qmsi_mst_0_config = {
	.spi = QM_SS_SPI_0,
#ifdef CONFIG_SPI_QMSI_CS_GPIO
	.cs_port = CONFIG_SPI_QMSI_PORT_0_CS_GPIO_PORT,
	.cs_pin = CONFIG_SPI_QMSI_PORT_0_CS_GPIO_PIN,
#endif
};

static struct ss_spi_qmsi_runtime spi_qmsi_mst_0_runtime;

DEVICE_INIT(ss_spi_master_0, CONFIG_SPI_QMSI_PORT_0_DRV_NAME,
	    ss_spi_qmsi_init, &spi_qmsi_mst_0_runtime, &spi_qmsi_mst_0_config,
	    SECONDARY, CONFIG_SPI_QMSI_INIT_PRIORITY);


#endif /* CONFIG_SPI_QMSI_PORT_0 */
#ifdef CONFIG_SPI_QMSI_PORT_1

static struct ss_spi_qmsi_config spi_qmsi_mst_1_config = {
	.spi = QM_SS_SPI_1,
#ifdef CONFIG_SPI_QMSI_CS_GPIO
	.cs_port = CONFIG_SPI_QMSI_PORT_1_CS_GPIO_PORT,
	.cs_pin = CONFIG_SPI_QMSI_PORT_1_CS_GPIO_PIN,
#endif
};

static struct ss_spi_qmsi_runtime spi_qmsi_mst_1_runtime;

DEVICE_INIT(ss_spi_master_1, CONFIG_SPI_QMSI_PORT_1_DRV_NAME,
	    ss_spi_qmsi_init, &spi_qmsi_mst_1_runtime, &spi_qmsi_mst_1_config,
	    SECONDARY, CONFIG_SPI_QMSI_INIT_PRIORITY);

#endif /* CONFIG_SPI_QMSI_PORT_1 */

static void ss_spi_err_isr(void *arg)
{
	struct device *dev = arg;
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;

	if (spi_config->spi == QM_SS_SPI_0) {
		qm_ss_spi_0_err_isr(NULL);
	} else {
		qm_ss_spi_1_err_isr(NULL);
	}
}

static void ss_spi_rx_isr(void *arg)
{
	struct device *dev = arg;
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;

	if (spi_config->spi == QM_SS_SPI_0) {
		qm_ss_spi_0_rx_isr(NULL);
	} else {
		qm_ss_spi_1_rx_isr(NULL);
	}
}

static void ss_spi_tx_isr(void *arg)
{
	struct device *dev = arg;
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;

	if (spi_config->spi == QM_SS_SPI_0) {
		qm_ss_spi_0_tx_isr(NULL);
	} else {
		qm_ss_spi_1_tx_isr(NULL);
	}
}

static int ss_spi_qmsi_init(struct device *dev)
{
	struct ss_spi_qmsi_config *spi_config = dev->config->config_info;
	struct ss_spi_qmsi_runtime *context = dev->driver_data;
	uint32_t *scss_intmask = NULL;

	switch (spi_config->spi) {
#ifdef CONFIG_SPI_QMSI_PORT_0
	case QM_SS_SPI_0:
		IRQ_CONNECT(IRQ_SPI0_ERR_INT, CONFIG_SPI_QMSI_PORT_0_PRI,
			    ss_spi_err_isr, DEVICE_GET(ss_spi_master_0), 0);
		irq_enable(IRQ_SPI0_ERR_INT);

		IRQ_CONNECT(IRQ_SPI0_RX_AVAIL, CONFIG_SPI_QMSI_PORT_0_PRI,
			    ss_spi_rx_isr, DEVICE_GET(ss_spi_master_0), 0);
		irq_enable(IRQ_SPI0_RX_AVAIL);

		IRQ_CONNECT(IRQ_SPI0_TX_REQ, CONFIG_SPI_QMSI_PORT_0_PRI,
			    ss_spi_tx_isr, DEVICE_GET(ss_spi_master_0), 0);
		irq_enable(IRQ_SPI0_TX_REQ);

		ss_clk_spi_enable(0);

		/* Route SPI interrupts to Sensor Subsystem */
		scss_intmask = (uint32_t *)&QM_SCSS_INT->int_ss_spi_0;
		*scss_intmask &= ~BIT(8);
		scss_intmask++;
		*scss_intmask &= ~BIT(8);
		scss_intmask++;
		*scss_intmask &= ~BIT(8);
		break;
#endif /* CONFIG_SPI_QMSI_PORT_0 */

#ifdef CONFIG_SPI_QMSI_PORT_1
	case QM_SS_SPI_1:
		IRQ_CONNECT(IRQ_SPI1_ERR_INT, CONFIG_SPI_QMSI_PORT_1_PRI,
			    ss_spi_err_isr, DEVICE_GET(ss_spi_master_1), 0);
		irq_enable(IRQ_SPI1_ERR_INT);

		IRQ_CONNECT(IRQ_SPI1_RX_AVAIL, CONFIG_SPI_QMSI_PORT_1_PRI,
			    ss_spi_rx_isr, DEVICE_GET(ss_spi_master_1), 0);
		irq_enable(IRQ_SPI1_RX_AVAIL);

		IRQ_CONNECT(IRQ_SPI1_TX_REQ, CONFIG_SPI_QMSI_PORT_1_PRI,
			    ss_spi_tx_isr, DEVICE_GET(ss_spi_master_1), 0);
		irq_enable(IRQ_SPI1_TX_REQ);

		ss_clk_spi_enable(1);

		/* Route SPI interrupts to Sensor Subsystem */
		scss_intmask = (uint32_t *)&QM_SCSS_INT->int_ss_spi_1;
		*scss_intmask &= ~BIT(8);
		scss_intmask++;
		*scss_intmask &= ~BIT(8);
		scss_intmask++;
		*scss_intmask &= ~BIT(8);
		break;
#endif /* CONFIG_SPI_QMSI_PORT_1 */

	default:
		return -EIO;
	}

#ifdef CONFIG_SPI_QMSI_CS_GPIO
	context->gpio_cs = gpio_cs_init(spi_config);
#endif
	device_sync_call_init(&context->sync);

	dev->driver_api = &ss_spi_qmsi_api;

	return 0;
}

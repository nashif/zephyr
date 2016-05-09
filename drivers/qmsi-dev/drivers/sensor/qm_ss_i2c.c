/*
 * {% copyright %}
 */
#define SPK_LEN_SS (1)
#define SPK_LEN_FS (2)
#define TX_TL (2)
#define RX_TL (5)

#include <string.h>
#include "qm_ss_i2c.h"
#include "clk.h"

/*
 * NOTE: There are a number of differences between this Sensor Subsystem I2C
 *	driver and the Lakemont version. The IP is not the same, the
 *	functionality is a subset of the features contained on the Lakemont
 *	version:
 *	1. Fast Mode Plus is not supported
 *	2. Slave mode is not supported
 *
 *	The registers are different and the register set is compressed.
 *	Some noteworthy differences are:
 *	1. Clock enable is contained in the QM_SS_I2C_CON register
 *	2. SPKLEN is contained in the QM_SS_I2C_CON register
 *	3. The high and low count values are contained within a single
 *          register
 *	4. There is no raw interrupt status register, QM_SS_I2C_INT_STAT
 *	    takes its place and is non-maskable
 *	5. There is a reduced number of TX abrt source status bits
 *	6. The QM_SS_I2C_DATA_CMD register is different and requires the
 *	    strobe bit to be written to indicate a QM_SS_I2C_DATA_CMD
 *	    register update. There is a push and pop mechanism for using
 *	    the FIFO.
 */

static uint32_t i2c_base[QM_SS_I2C_NUM] = {QM_SS_I2C_0_BASE, QM_SS_I2C_1_BASE};
static qm_ss_i2c_transfer_t i2c_transfer[QM_SS_I2C_NUM];
static uint32_t i2c_write_pos[QM_SS_I2C_NUM], i2c_read_pos[QM_SS_I2C_NUM],
    i2c_read_buffer_remaining[QM_SS_I2C_NUM];

static void controller_enable(const qm_ss_i2c_t i2c);
static void controller_disable(const qm_ss_i2c_t i2c);

static void qm_ss_i2c_isr_handler(const qm_ss_i2c_t i2c)
{
	uint32_t controller = i2c_base[i2c], data_cmd = 0,
		 count_tx = (QM_SS_I2C_FIFO_SIZE - TX_TL);
	qm_ss_i2c_status_t status = 0;
	int rc = 0;

	/* Check for errors */
	QM_ASSERT(!(__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
		    QM_SS_I2C_INTR_STAT_TX_OVER));
	QM_ASSERT(!(__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
		    QM_SS_I2C_INTR_STAT_RX_UNDER));
	QM_ASSERT(!(__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
		    QM_SS_I2C_INTR_STAT_RX_OVER));

	if ((__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
	     QM_SS_I2C_INTR_STAT_TX_ABRT)) {
		QM_ASSERT(
		    !(__builtin_arc_lr(controller + QM_SS_I2C_TX_ABRT_SOURCE) &
		      QM_SS_I2C_TX_ABRT_SBYTE_NORSTRT));

		status =
		    (__builtin_arc_lr(controller + QM_SS_I2C_TX_ABRT_SOURCE) &
		     QM_SS_I2C_TX_ABRT_SOURCE_ALL_MASK);

		/* clear intr */
		__builtin_arc_sr(QM_SS_I2C_INTR_CLR_TX_ABRT,
				 controller + QM_SS_I2C_INTR_CLR);

		/* mask interrupts */
		__builtin_arc_sr(QM_SS_I2C_INTR_MASK_ALL,
				 controller + QM_SS_I2C_INTR_MASK);

		rc = (status & QM_I2C_TX_ABRT_USER_ABRT) ? -ECANCELED : -EIO;

		if (i2c_transfer[i2c].callback) {
			i2c_transfer[i2c].callback(
			    i2c_transfer[i2c].callback_data, rc, status, 0);
		}
	}

	/* RX read from buffer */
	if ((__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
	     QM_SS_I2C_INTR_STAT_RX_FULL)) {

		while (i2c_read_buffer_remaining[i2c] &&
		       (__builtin_arc_lr(controller + QM_SS_I2C_RXFLR))) {
			__builtin_arc_sr(QM_SS_I2C_DATA_CMD_POP,
					 controller + QM_SS_I2C_DATA_CMD);
			/* IC_DATA_CMD[7:0] contains received data */
			i2c_transfer[i2c].rx[i2c_read_pos[i2c]] =
			    __builtin_arc_lr(controller + QM_SS_I2C_DATA_CMD);
			i2c_read_buffer_remaining[i2c]--;
			i2c_read_pos[i2c]++;

			if (i2c_read_buffer_remaining[i2c] == 0) {
				/* mask rx full interrupt if transfer
				 * complete
				 */
				QM_SS_REG_AUX_NAND(
				    (controller + QM_SS_I2C_INTR_MASK),
				    QM_SS_I2C_INTR_MASK_RX_FULL);

				if (i2c_transfer[i2c].stop) {
					controller_disable(i2c);
				}

				if (i2c_transfer[i2c].callback) {
					i2c_transfer[i2c].callback(
					    i2c_transfer[i2c].callback_data, 0,
					    QM_I2C_IDLE, i2c_read_pos[i2c]);
				}
			}
		}
		if (i2c_read_buffer_remaining[i2c] > 0 &&
		    i2c_read_buffer_remaining[i2c] < (RX_TL + 1)) {
			/* Adjust the RX threshold so the next 'RX_FULL'
			 * interrupt is generated when all the remaining
			 * data are received.
			 */
			QM_SS_REG_AUX_NAND((controller + QM_SS_I2C_TL),
					   QM_SS_I2C_TL_RX_TL_MASK);
			QM_SS_REG_AUX_OR((controller + QM_SS_I2C_TL),
					 (i2c_read_buffer_remaining[i2c] - 1));
		}

		/* RX_FULL INTR is autocleared when the buffer
		 * levels goes below the threshold
		 */
	}

	if ((__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
	     QM_SS_I2C_INTR_STAT_TX_EMPTY)) {

		if ((__builtin_arc_lr(controller + QM_SS_I2C_STATUS) &
		     QM_SS_I2C_STATUS_TFE) &&
		    (i2c_transfer[i2c].tx != NULL) &&
		    (i2c_transfer[i2c].tx_len == 0) &&
		    (i2c_transfer[i2c].rx_len == 0)) {

			QM_SS_REG_AUX_NAND((controller + QM_SS_I2C_INTR_MASK),
					   QM_SS_I2C_INTR_MASK_TX_EMPTY);

			/* if this is not a combined
			 * transaction, disable the controller now
			 */
			if ((i2c_read_buffer_remaining[i2c] == 0) &&
			    i2c_transfer[i2c].stop) {
				controller_disable(i2c);

				/* callback */
				if (i2c_transfer[i2c].callback) {
					i2c_transfer[i2c].callback(
					i2c_transfer[i2c].callback_data, 0,
					QM_I2C_IDLE, i2c_write_pos[i2c]);
				}
			}
		}

		while ((count_tx) && i2c_transfer[i2c].tx_len) {
			count_tx--;

			/* write command -IC_DATA_CMD[8] = 0 */
			/* fill IC_DATA_CMD[7:0] with the data */
			data_cmd = QM_SS_I2C_DATA_CMD_PUSH |
				   i2c_transfer[i2c].tx[i2c_write_pos[i2c]];
			i2c_transfer[i2c].tx_len--;

			/* if transfer is a combined transfer, only
			 * send stop at
			 * end of the transfer sequence */
			if (i2c_transfer[i2c].stop &&
			    (i2c_transfer[i2c].tx_len == 0) &&
			    (i2c_transfer[i2c].rx_len == 0)) {

				data_cmd |= QM_SS_I2C_DATA_CMD_STOP;
			}

			/* write data */
			__builtin_arc_sr(data_cmd,
					 controller + QM_SS_I2C_DATA_CMD);
			i2c_write_pos[i2c]++;

			/* TX_EMPTY INTR is autocleared when the buffer
			 * levels goes above the threshold
			 */
		}

		/* TX read command */
		count_tx =
		    QM_SS_I2C_FIFO_SIZE -
		    (__builtin_arc_lr(controller + QM_SS_I2C_TXFLR) +
		     (__builtin_arc_lr(controller + QM_SS_I2C_RXFLR) + 1));

		while (i2c_transfer[i2c].rx_len &&
		       (i2c_transfer[i2c].tx_len == 0) && count_tx) {
			count_tx--;
			i2c_transfer[i2c].rx_len--;

			/* if transfer is a combined transfer, only
			 * send stop at
			 * end of
			 * the transfer sequence */
			if (i2c_transfer[i2c].stop &&
			    (i2c_transfer[i2c].rx_len == 0) &&
			    (i2c_transfer[i2c].tx_len == 0)) {

				__builtin_arc_sr((QM_SS_I2C_DATA_CMD_CMD |
						  QM_SS_I2C_DATA_CMD_PUSH |
						  QM_SS_I2C_DATA_CMD_STOP),
						 controller +
						     QM_SS_I2C_DATA_CMD);
			} else {

				__builtin_arc_sr((QM_SS_I2C_DATA_CMD_CMD |
						  QM_SS_I2C_DATA_CMD_PUSH),
						 controller +
						     QM_SS_I2C_DATA_CMD);
			}
		}

		/* generate a tx_empty interrupt when tx fifo is fully
		 * empty */
		if ((i2c_transfer[i2c].tx_len == 0) &&
		    (i2c_transfer[i2c].rx_len == 0)) {
			QM_SS_REG_AUX_NAND((controller + QM_SS_I2C_TL),
					   QM_SS_I2C_TL_TX_TL_MASK);
		}
	}
}

QM_ISR_DECLARE(qm_ss_i2c_isr_0)
{
	qm_ss_i2c_isr_handler(QM_SS_I2C_0);
}

QM_ISR_DECLARE(qm_ss_i2c_isr_1)
{
	qm_ss_i2c_isr_handler(QM_SS_I2C_1);
}

static uint32_t get_lo_cnt(uint32_t lo_time_ns)
{
	return (((clk_sys_get_ticks_per_us() * lo_time_ns) / 1000) - 1);
}

static uint32_t get_hi_cnt(qm_ss_i2c_t i2c, uint32_t hi_time_ns)
{
	uint32_t controller = i2c_base[i2c];

	return (((clk_sys_get_ticks_per_us() * hi_time_ns) / 1000) - 7 -
		((__builtin_arc_lr(controller + QM_SS_I2C_CON) &
		  QM_SS_I2C_CON_SPKLEN_MASK) >>
		 QM_SS_I2C_CON_SPKLEN_OFFSET));
}

int qm_ss_i2c_set_config(const qm_ss_i2c_t i2c,
			 const qm_ss_i2c_config_t *const cfg)
{
	uint32_t controller = i2c_base[i2c], lcnt = 0, hcnt = 0, full_cnt = 0,
		 min_lcnt = 0, lcnt_diff = 0,
		 con = (__builtin_arc_lr(controller + QM_SS_I2C_CON) &
			QM_SS_I2C_CON_CLK_ENA);
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	/* mask all interrupts */
	__builtin_arc_sr(QM_SS_I2C_INTR_MASK_ALL,
			 controller + QM_SS_I2C_INTR_MASK);

	/* disable controller */
	controller_disable(i2c);

	/* set mode */
	con |= QM_SS_I2C_CON_RESTART_EN |
	       /* set 7/10 bit address mode */
	       (cfg->address_mode << QM_SS_I2C_CON_IC_10BITADDR_OFFSET);

	/*
	 * Timing generation algorithm:
	 * 1. compute hi/lo count so as to achieve the desired bus
	 *    speed at 50% duty cycle
	 * 2. adjust the hi/lo count to ensure that minimum hi/lo
	 *    timings are guaranteed as per spec.
	 */

	switch (cfg->speed) {
	case QM_SS_I2C_SPEED_STD:

		con |= QM_SS_I2C_CON_SPEED_SS |
		       SPK_LEN_SS << QM_SS_I2C_CON_SPKLEN_OFFSET;

		__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

		min_lcnt = get_lo_cnt(QM_I2C_MIN_SS_NS);
		lcnt = get_lo_cnt(QM_I2C_SS_50_DC_NS);
		hcnt = get_hi_cnt(i2c, QM_I2C_SS_50_DC_NS);
		break;

	case QM_SS_I2C_SPEED_FAST:
		con |= QM_SS_I2C_CON_SPEED_FS |
		       SPK_LEN_FS << QM_SS_I2C_CON_SPKLEN_OFFSET;

		__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

		min_lcnt = get_lo_cnt(QM_I2C_MIN_FS_NS);
		lcnt = get_lo_cnt(QM_I2C_FS_50_DC_NS);
		hcnt = get_hi_cnt(i2c, QM_I2C_FS_50_DC_NS);
		break;
	}

	if (hcnt > QM_SS_I2C_IC_HCNT_MAX || hcnt < QM_SS_I2C_IC_HCNT_MIN) {
		return -EINVAL;
	}

	if (lcnt > QM_SS_I2C_IC_LCNT_MAX || lcnt < QM_SS_I2C_IC_LCNT_MIN) {
		return -EINVAL;
	}

	/* Increment minimum low count to account for rounding down */
	min_lcnt++;
	if (lcnt < min_lcnt) {
		lcnt_diff = (min_lcnt - lcnt);
		lcnt += (lcnt_diff);
		hcnt -= (lcnt_diff);
	}

	full_cnt = (lcnt & 0xFFFF) |
		   (hcnt & 0xFFFF) << QM_SS_I2C_SS_FS_SCL_CNT_HCNT_OFFSET;

	if (QM_SS_I2C_SPEED_STD == cfg->speed) {
		__builtin_arc_sr(full_cnt, controller + QM_SS_I2C_SS_SCL_CNT);

	} else {
		__builtin_arc_sr(full_cnt, controller + QM_SS_I2C_FS_SCL_CNT);
	}

	return 0;
}

int qm_ss_i2c_set_speed(const qm_ss_i2c_t i2c, const qm_ss_i2c_speed_t speed,
			const uint16_t lo_cnt, const uint16_t hi_cnt)
{
	uint32_t full_cnt = 0, controller = i2c_base[i2c],
		 con = __builtin_arc_lr(controller + QM_SS_I2C_CON);
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	QM_CHECK(hi_cnt < QM_SS_I2C_IC_HCNT_MAX &&
		     lo_cnt > QM_SS_I2C_IC_HCNT_MIN,
		 -EINVAL);
	QM_CHECK(lo_cnt < QM_SS_I2C_IC_LCNT_MAX &&
		     lo_cnt > QM_SS_I2C_IC_LCNT_MIN,
		 -EINVAL);

	con &= ~QM_SS_I2C_CON_SPEED_MASK;

	full_cnt = (lo_cnt & QM_SS_I2C_SS_FS_SCL_CNT_16BIT_MASK) |
		   (hi_cnt & QM_SS_I2C_SS_FS_SCL_CNT_16BIT_MASK)
		       << QM_SS_I2C_SS_FS_SCL_CNT_HCNT_OFFSET;

	switch (speed) {
	case QM_SS_I2C_SPEED_STD:
		con |= QM_SS_I2C_CON_SPEED_SS;
		__builtin_arc_sr(full_cnt, controller + QM_SS_I2C_SS_SCL_CNT);
		break;

	case QM_SS_I2C_SPEED_FAST:
		con |= QM_SS_I2C_CON_SPEED_FS;
		__builtin_arc_sr(full_cnt, controller + QM_SS_I2C_FS_SCL_CNT);
		break;
	}

	__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

	return 0;
}

int qm_ss_i2c_get_status(const qm_ss_i2c_t i2c,
			 qm_ss_i2c_status_t *const status)
{
	uint32_t controller = i2c_base[i2c];
	QM_CHECK(status != NULL, -EINVAL);

	*status = 0;

	/* check if slave or master are active */
	if (__builtin_arc_lr(controller + QM_SS_I2C_STATUS) &
	    QM_SS_I2C_STATUS_BUSY_MASK) {
		*status |= QM_I2C_BUSY;
	}

	/* check for abort status */
	*status |= (__builtin_arc_lr(controller + QM_SS_I2C_TX_ABRT_SOURCE) &
		    QM_SS_I2C_TX_ABRT_SOURCE_ALL_MASK);

	return 0;
}

int qm_ss_i2c_master_write(const qm_ss_i2c_t i2c, const uint16_t slave_addr,
			   const uint8_t *const data, uint32_t len,
			   const bool stop, qm_ss_i2c_status_t *const status)
{
	uint8_t *d = (uint8_t *)data;
	uint32_t controller = i2c_base[i2c],
		 con = __builtin_arc_lr(controller + QM_SS_I2C_CON),
		 data_cmd = 0;
	int ret = 0;

	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	QM_CHECK(data != NULL, -EINVAL);
	QM_CHECK(len > 0, -EINVAL);

	/* write slave address to TAR */
	con &= ~QM_SS_I2C_CON_TAR_SAR_MASK;
	con |= (slave_addr & QM_SS_I2C_CON_TAR_SAR_10_BIT_MASK)
	       << QM_SS_I2C_CON_TAR_SAR_OFFSET;
	__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

	/* enable controller */
	controller_enable(i2c);

	while (len--) {

		/* wait if FIFO is full */
		while (!((__builtin_arc_lr(controller + QM_SS_I2C_STATUS)) &
			 QM_SS_I2C_STATUS_TFNF))
			;

		/* write command -IC_DATA_CMD[8] = 0 */
		/* fill IC_DATA_CMD[7:0] with the data */
		data_cmd = *d;
		data_cmd |= QM_SS_I2C_DATA_CMD_PUSH;

		/* send stop after last byte */
		if (len == 0 && stop) {
			data_cmd |= QM_SS_I2C_DATA_CMD_STOP;
		}

		__builtin_arc_sr(data_cmd, controller + QM_SS_I2C_DATA_CMD);
		d++;
	}

	/* this is a blocking call, wait until FIFO is empty or tx abrt
	 * error */
	while (!(__builtin_arc_lr(controller + QM_SS_I2C_STATUS) &
		 QM_SS_I2C_STATUS_TFE))
		;

	if ((__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
	     QM_SS_I2C_INTR_STAT_TX_ABRT)) {
		ret = -EIO;
	}

	/*  disable controller */
	if (true == stop) {
		controller_disable(i2c);
	}

	if (status != NULL) {
		qm_ss_i2c_get_status(i2c, status);
	}

	/* Clear abort status
	 * The controller flushes/resets/empties
	 * the TX FIFO whenever this bit is set. The TX
	 * FIFO remains in this flushed state until the
	 * register IC_CLR_TX_ABRT is read.
	 */
	__builtin_arc_sr(QM_SS_I2C_INTR_CLR_TX_ABRT,
			 controller + QM_SS_I2C_INTR_CLR);

	return ret;
}

int qm_ss_i2c_master_read(const qm_ss_i2c_t i2c, const uint16_t slave_addr,
			  uint8_t *const data, uint32_t len, const bool stop,
			  qm_ss_i2c_status_t *const status)
{
	uint32_t controller = i2c_base[i2c],
		 con = __builtin_arc_lr(controller + QM_SS_I2C_CON),
		 data_cmd = QM_SS_I2C_DATA_CMD_CMD | QM_SS_I2C_DATA_CMD_PUSH;
	uint8_t *d = (uint8_t *)data;
	int ret = 0;

	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	QM_CHECK(data != NULL, -EINVAL);
	QM_CHECK(len > 0, -EINVAL);

	/* write slave address to TAR */
	con &= ~QM_SS_I2C_CON_TAR_SAR_MASK;
	con |= (slave_addr & QM_SS_I2C_CON_TAR_SAR_10_BIT_MASK)
	       << QM_SS_I2C_CON_TAR_SAR_OFFSET;
	__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

	/* enable controller */
	controller_enable(i2c);

	while (len--) {
		if (len == 0 && stop) {
			data_cmd |= QM_SS_I2C_DATA_CMD_STOP;
		}

		__builtin_arc_sr(data_cmd, controller + QM_SS_I2C_DATA_CMD);

		/*  wait if rx fifo is empty, break if tx empty and
		 * error*/
		while (!(__builtin_arc_lr(controller + QM_SS_I2C_STATUS) &
			 QM_SS_I2C_STATUS_RFNE)) {
			if (__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
			    QM_SS_I2C_INTR_STAT_TX_ABRT) {
				break;
			}
		}

		if ((__builtin_arc_lr(controller + QM_SS_I2C_INTR_STAT) &
		     QM_SS_I2C_INTR_STAT_TX_ABRT)) {
			ret = -EIO;
			break;
		}

		__builtin_arc_sr(QM_SS_I2C_DATA_CMD_POP,
				 controller + QM_SS_I2C_DATA_CMD);

		/* wait until rx fifo is empty, indicating pop is complete*/
		while ((__builtin_arc_lr(controller + QM_SS_I2C_STATUS) &
			 QM_SS_I2C_STATUS_RFNE));

		/* IC_DATA_CMD[7:0] contains received data */
		*d = __builtin_arc_lr(controller + QM_SS_I2C_DATA_CMD);
		d++;
	}

	/*  disable controller */
	if (true == stop) {
		controller_disable(i2c);
	}

	if (status != NULL) {
		qm_ss_i2c_get_status(i2c, status);
	}

	/* Clear abort status
	 * The controller flushes/resets/empties
	 * the TX FIFO whenever this bit is set. The TX
	 * FIFO remains in this flushed state until the
	 * register IC_CLR_TX_ABRT is read.
	 */
	__builtin_arc_sr(QM_SS_I2C_INTR_CLR_TX_ABRT,
			 controller + QM_SS_I2C_INTR_CLR);

	return ret;
}

int qm_ss_i2c_master_irq_transfer(const qm_ss_i2c_t i2c,
				  const qm_ss_i2c_transfer_t *const xfer,
				  const uint16_t slave_addr)
{
	uint32_t controller = i2c_base[i2c],
		 con = __builtin_arc_lr(controller + QM_SS_I2C_CON);
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	QM_CHECK(NULL != xfer, -EINVAL);

	/* write slave address to TAR */
	con &= ~QM_SS_I2C_CON_TAR_SAR_MASK;
	con |= (slave_addr & QM_SS_I2C_CON_TAR_SAR_10_BIT_MASK)
	       << QM_SS_I2C_CON_TAR_SAR_OFFSET;
	__builtin_arc_sr(con, controller + QM_SS_I2C_CON);

	i2c_write_pos[i2c] = 0;
	i2c_read_pos[i2c] = 0;
	i2c_read_buffer_remaining[i2c] = xfer->rx_len;
	memcpy(&i2c_transfer[i2c], xfer, sizeof(i2c_transfer[i2c]));

	/* set threshold */
	if (xfer->rx_len > 0 && xfer->rx_len < (RX_TL + 1)) {
		/* If 'rx_len' is less than the default threshold, we have to
		 * change the threshold value so the 'RX FULL' interrupt is
		 * generated once all data from the transfer is received.
		 */
		__builtin_arc_sr(
		    ((TX_TL << QM_SS_I2C_TL_TX_TL_OFFSET) | (xfer->rx_len - 1)),
		    controller + QM_SS_I2C_TL);
	} else {
		__builtin_arc_sr(((TX_TL << QM_SS_I2C_TL_TX_TL_OFFSET) | RX_TL),
				 controller + QM_SS_I2C_TL);
	}

	/* mask interrupts */
	__builtin_arc_sr(QM_SS_I2C_INTR_MASK_ALL,
			 controller + QM_SS_I2C_INTR_MASK);

	/* enable controller */
	controller_enable(i2c);

	/* unmask interrupts */
	__builtin_arc_sr(
	    (QM_SS_I2C_INTR_MASK_TX_ABRT | QM_SS_I2C_INTR_MASK_TX_EMPTY |
	     QM_SS_I2C_INTR_MASK_TX_OVER | QM_SS_I2C_INTR_MASK_RX_FULL |
	     QM_SS_I2C_INTR_MASK_RX_OVER | QM_SS_I2C_INTR_MASK_RX_UNDER),
	    controller + QM_SS_I2C_INTR_MASK);

	return 0;
}

static void controller_enable(const qm_ss_i2c_t i2c)
{
	uint32_t controller = i2c_base[i2c];
	if (!(__builtin_arc_lr(controller + QM_SS_I2C_ENABLE_STATUS) &
	      QM_SS_I2C_ENABLE_STATUS_IC_EN)) {
		/* enable controller */
		QM_SS_REG_AUX_OR((controller + QM_SS_I2C_CON),
				 QM_SS_I2C_CON_ENABLE);
		/* wait until controller is enabled */
		while (
		    !(__builtin_arc_lr(controller + QM_SS_I2C_ENABLE_STATUS) &
		      QM_SS_I2C_ENABLE_STATUS_IC_EN))
			;
	}
}

static void controller_disable(const qm_ss_i2c_t i2c)
{
	uint32_t controller = i2c_base[i2c];
	if (__builtin_arc_lr(controller + QM_SS_I2C_ENABLE_STATUS) &
	    QM_SS_I2C_ENABLE_STATUS_IC_EN) {
		/* disable controller */
		QM_SS_REG_AUX_NAND((controller + QM_SS_I2C_CON),
				   QM_SS_I2C_CON_ENABLE);

		/* wait until controller is disabled */
		while ((__builtin_arc_lr(controller + QM_SS_I2C_ENABLE_STATUS) &
			QM_SS_I2C_ENABLE_STATUS_IC_EN))
			;
	}
}

int qm_ss_i2c_irq_transfer_terminate(const qm_ss_i2c_t i2c)
{
	uint32_t controller = i2c_base[i2c];
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);

	/* Abort:
	 * In response to an ABORT, the controller issues a STOP and
	 * flushes
	 * the Tx FIFO after completing the current transfer, then sets
	 * the
	 * TX_ABORT interrupt after the abort operation. The ABORT bit
	 * is
	 * cleared automatically by hardware after the abort operation.
	 */
	QM_SS_REG_AUX_OR((controller + QM_SS_I2C_CON), QM_SS_I2C_CON_ABORT);

	return 0;
}

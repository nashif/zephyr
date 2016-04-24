/*
 * {% copyright %}
 */

#include "qm_common.h"
#include "ss_clk.h"

int ss_clk_gpio_enable(const qm_ss_gpio_t gpio)
{
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	int addr =
	    (gpio == QM_SS_GPIO_0) ? QM_SS_GPIO_0_BASE : QM_SS_GPIO_1_BASE;
	__builtin_arc_sr(QM_SS_GPIO_LS_SYNC_CLK_EN |
			     QM_SS_GPIO_LS_SYNC_SYNC_LVL,
			 addr + QM_SS_GPIO_LS_SYNC);
	return 0;
}

int ss_clk_gpio_disable(const qm_ss_gpio_t gpio)
{
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	int addr =
	    (gpio == QM_SS_GPIO_0) ? QM_SS_GPIO_0_BASE : QM_SS_GPIO_1_BASE;
	__builtin_arc_sr(0, addr + QM_SS_GPIO_LS_SYNC);
	return 0;
}

int ss_clk_spi_enable(const qm_ss_spi_t spi)
{
	QM_CHECK(spi < QM_SS_SPI_NUM, -EINVAL);
	int addr = (spi == QM_SS_SPI_0) ? QM_SS_SPI_0_BASE : QM_SS_SPI_1_BASE;
	QM_SS_REG_AUX_OR(addr + QM_SS_SPI_CTRL, QM_SS_SPI_CTRL_CLK_ENA);
	return 0;
}

int ss_clk_spi_disable(const qm_ss_spi_t spi)
{
	QM_CHECK(spi < QM_SS_SPI_NUM, -EINVAL);
	int addr = (spi == QM_SS_SPI_0) ? QM_SS_SPI_0_BASE : QM_SS_SPI_1_BASE;
	QM_SS_REG_AUX_NAND(addr + QM_SS_SPI_CTRL, QM_SS_SPI_CTRL_CLK_ENA);
	return 0;
}

int ss_clk_i2c_enable(const qm_ss_i2c_t i2c)
{
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	int addr = (i2c == QM_SS_I2C_0) ? QM_SS_I2C_0_BASE : QM_SS_I2C_1_BASE;
	QM_SS_REG_AUX_OR(addr + QM_SS_I2C_CON, QM_SS_I2C_CON_CLK_ENA);
	return 0;
}

int ss_clk_i2c_disable(const qm_ss_i2c_t i2c)
{
	QM_CHECK(i2c < QM_SS_I2C_NUM, -EINVAL);
	int addr = (i2c == QM_SS_I2C_0) ? QM_SS_I2C_0_BASE : QM_SS_I2C_1_BASE;
	QM_SS_REG_AUX_NAND(addr + QM_SS_I2C_CON, QM_SS_I2C_CON_CLK_ENA);
	return 0;
}

int ss_clk_adc_enable(void)
{
	/* Enable the ADC clock */
	QM_SS_REG_AUX_OR(QM_SS_ADC_BASE + QM_SS_ADC_CTRL,
			 QM_SS_ADC_CTRL_CLK_ENA);
	return 0;
}

int ss_clk_adc_disable(void)
{
	/* Disable the ADC clock */
	QM_SS_REG_AUX_NAND(QM_SS_ADC_BASE + QM_SS_ADC_CTRL,
			   QM_SS_ADC_CTRL_CLK_ENA);
	return 0;
}

int ss_clk_adc_set_div(const uint32_t div)
{
	uint32_t reg;

	/*
	 * Scale the max divisor with the system clock speed. Clock speeds less
	 * than 1 MHz will not work properly.
	 */
	QM_CHECK(div <= QM_SS_ADC_DIV_MAX * clk_sys_get_ticks_per_us(),
		 -EINVAL);

	/* Set the ADC divisor */
	reg = __builtin_arc_lr(QM_SS_ADC_BASE + QM_SS_ADC_DIVSEQSTAT);
	reg &= ~(QM_SS_ADC_DIVSEQSTAT_CLK_RATIO_MASK);
	__builtin_arc_sr(reg | div, QM_SS_ADC_BASE + QM_SS_ADC_DIVSEQSTAT);

	return 0;
}

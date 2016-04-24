/*
 * {% copyright %}
 */

#ifndef __QM_WDT_H__
#define __QM_WDT_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Watchdog timer.
 *
 * @defgroup groupWDT WDT
 * @{
 */

/** Watchdog enable. */
#define QM_WDT_ENABLE (BIT(0))
/** Watchdog mode. */
#define QM_WDT_MODE (BIT(1))
/** Watchdog mode offset. */
#define QM_WDT_MODE_OFFSET (1)

/** Watchdog Timeout Mask. */
#define QM_WDT_TIMEOUT_MASK (0xF)

/**
 * WDT Mode type.
 */
typedef enum {
	/** Watchdog Reset Response Mode.
	 *
	 * The watchdog will request a SoC Warm Reset on a timeout.
	 */
	QM_WDT_MODE_RESET,

	/** Watchdog Interrupt Reset Response Mode.
	 *
	 * The watchdog will generate an interrupt on first timeout.
	 * If interrupt has not been cleared by the second timeout
	 * the watchdog will then request a SoC Warm Reset.
	 */
	QM_WDT_MODE_INTERRUPT_RESET
} qm_wdt_mode_t;

/**
 * WDT clock cycles for timeout type. This value is a power of 2.
 */
typedef enum {
	QM_WDT_2_POW_16_CYCLES, /**< 16 clock cycles timeout. */
	QM_WDT_2_POW_17_CYCLES, /**< 17 clock cycles timeout. */
	QM_WDT_2_POW_18_CYCLES, /**< 18 clock cycles timeout. */
	QM_WDT_2_POW_19_CYCLES, /**< 19 clock cycles timeout. */
	QM_WDT_2_POW_20_CYCLES, /**< 20 clock cycles timeout. */
	QM_WDT_2_POW_21_CYCLES, /**< 21 clock cycles timeout. */
	QM_WDT_2_POW_22_CYCLES, /**< 22 clock cycles timeout. */
	QM_WDT_2_POW_23_CYCLES, /**< 23 clock cycles timeout. */
	QM_WDT_2_POW_24_CYCLES, /**< 24 clock cycles timeout. */
	QM_WDT_2_POW_25_CYCLES, /**< 25 clock cycles timeout. */
	QM_WDT_2_POW_26_CYCLES, /**< 26 clock cycles timeout. */
	QM_WDT_2_POW_27_CYCLES, /**< 27 clock cycles timeout. */
	QM_WDT_2_POW_28_CYCLES, /**< 28 clock cycles timeout. */
	QM_WDT_2_POW_29_CYCLES, /**< 29 clock cycles timeout. */
	QM_WDT_2_POW_30_CYCLES, /**< 30 clock cycles timeout. */
	QM_WDT_2_POW_31_CYCLES, /**< 31 clock cycles timeout. */
	QM_WDT_2_POW_CYCLES_NUM
} qm_wdt_clock_timeout_cycles_t;

/**
 * QM WDT configuration type.
 */
typedef struct {
	qm_wdt_clock_timeout_cycles_t timeout; /**< Timeout in cycles. */
	qm_wdt_mode_t mode;		       /**< Watchdog response mode. */

	/**
	 * User callback.
	 *
	 * param[in] data Callback user data.
	 */
	void (*callback)(void *data);
	void *callback_data; /**< Callback user data. */
} qm_wdt_config_t;

/**
 * Start WDT.
 *
 * Once started, WDT can only be stopped by a SoC reset.
 *
 * @param[in] wdt WDT index.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_wdt_start(const qm_wdt_t wdt);

/**
 * Set configuration of WDT module.
 *
 * This includes the timeout period in PCLK cycles, the WDT mode of operation.
 * It also registers an ISR to the user defined callback.
 *
 * @param[in] wdt WDT index.
 * @param[in] cfg New configuration for WDT.
 *                This must not be NULL.
 *                If QM_WDT_MODE_INTERRUPT_RESET mode is set,
 *                the 'callback' cannot be null.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_wdt_set_config(const qm_wdt_t wdt, const qm_wdt_config_t *const cfg);

/**
 * Reload the WDT counter.
 *
 * Reload the WDT counter with safety value, i.e. service the watchdog.
 *
 * @param[in] wdt WDT index.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_wdt_reload(const qm_wdt_t wdt);

/**
 * @}
 */

#endif /* __QM_WDT_H__ */

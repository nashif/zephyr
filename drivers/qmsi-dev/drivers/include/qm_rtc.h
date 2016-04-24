/*
 * {% copyright %}
 */

#ifndef __QM_RTC_H__
#define __QM_RTC_H__

#include "qm_common.h"
#include "qm_soc_regs.h"
#include "clk.h"

/**
 * Real Time clock.
 *
 * @defgroup groupRTC RTC
 * @{
 */

#define QM_RTC_DIVIDER CLK_RTC_DIV_1

#define QM_RTC_CCR_INTERRUPT_ENABLE BIT(0)
#define QM_RTC_CCR_INTERRUPT_MASK BIT(1)
#define QM_RTC_CCR_ENABLE BIT(2)

#define QM_RTC_ALARM_SECOND (32768 / BIT(QM_RTC_DIVIDER))
#define QM_RTC_ALARM_MINUTE (QM_RTC_ALARM_SECOND * 60)
#define QM_RTC_ALARM_HOUR (QM_RTC_ALARM_MINUTE * 60)
#define QM_RTC_ALARM_DAY (QM_RTC_ALARM_HOUR * 24)

/**
 * RTC configuration type.
 */
typedef struct {
	uint32_t init_val;  /**< Initial value in RTC clocks. */
	bool alarm_en;      /**< Alarm enable. */
	uint32_t alarm_val; /**< Alarm value in RTC clocks. */
	/**
	 * User callback.
	 *
	 * @param[in] data User defined data.
	 */
	void (*callback)(void *data);
	void *callback_data; /**< Callback user data. */
} qm_rtc_config_t;

/**
 * Set RTC configuration.
 *
 * This includes the initial value in RTC clock periods, and the alarm value if
 * an alarm is required. If the alarm is enabled, register an ISR with the user
 * defined callback function.
 *
 * @param[in] rtc RTC index.
 * @param[in] cfg New RTC configuration. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_rtc_set_config(const qm_rtc_t rtc, const qm_rtc_config_t *const cfg);

/**
 * Set Alarm value.
 *
 * Set a new RTC alarm value after an alarm, that has been set using the
 * qm_rtc_set_config function, has expired and a new alarm value is required.
 *
 * @param[in] rtc RTC index.
 * @param[in] alarm_val Value to set alarm to.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_rtc_set_alarm(const qm_rtc_t rtc, const uint32_t alarm_val);

/**
 * @}
 */

#endif /* __QM_RTC_H__ */

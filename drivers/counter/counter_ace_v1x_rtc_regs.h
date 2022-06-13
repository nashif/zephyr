#ifndef __COUNTER_ACE_V1X_RTC_REGS__
#define __COUNTER_ACE_V1X_RTC_REGS__

#if CONFIG_ACE_V1X_RTC_COUNTER

union DfRTCWC {
	uint64_t full;
	struct {
		uint32_t	high : 32;
		uint32_t	low : 32;
	} part;
};

#define ACE_RTC_COUNTER_ID DT_NODELABEL(ace_rtc_counter)

/* RTC Wall Clock */
#define ACE_RTCWC	(DT_REG_ADDR(ACE_RTC_COUNTER_ID))

#endif

#endif /*__COUNTER_ACE_V1X_RTC_REGS__*/

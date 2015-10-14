/*
 * Copyright (c) 2015 Intel Corporation.
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

#ifndef _RTC_H_
#define _RTC_H_
#include <stdint.h>


#define RTC_DIVIDER 1

/** Number of RTC ticks in a second */
#define RTC_ALARM_SECOND (32768 / RTC_DIVIDER)
/** Number of RTC ticks in a minute */
#define RTC_ALARM_MINUTE (RTC_ALARM_SECOND * 60)
/** Number of RTC ticks in an hour */
#define RTC_ALARM_HOUR (RTC_ALARM_MINUTE * 60)
/** Number of RTC ticks in a day */
#define RTC_ALARM_DAY (RTC_ALARM_HOUR * 24)



typedef struct {
        uint32_t init_val;
} rtc_config_t;

typedef struct
{
    uint8_t alarm_enable;           /*!< enable/disable alarm  */
    uint32_t alarm_val;             /*!< initial configuration value for the 32bit RTC alarm value  */
    void (*cb_fn)(void);	    /*!< Pointer to function to call when alarm value matches current RTC value */
} rtc_alarm_t;


typedef void (*rtc_api_enable)(void);
typedef void (*rtc_api_disable)(void);
typedef void (*rtc_api_clock_disable)(void);
typedef int (*rtc_api_set_config)(rtc_config_t *config);
typedef int (*rtc_api_set_alarm)(rtc_alarm_t *alarm_val);
typedef uint32_t (*rtc_api_read)(void);

struct rtc_driver_api {
	rtc_api_enable enable;
	rtc_api_disable disable;
	rtc_api_clock_disable clock_disable;
	rtc_api_read read;
	rtc_api_set_config set_config;
	rtc_api_set_alarm set_alarm;
};

static inline uint32_t rtc_read(struct device *dev)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	return api->read();
}

static inline void rtc_enable(struct device *dev)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	api->enable();
}

static inline void rtc_clock_disable(struct device *dev)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	api->clock_disable();
}

static inline void rtc_disable(struct device *dev)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	api->disable();
}

static inline int rtc_set_config(struct device *dev, rtc_config_t *cfg)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	return api->set_config(cfg);
}

static inline int rtc_set_alarm(struct device *dev, rtc_alarm_t *alarm)
{
	struct rtc_driver_api *api;

	api = (struct rtc_driver_api *)dev->driver_api;
	return api->set_alarm(alarm);
}

#endif

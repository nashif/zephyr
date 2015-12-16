/*
 * Copyright (c) 2015 Intel Corporation
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

#include <zephyr.h>

#include <misc/printk.h>

#include <device.h>
#include <i2c.h>

#include <display/grove_lcd.h>

/**
 * @file Display a counter through ARC I2C and Grove LCD.
 */

#define SLEEPTIME  100
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 1000)

uint8_t clamp_rgb(int val)
{
	if (val > 255) {
		return 255;
	} else if (val < 0) {
		return 0;
	} else {
		return val;
	}
}

void main(void)
{
	struct nano_timer timer;
	uint32_t timer_data[2] = {0, 0};
	struct device *glcd;
	char str[20];
	int rgb[] = { 0x0, 0x0, 0x0 };
	uint8_t rgb_chg[3];
	const uint8_t rgb_step = 16;
	uint8_t set_config;
	int i, j, m;
	int cnt;

	nano_timer_init(&timer, timer_data);

	glcd = device_get_binding(GROVE_LCD_NAME);
	if (!glcd) {
		printk("Grove LCD: Device not found.\n");
	}

	/* Now configure the LCD the way we want it */

	set_config = GLCD_FS_ROWS_2
			| GLCD_FS_DOT_SIZE_LITTLE
			| GLCD_FS_8BIT_MODE;

	glcd_function_set(glcd, set_config);

	set_config = GLCD_DS_DISPLAY_ON | GLCD_DS_CURSOR_ON | GLCD_DS_BLINK_ON;

	glcd_display_state_set(glcd, set_config);

	/* Setup variables /*/
	for (i = 0; i < sizeof(str); i++) {
		str[i] = '\0';
	}

	/* Starting counting */
	cnt = 0;

	while (1) {
		glcd_clear(glcd);

		/* RGB values are from 0 - 511.
		 * First half means incrementally brighter.
		 * Second half is opposite (i.e. goes darker).
		 */

		/* Update the RGB values for backlight */
		m = (rgb[2] > 255) ? (512 - rgb[2]) : (rgb[2]);
		rgb_chg[2] = clamp_rgb(m);

		m = (rgb[1] > 255) ? (512 - rgb[1]) : (rgb[1]);
		rgb_chg[1] = clamp_rgb(m);

		m = (rgb[0] > 255) ? (512 - rgb[0]) : (rgb[0]);
		rgb_chg[0] = clamp_rgb(m);

		glcd_color_set(glcd, rgb_chg[0], rgb_chg[1], rgb_chg[2]);

		/* Display the counter
		 *
		 * well... sprintf() might be easier,
		 * but this is more fun.
		 */
		m = cnt;
		i = 1000000000;
		j = 0;
		while (i > 0) {
			str[j] = '0' + (m / i);

			m = m % i;
			i = i / 10;
			j++;
		}
		cnt++;

		glcd_print(glcd, str, j);

		/* Rotate RGB values */
		rgb[2] += rgb_step;
		if (rgb[2] > 511) {
			rgb[2] = 0;
			rgb[1] += rgb_step;
		}
		if (rgb[1] > 511) {
			rgb[1] = 0;
			rgb[0] += rgb_step;
		}
		if (rgb[0] > 511) {
			rgb[0] = 0;
		}

		/* wait a while */
		nano_task_timer_start(&timer, SLEEPTICKS);
		nano_task_timer_wait(&timer);
	}
}

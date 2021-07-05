/*
 * Copyright (c) 2015-2016 Wind River Systems, Inc.
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

/**
 * @file C++ Synchronization demo.  Uses basic C++ functionality.
 */

#include <zephyr.h>
#include <stdio.h>
#include <sys/printk.h>
#include "MCP9808.h"


Zephyr_MCP9808 tempsensor = Zephyr_MCP9808();

int main(void)
{
	float c;
	printk("testing...\n");
	tempsensor.begin();
	tempsensor.printSensorDetails();


	while(1) {
		c = tempsensor.readTempC();
		printk("Temp: %f C\n", c);
		k_msleep(1000);
	}
	return 0;
}

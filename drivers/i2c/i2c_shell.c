/*
 * Copyright (c) 2018 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <shell/shell.h>
#include <stdlib.h>
#include <i2c.h>
#include <ctype.h>
#include <sys/util.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(i2c_shell);

struct i2c_hdl {
	char *device_name;
};

struct i2c_hdl i2c_list[] = {
#ifdef CONFIG_I2C_0
	{
		.device_name = DT_I2C_0_NAME,
	},
#endif
#ifdef CONFIG_I2C_1
	{
		.device_name = DT_I2C_1_NAME,
	},
#endif
#ifdef CONFIG_I2C_2
	{
		.device_name = DT_I2C_2_NAME,
	},
#endif
};


static int get_i2c_from_list(char *name)
{
	int i2c_idx;

	for (i2c_idx = 0; i2c_idx < ARRAY_SIZE(i2c_list); i2c_idx++) {
		if (!strcmp(name, i2c_list[i2c_idx].device_name)) {
			return i2c_idx;
		}
	}
	return -ENODEV;
}


static int cmd_i2c_scan(const struct shell *shell,
			size_t argc, char **argv)
{
	struct device *dev;
	u8_t cnt = 0, first = 0x04, last = 0x77;
	int chosen_i2c;

	chosen_i2c = get_i2c_from_list(argv[-1]);
	if (chosen_i2c < 0) {
		shell_error(shell, "Device not in device list");
		return -EINVAL;
	}
	dev = device_get_binding(i2c_list[chosen_i2c].device_name);

	if (!dev) {
		shell_error(shell, "I2C: Device driver %s not found.",
			    i2c_list[chosen_i2c].device_name);
		return -ENODEV;
	}

	shell_print(shell,
		    "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
	for (u8_t i = 0; i <= last; i += 16) {
		shell_fprintf(shell, SHELL_NORMAL, "%02x: ", i);
		for (u8_t j = 0; j < 16; j++) {
			if (i + j < first || i + j > last) {
				shell_fprintf(shell, SHELL_NORMAL, "   ");
				continue;
			}

			struct i2c_msg msgs[1];
			u8_t dst;

			/* Send the address to read from */
			msgs[0].buf = &dst;
			msgs[0].len = 0U;
			msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;
			if (i2c_transfer(dev, &msgs[0], 1, i + j) == 0) {
				shell_fprintf(shell, SHELL_NORMAL,
					      "%02x ", i + j);
				++cnt;
			} else {
				shell_fprintf(shell, SHELL_NORMAL, "-- ");
			}
		}
		shell_print(shell, "");
	}

	shell_print(shell, "%u devices found on %s",
		    cnt, i2c_list[chosen_i2c].device_name);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_i2c_cmds,
			       SHELL_CMD(scan, NULL,
					 "Scan I2C devices", cmd_i2c_scan),
			       SHELL_SUBCMD_SET_END     /* Array terminated. */
			       );

SHELL_STATIC_SUBCMD_SET_CREATE(sub_i2c,
#ifdef CONFIG_I2C_0
			       SHELL_CMD(I2C_0, &sub_i2c_cmds, "I2C_0", NULL),
#endif
#ifdef CONFIG_I2C_1
			       SHELL_CMD(I2C_1, &sub_i2c_cmds, "I2C_1", NULL),
#endif
#ifdef CONFIG_I2C_2
			       SHELL_CMD(I2C_2, &sub_i2c_cmds, "I2C_2", NULL),
#endif
			       SHELL_SUBCMD_SET_END /* Array terminated. */
			       );


SHELL_CMD_REGISTER(i2c, &sub_i2c, "I2C commands", NULL);

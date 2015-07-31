#include <stdio.h>
#include <stdint.h>
#include <device.h>
#include <init.h>

#include "board.h"

/******************************************************************************
 ***
 ***           PURE_INIT functions
 ***
 *****************************************************************************/
#ifdef CONFIG_PINMUX

#include <pinmux/pinmux.h>

struct pinmux_config atp_pmux = {
	.base_address = CONFIG_PINMUX_BASE,
};

DECLARE_DEVICE_INIT_CONFIG(pmux,			/* config name */
			   PINMUX_NAME,			/* driver name */
			   &pinmux_initialize,		/* init function */
			   &atp_pmux);			/* config options*/
pure_init(pmux, NULL);

#endif /* CONFIG_PINMUX */

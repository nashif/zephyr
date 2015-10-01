ccflags-y +=-I$(srctree)/arch/x86
ccflags-y +=-I$(srctree)/include/drivers
ccflags-y +=-I$(srctree)/drivers
asflags-y := ${ccflags-y}

obj-y = system.o drvIntStubs.o platform_config.o
obj-$(CONFIG_PINMUX) += pinmux.o
obj-$(CONFIG_BOARD_CTB) += ctb_pinmux.o
obj-$(CONFIG_BOARD_APP) += app_pinmux.o
obj-$(CONFIG_BOARD_AAE) += aae_pinmux.o

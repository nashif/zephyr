EXTRA_CFLAGS +=-I$(srctree)/include/drivers
EXTRA_CFLAGS +=-I$(srctree)/arch/$(ARCH)
EXTRA_CFLAGS +=-I$(srctree)/arch/$(ARCH)/$(strip $(CONFIG_BSP_DIR))

obj-$(CONFIG_HPET_TIMER) += hpet.o
obj-$(CONFIG_LOAPIC_TIMER) += loApicTimer.o

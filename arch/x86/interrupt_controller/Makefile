EXTRA_CFLAGS +=-I$(srctree)/include/drivers
EXTRA_CFLAGS +=-I$(srctree)/arch/$(ARCH)
EXTRA_CFLAGS +=-I$(srctree)/arch/$(ARCH)/$(strip $(CONFIG_BSP_DIR))

obj-$(CONFIG_LOAPIC) += loApicIntr.o
obj-$(CONFIG_IOAPIC) += ioApicIntr.o

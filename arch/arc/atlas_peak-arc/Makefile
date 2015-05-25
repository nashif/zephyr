EXTRA_CFLAGS +=-I$(srctree)/arch/arc/$(strip $(CONFIG_BSP_DIR))
EXTRA_AFLAGS +=-I$(srctree)/arch/arc/$(strip $(CONFIG_BSP_DIR))
EXTRA_CFLAGS +=-I$(srctree)/include/bsp
EXTRA_CFLAGS +=-I$(srctree)/include/drivers
EXTRA_CFLAGS +=-I$(srctree)/drivers

EXTRA_AFLAGS := ${EXTRA_CFLAGS}

obj-y = system.o
obj-$(CONFIG_IRQ_VECTOR_TABLE_BSP) += irq_vector_table.o
obj-$(CONFIG_SW_ISR_TABLE_BSP) += sw_isr_table.o

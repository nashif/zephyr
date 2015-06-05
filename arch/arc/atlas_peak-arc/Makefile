ccflags-y +=-I$(srctree)/arch/arc/$(strip $(CONFIG_BSP_DIR))
asflags-y +=-I$(srctree)/arch/arc/$(strip $(CONFIG_BSP_DIR))
ccflags-y +=-I$(srctree)/include/bsp
ccflags-y +=-I$(srctree)/include/drivers
ccflags-y +=-I$(srctree)/drivers

asflags-y := ${ccflags-y}

obj-y = system.o
obj-$(CONFIG_IRQ_VECTOR_TABLE_BSP) += irq_vector_table.o
obj-$(CONFIG_SW_ISR_TABLE_BSP) += sw_isr_table.o

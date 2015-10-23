ccflags-y +=-I$(srctree)/drivers
ccflags-$(CONFIG_QMI_GPIO) +=-I$(CONFIG_LIBQMI_PATH)/include

obj-$(CONFIG_GPIO) += gpio_static_irq_stubs.o
obj-$(CONFIG_GPIO_DW) += gpio_dw.o
obj-$(CONFIG_GPIO_PCAL9535A) += gpio_pcal9535a.o
obj-$(CONFIG_GPIO_MMIO) += gpio_mmio.o
obj-$(CONFIG_QMI_GPIO) += qmi_gpio.o

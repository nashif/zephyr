ccflags-y +=-I$(srctree)/drivers
ccflags-$(CONFIG_QMI_GPIO) +=-I$(CONFIG_LIBQMI_PATH)/include

obj-$(CONFIG_GPIO_DW) += gpio-dw.o
obj-$(CONFIG_GPIO_PCAL9535A) += gpio-pcal9535a.o
obj-$(CONFIG_GPIO_MMIO) += gpio-mmio.o
obj-$(CONFIG_QMI_GPIO) += qmi_gpio.o

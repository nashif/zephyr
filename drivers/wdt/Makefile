ccflags-y +=-I$(srctree)/arch/$(ARCH)
ccflags-y +=-I$(srctree)/arch/$(ARCH)/$(subst $(DQUOTE),,$(CONFIG_PLATFORM))
asflags-y +=-I$(srctree)/arch/$(ARCH)/$(subst $(DQUOTE),,$(CONFIG_PLATFORM))

obj-$(CONFIG_DW_WDT) = dw_wdt.o

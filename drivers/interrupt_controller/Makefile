obj-${CONFIG_PIC_DISABLE} = i8259.o

obj-$(CONFIG_LOAPIC) += system_apic.o
obj-$(CONFIG_MVIC) += mvic.o

ifneq ($(CONFIG_MVIC),y)
obj-$(CONFIG_LOAPIC) += loapic_intr.o
obj-$(CONFIG_IOAPIC) += ioapic_intr.o
endif

obj-$(CONFIG_ARCV2_INTERRUPT_UNIT) += arcv2_irq_unit.o

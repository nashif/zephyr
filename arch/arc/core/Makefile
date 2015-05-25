EXTRA_CFLAGS += -I$(srctree)/kernel/nanokernel/include
EXTRA_CFLAGS += -I$(srctree)/kernel/microkernel/include

obj-y = offsets/

obj-y += atomic.o context.o context_wrapper.o \
	cpu_idle.o  fast_irq.o  fatal.o  fault.o  \
	fault_s.o  ffs.o  irq_lock.o  irq_manage.o  \
	isr_wrapper.o  offsets  regular_irq.o  swap_macros.h  swap.o

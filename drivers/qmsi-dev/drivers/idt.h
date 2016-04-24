/*
 * {% copyright %}
 */
#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>
#include <string.h>
#include "qm_common.h"
#include "qm_soc_regs.h"

#if (QUARK_SE)
#define IDT_NUM_GATES (68)
#elif(QUARK_D2000)
#define IDT_NUM_GATES (52)
#endif

#define IDT_SIZE (sizeof(intr_gate_desc_t) * IDT_NUM_GATES)

typedef struct idtr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idtr_t;

typedef struct intr_gate_desc {
	uint16_t isr_low;
	uint16_t selector; /* Segment selector */

	/* The format of conf is the following:

	    15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
	   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	   |p |dpl  |ss|d |type |         unused           |

	   type: Gate type
	   d: size of Gate
	   ss: Storage Segment
	   dpl: Descriptor Privilege level
	   p: Segment present level
	*/
	uint16_t conf;
	uint16_t isr_high;
} __attribute__((packed)) intr_gate_desc_t;

extern intr_gate_desc_t __idt_start[];

/*
 * Setup IDT gate as an interrupt descriptor and assing the ISR entry point
 */
static __inline__ void idt_set_intr_gate_desc(uint32_t vector, uint32_t isr)
{
	intr_gate_desc_t *desc;

	desc = __idt_start + vector;

	desc->isr_low = isr & 0xFFFF;
	desc->selector = 0x08; /* Code segment offset in GDT */

	desc->conf = 0x8E00; /* type: 0b11 (Interrupt)
				d: 1 (32 bits)
				ss: 0
				dpl: 0
				p: 1
			     */
	desc->isr_high = (isr >> 16) & 0xFFFF;
}

/*
 * Initialize Interrupt Descriptor Table.
 * The IDT is initialized with null descriptors: any interrupt at this stage
 * will cause a triple fault.
 */
static __inline__ void idt_init(void)
{
	idtr_t idtr;

	memset(__idt_start, 0x00, IDT_SIZE);

	/* Initialize idtr structure */
	idtr.limit = IDT_SIZE - 1;
	idtr.base = (uint32_t)__idt_start;

	/* Load IDTR register */
	__asm__ __volatile__("lidt %0\n\t" ::"m"(idtr));
}
#endif /* __IDT_H__ */

#ifndef __COUNTER_ACE_V1X_ART_REGS__
#define __COUNTER_ACE_V1X_ART_REGS__

#if CONFIG_ACE_V1X_ART_COUNTER

struct DfTSCTRL {
	union {
		uint32_t full;
		struct {
			/**
			 * Capture DMA Select
			 * type: RW, rst: 00h, rst domain: gHUBULPRST
			 *
			 * Used to select which DMA?s link position value is to be captured for the
			 * timestamping event.
			 * For HD-A DMA:
			 * Bit 4 = 1 for ODMA, 0 for IDMA
			 * Bit 3:0 indicates the respective DMA engine index.
			 * For GPDMA:
			 * Bit 4:0 indicates the respective DMA channel index, starting from value 0
			 * to n*m-1, selecting GPDMAC0 ch 0 to ch n, followed by next GPDMA ch 0
			 * to ch n, until GPDMACm ch 0 to ch n.
			 * Must be programmed before ODTS or HHTSE is set.
			 */
			uint32_t    cdmas :  5;
			/**
			 * On Demand Time Stamp
			 * type: RW/1S, rst: 0b, rst domain: gHUBULPRST
			 *
			 * Request of the audio timestamping event on demand. When the bit
			 * is written, it toggles local on demand timestamp wire which causes
			 * coordinated timestamp capture within the audio cluster. The bit
			 * is cleared when the new local timestamp is available.
			 * Must be mutually exclusive with HHTSE.
			 */
			uint32_t     odts :  1;
			/**
			 * Link Wall Clock Select
			 * type: RW/L, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When set, it selects link wall clock value is to be captured in
			 * addition to the DSP wall clock.
			 * Note that the bit only functional for USB Audio Offload Link, HD-A Link,
			 * or iDisp-A Link.
			 * Must be programmed before ODTS or HHTSE is set.
			 * Locked to appear as RO per allocation programmed in
			 * HfUAOLA.LC + HfHDALA.LC register fields.
			 */
			uint32_t     lwcs :  1;
			/**
			 * Hammock Harbor Time Stamp Enable
			 * type: RW/1S, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When set, it initiates a system-wide (HH) timestamping events.
			 * The bit is cleared when the HH timestamping process is complete
			 ^ and ART is delivered as the global
			 * time stamp counter. Do not set this bit when Vnn is off.
			 * Must be mutually exclusive with ODTS.
			 */
			uint32_t    hhtse :  1;
			/**
			 * Reserved
			 * type: RO, rst: 00b, rst domain: nan
			 */
			uint32_t    rsvd9 :  2;
			/**
			 * Capture Link Select
			 * type: RW/L, rst: 0b, rst domain: gHUBULPRST
			 *
			 * Used to select which link wall clock to time stamp.
			 * 00: USB Audio Offload Link
			 * 01: Reserved
			 * 10: HD-A Link
			 * 11: iDisp-A link
			 * Must be programmed before ODTS or HHTSE is set.
			 * Locked to appear as RO per allocation programmed
			 * in HfUAOLA.LC + HfHDALA.LC
			 * register fields.
			 */
			uint32_t    clnks :  2;
			/**
			 * DMA Type Select
			 * type: RW, rst: 0b, rst domain: gHUBULPRST
			 *
			 * Used to select which DMA type to time stamp, or to skip DMA time stamp
			 * if none of them are active.
			 * 00: GPDMA
			 * 01: HD-A DMA
			 * 1x: no DMA active for time stamp
			 * Must be programmed before ODTS or HHTSE is set.
			 */
			uint32_t    dmats :  2;
			/**
			 * Reserved
			 * type: RO, rst: 0000h, rst domain: nan
			 */
			uint32_t   rsvd29 : 16;
			/**
			 * Interrupt on New Timestamp Enable
			 * type: RW, rst: 0b, rst domain: gHUBULPRST
			 *
			 * This bit controls whether an interrupt request to DSP will be sent on the
			 * timestamping event when the new timestamp is ready.
			 * When set to 1, it allows NTK bit to propagate for causing DSP interrupt
			 * (if the owner is DSP FW).
			 */
			uint32_t    ionte :  1;
			/**
			 * New Timestamp Taken
			 * type: RW/1C, rst: 0b, rst domain: gHUBULPRST
			 *
			 * This bit is set when a new timestamp was taken either as a result
			 * of HH event or on-demand local timestamp.
			 * SW needs to clear this bit to 0 by writing a 1 before starting
			 * the next time stamp capture.
			 */
			uint32_t      ntk :  1;
		} part;
	} u;
};

union DfDWCCS {
	uint64_t full;
	struct {
		/**
		 * Snapshot of DSP Wall Clock
		 * type: RO/V, rst: 0000 0000 0000 0000 0000 0000 0000 0000h, rst domain: gHUBULPRST
		 *
		 * Snapshot of DSP Wall Clock; these bits are not affected by writes.
		 * Valid and static when TSCTRL.NTK = 1.
		 */
		uint64_t      dwc : 64;
	} part;
};

union DfARTCS {
	uint64_t full;
	struct {
		/**
		 * Snapshot of ART Counter
		 * type: RO/V, rst: 0000 0000 0000 0000 0000 0000 0000 0000h, rst domain: gHUBULPRST
		 *
		 * Indicates the 64 bit ART counter value captured as a result of Global Time
		 * Synchronization capture request.
		 * Valid and static when TSCTRL.NTK = 1.
		 */
		uint32_t	high : 32;
		uint32_t	low : 32;
	} part;
};

#define ACE_ART_COUNTER_ID DT_NODELABEL(ace_art_counter)

/* RTC Wall Clock */
#define ACE_ARTCS	(DT_REG_ADDR(ACE_ART_COUNTER_ID))

#endif

#endif /*__COUNTER_ACE_V1X_ART_REGS__*/

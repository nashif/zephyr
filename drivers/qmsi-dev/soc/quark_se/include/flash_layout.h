/*
 * {% copyright %}
 */

#ifndef __FLASH_LAYOUT_H__
#define __FLASH_LAYOUT_H__

/**
 * Flash Layout for Quark SE Microcontrollers.
 *
 * @defgroup groupSEFlash Quark SE Flash Layout
 * @{
 */

typedef struct {
	QM_RW uint32_t magic;	  /**< Magic Number. */
	QM_RW uint16_t version;	/**< 0x0100. */
	QM_RW uint16_t reserved;       /**< Reserved. */
	QM_RW uint16_t osc_trim_32mhz; /**< 32MHz Oscillator trim code. */
	QM_RW uint16_t osc_trim_16mhz; /**< 16MHz Oscillator trim code. */
	QM_RW uint16_t osc_trim_8mhz;  /**< 8MHz Oscillator trim code. */
	QM_RW uint16_t osc_trim_4mhz;  /**< 4MHz Oscillator trim code. */
} qm_flash_otp_trim_t;

#if (UNIT_TEST)
extern uint8_t test_flash_page[0x800];
#define QM_FLASH_OTP_TRIM_CODE_BASE (&test_flash_page[0])
#else
#define QM_FLASH_OTP_TRIM_CODE_BASE (0xFFFFE1F0)
#endif

#define QM_FLASH_OTP_TRIM_CODE                                                 \
	((qm_flash_otp_trim_t *)QM_FLASH_OTP_TRIM_CODE_BASE)
#define QM_FLASH_OTP_SOC_DATA_VALID (0x24535021) /**< $SP! */
#define QM_FLASH_OTP_TRIM_MAGIC (QM_FLASH_OTP_TRIM_CODE->magic)

typedef union {
	struct trim_fields {
		QM_RW uint16_t
		    osc_trim_32mhz; /**< 32MHz Oscillator trim code. */
		QM_RW uint16_t
		    osc_trim_16mhz; /**< 16MHz Oscillator trim code. */
		QM_RW uint16_t osc_trim_8mhz; /**< 8MHz Oscillator trim code. */
		QM_RW uint16_t osc_trim_4mhz; /**< 4MHz Oscillator trim code. */
	} fields;
	QM_RW uint32_t osc_trim_u32[2]; /**< Oscillator trim code array.*/
	QM_RW uint16_t osc_trim_u16[2]; /**< Oscillator trim code array.*/
} qm_flash_data_trim_t;

#if (UNIT_TEST)
#define QM_FLASH_DATA_TRIM_BASE (&test_flash_page[100])
#define QM_FLASH_DATA_TRIM_OFFSET (100)
#else
#define QM_FLASH_DATA_TRIM_BASE (0x4002F000)
#define QM_FLASH_DATA_TRIM_OFFSET ((uint32_t)QM_FLASH_DATA_TRIM_BASE & 0x3FFFF)
#endif

#define QM_FLASH_DATA_TRIM ((qm_flash_data_trim_t *)QM_FLASH_DATA_TRIM_BASE)
#define QM_FLASH_DATA_TRIM_CODE (&QM_FLASH_DATA_TRIM->fields)
#define QM_FLASH_DATA_TRIM_REGION QM_FLASH_REGION_SYS

#define QM_FLASH_TRIM_PRESENT_MASK (0xFC00)
#define QM_FLASH_TRIM_PRESENT (0x0000)

/**
 * @}
 */

#endif /* __FLASH_LAYOUT_H__ */

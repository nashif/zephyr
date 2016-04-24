/*
 * {% copyright %}
 */

#ifndef __QM_GPIO_H__
#define __QM_GPIO_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * General Purpose IO.
 *
 * @defgroup groupGPIO GPIO
 * @{
 */

/**
 * GPIO pin states.
 */
typedef enum {
	QM_GPIO_LOW,      /**< GPIO low state. */
	QM_GPIO_HIGH,     /**< GPIO high state. */
	QM_GPIO_STATE_NUM /**< Number of GPIO states. */
} qm_gpio_state_t;

/**
 * GPIO port configuration type.
 *
 * Each bit in the registers control a GPIO pin.
 */
typedef struct {
	uint32_t direction;    /**< GPIO direction, 0b: input, 1b: output. */
	uint32_t int_en;       /**< Interrupt enable. */
	uint32_t int_type;     /**< Interrupt type, 0b: level; 1b: edge. */
	uint32_t int_polarity; /**< Interrupt polarity, 0b: low, 1b: high. */
	uint32_t int_debounce; /**< Interrupt debounce on/off. */
	uint32_t int_bothedge; /**< Interrupt on rising and falling edges. */

	/**
	 * Transfer callback.
	 *
	 * @param[in] data Callback user data.
	 * @param[in] int_status GPIO interrupt status.
	 */
	void (*callback)(void *data, uint32_t int_status);
	void *callback_data; /**< Callback user data. */
} qm_gpio_port_config_t;

/**
 * Set GPIO port configuration.
 *
 * This includes if interrupts are enabled or not, the level on which an
 * interrupt is generated, the polarity of interrupts and if GPIO-debounce is
 * enabled or not. If interrupts are enabled it also registers the user defined
 * callback function.
 *
 * @param[in] gpio GPIO port index to configure.
 * @param[in] cfg New configuration for GPIO port. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 *
 */
int qm_gpio_set_config(const qm_gpio_t gpio,
		       const qm_gpio_port_config_t *const cfg);

/**
 * Read the current state of a single pin on a given GPIO port.
 *
 * @param[in] gpio GPIO port index.
 * @param[in] pin Pin of GPIO port to read.
 * @param[out] state Current state of the pin. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_read_pin(const qm_gpio_t gpio, const uint8_t pin,
		     qm_gpio_state_t *const state);

/**
 * Set a single pin on a given GPIO port.
 *
 * @param[in] gpio GPIO port index.
 * @param[in] pin Pin of GPIO port to set.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_set_pin(const qm_gpio_t gpio, const uint8_t pin);

/**
 * Clear a single pin on a given GPIO port.
 *
 * @param[in] gpio GPIO port index.
 * @param[in] pin Pin of GPIO port to clear.
 * @return int 0 on success, error code otherwise.
 *
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_clear_pin(const qm_gpio_t gpio, const uint8_t pin);

/**
 * Set or clear a single GPIO pin using a state variable.
 *
 * @param[in] gpio GPIO port index.
 * @param[in] pin Pin of GPIO port to update.
 * @param[in] state QM_GPIO_LOW for low or QM_GPIO_HIGH for high.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_set_pin_state(const qm_gpio_t gpio, const uint8_t pin,
			  const qm_gpio_state_t state);

/**
 * Read the value of every pin on a GPIO port.
 *
 * Each bit of the val parameter is set to the current value of each pin on the
 * port. Maximum 32 pins per port.
 *
 * @param[in] gpio GPIO port index.
 * @param[out] port State of every pin in a GPIO port. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_read_port(const qm_gpio_t gpio, uint32_t *const port);

/**
 * Write a value to every pin on a GPIO port.
 *
 * Each pin on the GPIO port is set to the corresponding value set in the val
 * parameter. Maximum 32 pins per port.
 *
 * @param[in] gpio GPIO port index.
 * @param[in] val Value of all pins on GPIO port.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_gpio_write_port(const qm_gpio_t gpio, const uint32_t val);

/**
 * @}
 */

#endif /* __QM_GPIO_H__ */

/*!
 *  @file Zephyr_MCP9808.cpp
 *
 *  @mainpage Adafruit MCP9808 I2C Temp Sensor
 *
 *  @section intro_sec Introduction
 *
 * 	I2C Driver for Microchip's MCP9808 I2C Temp sensor
 *
 * 	This is a library for the Adafruit MCP9808 breakout:
 * 	http://www.adafruit.com/products/1782
 *
 * 	Adafruit invests time and resources providing this open source code,
 *  please support Adafruit and open-source hardware by purchasing products from
 * 	Adafruit!
 *
 *  @section author Author
 *
 *  K.Townsend (Adafruit Industries)
 *
 * 	@section license License
 *
 * 	BSD (see license.txt)
 *
 * 	@section  HISTORY
 *
 *     v1.0 - First release
 */

#include <devicetree.h>
#include <device.h>
#include "MCP9808.h"
#include <sys/byteorder.h>

/*!
 *    @brief  Instantiates a new MCP9808 class
 */
Zephyr_MCP9808::Zephyr_MCP9808() {}


/*!
 *    @brief  Setups the HW with default address
 *    @return True if initialization was successful, otherwise false.
 */
bool Zephyr_MCP9808::begin() {
  i2c_master = device_get_binding(i2c_bus);

  return init();
}

/*!
 *    @brief  init function
 *    @return True if initialization was successful, otherwise false.
 */
bool Zephyr_MCP9808::init() {
  if (read16(MCP9808_REG_MANUF_ID) != 0x0054)
    return false;
  if (read16(MCP9808_REG_DEVICE_ID) != 0x0400)
    return false;

  write16(MCP9808_REG_CONFIG, 0x0);
  return true;
}


/*!
 *   @brief  Reads the 16-bit temperature register and returns the Centigrade
 *           temperature as a float.
 *   @return Temperature in Centigrade.
 */
float Zephyr_MCP9808::readTempC() {
  float temp;
  uint16_t t = read16(MCP9808_REG_AMBIENT_TEMP);

  if (t != 0xFFFF) {
    temp = t & 0x0FFF;
    temp /= 16.0;
    if (t & 0x1000)
      temp -= 256;
  }

  return temp;
}

/*!
 *   @brief  Reads the 16-bit temperature register and returns the Fahrenheit
 *           temperature as a float.
 *   @return Temperature in Fahrenheit.
 */
float Zephyr_MCP9808::readTempF() {
  float temp;
  uint16_t t = read16(MCP9808_REG_AMBIENT_TEMP);

  if (t != 0xFFFF) {
    temp = t & 0x0FFF;
    temp /= 16.0;
    if (t & 0x1000)
      temp -= 256;

    temp = temp * 9.0 / 5.0 + 32;
  }

  return temp;
}

/*!
 *   @brief  Set Sensor to Shutdown-State or wake up (Conf_Register BIT8)
 *   @param  sw true = shutdown / false = wakeup
 */
void Zephyr_MCP9808::shutdown_wake(bool sw) {
  uint16_t conf_shutdown;
  uint16_t conf_register = read16(MCP9808_REG_CONFIG);
  if (sw == true) {
    conf_shutdown = conf_register | MCP9808_REG_CONFIG_SHUTDOWN;
    write16(MCP9808_REG_CONFIG, conf_shutdown);
  }
  if (sw == false) {
    conf_shutdown = conf_register & ~MCP9808_REG_CONFIG_SHUTDOWN;
    write16(MCP9808_REG_CONFIG, conf_shutdown);
  }
}

/*!
 *   @brief  Shutdown MCP9808
 */
void Zephyr_MCP9808::shutdown() { shutdown_wake(true); }

/*!
 *   @brief  Wake up MCP9808
 */
void Zephyr_MCP9808::wake() {
  shutdown_wake(false);
  k_msleep(260);
}

/*!
 *   @brief  Get Resolution Value
 *   @return Resolution value
 */
uint8_t Zephyr_MCP9808::getResolution() {
  return read8(MCP9808_REG_RESOLUTION);
}

/*!
 *   @brief  Set Resolution Value
 *   @param  value
 */
void Zephyr_MCP9808::setResolution(uint8_t value) {
  write8(MCP9808_REG_RESOLUTION, value & 0x03);
}

/*!
 *    @brief  Low level 16 bit write procedures
 *    @param  reg
 *    @param  value
 */
void Zephyr_MCP9808::write16(uint8_t reg, uint16_t value) {
  uint8_t buf[3];

  buf[0] = reg;
  sys_put_be16(value, &buf[1]);

  i2c_write(i2c_master, buf, sizeof(buf), i2c_addr);
}

/*!
 *    @brief  Low level 16 bit read procedure
 *    @param  reg
 *    @return value
 */
uint16_t Zephyr_MCP9808::read16(uint8_t reg) {
  uint16_t val;
  int rc = i2c_write_read(i2c_master, i2c_addr,
                          &reg, sizeof(reg),
                          &val, sizeof(val));

  if (rc == 0) {
          val = sys_be16_to_cpu(val);
  }

  return val;
}

/*!
 *    @brief  Low level 8 bit write procedure
 *    @param  reg
 *    @param  value
 */
void Zephyr_MCP9808::write8(uint8_t reg, uint8_t value) {
  uint8_t buf[2] = {
          reg,
          value,
  };

  i2c_write(i2c_master, buf, sizeof(buf), i2c_addr);
}

/*!
 *    @brief  Low level 8 bit read procedure
 *    @param  reg
 *    @return value
 */
uint8_t Zephyr_MCP9808::read8(uint8_t reg) {
  uint8_t val;
  return val;
}

/**************************************************************************/
/*!
    @brief  Gets the pressure sensor and temperature values as sensor events

    @param  temp Sensor event object that will be populated with temp data
    @returns True
*/
/**************************************************************************/
bool Zephyr_MCP9808::getEvent(sensors_event_t *temp) {
  uint32_t t = k_uptime_get_32();

  // use helpers to fill in the events
  memset(temp, 0, sizeof(sensors_event_t));
  temp->version = sizeof(sensors_event_t);
  temp->sensor_id = _sensorID;
  temp->type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
  temp->timestamp = t;
  temp->temperature = readTempC();
  return true;
}

/**************************************************************************/
/*!
    @brief  Gets the overall sensor_t data including the type, range and
   resulution
    @param  sensor Pointer to Zephyr_Sensor sensor_t object that will be
   filled with sensor type data
*/
/**************************************************************************/
void Zephyr_MCP9808::getSensor(sensor_t *sensor) {
  /* Clear the sensor_t object */
  memset(sensor, 0, sizeof(sensor_t));

  /* Insert the sensor name in the fixed length char array */
  strncpy(sensor->name, "MCP9808", sizeof(sensor->name) - 1);
  sensor->name[sizeof(sensor->name) - 1] = 0;
  sensor->version = 1;
  sensor->sensor_id = _sensorID;
  sensor->type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
  sensor->min_delay = 0;
  sensor->max_value = 100.0;
  sensor->min_value = -20.0;
  sensor->resolution = 0.0625;
}
/*******************************************************/


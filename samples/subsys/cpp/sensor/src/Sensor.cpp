#include "Sensor.h"
#include <sys/printk.h>

/**************************************************************************/
/*!
    @brief  Prints sensor information to serial console
*/
/**************************************************************************/
void Zephyr_Sensor::printSensorDetails(void) {
  sensor_t sensor;
  getSensor(&sensor);
  printk("------------------------------------\n");
  printk("Sensor:       %s\n", sensor.name);
  printk("Type:         ");
  switch ((sensors_type_t)sensor.type) {
  case SENSOR_TYPE_ACCELEROMETER:
    printk("Acceleration (m/s2)\n");
    break;
  case SENSOR_TYPE_MAGNETIC_FIELD:
    printk("Magnetic (uT)\n");
    break;
  case SENSOR_TYPE_ORIENTATION:
    printk("Orientation (degrees)\n");
    break;
  case SENSOR_TYPE_GYROSCOPE:
    printk("Gyroscopic (rad/s)\n");
    break;
  case SENSOR_TYPE_LIGHT:
    printk("Light (lux)\n");
    break;
  case SENSOR_TYPE_PRESSURE:
    printk("Pressure (hPa)\n");
    break;
  case SENSOR_TYPE_PROXIMITY:
    printk("Distance (cm)\n");
    break;
  case SENSOR_TYPE_GRAVITY:
    printk("Gravity (m/s2)\n");
    break;
  case SENSOR_TYPE_LINEAR_ACCELERATION:
    printk("Linear Acceleration (m/s2)\n");
    break;
  case SENSOR_TYPE_ROTATION_VECTOR:
    printk("Rotation vector\n");
    break;
  case SENSOR_TYPE_RELATIVE_HUMIDITY:
    printk("Relative Humidity (%%)\n");
    break;
  case SENSOR_TYPE_AMBIENT_TEMPERATURE:
    printk("Ambient Temp (C)\n");
    break;
  case SENSOR_TYPE_OBJECT_TEMPERATURE:
    printk("Object Temp (C)\n");
    break;
  case SENSOR_TYPE_VOLTAGE:
    printk("Voltage (V)\n");
    break;
  case SENSOR_TYPE_CURRENT:
    printk("Current (mA)\n");
    break;
  case SENSOR_TYPE_COLOR:
    printk("Color (RGBA)\n");
    break;
  }

  printk("Driver Ver:   %d\n", sensor.version);
  printk("Unique ID:    %d\n", sensor.sensor_id);
  printk("Min Value:    %f\n", sensor.min_value);
  printk("Max Value:    %f\n", sensor.max_value);
  printk("Resolution:   %f\n", sensor.resolution);
  printk("------------------------------------\n\n");
}

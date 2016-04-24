#
# {% copyright %}
#

### Variables
SENSOR_DIR = $(BASE_DIR)/drivers/sensor
OBJ_DIRS += $(SENSOR_DIR)/$(BUILD)/$(SOC)/$(TARGET)
SENSOR_SOURCES = $(wildcard $(SENSOR_DIR)/*.c)
OBJECTS += $(addprefix $(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ)/,$(notdir $(SENSOR_SOURCES:.c=.o)))

### Flags
CFLAGS += -I$(SENSOR_DIR)
CFLAGS += -I$(SENSOR_DIR)/include
CFLAGS += -I$(BASE_DIR)/soc/$(SOC_ROOT_DIR)/include

### Build C files
$(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ)/%.o: $(SENSOR_DIR)/%.c
	$(call mkdir, $(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ))
	$(CC) $(CFLAGS) -c -o $@ $<

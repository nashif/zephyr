#
# {% copyright %}
#

### Variables
OBJ_DIRS += $(BASE_DIR)/drivers/$(BUILD)/$(SOC)/$(TARGET)
GENERATED_DIRS += $(BASE_DIR)/drivers/$(BUILD)

### Flags
CFLAGS += -I$(LIBQMSI_INCLUDE_DIR)
LDFLAGS += -L$(LIBQMSI_LIB_DIR)
LDLIBS += -l$(LDLIBS_FILENAME)

.PHONY: libqmsi

libqmsi:
	$(MAKE) -C $(BASE_DIR)/drivers SOC=$(SOC) TARGET=$(TARGET) BUILD=$(BUILD) V=$(V) CSTD=$(CSTD)

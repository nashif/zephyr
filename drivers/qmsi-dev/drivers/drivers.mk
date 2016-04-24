#
# {% copyright %}
#

### Variables
DRV_DIR = $(BASE_DIR)/drivers
OBJ_DIRS += $(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)
LMT_SOURCES = $(wildcard $(DRV_DIR)/*.c)
ifeq ($(TARGET), sensor)
### Excluding the pic timer in the SENSOR build
LMT_SOURCES := $(subst qm_pic_timer.c,,${LMT_SOURCES})
endif
### Excluding the mailbox the d2000 build
ifeq ($(SOC), quark_d2000)
LMT_SOURCES := $(subst qm_mailbox.c,,${LMT_SOURCES})
endif
OBJECTS += $(addprefix $(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ)/,$(notdir $(LMT_SOURCES:.c=.o)))

### Flags
CFLAGS += -I$(DRV_DIR)
CFLAGS += -I$(DRV_DIR)/include

### Build C files
$(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ)/%.o: $(DRV_DIR)/%.c
	$(call mkdir, $(DRV_DIR)/$(BUILD)/$(SOC)/$(TARGET)/$(OBJ))
	$(CC) $(CFLAGS) -c -o $@ $<

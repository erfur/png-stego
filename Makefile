TARGET ?= png-stego
SRC_DIRS ?= ./src
SAMPLES_DIR ?= samples

SYMBOLS := -g
LIBS := -lz -lm
SRCS := $(shell find $(SRC_DIRS) -name *.c)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

$(TARGET):
	$(CC) $(LDFLAGS) $(SYMBOLS) $(LIBS) $(SRCS) -o $(TARGET)
	
debug:	$(TARGET)
	gdb $(TARGET)
	
test:
	cd samples && for file in $$(ls *.png); \
	do \
		../$(TARGET) $$file ../output/$$file; \
	done

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS) output/*

-include $(DEPS)

TARGET = test_shake
SOURCES = detect_shake.c
OBJECTS = $(SOURCES:%.c=%.o)

CFLAGS = -g -O2 -Wall
LDFLAGS =
CC = arm-none-linux-gnueabi-gcc
LD = arm-none-linux-gnueabi-ld

default: $(SOURCES) $(TARGET)

$(TARGET): $(OBJECTS)
	@echo [ARM-LD] $@
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	@echo [ARM-CC] $<...
	@$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo [CLEAN] removing object files + $(TARGET)...
	@rm -f $(OBJECTS) $(TARGET)

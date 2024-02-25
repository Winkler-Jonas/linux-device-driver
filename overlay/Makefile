.PHONY = all compile install clean

SRC_DIRS ?= .
INSTALL_DIR ?= /boot/overlays

SRCS := $(shell find $(SRC_DIRS) -name "*.dts")
OBJS := $(addsuffix .dtbo,$(basename $(SRCS)))

all: install

clean:
	@rm -rf *.dtbo

install: compile
	@sudo cp -v $(OBJS) $(INSTALL_DIR)

show:
	dtc -I fs /sys/firmware/devicetree/base

compile: $(OBJS)

%.dtbo: %.dts
	dtc -W no-unit_address_vs_reg -I dts -O dtb -o $@ $<

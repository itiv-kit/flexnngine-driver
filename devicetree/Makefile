SRCS = $(wildcard *.dtsi)
OBJS = $(SRCS:%.dtsi=%.dtbo)

.PHONY: all

all: $(OBJS)

%.dtbo: %.dtsi
	dtc -@ -O dtb -o $@ $<

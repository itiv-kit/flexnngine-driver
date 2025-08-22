SRCS = $(wildcard *.dtsi)
OBJS = $(SRCS:%.dtsi=%.dtbo)

.PHONY: all clean

all: $(OBJS)

clean:
	rm $(OBJS)

%.dtbo: %.dtsi
	dtc -@ -O dtb -o $@ $<

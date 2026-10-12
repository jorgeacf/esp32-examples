.PHONY: all build upload flash monitor flash-monitor clean

all: build

build:
	pio run

upload flash:
	pio run --target upload

monitor:
	pio device monitor

flash-monitor:
	pio run --target upload --target monitor

clean:
	pio run --target clean

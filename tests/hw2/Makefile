MINEMU ?= minemu

.PHONY: all prepare-hello prepare-devices prepare-ioctl-abi clean

all: prepare-hello prepare-devices prepare-ioctl-abi

prepare-hello:
	$(MAKE) -C fixtures/hello MINEMU="$(MINEMU)" all

prepare-devices:
	$(MAKE) -C fixtures/devices MINEMU="$(MINEMU)" all

prepare-ioctl-abi:
	$(MAKE) -C fixtures/ioctl-abi MINEMU="$(MINEMU)" all

prepare-%:
	@:

clean:
	$(MAKE) -C fixtures/hello clean
	$(MAKE) -C fixtures/devices clean
	$(MAKE) -C fixtures/ioctl-abi clean

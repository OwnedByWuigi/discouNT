ARCH ?= x86
ISO_NAME ?= discount-$(ARCH).iso

include mk/common.mk

ifeq ($(ARCH),amd64)
include mk/arch/amd64.mk
else ifeq ($(ARCH),loongarch64)
include mk/arch/loongarch64.mk
else
include mk/kernel.mk
include mk/drivers.mk
include mk/dlls.mk
include mk/apps.mk
include mk/image.mk
include mk/targets.mk
endif

.PHONY: x86 amd64 loongarch64
x86:
	$(MAKE) ARCH=x86 all
amd64:
	$(MAKE) ARCH=amd64 all
loongarch64:
	$(MAKE) ARCH=loongarch64 all

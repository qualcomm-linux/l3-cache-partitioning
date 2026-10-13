# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.

ifdef KERNEL_SRC
  KDIR ?= $(KERNEL_SRC)
else
  KDIR ?= /lib/modules/$(shell uname -r)/build
endif

all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" modules

modules_install:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" modules_install

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" clean

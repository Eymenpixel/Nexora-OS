.PHONY: all kernel kaya ust cli check kaya-package kaya-demo run nexora-iso clean

all: kernel kaya cli

BUILD := build
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/nexora: src/main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

cli: $(BUILD)/nexora

kernel:
	$(MAKE) -C kernel iso

kaya:
	$(MAKE) -C Kaya/kayp

ust: | $(BUILD)
	cmake -S Kaya/ust -B $(BUILD)/ust -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD)/ust --parallel 2

kaya-package:
	bash ./scripts/build-kaya-package.sh

check:
	$(MAKE) -C kernel check

kaya-demo: kaya
	bash ./scripts/kaya-smoke.sh

run: kernel
	$(MAKE) -C kernel run

nexora-iso: kernel | $(BUILD)
	mkdir -p $(BUILD)/nexora-iso/boot/grub/fonts
	cp kernel/build/blackbit.elf $(BUILD)/nexora-iso/boot/blackbit.elf
	cp Assets/gui/boot-splash.png $(BUILD)/nexora-iso/boot/nexora-splash.png
	cp iso/nexora/grub.cfg $(BUILD)/nexora-iso/boot/grub/grub.cfg
	cp /usr/share/grub/unicode.pf2 $(BUILD)/nexora-iso/boot/grub/fonts/unicode.pf2
	grub-mkrescue -o $(BUILD)/nexora.iso $(BUILD)/nexora-iso

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C Kaya/kayp clean
	rm -rf $(BUILD)
	rm -f Kaya/packages/nexora-welcome_0.1.0.kaya


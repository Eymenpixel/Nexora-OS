.PHONY: all kernel kaya ust cli check kaya-package kaya-demo run clean

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

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C Kaya/kayp clean
	rm -rf $(BUILD)
	rm -f Kaya/packages/nexora-welcome_0.1.0.kaya


# Runs the DSP unit test on macOS/Linux:  make -C Tests
CC ?= cc
OUT ?= /tmp/fundstueck-test
test:
	mkdir -p $(OUT)
	$(CC) -std=c11 -D_DEFAULT_SOURCE -O2 -Wall -Wextra -o $(OUT)/dsp_test dsp_test.c ../Shared/DSP/*.c -lm -lpthread
	$(OUT)/dsp_test $(OUT)

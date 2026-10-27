# OpenMP lab — build rules
#
#   make            build everything into bin/
#   make image      generate data/input.ppm (4000 x 3000)
#   make run        serial baseline, then your parallel version
#   make clean      remove build/ and bin/
#
# Add -fopenmp to BOTH the compile and the link step, or the pragmas are
# silently ignored and the program runs sequentially without telling you.

CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -O2 -fopenmp
LDFLAGS  ?= -fopenmp -lm

SRC_DIR   := src
SOL_DIR   := solution
INC_DIR   := include
BUILD_DIR := build
BIN_DIR   := bin
DATA_DIR  := data

CPPFLAGS ?= -I$(INC_DIR) -MMD -MP

COMMON := $(BUILD_DIR)/ppm.o $(BUILD_DIR)/filters.o

PROGRAMS := gen_image pipeline_serial pipeline_omp
ifneq ($(wildcard $(SOL_DIR)/main_omp_solution.c),)
PROGRAMS += pipeline_omp_solution
endif
BINARIES := $(addprefix $(BIN_DIR)/,$(PROGRAMS))

.PHONY: all image run clean

all: $(BINARIES)

$(BIN_DIR)/gen_image: $(BUILD_DIR)/gen_image.o $(BUILD_DIR)/ppm.o | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/pipeline_serial: $(BUILD_DIR)/main_serial.o $(COMMON) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/pipeline_omp: $(BUILD_DIR)/main_omp.o $(COMMON) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/pipeline_omp_solution: $(BUILD_DIR)/main_omp_solution.o $(COMMON) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SOL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BUILD_DIR) $(BIN_DIR) $(DATA_DIR):
	mkdir -p $@

image: $(BIN_DIR)/gen_image | $(DATA_DIR)
	./$(BIN_DIR)/gen_image $(DATA_DIR)/input.ppm 4000 3000

run: all
	./$(BIN_DIR)/pipeline_serial $(DATA_DIR)/input.ppm
	./$(BIN_DIR)/pipeline_omp    $(DATA_DIR)/input.ppm

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)

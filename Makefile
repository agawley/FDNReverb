# Project Name — change this for each effect
TARGET = effect

# Sources
CPP_SOURCES = main.cpp hothouse.cpp

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Initialize git submodules and build both libraries
submodules:
	git submodule update --init --recursive
	$(MAKE) -C $(LIBDAISY_DIR)
	$(MAKE) -C $(DAISYSP_DIR)

.PHONY: submodules

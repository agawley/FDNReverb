CXX ?= g++

BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)

DAISYSP_SRC = ../DaisySP/Source

CXXFLAGS = -std=gnu++17 -DUNIT_TEST -O2 -g \
           -I$(DAISYSP_SRC) \
           -I$(DAISYSP_SRC)/Utility \
           -I$(DAISYSP_SRC)/Synthesis \
           -I$(DAISYSP_SRC)/Filters \
           -I.. \
           -I$(BREW_PREFIX)/include

SRCS = reverb_test.cpp \
       ../Reverb.cpp \
       $(DAISYSP_SRC)/Synthesis/oscillator.cpp \
       $(DAISYSP_SRC)/Utility/dcblock.cpp

LDFLAGS = -L$(BREW_PREFIX)/lib -lgtest -lgtest_main -lpthread

TARGET = reverb_test

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).dSYM

.PHONY: all clean

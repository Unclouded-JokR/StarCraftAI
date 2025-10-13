# This file is based on "Makefile Cookbook" from the https://makefiletutorial.com/.

TARGET_EXEC := StarterBot.exe
# Change to ./bin_linux for Linux builds
BIN_DIR := ./bin
SRC_DIR := ./src
BWEM_DIR := ./BWEM
VSDIR := ./visualstudio

CXX := i686-w64-mingw32-g++

SRCS := $(shell find $(SRC_DIR) -name '*.cpp') \
        $(shell find $(BWEM_DIR)/src -name '*.cpp' \
            -not -iname 'mapPrinter.cpp' \
            -not -iname 'examples.cpp' \
            -not -iname 'exampleWall.cpp') \
        $(shell find $(VSDIR) -name '*.cpp')



OBJS := $(SRCS:%=$(BIN_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Every folder in ./src will need to be passed to GCC so that it can find header files
INC_DIRS := $(shell find $(SRC_DIR) -type d)
INC_DIRS += $(BWEM_DIR)/src
INC_DIRS += $(VSDIR)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))
INC_FLAGS += -Isrc/bwapi/include -I./BWEM/src

# The -MMD and -MP flags together generate Makefiles for us!
# These files will have .d instead of .o as the output.
CXXFLAGS += -std=gnu++17 -O0 -g 
# CXXFLAGS += -static-libgcc -static-libstdc++            
CPPFLAGS := $(INC_FLAGS) -MMD -MP -DNOMINMAX

LDFLAGS += -static-libgcc -static-libstdc++
LDLIBS  += -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic

# The final build step.
$(BIN_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# Build step for C++ source
$(BIN_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR)/src $(BIN_DIR)/BWEM $(BIN_DIR)/$(TARGET_EXEC)

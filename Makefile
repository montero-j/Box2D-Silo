# Makefile para el proyecto Silo

# Configuración
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall
BOX2D_INCLUDE_DIR := ../../box2d/include
BOX2D_LIB_DIR := ../../box2d/build/src
BOX2D_INC := -I$(BOX2D_INCLUDE_DIR)
BOX2D_LIB := -L$(BOX2D_LIB_DIR) -lbox2d
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin

# Archivos fuente
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET := $(BIN_DIR)/silo_simulator

# Regla principal
all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(BOX2D_LIB)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(BOX2D_INC) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean

# Configuración
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wno-unused-variable
BOX2D_INCLUDE_DIR := box2d/include
BOX2D_LIB_DIR := box2d/build/src
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

# Nueva regla para ejecutar simulación específica
run-specific:
	@if [ -z "$(R)" ] || [ -z "$(CHI)" ]; then \
		echo "Usage: make run-specific R=<ratio> CHI=<chi>"; \
		exit 1; \
	fi
	./scripts/run_specific_simulation.sh $(R) $(CHI)

# Tests
test: $(TARGET)
	@echo "Ejecutando tests básicos..."
	@cd tests && ./run_basic_tests.sh

# Ejemplo rápido
example: $(TARGET)
	@echo "Ejecutando ejemplo de círculos..."
	@cd examples && ./quick_circle_simulation.sh

# Análisis de ejemplo
analyze:
	@echo "Analizando resultados..."
	@cd examples && python3 basic_analysis.py

# Estudio completo de forma
study-%: $(TARGET)
	@echo "Ejecutando estudio de $*..."
	@python3 scripts/run_shape_study.py $* --target 100 --time 60

# Limpiar datos
clean-data:
	@echo "Limpiando datos de simulaciones..."
	@rm -rf data/simulations/sim_* data/shape_study_results_*
	@rm -rf simulations/ tests/simulations/sim_* examples/simulations/sim_*
	@echo "Datos limpiados (estructura de directorios preservada)"

# Limpiar datos de ejemplo y tests solamente
clean-temp:
	@echo "Limpiando datos temporales..."
	@rm -rf simulations/ tests/simulations/sim_* examples/simulations/sim_*
	@echo "Datos temporales limpiados"

# Limpiar todo
clean-all: clean clean-data

# Instalación de dependencias Python
install-deps:
	@echo "Instalando dependencias Python..."
	@pip3 install pandas numpy matplotlib opencv-python

# Verificar setup
verify:
	@echo "Verificando setup del proyecto..."
	@python3 -c "import pandas, numpy, matplotlib; print('Python dependencies OK')"
	@[ -f bin/silo_simulator ] && echo "Simulador compilado" || echo "Falta compilar simulador"
	@echo "Verificación completada"

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean run-specific test example analyze study-% clean-data clean-temp clean-all install-deps verify
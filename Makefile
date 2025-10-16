# Nombre del ejecutable final
TARGET = ./bin/silo_simulator

# Carpeta de salida para el ejecutable y archivos objeto
BIN_DIR = bin
OBJ_DIR = obj

# Directorios de fuentes y cabeceras de tu proyecto
INC_DIR = include
SRC_DIR = src

# Compilador a usar
CXX = g++

# 1. Rutas de Archivos
# ----------------------------------------------------------------------------------
# Obtener todos los archivos .cpp de la carpeta src/
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
# Generar archivos objeto en la carpeta obj/
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
# Archivos de dependencia (.d)
DEPS = $(OBJECTS:.o=.d)

# 2. Configuración de Box2D (Rutas Encontradas en tu 'listado.txt')
# ----------------------------------------------------------------------------------
# RUTA EXACTA del directorio que contiene libbox2d.a (es 'box2d/lib/')
BOX2D_LIB_DIR = box2d/build/src/

# Rutas de búsqueda de librerías (-L) y enlace a la librería (-lbox2d)
BOX2D_LDFLAGS = -L$(BOX2D_LIB_DIR) -lbox2d 

# 3. Banderas de Compilación y Enlace
# ----------------------------------------------------------------------------------
# CXXFLAGS:
# -I$(INC_DIR): Busca tus headers locales (include/)
# -Ibox2d/include: Busca la carpeta 'include' dentro de 'box2d' para encontrar box2d/box2d.h
CXXFLAGS = -std=c++17 -O3 -Wall -MMD -I$(INC_DIR) -Ibox2d/include

# LDFLAGS:
LDFLAGS = $(BOX2D_LDFLAGS)

# ==================================================================================
# REGLAS DE COMPILACIÓN
# ==================================================================================

.PHONY: all clean

# Regla Principal: construye el ejecutable en bin/
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	@echo "Enlazando módulos para crear el ejecutable: $(TARGET)"
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Compilación exitosa! 🚀"

# Regla para compilar cada archivo fuente en un archivo objeto
# Se compila src/X.cpp para generar obj/X.o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regla para limpiar todos los archivos generados
clean:
	@echo "Limpiando archivos de compilación..."
	rm -rf $(TARGET) $(OBJECTS) $(DEPS) $(OBJ_DIR) $(BIN_DIR)
	rm -rf ./simulations # Opcional: limpiar directorio de resultados
	@echo "Limpieza completada."

# Incluir archivos de dependencia generados automáticamente (para recompilación inteligente)
-include $(DEPS)
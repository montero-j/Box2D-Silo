#!/bin/bash

# Script para compilar y ejecutar silo_simulator con parámetros desde archivo
# Uso: ./ejecutar_simulacion_1.sh [archivo_parametros]
# Si no se especifica archivo, usa 'parametros_ejemplo.txt'

set -e  # Salir si cualquier comando falla

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para mostrar ayuda
show_help() {
    echo "Uso: $0 [archivo_parametros]"
    echo ""
    echo "Este script compila y ejecuta silo_simulator con parámetros desde un archivo."
    echo ""
    echo "Argumentos:"
    echo "  archivo_parametros    Archivo con parámetros (obligatorio)"
    echo ""
    echo "Formato del archivo de parámetros:"
    echo "  PARAMETRO=VALOR"
    echo "  # Los comentarios empiezan con #"
    echo ""
    echo "Parámetros soportados:"
    echo "  BASE_RADIUS, TOTAL_PARTICLES, NUM_LARGE_CIRCLES, NUM_SMALL_CIRCLES,"
    echo "  NUM_POLYGON_PARTICLES, NUM_SIDES, CURRENT_SIM, TOTAL_SIMS, SAVE_SIM_DATA,"
    echo "  SILO_HEIGHT, SILO_WIDTH, OUTLET_WIDTH, MIN_TIME"
    echo ""
    echo "Ejemplo:"
    echo "  $0 parametros_1.txt"
}

# Verificar si se pidió ayuda
if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    show_help
    exit 0
fi

# Verificar que se pase un archivo de parámetros
if [[ $# -lt 1 ]]; then
    echo -e "${RED}Error:${NC} Debes indicar un archivo de parámetros."
    echo "Uso: $0 archivo_parametros"
    exit 1
fi

# Archivo de parámetros
PARAM_FILE="$1"

# Verificar que el archivo de parámetros existe
if [[ ! -f "$PARAM_FILE" ]]; then
    echo -e "${RED}Error: El archivo de parámetros '$PARAM_FILE' no existe.${NC}"
    echo -e "${YELLOW}Para crear un archivo de ejemplo, copia 'parametros_ejemplo.txt' y modifícalo según tus necesidades.${NC}"
    exit 1
fi

echo -e "${BLUE}=== Script de Ejecución de Silo Simulator ===${NC}"
echo -e "${YELLOW}Archivo de parámetros: $PARAM_FILE${NC}"
echo ""

# Leer parámetros del archivo
echo -e "${BLUE}Leyendo parámetros...${NC}"
declare -A params

while IFS='=' read -r key value; do
    # Ignorar líneas vacías y comentarios
    if [[ -z "$key" ]] || [[ "$key" =~ ^[[:space:]]*# ]]; then
        continue
    fi
    
    # Limpiar espacios en blanco
    key=$(echo "$key" | tr -d '[:space:]')
    value=$(echo "$value" | tr -d '[:space:]')
    
    # Almacenar parámetro
    params["$key"]="$value"
    echo "  $key = $value"
done < "$PARAM_FILE"

echo ""


# Construir argumentos para silo_simulator
echo ""
echo -e "${BLUE}Preparando ejecución...${NC}"
ARGS=""

# Mapeo de parámetros a argumentos del programa
declare -A param_map=(
    ["BASE_RADIUS"]="--base-radius"
    ["CHI"]="--chi"
    ["SIZE-RATIO"]="--size-ratio"
    ["TOTAL_PARTICLES"]="--total-particles"
    ["NUM_LARGE_CIRCLES"]="--num-large-circles"
    ["NUM_SMALL_CIRCLES"]="--num-small-circles"
    ["NUM_POLYGON_PARTICLES"]="--num-polygon-particles"
    ["NUM_SIDES"]="--num-sides"
    ["CURRENT_SIM"]="--current-sim"
    ["TOTAL_SIMS"]="--total-sims"
    ["SAVE_SIM_DATA"]="--save-sim-data"
    ["SILO_HEIGHT"]="--silo-height"
    ["SILO_WIDTH"]="--silo-width"
    ["OUTLET_WIDTH"]="--outlet-width"
    ["MIN_TIME"]="--min-time"
)

# Construir argumentos
for param in "${!params[@]}"; do
    if [[ -n "${param_map[$param]}" ]]; then
        ARGS="$ARGS ${param_map[$param]} ${params[$param]}"
    else
        echo -e "${YELLOW}Advertencia: Parámetro desconocido '$param' ignorado.${NC}"
    fi
done

# Mostrar comando que se va a ejecutar
echo -e "${YELLOW}Comando a ejecutar:${NC}"
echo "./bin/silo_simulator$ARGS"
echo ""

# Ejecutar el simulador
echo -e "${BLUE}Ejecutando simulación...${NC}"
echo -e "${GREEN}===========================================${NC}"

if ./bin/silo_simulator$ARGS; then
    echo ""
    echo -e "${GREEN}===========================================${NC}"
    echo -e "${GREEN}Simulación completada exitosamente.${NC}"
else
    echo ""
    echo -e "${RED}===========================================${NC}"
    echo -e "${RED}Error durante la ejecución de la simulación.${NC}"
    exit 1
fi


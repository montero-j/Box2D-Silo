#!/bin/bash

# Script para renderizar simulación basada en parámetros de archivo
# Uso: ./renderizar_simulacion.sh [archivo_parametros] [duracion_video] [archivo_salida] [resolucion] [calidad]

set -e

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para mostrar ayuda
show_help() {
    echo "Uso: $0 [archivo_parametros] [duracion_video] [archivo_salida] [resolucion] [calidad]"
    echo ""
    echo "Este script renderiza una simulación basada en parámetros de archivo."
    echo ""
    echo "Argumentos:"
    echo "  archivo_parametros    Archivo con parámetros (por defecto: parametros_ejemplo.txt)"
    echo "  duracion_video       Duración del video en segundos (por defecto: 10)"
    echo "  archivo_salida       Nombre del archivo de video (por defecto: auto-generado)"
    echo "  resolucion           Resolución del video (por defecto: default)"
    echo "  calidad              Calidad de renderizado (por defecto: standard)"
    echo ""
    echo "Opciones de resolución disponibles:"
    echo "  hd         - HD (1280x1024)"
    echo "  full-hd    - Full HD (1920x1536)"
    echo "  4k         - 4K (3840x3072)"
    echo "  8k         - 8K (7680x6144)"
    echo "  default    - Resolución por defecto (1920x2560)"
    echo "  WIDTHxHEIGHT - Resolución personalizada (ej: 2560x2048)"
    echo ""
    echo "Opciones de calidad disponibles:"
    echo "  standard   - Calidad estándar (más rápido)"
    echo "  high       - Alta calidad (antialiasing mejorado, partículas más suaves)"
    echo ""
    echo "El script buscará automáticamente la simulación más reciente que coincida"
    echo "con los parámetros especificados en el archivo."
    echo ""
    echo "Ejemplos:"
    echo "  $0 simulacion_pequena.txt 15 mi_video.mp4 4k high"
    echo "  $0 parametros.txt 20 video_hd.mp4 full-hd standard"
    echo "  $0 test.txt 30 ultra.mp4 3840x2160 high"
}

# Verificar si se pidió ayuda
if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    show_help
    exit 0
fi

# Parámetros del script
PARAM_FILE="${1:-parametros_ejemplo.txt}"
VIDEO_DURATION="${2:-10}"
OUTPUT_FILE="$3"
RESOLUTION="${4:-default}"
QUALITY="${5:-standard}"

# Verificar que el archivo de parámetros existe
if [[ ! -f "$PARAM_FILE" ]]; then
    echo -e "${RED}Error: El archivo de parámetros '$PARAM_FILE' no existe.${NC}"
    exit 1
fi

echo -e "${BLUE}=== Script de Renderizado de Simulación ===${NC}"
echo -e "${YELLOW}Archivo de parámetros: $PARAM_FILE${NC}"
echo -e "${YELLOW}Duración del video: ${VIDEO_DURATION}s${NC}"
echo -e "${YELLOW}Resolución: $RESOLUTION${NC}"
echo -e "${YELLOW}Calidad: $QUALITY${NC}"
echo ""

#rm -r output_frames

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

# Extraer valores importantes para buscar la simulación
TOTAL_PARTICLES="${params[TOTAL_PARTICLES]:-2000}"
NUM_LARGE_CIRCLES="${params[NUM_LARGE_CIRCLES]:-2000}"
NUM_SMALL_CIRCLES="${params[NUM_SMALL_CIRCLES]:-0}"
NUM_POLYGON_PARTICLES="${params[NUM_POLYGON_PARTICLES]:-0}"
NUM_SIDES="${params[NUM_SIDES]:-5}"
BASE_RADIUS="${params[BASE_RADIUS]:-0.5}"
SILO_HEIGHT="${params[SILO_HEIGHT]:-120.0}"
SILO_WIDTH="${params[SILO_WIDTH]:-20.2}"
OUTLET_WIDTH="${params[OUTLET_WIDTH]:-3.9}"

# Generar nombre de archivo de salida si no se especificó
if [[ -z "$OUTPUT_FILE" ]]; then
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    OUTPUT_FILE="video_${TOTAL_PARTICLES}particles_${NUM_SIDES}sides_${TIMESTAMP}.mp4"
fi

echo -e "${BLUE}Buscando simulación correspondiente...${NC}"

# Buscar directorios de simulación que coincidan con los parámetros
SEARCH_PATTERN="*${TOTAL_PARTICLES}*lg${NUM_LARGE_CIRCLES}*sm${NUM_SMALL_CIRCLES}*poly${NUM_POLYGON_PARTICLES}*sides${NUM_SIDES}*br${BASE_RADIUS}*"

# Buscar en diferentes ubicaciones posibles
SIMULATION_DIRS=(
    "./simulations/sim_${SEARCH_PATTERN}"
    "./data/simulations/sim_${SEARCH_PATTERN}"
    "./examples/simulations/sim_${SEARCH_PATTERN}"
    "./tests/simulations/sim_${SEARCH_PATTERN}"
)

FOUND_SIM=""
for pattern in "${SIMULATION_DIRS[@]}"; do
    for dir in $pattern; do
        if [[ -d "$dir" ]] && [[ -f "$dir/simulation_data.csv" ]]; then
            FOUND_SIM="$dir"
            break 2
        fi
    done
done

# Si no se encontró con el patrón específico, buscar la simulación más reciente
if [[ -z "$FOUND_SIM" ]]; then
    echo -e "${YELLOW}No se encontró simulación con patrón específico, buscando la más reciente...${NC}"
    
    # Buscar la simulación más reciente en todas las ubicaciones
    RECENT_SIM=$(find ./simulations/ ./data/simulations/ ./examples/simulations/ ./tests/simulations/ -name "simulation_data.csv" -type f 2>/dev/null | head -1)
    
    if [[ -n "$RECENT_SIM" ]]; then
        FOUND_SIM=$(dirname "$RECENT_SIM")
    fi
fi

if [[ -z "$FOUND_SIM" ]]; then
    echo -e "${RED}Error: No se encontró ninguna simulación para renderizar.${NC}"
    echo -e "${YELLOW}Asegúrate de haber ejecutado una simulación primero con:${NC}"
    echo -e "${YELLOW}  ./ejecutar_simulacion.sh $PARAM_FILE${NC}"
    exit 1
fi

echo -e "${GREEN}Simulación encontrada: $FOUND_SIM${NC}"

# Verificar que existe el archivo de datos de simulación
DATA_FILE="$FOUND_SIM/simulation_data.csv"
if [[ ! -f "$DATA_FILE" ]]; then
    echo -e "${RED}Error: No se encontró el archivo de datos: $DATA_FILE${NC}"
    exit 1
fi

# Verificar que el entorno de Python esté disponible
echo -e "${BLUE}Verificando entorno de Python...${NC}"
if command -v python3 &> /dev/null; then
    PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
    PYTHON_CMD="python"
else
    echo -e "${RED}Error: Python no está disponible.${NC}"
    exit 1
fi

# Verificar si el script de renderizado existe
RENDER_SCRIPT="analysis/render_simulation.py"
if [[ ! -f "$RENDER_SCRIPT" ]]; then
    echo -e "${RED}Error: El script de renderizado no existe: $RENDER_SCRIPT${NC}"
    exit 1
fi

# Construir comando de renderizado
echo -e "${BLUE}Preparando renderizado...${NC}"

# Configurar resolución
RESOLUTION_ARGS=""
case "$RESOLUTION" in
    "hd")
        RESOLUTION_ARGS="--hd"
        echo -e "${YELLOW}Usando resolución HD (1280x1024)${NC}"
        ;;
    "full-hd")
        RESOLUTION_ARGS="--full-hd"
        echo -e "${YELLOW}Usando resolución Full HD (1920x1536)${NC}"
        ;;
    "4k")
        RESOLUTION_ARGS="--4k"
        echo -e "${YELLOW}Usando resolución 4K (3840x3072)${NC}"
        ;;
    "8k")
        RESOLUTION_ARGS="--8k"
        echo -e "${YELLOW}Usando resolución 8K (7680x6144)${NC}"
        ;;
    "default")
        RESOLUTION_ARGS=""
        echo -e "${YELLOW}Usando resolución por defecto (1920x2560)${NC}"
        ;;
    *x*)
        # Resolución personalizada en formato WIDTHxHEIGHT
        WIDTH=$(echo "$RESOLUTION" | cut -d'x' -f1)
        HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f2)
        if [[ "$WIDTH" =~ ^[0-9]+$ ]] && [[ "$HEIGHT" =~ ^[0-9]+$ ]]; then
            RESOLUTION_ARGS="--width $WIDTH --height $HEIGHT"
            echo -e "${YELLOW}Usando resolución personalizada (${WIDTH}x${HEIGHT})${NC}"
        else
            echo -e "${RED}Error: Formato de resolución personalizada inválido: $RESOLUTION${NC}"
            echo -e "${YELLOW}Use el formato WIDTHxHEIGHT, ejemplo: 2560x2048${NC}"
            exit 1
        fi
        ;;
    *)
        echo -e "${RED}Error: Resolución no reconocida: $RESOLUTION${NC}"
        echo -e "${YELLOW}Opciones válidas: hd, full-hd, 4k, 8k, default, o WIDTHxHEIGHT${NC}"
        exit 1
        ;;
esac

# Configurar calidad
QUALITY_ARGS=""
case "$QUALITY" in
    "standard")
        QUALITY_ARGS=""
        echo -e "${YELLOW}Usando calidad estándar${NC}"
        ;;
    "high")
        QUALITY_ARGS="--high-quality"
        echo -e "${YELLOW}Usando alta calidad (antialiasing mejorado, partículas más suaves)${NC}"
        ;;
    *)
        echo -e "${RED}Error: Calidad no reconocida: $QUALITY${NC}"
        echo -e "${YELLOW}Opciones válidas: standard, high${NC}"
        exit 1
        ;;
esac

RENDER_CMD="$PYTHON_CMD $RENDER_SCRIPT"
RENDER_CMD="$RENDER_CMD --target-video-duration $VIDEO_DURATION"
RENDER_CMD="$RENDER_CMD --video-output $OUTPUT_FILE"
RENDER_CMD="$RENDER_CMD --data-path $DATA_FILE"
RENDER_CMD="$RENDER_CMD --total-particles $TOTAL_PARTICLES"
RENDER_CMD="$RENDER_CMD --num-large-circles $NUM_LARGE_CIRCLES"
RENDER_CMD="$RENDER_CMD --num-small-circles $NUM_SMALL_CIRCLES"
RENDER_CMD="$RENDER_CMD --num-polygon-particles $NUM_POLYGON_PARTICLES"
RENDER_CMD="$RENDER_CMD --base-radius $BASE_RADIUS"
RENDER_CMD="$RENDER_CMD --silo-height $SILO_HEIGHT"
RENDER_CMD="$RENDER_CMD --silo-width $SILO_WIDTH"
RENDER_CMD="$RENDER_CMD --outlet-width $OUTLET_WIDTH"

# Añadir argumentos de resolución
if [[ -n "$RESOLUTION_ARGS" ]]; then
    RENDER_CMD="$RENDER_CMD $RESOLUTION_ARGS"
fi

# Añadir argumentos de calidad
if [[ -n "$QUALITY_ARGS" ]]; then
    RENDER_CMD="$RENDER_CMD $QUALITY_ARGS"
fi

# Agregar tiempo mínimo si está especificado
if [[ -n "${params[MIN_TIME]}" ]]; then
    RENDER_CMD="$RENDER_CMD --min-time ${params[MIN_TIME]}"
fi

# Mostrar comando que se va a ejecutar
echo -e "${YELLOW}Comando de renderizado:${NC}"
echo "$RENDER_CMD"
echo ""

# Ejecutar renderizado
echo -e "${BLUE}Iniciando renderizado...${NC}"
echo -e "${GREEN}===========================================${NC}"

if eval "$RENDER_CMD"; then
    echo ""
    echo -e "${GREEN}===========================================${NC}"
    echo -e "${GREEN}Renderizado completado exitosamente.${NC}"
    echo -e "${YELLOW}Video generado: $OUTPUT_FILE${NC}"
    
    # Mostrar información del archivo generado si existe
    if [[ -f "$OUTPUT_FILE" ]]; then
        FILE_SIZE=$(ls -lh "$OUTPUT_FILE" | awk '{print $5}')
        echo -e "${BLUE}Tamaño del archivo: $FILE_SIZE${NC}"
    fi
else
    echo ""
    echo -e "${RED}===========================================${NC}"
    echo -e "${RED}Error durante el renderizado.${NC}"
    exit 1
fi

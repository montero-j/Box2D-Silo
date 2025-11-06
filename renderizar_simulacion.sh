#!/bin/bash
# Renderizador para el nuevo CSV:
#   Time, circles_begin, cx,cy,r, ..., circles_end, polygons_begin, x1,y1,...,xN,yN, ..., polygons_end
# Requiere --poly-sides (N lados por polígono)

set -e

# Colores
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

show_help() {
  echo "Uso: $0 [archivo_parametros] [duracion_video] [archivo_salida] [resolucion] [calidad]"
  echo
  echo "Ejemplos:"
  echo "  $0 parametros.txt 20 video_hd.mp4 full-hd high"
  echo "  $0 parametros.txt 15 out.mp4 4k standard"
  echo
  echo "Resoluciones: hd | full-hd | 4k | 8k | default | WIDTHxHEIGHT"
  echo "Calidad: standard | high"
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then show_help; exit 0; fi

# -------------------------
# Parámetros
# -------------------------
PARAM_FILE="${1:-parametros_ejemplo_2.txt}"
VIDEO_DURATION="${2:-162}"
OUTPUT_FILE="$3"
RESOLUTION="${4:-default}"
QUALITY="${5:-standard}"

if [[ ! -f "$PARAM_FILE" ]]; then
  echo -e "${RED}Error: no existe '$PARAM_FILE'.${NC}"; exit 1
fi

echo -e "${BLUE}=== Script de Renderizado de Simulación (formato círculos+vértices) ===${NC}"
echo -e "${YELLOW}Archivo de parámetros: $PARAM_FILE${NC}"
echo -e "${YELLOW}Duración del video: ${VIDEO_DURATION}s${NC}"
echo -e "${YELLOW}Resolución: $RESOLUTION${NC}"
echo -e "${YELLOW}Calidad: $QUALITY${NC}"
echo

# -------------------------
# Leer parámetros key=value
# -------------------------
declare -A params
while IFS='=' read -r key value; do
  # Ignorar vacíos/comentarios
  [[ -z "$key" ]] && continue
  [[ "$key" =~ ^[[:space:]]*# ]] && continue
  # Limpieza
  key=$(echo "$key" | tr -d '[:space:]')
  value=$(echo "$value" | tr -d '[:space:]')
  params["$key"]="$value"
  echo "  $key = $value"
done < "$PARAM_FILE"
echo

# -------------------------
# Extraer valores
# -------------------------
TOTAL_PARTICLES="${params[TOTAL_PARTICLES]:-2000}"
NUM_LARGE_CIRCLES="${params[NUM_LARGE_CIRCLES]:-2000}"
NUM_SMALL_CIRCLES="${params[NUM_SMALL_CIRCLES]:-0}"
NUM_POLYGON_PARTICLES="${params[NUM_POLYGON_PARTICLES]:-0}"
NUM_SIDES="${params[NUM_SIDES]:-5}"
BASE_RADIUS="${params[BASE_RADIUS]:-0.5}"
SILO_HEIGHT="${params[SILO_HEIGHT]:-120.0}"
SILO_WIDTH="${params[SILO_WIDTH]:-20.2}"
OUTLET_WIDTH="${params[OUTLET_WIDTH]:-3.9}"

# Nombre de salida si faltó
if [[ -z "$OUTPUT_FILE" ]]; then
  TIMESTAMP=$(date +%Y%m%d_%H%M%S)
  OUTPUT_FILE="video_${TOTAL_PARTICLES}particles_${NUM_SIDES}sides_${TIMESTAMP}.mp4"
fi

# -------------------------
# Encontrar simulación
# -------------------------
echo -e "${BLUE}Buscando simulación correspondiente...${NC}"

# El nombre de carpeta lo arma C++ como:
# sim_<...>part_<TOTAL>_chi<..>_ratio<..>_br<..>_lg<..>_sm<..>_poly<..>_sides<..>_outlet<..>_maxAva<..>
# Para evitar mismatch por decimales de chi/ratio, no los usamos en el patrón.
SEARCH_PATTERN="*part_${TOTAL_PARTICLES}*br${BASE_RADIUS}*lg${NUM_LARGE_CIRCLES}*sm${NUM_SMALL_CIRCLES}*poly${NUM_POLYGON_PARTICLES}*sides${NUM_SIDES}*outlet${OUTLET_WIDTH}*"

SIMULATION_DIRS=(
  "/home/jmontero/Box2D/Box2D-Silo/simulations/sim_${SEARCH_PATTERN}"
  "./simulations/sim_${SEARCH_PATTERN}"
)

FOUND_SIM=""
for pattern in "${SIMULATION_DIRS[@]}"; do
  for dir in $pattern; do
    if [[ -d "$dir" && -f "$dir/simulation_data.csv" ]]; then
      FOUND_SIM="$dir"
      break 2
    fi
  done
done

# Último recurso: más reciente
if [[ -z "$FOUND_SIM" ]]; then
  echo -e "${YELLOW}No se encontró por patrón; buscando la simulación más reciente...${NC}"
  RECENT_SIM=$(find ./simulations/ ./data/simulations/ ./examples/simulations/ ./tests/simulations/ \
               -name "simulation_data.csv" -type f 2>/dev/null | sort -r | head -1)
  [[ -n "$RECENT_SIM" ]] && FOUND_SIM=$(dirname "$RECENT_SIM")
fi

if [[ -z "$FOUND_SIM" ]]; then
  echo -e "${RED}Error: no se encontró ninguna simulación para renderizar.${NC}"
  echo -e "${YELLOW}Ejecutá primero:  ./ejecutar_simulacion.sh $PARAM_FILE${NC}"
  exit 1
fi

echo -e "${GREEN}Simulación encontrada: $FOUND_SIM${NC}"

DATA_FILE="$FOUND_SIM/simulation_data.csv"
if [[ ! -f "$DATA_FILE" ]]; then
  echo -e "${RED}Error: falta $DATA_FILE${NC}"; exit 1
fi

# -------------------------
# Python
# -------------------------
echo -e "${BLUE}Verificando Python...${NC}"
if command -v python3 &>/dev/null; then PYTHON_CMD="python3"
elif command -v python &>/dev/null; then PYTHON_CMD="python"
else echo -e "${RED}Error: Python no disponible.${NC}"; exit 1; fi

# **Ruta del nuevo renderer** (ajustá si lo pusiste en otra carpeta)
RENDER_SCRIPT="script/render_simulation_cpu.py"
if [[ ! -f "$RENDER_SCRIPT" ]]; then
  echo -e "${RED}Error: no existe el render: $RENDER_SCRIPT${NC}"
  echo -e "${YELLOW}Asegurate de actualizar la ruta al archivo Python nuevo.${NC}"
  exit 1
fi

# -------------------------
# Resolución
# -------------------------
RESOLUTION_ARGS=""
case "$RESOLUTION" in
  "hd") RESOLUTION_ARGS="--hd"; echo -e "${YELLOW}Resolución: HD (1280x1024)${NC}";;
  "full-hd") RESOLUTION_ARGS="--full-hd"; echo -e "${YELLOW}Resolución: Full HD (1920x1536)${NC}";;
  "4k") RESOLUTION_ARGS="--4k"; echo -e "${YELLOW}Resolución: 4K (3840x3072)${NC}";;
  "8k") RESOLUTION_ARGS="--8k"; echo -e "${YELLOW}Resolución: 8K (7680x6144)${NC}";;
  "default") RESOLUTION_ARGS=""; echo -e "${YELLOW}Resolución por defecto (1920x2560)${NC}";;
  *x*)
    WIDTH=$(echo "$RESOLUTION" | cut -d'x' -f1)
    HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f2)
    if [[ "$WIDTH" =~ ^[0-9]+$ && "$HEIGHT" =~ ^[0-9]+$ ]]; then
      RESOLUTION_ARGS="--width $WIDTH --height $HEIGHT"
      echo -e "${YELLOW}Resolución personalizada (${WIDTH}x${HEIGHT})${NC}"
    else
      echo -e "${RED}Error: resolución inválida '$RESOLUTION' (use WIDTHxHEIGHT).${NC}"
      exit 1
    fi
    ;;
  *) echo -e "${RED}Error: resolución no reconocida '$RESOLUTION'.${NC}"; exit 1;;
esac

# -------------------------
# Calidad
# -------------------------
QUALITY_ARGS=""
case "$QUALITY" in
  "standard") QUALITY_ARGS=""; echo -e "${YELLOW}Calidad estándar${NC}";;
  "high") QUALITY_ARGS="--high-quality"; echo -e "${YELLOW}Alta calidad${NC}";;
  *) echo -e "${RED}Error: calidad no reconocida '$QUALITY'.${NC}"; exit 1;;
esac

# -------------------------
# Comando de render
# -------------------------
RENDER_CMD=()
RENDER_CMD+=("$PYTHON_CMD" "$RENDER_SCRIPT")
RENDER_CMD+=("--target-video-duration" "$VIDEO_DURATION")
RENDER_CMD+=("--video-output" "$OUTPUT_FILE")
RENDER_CMD+=("--data-path" "$DATA_FILE")

# Parámetros geométricos (útiles para walls/proporciones)
RENDER_CMD+=("--base-radius" "$BASE_RADIUS")
RENDER_CMD+=("--silo-height" "$SILO_HEIGHT")
RENDER_CMD+=("--silo-width" "$SILO_WIDTH")
RENDER_CMD+=("--outlet-width" "$OUTLET_WIDTH")

# Polígonos: **obligatorio** informar lados
RENDER_CMD+=("--poly-sides" "$NUM_SIDES")

# Rango temporal (opcional)
if [[ -n "${params[MIN_TIME]}" ]]; then
  RENDER_CMD+=("--min-time" "${params[MIN_TIME]}")
fi
if [[ -n "${params[MAX_TIME]}" ]]; then
  RENDER_CMD+=("--max-time" "${params[MAX_TIME]}")
fi

# Resolución/calidad
[[ -n "$RESOLUTION_ARGS" ]] && RENDER_CMD+=($RESOLUTION_ARGS)
[[ -n "$QUALITY_ARGS" ]]    && RENDER_CMD+=($QUALITY_ARGS)

# Mostrar comando
echo -e "${YELLOW}Comando de renderizado:${NC}"
printf '%q ' "${RENDER_CMD[@]}"; echo
echo

# -------------------------
# Ejecutar
# -------------------------
echo -e "${BLUE}Iniciando renderizado...${NC}"
echo -e "${GREEN}===========================================${NC}"

if "${RENDER_CMD[@]}"; then
  echo
  echo -e "${GREEN}===========================================${NC}"
  echo -e "${GREEN}Renderizado completado exitosamente.${NC}"
  echo -e "${YELLOW}Video generado: $OUTPUT_FILE${NC}"
  if [[ -f "$OUTPUT_FILE" ]]; then
    FILE_SIZE=$(ls -lh "$OUTPUT_FILE" | awk '{print $5}')
    echo -e "${BLUE}Tamaño del archivo: $FILE_SIZE${NC}"
  fi
else
  echo
  echo -e "${RED}===========================================${NC}"
  echo -e "${RED}Error durante el renderizado.${NC}"
  exit 1
fi

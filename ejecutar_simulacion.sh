#!/bin/bash
# =============================================================================
# ejecutar_simulacion_1.sh
# -----------------------------------------------------------------------------
# Compila (opcional) y ejecuta ./bin/silo_simulator leyendo parámetros desde
# un archivo tipo KEY=VALUE (líneas con # se ignoran).
#
# Uso:
#   ./ejecutar_simulacion_1.sh run/discos/param_files/parametros_1.txt
#
# Parámetros soportados en el archivo:
#   BASE_RADIUS, SIZE-RATIO, CHI, TOTAL_PARTICLES,
#   NUM_LARGE_CIRCLES, NUM_SMALL_CIRCLES, NUM_POLYGON_PARTICLES, NUM_SIDES,
#   CURRENT_SIM, TOTAL_SIMS, SAVE_SIM_DATA,
#   SILO_HEIGHT, SILO_WIDTH, OUTLET_WIDTH,
#   EXIT_CHECK_EVERY_STEPS, SAVE_FRAME_EVERY_STEPS
#
# Flags de ayuda:
#   -h / --help            Muestra esta ayuda
#   --no-build             No intenta compilar (usa binario existente)
#   --dry-run              Muestra el comando final pero no ejecuta
#
# Requisitos:
#   - ./bin/silo_simulator (o Makefile / build.sh para compilar)
# =============================================================================

set -Eeuo pipefail

# ----------------------------------------
# Colores
# ----------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# ----------------------------------------
# Manejo de errores
# ----------------------------------------
trap 'echo -e "\n${RED}[ERROR]${NC} Falló en línea ${BASH_LINENO[0]}: ${BASH_COMMAND}" >&2' ERR

# ----------------------------------------
# Ayuda
# ----------------------------------------
show_help() {
  cat <<EOF
${BOLD}Uso:${NC} $0 [opciones] <archivo_parametros>

${BOLD}Opciones:${NC}
  --no-build                 No compilar (usar binario existente)
  --dry-run                  Sólo mostrar el comando a ejecutar, sin correrlo
  -h, --help                 Mostrar esta ayuda

${BOLD}Archivo de parámetros (formato KEY=VALUE):${NC}
  BASE_RADIUS                Radio base
  SIZE-RATIO                 Razón de tamaños (r)
  CHI                        Fracción de partículas pequeñas
  TOTAL_PARTICLES            Total de partículas
  NUM_LARGE_CIRCLES          Cantidad de discos grandes
  NUM_SMALL_CIRCLES          Cantidad de discos chicos
  NUM_POLYGON_PARTICLES      Cantidad de partículas poligonales
  NUM_SIDES                  Lados de los polígonos
  CURRENT_SIM                Índice de simulación inicial
  TOTAL_SIMS                 Total de simulaciones a ejecutar
  SAVE_SIM_DATA              0/1 guardar simulation_data.csv
  SILO_HEIGHT                Altura del silo
  SILO_WIDTH                 Ancho del silo
  OUTLET_WIDTH               Abertura del silo
  EXIT_CHECK_EVERY_STEPS     Verificar salida cada N pasos (default 10)
  SAVE_FRAME_EVERY_STEPS     Guardar frames cada M pasos (default 100)

${BOLD}Ejemplo:${NC}
  $0 run/discos/param_files/parametros_1.txt
EOF
}

# ----------------------------------------
# Parseo de flags propios del script
# ----------------------------------------
DO_BUILD=1
DRY_RUN=0

POSITIONAL_ARGS=()
while (($#)); do
  case "$1" in
    --no-build) DO_BUILD=0; shift ;;
    --dry-run)  DRY_RUN=1; shift ;;
    -h|--help)  show_help; exit 0 ;;
    --) shift; break ;;
    -*)
      echo -e "${YELLOW}[AVISO]${NC} Opción desconocida para el script: $1"
      shift
      ;;
    *)
      POSITIONAL_ARGS+=("$1"); shift ;;
  esac
done
set -- "${POSITIONAL_ARGS[@]}"

# ----------------------------------------
# Validar archivo de parámetros
# ----------------------------------------
if [[ $# -lt 1 ]]; then
  echo -e "${RED}[ERROR]${NC} Debes indicar el archivo de parámetros."
  show_help
  exit 1
fi
PARAM_FILE="$1"
if [[ ! -f "$PARAM_FILE" ]]; then
  echo -e "${RED}[ERROR]${NC} No existe el archivo: ${PARAM_FILE}"
  exit 1
fi

echo -e "${BLUE}=== Script de Ejecución de Silo Simulator ===${NC}"
echo -e "${CYAN}Archivo de parámetros:${NC} ${PARAM_FILE}"
echo

# ----------------------------------------
# Compilación (opcional)
# ----------------------------------------
if (( DO_BUILD )); then
  if [[ -f Makefile ]]; then
    echo -e "${BLUE}Compilando (make -j) ...${NC}"
    make -j
    echo
  elif [[ -f build.sh ]]; then
    echo -e "${BLUE}Compilando (build.sh) ...${NC}"
    bash build.sh
    echo
  else
    echo -e "${YELLOW}[AVISO]${NC} No se encontró Makefile ni build.sh. Se asume binario existente."
    echo
  fi
fi

if [[ ! -x ./bin/silo_simulator ]]; then
  echo -e "${RED}[ERROR]${NC} No se encontró el binario ./bin/silo_simulator"
  exit 1
fi

# ----------------------------------------
# Lectura de parámetros
# ----------------------------------------
declare -A params=()
# Permite valores con espacios si están al final (se recortan espacios laterales)
while IFS='=' read -r raw_key raw_val; do
  # Ignorar comentarios y líneas vacías
  [[ -z "${raw_key// /}" ]] && continue
  [[ "${raw_key}" =~ ^[[:space:]]*# ]] && continue

  # Normalizar key y value (quitar espacios laterales)
  key="$(echo -n "${raw_key}" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
  val="$(echo -n "${raw_val}" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"

  # Evitar claves vacías
  [[ -z "$key" ]] && continue

  params["$key"]="$val"
done < "$PARAM_FILE"

# Mostrar resumen ordenado por clave
echo -e "${BLUE}Parámetros leídos:${NC}"
for k in $(printf "%s\n" "${!params[@]}" | sort); do
  printf "  %-26s = %s\n" "$k" "${params[$k]}"
done
echo

# ----------------------------------------
# Mapeo KEY -> flags del binario
# ----------------------------------------
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
  # Nuevos (frecuencias)
  ["EXIT_CHECK_EVERY_STEPS"]="--exit-check-every"
  ["SAVE_FRAME_EVERY_STEPS"]="--save-frame-every"
)

# ----------------------------------------
# Construir ARGS
# ----------------------------------------
ARGS=""
UNKNOWN_KEYS=()

for key in "${!params[@]}"; do
  if [[ -n "${param_map[$key]:-}" ]]; then
    # Quote por si el valor trae algo raro (aunque normalmente son numéricos)
    ARGS+=" ${param_map[$key]} ${params[$key]}"
  else
    UNKNOWN_KEYS+=("$key")
  fi
done

# Avisar por claves desconocidas (pero no abortar)
if ((${#UNKNOWN_KEYS[@]})); then
  echo -e "${YELLOW}[AVISO]${NC} Se ignoraron ${#UNKNOWN_KEYS[@]} clave(s) no mapeadas:"
  for k in $(printf "%s\n" "${UNKNOWN_KEYS[@]}" | sort); do
    echo "  - $k"
  done
  echo
fi

# ----------------------------------------
# Mostrar comando final
# ----------------------------------------
echo -e "${YELLOW}Comando a ejecutar:${NC}"
echo "./bin/silo_simulator${ARGS}"
echo

# ----------------------------------------
# Ejecutar
# ----------------------------------------
if (( DRY_RUN )); then
  echo -e "${YELLOW}[DRY-RUN]${NC} No se ejecuta el simulador."
  exit 0
fi

echo -e "${GREEN}===========================================${NC}"
./bin/silo_simulator${ARGS}
RET=$?
echo -e "${GREEN}===========================================${NC}"

if (( RET == 0 )); then
  echo -e "${GREEN}Simulación completada exitosamente.${NC}"
else
  echo -e "${RED}Error durante la ejecución de la simulación (exit ${RET}).${NC}"
  exit "$RET"
fi

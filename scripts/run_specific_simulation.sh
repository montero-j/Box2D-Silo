#!/bin/bash

# run_specific_simulation.sh
# Ejecuta 50 simulaciones con los mismos parámetros r y chi

# Configuración
BASE_DIR=$(pwd)
RESULTADOS_DIR="${BASE_DIR}/resultados"
MAX_AVALANCHES=100
TOTAL_SIMULATIONS=1
SAVE_DATA_FIRST=5
MAX_PARALLEL=5  # Número máximo de simulaciones en paralelo

# Verificar argumentos
if [ $# -ne 2 ]; then
    echo "Uso: $0 <target_r> <target_chi>"
    exit 1
fi

# Parámetros específicos
TARGET_R=$1
TARGET_CHI=$2

# Crear directorio principal para estos parámetros
MAIN_DIR="${RESULTADOS_DIR}/r_${TARGET_R}/chi_${TARGET_CHI}"
mkdir -p "$MAIN_DIR"

# Compilar código
echo "Compilando código..."
make clean
make -C "$BASE_DIR"  # Asegurarse de compilar en el directorio base
if [ $? -ne 0 ]; then
    echo "Error de compilación"
    exit 1
fi

# Contador para trabajos en paralelo
current_jobs=0

# Ejecutar múltiples simulaciones
for ((sim=1; sim<=TOTAL_SIMULATIONS; sim++)); do
    SIM_DIR="${MAIN_DIR}/sim_${sim}"
    mkdir -p "$SIM_DIR"
    
    # Determinar si guardar datos de simulación
    SAVE_DATA=$((sim <= SAVE_DATA_FIRST ? 1 : 0))
    
    # Copiar ejecutable
    cp "${BASE_DIR}/bin/silo_simulator" "$SIM_DIR/"
    
    # Ejecutar simulación en segundo plano
    (
        cd "$SIM_DIR"
        echo "Iniciando simulación $sim de $TOTAL_SIMULATIONS (r=$TARGET_R, chi=$TARGET_CHI)"
        
        ./silo_simulator \
            --target-r $TARGET_R \
            --target-chi $TARGET_CHI \
            --current-sim $sim \
            --total-sims $TOTAL_SIMULATIONS \
            --save-sim-data $SAVE_DATA \
            --max-avalanches $MAX_AVALANCHES \
            --base-radius 0.25 \
            --total-particles 500 \
            --polygon-particles 0 > simulation.log 2>&1
        
        echo "Simulación $sim completada"
    ) &
    
    # Controlar el número de trabajos en paralelo
    current_jobs=$((current_jobs + 1))
    if (( current_jobs >= MAX_PARALLEL )); then
        # Esperar a que al menos un trabajo termine
        wait -n
        current_jobs=$((current_jobs - 1))
    fi
done

# Esperar a que terminen todos los trabajos pendientes
wait
echo "Todas las simulaciones para r=$TARGET_R, chi=$TARGET_CHI completadas!"
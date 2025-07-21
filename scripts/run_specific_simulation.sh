#!/bin/bash

# run_specific_simulation.sh
# Ejecuta N simulaciones con los mismos parámetros r, chi y total_particles

# Configuración
BASE_DIR=$(pwd)
RESULTADOS_DIR="${BASE_DIR}/resultados"

# Número de simulaciones a correr por cada configuración (puedes ajustarlo aquí)
# Para correr una 'configuración específica' como pidió el usuario, se deja en 1.
TOTAL_SIMULATIONS=1 # Cambiar a un valor mayor (ej. 50) si quieres múltiples simulaciones por cada configuración
SAVE_DATA_FIRST=1   # Guardar datos de simulación para al menos la primera ejecución
MAX_PARALLEL=5      # Número máximo de simulaciones en paralelo

# Verificar argumentos
if [ $# -ne 3 ]; then
    echo "Uso: $0 <target_r> <target_chi> <total_particles>"
    echo "  Ejemplo: $0 0.4 0.2 250"
    exit 1
fi

# Parámetros específicos
TARGET_R=$1
TARGET_CHI=$2
TOTAL_P=$3 # Nuevo argumento para el número total de partículas

# Crear directorio principal para estos parámetros
MAIN_DIR="${RESULTADOS_DIR}/r_${TARGET_R}/chi_${TARGET_CHI}/total_p_${TOTAL_P}"
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
    
    # Determinar si guardar datos de simulación (solo para los primeros SAVE_DATA_FIRST runs)
    SAVE_DATA=$((sim <= SAVE_DATA_FIRST ? 1 : 0))
    
    # Copiar ejecutable
    cp "${BASE_DIR}/bin/silo_simulator" "$SIM_DIR/"
    
    # Ejecutar simulación en segundo plano
    (
        cd "$SIM_DIR"
        echo "Iniciando simulación $sim de $TOTAL_SIMULATIONS (r=$TARGET_R, chi=$TARGET_CHI, total_p=$TOTAL_P)"
        
        ./silo_simulator \
            --size-ratio $TARGET_R \
            --chi $TARGET_CHI \
            --current-sim $sim \
            --total-sims $TOTAL_SIMULATIONS \
            --save-sim-data $SAVE_DATA \
            --total-particles $TOTAL_P \
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
echo "Todas las simulaciones para r=$TARGET_R, chi=$TARGET_CHI, total_p=$TOTAL_P completadas!"
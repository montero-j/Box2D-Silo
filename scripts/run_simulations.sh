#!/bin/bash

# Configuración
BASE_DIR=$(pwd)
SRC_DIR="${BASE_DIR}/src"
SCRIPTS_DIR="${BASE_DIR}/scripts"
RESULTADOS_DIR="${BASE_DIR}/resultados"
MAX_PARALLEL=1  # Número máximo de simulaciones en paralelo (por defecto)
MAX_AVALANCHES=100  # Número de avalanchas por defecto

# Procesar argumentos de línea de comandos
while getopts "j:m:" opt; do
  case $opt in
    j)
      MAX_PARALLEL=$OPTARG
      ;;
    m)
      MAX_AVALANCHES=$OPTARG
      ;;
    \?)
      echo "Opción inválida: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

# Valores a variar
size_ratios=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)
chis=(0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)

# Crear directorio de resultados
mkdir -p "$RESULTADOS_DIR"

# Compilar código base una sola vez
echo "Compilando código base con máximo de avalanchas=$MAX_AVALANCHES..."
make -C "$BASE_DIR" clean
make -C "$BASE_DIR"
if [ $? -ne 0 ]; then
    echo "Error de compilación base"
    exit 1
fi

# Contador para trabajos en paralelo
current_jobs=0

# Crear y ejecutar todas las simulaciones
for r in "${size_ratios[@]}"; do
    for chi in "${chis[@]}"; do
        # Crear directorio para esta simulación
        sim_dir="${RESULTADOS_DIR}/r_${r}/chi_${chi}"
        mkdir -p "$sim_dir"
        
        # Copiar ejecutable a la carpeta
        cp "${BASE_DIR}/bin/silo_simulator" "$sim_dir/"
        
        # Crear script de ejecución específico
        cat > "${sim_dir}/run_simulation.sh" <<EOF
#!/bin/bash

# Parámetros para esta simulación
RATIO=$r
CHI=$chi
MAX_AVAL=$MAX_AVALANCHES

# Ejecutar simulador con parámetros
./silo_simulator \\
    --size-ratio \$RATIO \\
    --chi \$CHI \\
    --max-avalanches \$MAX_AVAL \\
    --base-radius 0.25 \\
    --total-particles 780 \\
    --polygon-particles 0
EOF

        # Dar permisos al script
        chmod +x "${sim_dir}/run_simulation.sh"
        
        # Crear Makefile específico para esta carpeta
        cat > "${sim_dir}/Makefile" <<EOF
# Makefile para simulación r=$r, chi=$chi

.PHONY: run clean

run:
	./run_simulation.sh

clean:
	rm -rf simulation_data
EOF
        
        # Ejecutar simulación en segundo plano
        (
            cd "$sim_dir"
            echo "Iniciando simulación: r=${r}, chi=${chi}"
            ./run_simulation.sh > simulation.log 2>&1
            echo "Simulación r=${r}, chi=${chi} completada"
        ) &
        
        # Controlar el número de trabajos en paralelo
        current_jobs=$((current_jobs + 1))
        if (( current_jobs >= MAX_PARALLEL )); then
            # Esperar a que al menos un trabajo termine
            wait -n
            current_jobs=$((current_jobs - 1))
        fi
    done
done

# Esperar a que terminen todos los trabajos pendientes
wait

echo "Todas las simulaciones completadas!"

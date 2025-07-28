#!/bin/bash
# Ejemplo: Ejecutar simulación rápida de círculos
../bin/silo_simulator \
    --base-radius 0.065 \
    --outlet-width 0.26 \
    --particles 500 \
    --time 60 \
    --polygon 0 \
    --save-flow 1 \
    --save-avalanches 1

echo "EJEMPLO: Simulación Rápida de Círculos"
echo "==========================================="

# Compilar si es necesario
if [ ! -f "../bin/silo_simulator" ]; then
    echo "Compilando simulador..."
    cd ..
    make clean && make
    cd examples
fi

# Verificar ejecutable
if [ ! -f "../bin/silo_simulator" ]; then
    echo "Error: No se pudo compilar el simulador"
    exit 1
fi

echo "Ejecutando simulación de ejemplo..."
echo "   - Forma: Círculos"
echo "   - Partículas: 500"
echo "   - Tiempo: 60 segundos"
echo "   - D/d ratio: 4.0"

# Ejecutar simulación
../bin/silo_simulator \
    --base-radius 0.065 \
    --outlet-width 0.26 \
    --particles 500 \
    --time 60 \
    --polygon 0 \
    --save-flow 1 \
    --save-avalanches 1

echo ""
echo "Simulación completada!"
echo "Resultados en: ../data/simulations/"

# Mostrar resultados básicos si existen
if [ -d "../data/simulations" ]; then
    latest_sim=$(ls -dt ../data/simulations/sim_* 2>/dev/null | head -1)
    if [ -n "$latest_sim" ]; then
        echo "Simulación más reciente: $(basename "$latest_sim")"
        
        if [ -f "$latest_sim/avalanche_data.csv" ]; then
            avalanche_count=$(grep -c "Avalancha" "$latest_sim/avalanche_data.csv" 2>/dev/null || echo "0")
            echo "Avalanchas detectadas: $avalanche_count"
        fi
        
        if [ -f "$latest_sim/flow_data.csv" ]; then
            flow_records=$(wc -l < "$latest_sim/flow_data.csv" 2>/dev/null || echo "0")
            echo "Registros de flujo: $flow_records"
        fi
    fi
fi

echo ""
echo "Para análisis más detallado:"
echo "   python3 ../analysis/avalanche_distribution.py --input $latest_sim"

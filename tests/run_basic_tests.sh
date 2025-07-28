#!/bin/bash
# Tests básicos del simulador

echo "EJECUTANDO TESTS BÁSICOS"
echo "============================"

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0

# Función para reportar resultados
report_test() {
    local test_name="$1"
    local result="$2"

    if [ "$result" -eq 0 ]; then
        echo -e "  PASS: $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "  ❌ ${RED}FAIL${NC}: $test_name"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 1: Verificar compilación
echo "📋 Test 1: Verificar compilación del simulador"
if [ -f "../bin/silo_simulator" ]; then
    report_test "Ejecutable existe" 0
else
    echo "   ⚙️ Intentando compilar..."
    cd .. && make clean && make >/dev/null 2>&1
    cd tests
    if [ -f "../bin/silo_simulator" ]; then
        report_test "Compilación exitosa" 0
    else
        report_test "Compilación" 1
    fi
fi

# Test 2: Verificar ayuda del simulador
echo ""
echo "📋 Test 2: Verificar argumentos del simulador"
../bin/silo_simulator --help >/dev/null 2>&1
if [ $? -eq 0 ]; then
    report_test "Comando --help" 0
else
    report_test "Comando --help" 1
fi

# Test 3: Simulación básica muy corta
echo ""
echo "📋 Test 3: Simulación básica (test mode)"
echo "   🔄 Ejecutando simulación de 5 segundos..."

# Limpiar simulaciones previas
rm -rf ../data/simulations/sim_* 2>/dev/null

# Ejecutar simulación corta
timeout 30 ../bin/silo_simulator \
    --base-radius 0.065 \
    --outlet-width 0.26 \
    --particles 100 \
    --time 5 \
    --polygon 0 \
    --save-flow 1 \
    --save-avalanches 1 >/dev/null 2>&1

if [ $? -eq 0 ]; then
    report_test "Simulación básica" 0
else
    report_test "Simulación básica" 1
fi

# Test 4: Verificar archivos de salida
echo ""
echo "📋 Test 4: Verificar generación de archivos"
latest_sim=$(ls -dt ../data/simulations/sim_* 2>/dev/null | head -1)

if [ -n "$latest_sim" ]; then
    report_test "Directorio de simulación creado" 0

    if [ -f "$latest_sim/flow_data.csv" ]; then
        report_test "Archivo flow_data.csv" 0
    else
        report_test "Archivo flow_data.csv" 1
    fi

    if [ -f "$latest_sim/simulation_data.csv" ]; then
        report_test "Archivo simulation_data.csv" 0
    else
        report_test "Archivo simulation_data.csv" 1
    fi
else
    report_test "Directorio de simulación creado" 1
    report_test "Archivos de salida" 1
fi

# Test 5: Verificar scripts Python
echo ""
echo "📋 Test 5: Verificar scripts Python"

python3 -c "import sys, pandas, numpy" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    report_test "Dependencias Python disponibles" 0
else
    report_test "Dependencias Python disponibles" 1
fi

if [ -f "../scripts/run_shape_study.py" ]; then
    python3 ../scripts/run_shape_study.py --help >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        report_test "Script run_shape_study.py" 0
    else
        report_test "Script run_shape_study.py" 1
    fi
else
    report_test "Script run_shape_study.py" 1
fi

# Test 6: Verificar análisis básico
echo ""
echo "📋 Test 6: Verificar análisis de datos"
if [ -n "$latest_sim" ] && [ -f "$latest_sim/flow_data.csv" ]; then
    python3 -c "
import pandas as pd
import sys
try:
    df = pd.read_csv('$latest_sim/flow_data.csv')
    if len(df) > 0 and 'time' in df.columns:
        sys.exit(0)
    else:
        sys.exit(1)
except:
    sys.exit(1)
" 2>/dev/null

    if [ $? -eq 0 ]; then
        report_test "Lectura de datos CSV" 0
    else
        report_test "Lectura de datos CSV" 1
    fi
else
    report_test "Lectura de datos CSV" 1
fi

# Resumen final
echo ""
echo "RESUMEN DE TESTS"
echo "==================="
echo -e "Tests pasados: ${GREEN}$TESTS_PASSED${NC}"
echo -e "❌ Tests fallidos: ${RED}$TESTS_FAILED${NC}"
echo -e "Total tests: $((TESTS_PASSED + TESTS_FAILED))"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}TODOS LOS TESTS PASARON${NC}"
    exit 0
else
    echo -e "\n⚠️ ${YELLOW}ALGUNOS TESTS FALLARON${NC}"
    echo ""
    echo "Posibles soluciones:"
    echo "   - Ejecutar: make clean && make"
    echo "   - Instalar: pip3 install pandas numpy matplotlib"
    echo "   - Verificar permisos de archivos"
    exit 1
fi

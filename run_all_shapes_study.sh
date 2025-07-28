#!/bin/bash

# Script para ejecutar estudios de 500 avalanchas para todas las formas
# Guarda datos completos para renderizado

echo "🚀 === ESTUDIO DE TODAS LAS FORMAS GEOMÉTRICAS ==="
echo "🎯 Objetivo: 500 avalanchas por forma"
echo "⏱️ Tiempo por simulación: 150 segundos"
echo "🔧 Base radius: 0.065 m"
echo "🕳️ Outlet width: 0.26 m (D=4.0)"
echo "🔵 Partículas: 2000"
echo ""

# Lista de formas a simular
SHAPES=("circles" "triangles" "squares" "pentagons" "hexagons")

# Verificar que existe el ejecutable
if [ ! -f "bin/silo_simulator" ]; then
    echo "❌ ERROR: No se encontró bin/silo_simulator"
    echo "   Ejecuta 'make' primero para compilar el simulador"
    exit 1
fi

# Crear directorio para logs generales
mkdir -p all_shapes_study_logs

# Timestamp del inicio
START_TIME=$(date '+%Y-%m-%d %H:%M:%S')
echo "⏰ Iniciado: $START_TIME" | tee all_shapes_study_logs/master_log.txt

# Contador de formas completadas
COMPLETED=0
FAILED=0

# Ejecutar cada forma
for shape in "${SHAPES[@]}"; do
    echo ""
    echo "🔄 ==============================================="
    echo "🚀 INICIANDO: $shape"
    echo "🔄 ==============================================="
    
    # Ejecutar estudio para esta forma
    if python3 run_shape_study.py "$shape" --target 500 --time 150 --particles 2000; then
        echo "✅ $shape COMPLETADO" | tee -a all_shapes_study_logs/master_log.txt
        COMPLETED=$((COMPLETED + 1))
    else
        echo "❌ $shape FALLÓ" | tee -a all_shapes_study_logs/master_log.txt
        FAILED=$((FAILED + 1))
    fi
    
    # Pequeña pausa entre formas
    sleep 2
done

# Resumen final
END_TIME=$(date '+%Y-%m-%d %H:%M:%S')
echo ""
echo "🏁 ==============================================="
echo "🏁 ESTUDIO COMPLETO DE TODAS LAS FORMAS"
echo "🏁 ==============================================="
echo "⏰ Iniciado: $START_TIME"
echo "⏰ Finalizado: $END_TIME"
echo "✅ Formas completadas: $COMPLETED"
echo "❌ Formas fallidas: $FAILED"
echo "📊 Total formas: ${#SHAPES[@]}"

# Log final
{
    echo ""
    echo "=== RESUMEN FINAL ==="
    echo "Finalizado: $END_TIME"
    echo "Completadas: $COMPLETED"
    echo "Fallidas: $FAILED"
    echo "Total: ${#SHAPES[@]}"
    echo ""
    echo "=== DIRECTORIOS GENERADOS ==="
    for shape in "${SHAPES[@]}"; do
        if [ -d "shape_study_results_$shape" ]; then
            echo "📁 shape_study_results_$shape/"
            echo "   📊 Datos consolidados"
            echo "   🎬 Archivos para renderizado"
        fi
    done
} | tee -a all_shapes_study_logs/master_log.txt

echo ""
echo "📁 Logs maestros en: all_shapes_study_logs/"
echo "📁 Resultados por forma en: shape_study_results_[forma]/"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎉 ¡TODOS LOS ESTUDIOS COMPLETADOS EXITOSAMENTE!"
    exit 0
else
    echo "⚠️ Algunos estudios fallaron. Revisa los logs para detalles."
    exit 1
fi

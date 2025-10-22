#!/bin/bash

# ==============================================================================
# Script REVISADO con diagnóstico: Generar datos correctos de avalanchas
# ==============================================================================

BASE_DIR="../simulations"

echo "Iniciando extracción de tamaños de avalancha con diagnóstico..."

# Iterar sobre todos los directorios en simulations
for DIR_NAME in "$BASE_DIR"/*/; do
    FULL_PATH="$DIR_NAME/flow_data.csv"
    DIR_BASENAME=$(basename "$DIR_NAME")

    if [ -f "$FULL_PATH" ]; then
        echo "Procesando: $DIR_BASENAME"

        # Extraer parámetros del nombre de la carpeta
        OUTLET_WIDTH=$(echo "$DIR_BASENAME" | grep -oP 'outlet\K[0-9]+\.[0-9]+')
        POLY=$(echo "$DIR_BASENAME" | grep -oP 'poly\K[0-9]+')
        SIDES=$(echo "$DIR_BASENAME" | grep -oP 'sides\K[0-9]+')
        
        # Extraer el valor de chi del nombre del directorio
        CHI=$(echo "$DIR_BASENAME" | grep -oP 'chi\K[0-9]+\.[0-9]+') 
        CHI_FORMATTED=$(echo "$CHI" | tr '.' '_')

        # Determinar el tipo de partícula
        if [ "$POLY" -eq 0 ]; then
            PARTICLE_TYPE="discs"
        else
            PARTICLE_TYPE="poly${SIDES}"
        fi

        # Archivos de salida
        HISTOGRAM_FILE="avalanche_sizes_${PARTICLE_TYPE}_chi${CHI_FORMATTED}.csv"
        DEBUG_FILE="debug_${PARTICLE_TYPE}_chi${CHI_FORMATTED}.txt"

        # Crear archivos con encabezados
        echo "Avalanche_Size,Outlet_Width" > "$HISTOGRAM_FILE"
        echo "Time,NoPTotal,NoPOriginalTotal,Event" > "$DEBUG_FILE"

        # Procesar con AWK REVISADO
        awk -F',' '
        BEGIN {
            jam_threshold = 5.0
            first_line = 1
            in_avalanche = 0
            avalanche_start_original = 0
            avalanche_start_time = 0
            last_time = 0
            last_nop_total = 0
            last_nop_original = 0
            stable_duration = 0
            avalanche_count = 0
            outlet_width = "'$OUTLET_WIDTH'"
            debug_file = "'$DEBUG_FILE'"
        }
        NR==1 {next}  # saltar encabezado
        
        {
            current_time = $1
            current_nop_total = $4
            current_nop_original = $8

            if (first_line == 1) {
                # Inicializar con primeros valores
                last_time = current_time
                last_nop_total = current_nop_total
                last_nop_original = current_nop_original
                first_line = 0
                print current_time "," current_nop_total "," current_nop_original ",INIT" >> debug_file
                next
            }

            # Calcular tiempo desde última medición
            delta_time = current_time - last_time

            # DETECTAR CAMBIOS - LÓGICA CORREGIDA
            nop_total_changed = (current_nop_total != last_nop_total)
            nop_original_changed = (current_nop_original != last_nop_original)

            if (nop_total_changed) {
                # HAY FLUJO - cambio en NoPTotal
                if (in_avalanche == 0) {
                    # INICIO DE NUEVA AVALANCHA
                    in_avalanche = 1
                    avalanche_start_original = last_nop_original  # Valor ANTES del cambio
                    avalanche_start_time = current_time
                    stable_duration = 0
                    avalanche_count++
                    print current_time "," current_nop_total "," current_nop_original ",AVALANCHE_START (id: " avalanche_count " start_original: " avalanche_start_original ")" >> debug_file
                } else {
                    # CONTINÚA AVALANCHA ACTUAL
                    stable_duration = 0
                    print current_time "," current_nop_total "," current_nop_original ",AVALANCHE_CONTINUE" >> debug_file
                }
            } else {
                # NO HAY CAMBIO en NoPTotal
                stable_duration += delta_time
                
                if (in_avalanche == 1 && stable_duration >= jam_threshold) {
                    # FIN DE AVALANCHA - 5+ segundos sin cambios
                    in_avalanche = 0
                    avalanche_end_original = last_nop_original  # Valor al final
                    avalanche_size = avalanche_end_original - avalanche_start_original
                    
                    # Validar y guardar tamaño
                    if (avalanche_size >= 0) {
                        print avalanche_size "," outlet_width
                        print current_time "," current_nop_total "," current_nop_original ",AVALANCHE_END (size: " avalanche_size " start: " avalanche_start_original " end: " avalanche_end_original ")" >> debug_file
                    } else {
                        print current_time "," current_nop_total "," current_nop_original ",AVALANCHE_END_INVALID (negative size: " avalanche_size ")" >> debug_file
                    }
                } else if (in_avalanche == 0) {
                    # EN ATASCO
                    print current_time "," current_nop_total "," current_nop_original ",JAM (stable: " stable_duration "s)" >> debug_file
                } else {
                    # EN AVALANCHA pero aún no alcanza umbral de atasco
                    print current_time "," current_nop_total "," current_nop_original ",AVALANCHE_PAUSE (stable: " stable_duration "s)" >> debug_file
                }
            }

            # Actualizar valores anteriores
            last_time = current_time
            last_nop_total = current_nop_total
            last_nop_original = current_nop_original
        }
        END {
            # Procesar última avalancha si está activa
            if (in_avalanche == 1) {
                avalanche_end_original = last_nop_original
                avalanche_size = avalanche_end_original - avalanche_start_original
                if (avalanche_size >= 0) {
                    print avalanche_size "," outlet_width
                    print "END,,,AVALANCHE_END_LAST (size: " avalanche_size ")" >> debug_file
                }
            }
            print "END,,,PROCESSING_COMPLETE (total_avalanches: " avalanche_count ")" >> debug_file
        }' "$FULL_PATH" >> "$HISTOGRAM_FILE"

        # Mostrar diagnóstico rápido
        echo "  Diagnóstico para $DIR_BASENAME:"
        echo "  - Total de avalanchas detectadas: $(tail -1 "$DEBUG_FILE" | grep -o 'total_avalanches: [0-9]*' | cut -d' ' -f2)"
        echo "  - Tamaños únicos encontrados: $(awk -F',' 'NR>1 {print $1}' "$HISTOGRAM_FILE" | sort -nu | head -5 | tr '\n' ' ')"
        echo "  - Archivo de debug: $DEBUG_FILE"
        
    fi
done

echo "✅ Proceso completado."
echo ""
echo "📊 Para verificar la corrección:"
echo "   - Revisa los archivos debug_*.txt para ver la secuencia de eventos"
echo "   - Los tamaños en avalanche_sizes_*.csv deberían ser pequeños (0,1,2,3...)"
echo "   - Si los tamaños siguen siendo acumulativos, revisa el debug para identificar el problema"
#!/bin/bash

# ==============================================================================
# Script: Calcular tamaño medio de avalancha y número de avalanchas
# ==============================================================================

BASE_DIR="../simulations"

echo "Iniciando cálculo del tamaño medio de avalancha..."

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
        # Formatear CHI para usarlo en el nombre de archivo
        CHI_FORMATTED=$(echo "$CHI" | tr '.' '_')

        # Decidir archivo temporal según tipo de partícula y CHI
        if [ "$POLY" -eq 0 ]; then
            TMP_FILE="tmp_discs_chi${CHI_FORMATTED}.csv"
        else
            TMP_FILE="tmp_poly${SIDES}_chi${CHI_FORMATTED}.csv"
        fi

        # Crear archivo temporal si no existe (con encabezado)
        if [ ! -f "$TMP_FILE" ]; then
            echo "Outlet_Width,Chi,Average_Avalanche_Size,Total_Avalanches,Total_Particles_Exited" > "$TMP_FILE"
        fi

        # Procesar flow_data.csv con AWK para detectar avalanchas - VERSIÓN CORREGIDA
        AWK_RESULTS=$(awk -F',' '
        BEGIN {
            jam_threshold = 5.0  # segundos sin cambio para considerar atasco
            first_line = 1
            in_jam = 1  # empezamos asumiendo que hay atasco (antes del primer flujo)
            last_avalanche_start = 0
            current_avalanche_particles = 0
            total_avalanches = 0
            total_particles = 0
        }
        NR==1 {next}  # saltar encabezado
        
        {
            time = $1
            nop_total = $4        # columna NoPTotal (CORRECTO)
            nop_original = $8     # columna NoPOriginalTotal (CORRECTO)
            
            if (first_line == 1) {
                prev_time = time
                prev_nop_total = nop_total
                prev_nop_original = nop_original
                first_line = 0
                next
            }

            # Calcular tiempo transcurrido desde la última medición
            delta_time = time - prev_time

            # Detectar si hay cambio en el número total de partículas
            if (nop_total != prev_nop_total) {
                # HAY FLUJO - no estamos en atasco
                if (in_jam == 1) {
                    # SALIMOS DE UN ATASCO - inicio de nueva avalancha
                    in_jam = 0
                    last_avalanche_start = nop_original
                    stable_duration = 0
                }
                # Actualizar contadores
                prev_nop_total = nop_total
                stable_duration = 0
            } else {
                # NO HAY CAMBIO - acumular tiempo estable
                stable_duration += delta_time
                
                # Verificar si alcanzamos el umbral de atasco
                if (stable_duration >= jam_threshold && in_jam == 0) {
                    # ENTRAMOS EN ATASCO - fin de avalancha actual
                    in_jam = 1
                    avalanche_size = nop_original - last_avalanche_start
                    
                    if (avalanche_size > 0) {
                        total_avalanches++
                        total_particles += avalanche_size
                    }
                }
            }

            prev_time = time
            prev_nop_original = nop_original
        }
        END {
            # Contar la última avalancha si terminamos en flujo (no en atasco)
            if (in_jam == 0 && nop_original > last_avalanche_start) {
                avalanche_size = nop_original - last_avalanche_start
                if (avalanche_size > 0) {
                    total_avalanches++
                    total_particles += avalanche_size
                }
            }
            
            if (total_avalanches > 0) {
                avg_size = total_particles / total_avalanches
            } else {
                avg_size = 0
            }
            printf "%.5f,%d", total_particles, total_avalanches
        }' "$FULL_PATH")

        TOTAL_PARTICLES_EXITED=$(echo "$AWK_RESULTS" | cut -d',' -f1)
        TOTAL_AVALANCHES=$(echo "$AWK_RESULTS" | cut -d',' -f2)

        if [ "$TOTAL_AVALANCHES" -gt 0 ]; then
            AVERAGE_SIZE=$(echo "scale=5; $TOTAL_PARTICLES_EXITED / $TOTAL_AVALANCHES" | bc -l)
        else
            AVERAGE_SIZE=0.0
        fi

        # Guardar resultados crudos en el archivo temporal
        echo "$OUTLET_WIDTH,$CHI,$AVERAGE_SIZE,$TOTAL_AVALANCHES,$TOTAL_PARTICLES_EXITED" >> "$TMP_FILE"
    fi
done

# El resto del script (agrupamiento) permanece igual...
echo "Agrupando resultados por orificio (Outlet_Width) y Chi..."

# Procesar cada archivo temporal y generar resumen final
for TMP_FILE in tmp_*.csv; do
    if [ -f "$TMP_FILE" ]; then
        # Extraer el sufijo del archivo (ej: discs_chi0_10.csv, poly6_chi0_10.csv)
        FILE_SUFFIX="${TMP_FILE#tmp_}"
        
        OUTPUT_FILE="summary_${FILE_SUFFIX}"
        COUNTS_FILE="counts_${FILE_SUFFIX}"

        # Crear archivos de salida con encabezados
        echo "Outlet_Width,Average_Avalanche_Size,Total_Avalanches,Total_Particles_Exited" > "$OUTPUT_FILE"
        echo "Outlet_Width,Total_Avalanches" > "$COUNTS_FILE"

        awk -F',' 'NR>1 {
            outlet=$1  # Outlet_Width
            avg_size[outlet]+=$3*$4   # acumulamos tamaño promedio * cantidad avalanchas
            total_avalanches[outlet]+=$4
            total_particles[outlet]+=$5
        } END {
            for (o in avg_size) {
                if (total_avalanches[o] > 0) {
                    weighted_avg = avg_size[o] / total_avalanches[o]
                } else {
                    weighted_avg = 0
                }
                printf "%s,%.5f,%d,%.0f\n", o, weighted_avg, total_avalanches[o], total_particles[o] >> "'$OUTPUT_FILE'"
                printf "%s,%d\n", o, total_avalanches[o] >> "'$COUNTS_FILE'"
            }
        }' "$TMP_FILE"

        # Ordenar ambos archivos por Outlet_Width
        { head -n1 "$OUTPUT_FILE"; tail -n +2 "$OUTPUT_FILE" | sort -t',' -k1,1n; } > tmp && mv tmp "$OUTPUT_FILE"
        { head -n1 "$COUNTS_FILE"; tail -n +2 "$COUNTS_FILE" | sort -t',' -k1,1n; } > tmp && mv tmp "$COUNTS_FILE"

        rm "$TMP_FILE"
    fi
done

echo "✅ Proceso completado. Archivos generados:"
ls summary_*.csv counts_*.csv 2>/dev/null || echo "No se generaron archivos de resultados"
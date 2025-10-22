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
                # Procesar flow_data.csv con AWK para detectar avalanchas - VERSIÓN MEJORADA
        AWK_RESULTS=$(awk -F',' '
        BEGIN {
            jam_threshold = 5.0
            first_line = 1
            in_avalanche = 0
            avalanche_start_original = 0
            last_activity_time = 0
            total_avalanches = 0
            total_particles = 0
        }
        NR==1 {next}  # saltar encabezado

        {
            time = $1
            nop_total = $4
            nop_original = $8
            
            if (first_line == 1) {
                # Inicializar con primera línea de datos
                prev_nop_total = nop_total
                prev_nop_original = nop_original
                last_activity_time = time
                first_line = 0
                next
            }

            # Detectar actividad (cambio en NoPTotal)
            has_activity = (nop_total != prev_nop_total)
            
            if (has_activity) {
                last_activity_time = time
                
                # Si no hay avalancha en curso, iniciar una nueva
                if (!in_avalanche) {
                    in_avalanche = 1
                    avalanche_start_original = prev_nop_original
                }
            }

            # Verificar condición de fin de avalancha
            time_since_activity = time - last_activity_time
            
            if (in_avalanche && time_since_activity >= jam_threshold) {
                # Fin de avalancha
                avalanche_size = nop_original - avalanche_start_original
                
                if (avalanche_size >= 0) {  # Permitir tamaño 0
                    total_avalanches++
                    total_particles += avalanche_size
                }
                
                in_avalanche = 0
            }

            # Actualizar valores anteriores
            prev_nop_total = nop_total
            prev_nop_original = nop_original
        }
        END {
            # Procesar última avalancha si aún está activa
            if (in_avalanche) {
                avalanche_size = nop_original - avalanche_start_original
                if (avalanche_size >= 0) {
                    total_avalanches++
                    total_particles += avalanche_size
                }
            }
            
            # Imprimir resultados
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
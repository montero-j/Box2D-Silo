#!/bin/bash

# ==============================================================================
# Script: Calcular tamaño medio de avalancha y número de avalanchas
#
# Descripción:
#   - Busca en ./simulations todos los directorios de simulaciones.
#   - Cada simulación tiene un archivo avalanche_data.csv con datos de avalanchas.
#   - Se extraen:
#       * Outlet_Width (del nombre del directorio)
#       * Tipo de partícula:
#           - poly0  -> Discos  -> summary_discs.csv + counts_discs.csv
#           - poly>0 -> Polígonos con N lados -> summary_polyN.csv + counts_polyN.csv
#   - Para cada simulación:
#       * Se suman las partículas descargadas y el número de avalanchas.
#       * Se calcula el tamaño promedio de avalancha (si hubo avalanchas).
#   - Se guardan resultados temporales por tipo de partícula.
#   - Al final, se agrupan por Outlet_Width usando promedio ponderado:
#       Average_Avalanche_Size = (Σ tamaño_medio * #avalanchas) / (Σ #avalanchas)
#   - Se generan archivos finales:
#       summary_discs.csv, summary_polyN.csv
#       counts_discs.csv,  counts_polyN.csv
#   - Los resultados quedan ordenados por Outlet_Width ascendente.
# ==============================================================================

BASE_DIR="./simulations"

echo "Iniciando cálculo del tamaño medio de avalancha..."

# Iterar sobre todos los directorios en simulations
for DIR_NAME in "$BASE_DIR"/*/; do
    FULL_PATH="$DIR_NAME/avalanche_data.csv"
    DIR_BASENAME=$(basename "$DIR_NAME")

    if [ -f "$FULL_PATH" ]; then
        echo "Procesando: $DIR_BASENAME"

        # Extraer parámetros del nombre de la carpeta
        OUTLET_WIDTH=$(echo "$DIR_BASENAME" | grep -oP 'outlet\K[0-9]+\.[0-9]+')
        POLY=$(echo "$DIR_BASENAME" | grep -oP 'poly\K[0-9]+')
        SIDES=$(echo "$DIR_BASENAME" | grep -oP 'sides\K[0-9]+')

        # Decidir archivo temporal según tipo de partícula
        if [ "$POLY" -eq 0 ]; then
            TMP_FILE="tmp_discs.csv"
        else
            TMP_FILE="tmp_poly${SIDES}.csv"
        fi

        # Crear archivo temporal si no existe (con encabezado)
        if [ ! -f "$TMP_FILE" ]; then
            echo "Outlet_Width,Average_Avalanche_Size,Total_Avalanches,Total_Particles_Exited" > "$TMP_FILE"
        fi

        # Leer datos CSV, ignorando encabezados/comentarios
        CSV_DATA=$(grep -v '^#\|^=====\|^Time' "$FULL_PATH")

        if [ -z "$CSV_DATA" ]; then
            TOTAL_PARTICLES_EXITED=0
            TOTAL_AVALANCHES=0
            AVERAGE_SIZE=0.0
        else
            AWK_RESULTS=$(echo "$CSV_DATA" | awk -F',' '{
                sum_particles += $5;
                if ($5 > 0) {
                    count_avalanches++;
                }
            } END {
                printf "%.5f,%d", sum_particles, count_avalanches
            }')

            TOTAL_PARTICLES_EXITED=$(echo "$AWK_RESULTS" | cut -d',' -f1)
            TOTAL_AVALANCHES=$(echo "$AWK_RESULTS" | cut -d',' -f2)

            if [ "$TOTAL_AVALANCHES" -gt 0 ]; then
                AVERAGE_SIZE=$(echo "scale=5; $TOTAL_PARTICLES_EXITED / $TOTAL_AVALANCHES" | bc -l)
            else
                AVERAGE_SIZE=0.0
            fi
        fi

        # Guardar resultados crudos en el archivo temporal
        echo "$OUTLET_WIDTH,$AVERAGE_SIZE,$TOTAL_AVALANCHES,$TOTAL_PARTICLES_EXITED" >> "$TMP_FILE"
    fi
done

echo "Agrupando resultados por orificio (Outlet_Width)..."

# Procesar cada archivo temporal y generar resumen final
for TMP_FILE in tmp_*.csv; do
    if [ -f "$TMP_FILE" ]; then
        case "$TMP_FILE" in
            tmp_discs.csv) 
                OUTPUT_FILE="summary_discs.csv"
                COUNTS_FILE="counts_discs.csv"
                ;;
            tmp_poly*.csv) 
                OUTPUT_FILE="summary_${TMP_FILE#tmp_}"
                COUNTS_FILE="counts_${TMP_FILE#tmp_}"
                ;;
        esac

        # Crear archivos de salida con encabezados
        echo "Outlet_Width,Average_Avalanche_Size,Total_Avalanches,Total_Particles_Exited" > "$OUTPUT_FILE"
        echo "Outlet_Width,Total_Avalanches" > "$COUNTS_FILE"

        awk -F',' 'NR>1 {
            outlet=$1
            avg_size[outlet]+=$2*$3   # acumulamos tamaño promedio * cantidad avalanchas
            total_avalanches[outlet]+=$3
            total_particles[outlet]+=$4
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
ls summary_*.csv counts_*.csv

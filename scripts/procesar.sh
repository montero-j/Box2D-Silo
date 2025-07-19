#!/bin/bash

total_particles=0
total_time=0
count=0

for i in $(seq 1 50); do
    folder="../resultados/r_0.4/chi_0.6/sim_$i/simulation_data"
    file="$folder/r_0.400000_chi_0.600000_sim_$i/flow_data.csv"
    if [[ -f "$file" ]]; then
        # Leer última línea (ignorando encabezado)
        last_line=$(tail -n 1 "$file")

        # Separar campos por coma
        IFS=',' read -r time _ _ nop_total _ <<< "$last_line"

        # Validar que ambos valores existan
        if [[ "$time" =~ ^[0-9.]+$ ]] && [[ "$nop_total" =~ ^[0-9]+$ ]]; then
            total_time=$(echo "$total_time + $time" | bc)
            total_particles=$((total_particles + nop_total))
            ((count++))
        else
            echo "⚠️  Formato inválido en $file: $last_line"
        fi
    else
        echo "⚠️  No se encontró el archivo: $file"
    fi

done

if (( count > 0 )); then
    avg_particles=$(echo "scale=2; $total_particles / $count" | bc)
    avg_time=$(echo "scale=2; $total_time / $count" | bc)
    echo "Promedio partículas descargadas: $avg_particles"
    echo "Promedio tiempo de descarga: $avg_time"
else
    echo "❌ No se procesó ningún archivo válido."
fi


#!/bin/bash

# Define el archivo base y el número total de copias
BASE_FILE="param_sim_.txt"
TOTAL_SIMS=50

# Itera desde 1 hasta TOTAL_SIMS
for i in $(seq 1 $TOTAL_SIMS); do
  # Define el nombre del nuevo archivo
  NEW_FILE="param_sim_${i}.txt"

  # Copia el archivo base al nuevo nombre
  cp "$BASE_FILE" "$NEW_FILE"

  # Reemplaza CURRENT_SIM con el valor actual del bucle
  sed -i "s/CURRENT_SIM=.*/CURRENT_SIM=$i/" "$NEW_FILE"
  
  # Reemplaza TOTAL_SIMS con el número total de simulaciones
  sed -i "s/TOTAL_SIMS=.*/TOTAL_SIMS=$TOTAL_SIMS/" "$NEW_FILE"

  # Condición para SAVE_SIM_DATA
  if [ "$i" -eq 1 ]; then
    # Para el primer archivo, SAVE_SIM_DATA=1
    sed -i "s/SAVE_SIM_DATA=.*/SAVE_SIM_DATA=1/" "$NEW_FILE"
  else
    # Para los demás, SAVE_SIM_DATA=0
    sed -i "s/SAVE_SIM_DATA=.*/SAVE_SIM_DATA=0/" "$NEW_FILE"
  fi
  
  echo "Archivo creado y modificado: $NEW_FILE"
done

echo "¡Proceso completado!"

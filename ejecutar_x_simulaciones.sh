#!/bin/bash

# Comprobar si se ha proporcionado el parámetro chi
if [ -z "$1" ]; then
  echo "Uso: $0 <valor_chi>"
  echo "Ejemplo: $0 0.2"
  exit 1
fi

# Asignar el valor del primer argumento a la variable chi_value
chi_value=$1

# Número de simulaciones a ejecutar
total_sims=50

# Bucle para ejecutar las simulaciones
for ((i=1; i<=total_sims; i++))
do
    echo "Ejecutando simulación $i de $total_sims con chi_$chi_value..."
    ./ejecutar_simulacion.sh "parametros_sims/chi_$chi_value/param_sim_$i.txt"
    echo "Simulación $i completada."
    echo "----------------------------------"
done

echo "Todas las $total_sims simulaciones han sido completadas."
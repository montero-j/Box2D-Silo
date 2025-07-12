import os
import pandas as pd
import numpy as np

# Ruta base donde están las carpetas sim1/, sim2/, ..., sim100/
base_path = "./"

# Número de simulaciones
n_simulations = 50

# Lista para almacenar flujos de cada simulación
average_flows = []

for i in range(1, n_simulations + 1):
    file_path = os.path.join(base_path, f"sim{i}", "Measurements.csv")
    if os.path.exists(file_path):
        df = pd.read_csv(file_path)

        # Asegurar que las columnas necesarias estén presentes
        if {'Time', 'NoPTotal'}.issubset(df.columns):
            time = df['Time'].values
            nop_total = df['NoPTotal'].values

            if len(time) > 1:
                # Calcular flujo como (NoP_final - NoP_inicial) / (t_final - t_inicial)
                delta_nop = nop_total[-1] - nop_total[0]
                delta_t = time[-1] - time[0]

                if delta_t > 0:
                    avg_flow = delta_nop / delta_t
                    average_flows.append(avg_flow)
                else:
                    print(f"Tiempo inválido en {file_path}")
            else:
                print(f"Datos insuficientes en {file_path}")
        else:
            print(f"Columnas necesarias no encontradas en {file_path}")
    else:
        print(f"Archivo no encontrado: {file_path}")

# Calcular flujo promedio total
if average_flows:
    overall_average_flow = np.mean(average_flows)
    print(f"Flujo promedio de descarga en todas las simulaciones: {overall_average_flow:.3f} partículas/unidad de tiempo")
else:
    print("No se pudo calcular el flujo promedio. Verifica los archivos y sus columnas.")


import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Parámetros fijos
chi_val = "0.4"
r_values = [f"{i/10:.1f}" for i in range(1, 10)]  # r desde 0.1 hasta 0.9

# Lista para almacenar flujos de cada simulación
average_flows = []

for r in r_values:
    folder_path = os.path.join(f"r_{r}", f"chi_{chi_val}")
    file_path = os.path.join(folder_path, "simulation_data", "flow_data.csv")
    
    if os.path.exists(file_path):
        df = pd.read_csv(file_path)
        
        if {'Time', 'NoPFlowRate'}.issubset(df.columns):
            time = df['Time'].values
            flow_rate = df['NoPFlowRate'].values
            
            if len(time) > 1:
                # Calcular flujo promedio (ya que tenemos tasa de flujo)
                avg_flow = np.mean(flow_rate)
                average_flows.append(avg_flow)
            else:
                print(f"Datos insuficientes en {file_path}")
        else:
            print(f"Columnas necesarias no encontradas en {file_path}")
    else:
        print(f"Archivo no encontrado: {file_path}")

# Crear gráfico para diferentes valores de r
if average_flows:
    fig, ax = plt.subplots()
    ax.plot([float(r) for r in r_values], average_flows, 'o-')
    ax.set_xlabel("Radio (r)")
    ax.set_ylabel("Flujo promedio (partículas/segundo)")
    ax.set_title(f"Flujo de descarga para χ = {chi_val}")
    plt.grid(True)
    plt.tight_layout()
    plt.show()
else:
    print("No se pudieron calcular los flujos. Verifica los archivos.")

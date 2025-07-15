import os
import pandas as pd
import numpy as np
from glob import glob

# Configuración de parámetros
BASE_DIR = "../resultados"
TARGET_R = "0.4"  # Radio objetivo
CHI_VALUES = [f"{i/10:.1f}" for i in range(3, 11)]  # Desde chi_0.3 hasta chi_1.0
N_SIMULATIONS = 50

# Diccionario para almacenar resultados
results = {chi: [] for chi in CHI_VALUES}

for chi in CHI_VALUES:
    for sim in range(1, N_SIMULATIONS + 1):
        # Construir ruta al archivo de flujo
        flow_path = os.path.join(BASE_DIR, f"r_{TARGET_R}", f"chi_{chi}", f"sim_{sim}", f"simulation_data", f"r_0.400000_chi_{chi}00000_sim_{sim}", "flow_data.csv")

        print(f"Procesando: {flow_path}")
        if os.path.exists(flow_path):
            try:
                df = pd.read_csv(flow_path)
                
                if {'Time', 'NoPFlowRate'}.issubset(df.columns):
                    # Calcular flujo promedio ponderado por tiempo
                    valid_data = df[df['NoPFlowRate'] > 0]
                    if len(valid_data) > 1:
                        time_intervals = np.diff(valid_data['Time'])
                        avg_flow = np.average(valid_data['NoPFlowRate'][1:], weights=time_intervals)
                        results[chi].append(avg_flow)
            except Exception as e:
                print(f"Error procesando {flow_path}: {str(e)}")

# Calcular estadísticas por valor de chi
stats = pd.DataFrame({
    'Chi': CHI_VALUES,
    'Mean_Flow': [np.mean(results[chi]) for chi in CHI_VALUES],
    'Std_Flow': [np.std(results[chi]) for chi in CHI_VALUES],
    'Min_Flow': [np.min(results[chi]) for chi in CHI_VALUES],
    'Max_Flow': [np.max(results[chi]) for chi in CHI_VALUES],
    'N_Sims': [len(results[chi]) for chi in CHI_VALUES]
})

print("\nEstadísticas de flujo por valor de chi:")
print(stats.to_string(index=False))

# Guardar resultados consolidados
stats.to_csv(os.path.join(BASE_DIR, f"flow_stats_r_{TARGET_R}.csv"), index=False)
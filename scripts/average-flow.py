import os
import pandas as pd
import numpy as np
from glob import glob

# Configuración de parámetros
BASE_DIR = "../resultados"
TARGET_R = "0.4"  # Radio objetivo
CHI_VALUES = [f"{i/10:.1f}" for i in range(1, 11)]  # Desde chi_0.3 hasta chi_1.0
PURE_CASE = True  # Flag para incluir el caso puro
N_SIMULATIONS = 50
TOTAL_P = 250

# Diccionario para almacenar resultados
results = {chi: [] for chi in CHI_VALUES}
if PURE_CASE:
    results['1.0_pure'] = []  # Caso puro chi=1.0, r=0.0

def process_flow_file(path):
    """Función mejorada para procesar archivo flow_data.csv"""
    try:
        # Verificar si el archivo existe
        if not os.path.exists(path):
            print(f"Archivo no encontrado: {path}")
            return None
            
        df = pd.read_csv(path)
        
        # Verificar columnas requeridas
        required_cols = {'Time', 'NoPTotal'}
        if not required_cols.issubset(df.columns):
            print(f"Columnas requeridas no encontradas en {path}")
            return None
            
        # Calcular usando tiempo total y partículas descargadas
        if len(df) < 2:
            print(f"Datos insuficientes en {path}")
            return None
            
        initial_particles = df['NoPTotal'].iloc[0]
        final_particles = df['NoPTotal'].iloc[-1]
        total_time = df['Time'].iloc[-1] - df['Time'].iloc[0]
        
        if total_time <= 0:
            print(f"Tiempo total inválido en {path}")
            return None
            
        total_discharged = final_particles - initial_particles
        avg_flow = total_discharged / total_time
        
        print(f"Procesado: {path} - Partículas: {total_discharged}, Tiempo: {total_time:.2f}, Flujo: {avg_flow:.2f}")
        return avg_flow
        
    except Exception as e:
        print(f"Error procesando {path}: {str(e)}")
        return None

# Procesar casos normales (r=0.4)
for chi in CHI_VALUES:
    for sim in range(1, N_SIMULATIONS + 1):
        flow_path = os.path.join(
            BASE_DIR, 
            f"r_{TARGET_R}", 
            f"chi_{chi}", 
            f"total_p_{TOTAL_P}",
            f"sim_{sim}", 
            "flow_data.csv"
        )
        print(f"\nIntentando abrir: {flow_path}")
        
        flow_value = process_flow_file(flow_path)
        if flow_value is not None:
            results[chi].append(flow_value)
        else:
            print(f"Fallo al procesar simulación {sim} para chi={chi}")

# Procesar caso puro (r=0.0, chi=1.0)
if PURE_CASE:
    for sim in range(1, N_SIMULATIONS + 1):
        pure_flow_path = os.path.join(
            BASE_DIR,
            "r_0.0",
            "chi_0.0",
            f"total_p_{TOTAL_P}",
            f"sim_{sim}", 
            "flow_data.csv"
        )
        print(f"\nIntentando abrir (puro): {pure_flow_path}")
        
        flow_value = process_flow_file(pure_flow_path)
        if flow_value is not None:
            results['1.0_pure'].append(flow_value)
        else:
            print(f"Fallo al procesar simulación pura {sim}")

# Calcular estadísticas solo para casos con datos
stats_data = []
for chi, values in results.items():
    if len(values) > 0:
        stats_data.append({
            'Chi': chi,
            'R': '0.0' if chi == '1.0_pure' else TARGET_R,
            'Mean_Flow': np.mean(values),
            'Std_Flow': np.std(values),
            'Min_Flow': np.min(values),
            'Max_Flow': np.max(values),
            'N_Sims': len(values),
            'Case_Type': 'pure' if chi == '1.0_pure' else 'normal'
        })

if stats_data:
    stats = pd.DataFrame(stats_data)
    print("\nEstadísticas de flujo por valor de chi:")
    print(stats.to_string(index=False))
    
    # Guardar resultados consolidados
    output_file = os.path.join(BASE_DIR, f"flow_stats_comparison_r_{TARGET_R}.csv")
    stats.to_csv(output_file, index=False)
    print(f"\nResultados guardados en: {output_file}")
else:
    print("\nNo se encontraron datos válidos para generar estadísticas")
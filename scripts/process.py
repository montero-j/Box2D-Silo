#!/usr/bin/env python3
import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# Configuración de estilo para gráficas
rcParams['font.family'] = 'serif'
rcParams['axes.grid'] = True

def tiempo_atasco(array):
    """Calcula duraciones de eventos de atasco (flujo binario = 0)."""
    atasco = []
    atascado = True
    tiempo_inicio = 0.0
    for t, flujo in array:
        if flujo < 0.5:  # Detecta atasco
            if not atascado:
                atascado = True
                tiempo_inicio = t
        else:  # Detecta flujo
            if atascado:
                atascado = False
                atasco.append(t - tiempo_inicio)
    return np.array(atasco)

def tiempo_flujo(array):
    """Calcula duraciones de eventos de flujo (flujo binario = 1)."""
    flujo = []
    fluyendo = False
    tiempo_inicio = 0.0
    for t, flujo_val in array:
        if flujo_val > 0.5:  # Detecta flujo
            if not fluyendo:
                fluyendo = True
                tiempo_inicio = t
        else:  # Detecta atasco
            if fluyendo:
                fluyendo = False
                flujo.append(t - tiempo_inicio)
    return np.array(flujo)

def leer_datos_simulacion(path):
    """Lee datos de un archivo flow_data.csv específico."""
    try:
        datos = np.genfromtxt(path, delimiter=',', skip_header=1)
        tiempo = datos[:, 0]
        flujo = datos[:, 4]  # NoPFlowRate
        flujo_binario = (flujo > 0.1).astype(float)  # Umbral ajustable
        
        # Tiempo relativo y corrección de fin de simulación
        tiempo_relativo = tiempo - tiempo[0]
        ventana_final = 30.0
        if len(tiempo_relativo) > 1 and tiempo_relativo[-1] - tiempo_relativo[0] >= ventana_final:
            idx_ventana = np.where(tiempo_relativo >= tiempo_relativo[-1] - ventana_final)[0][0]
            if np.all(flujo_binario[idx_ventana:] == 0):
                flujo_binario[-1] = 1.0
        
        return np.column_stack((tiempo_relativo, flujo_binario))
    except Exception as e:
        print(f"Error leyendo {path}: {str(e)}")
        return None

def procesar_simulacion(path_simulacion, r_value, chi_value):
    """Procesa un archivo flow_data.csv y guarda resultados."""
    datos = leer_datos_simulacion(path_simulacion)
    if datos is None:
        return None
    
    duraciones_atasco = tiempo_atasco(datos)
    duraciones_flujo = tiempo_flujo(datos)
    
    # Guardar resultados
    output_dir = os.path.dirname(path_simulacion)
    np.savetxt(os.path.join(output_dir, f'duraciones_atasco_r_{r_value}_chi_{chi_value}.csv'), 
               duraciones_atasco, delimiter=',', header='Duracion(s)')
    np.savetxt(os.path.join(output_dir, f'duraciones_flujo_r_{r_value}_chi_{chi_value}.csv'), 
               duraciones_flujo, delimiter=',', header='Duracion(s)')
    
    return duraciones_atasco

def graficar_por_combinacion(directorio_raiz, nombre_salida):
    """Genera gráficas separadas por cada combinación r_X.X y chi_Y.Y."""
    # Buscar todas las combinaciones r_X.X/chi_Y.Y
    combinaciones = []
    for r_folder in sorted([d for d in os.listdir(directorio_raiz) if d.startswith('r_')]):
        r_path = os.path.join(directorio_raiz, r_folder)
        for chi_folder in sorted([d for d in os.listdir(r_path) if d.startswith('chi_')]):
            chi_path = os.path.join(r_path, chi_folder, 'simulation_data')
            if os.path.exists(chi_path):
                combinaciones.append((r_folder, chi_folder, chi_path))
    
    # Procesar cada combinación
    for r_folder, chi_folder, chi_path in combinaciones:
        r_value = r_folder.split('_')[1]
        chi_value = chi_folder.split('_')[1]
        csv_path = os.path.join(chi_path, 'flow_data.csv')
        
        print(f"\nProcesando: r = {r_value}, χ = {chi_value}")
        duraciones = procesar_simulacion(csv_path, r_value, chi_value)
        
        if duraciones is not None and len(duraciones) > 0:
            # Gráfica individual para esta combinación
            plt.figure(figsize=(8, 6))
            duraciones_sorted = np.sort(duraciones)[::-1]
            prob_acumulada = np.arange(1, len(duraciones_sorted) + 1) / len(duraciones_sorted)
            
            plt.plot(duraciones_sorted, prob_acumulada, 'o-', markersize=4, label='Datos')
            plt.xscale('log')
            plt.yscale('log')
            plt.xlabel('Duración del atasco (s)')
            plt.ylabel('Probabilidad acumulada')
            plt.title(f'Distribución de atascos (r = {r_value}, χ = {chi_value})')
            plt.legend()
            plt.grid(True, which="both", linestyle='--', alpha=0.4)
            plt.tight_layout()
            
            # Guardar gráfica
            output_file = os.path.join(
                directorio_raiz, 
                f'{nombre_salida}_r_{r_value}_chi_{chi_value}.pdf'
            )
            plt.savefig(output_file, dpi=300)
            plt.close()
            print(f"  → Gráfica guardada en: {output_file}")
        else:
            print(f"  → No se detectaron atascos en esta combinación")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Uso: python process.py <directorio_raiz> <nombre_salida>")
        sys.exit(1)
    
    directorio_raiz = sys.argv[1]
    nombre_salida = sys.argv[2]
    
    graficar_por_combinacion(directorio_raiz, nombre_salida)

"""
Ejemplo: Análisis básico de resultados de simulación

Este script muestra cómo analizar los resultados de una simulación
de manera programática usando pandas.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import sys
import glob

def find_latest_simulation():
    """Encontrar la simulación más reciente"""
    sim_dirs = glob.glob("../data/simulations/sim_*")
    if not sim_dirs:
        print("No se encontraron simulaciones")
        return None

    # Directorio más reciente
    latest = max(sim_dirs, key=lambda x: Path(x).stat().st_mtime)
    return Path(latest)

def analyze_flow_data(sim_dir):
    """Analizar datos de flujo"""
    flow_file = sim_dir / "flow_data.csv"

    if not flow_file.exists():
        print(f"No se encontró {flow_file}")
        return None

    df = pd.read_csv(flow_file)

    print(f"\nANÁLISIS DE FLUJO:")
    print(f"   - Duración total: {df['time'].max():.1f} segundos")
    print(f"   - Partículas salidas: {df['cumulative_particles'].max():.0f}")
    print(f"   - Tasa promedio: {df['particles_per_second'].mean():.2f} part/s")
    print(f"   - Tasa máxima: {df['particles_per_second'].max():.2f} part/s")

    # Detectar períodos de flujo/bloqueo
    flowing = df[df['particles_per_second'] > 0.1]
    blocked = df[df['particles_per_second'] <= 0.1]

    flow_percentage = len(flowing) / len(df) * 100
    print(f"   - Tiempo fluyendo: {flow_percentage:.1f}%")
    print(f"   - Tiempo bloqueado: {100-flow_percentage:.1f}%")

    return df

def analyze_avalanche_data(sim_dir):
    """Analizar datos de avalanchas"""
    avalanche_file = sim_dir / "avalanche_data.csv"

    if not avalanche_file.exists():
        print(f"No se encontró {avalanche_file}")
        return None

    df = pd.read_csv(avalanche_file)

    if len(df) == 0:
        print("No se detectaron avalanchas")
        return None

    print(f"\nANÁLISIS DE AVALANCHAS:")
    print(f"   - Total avalanchas: {len(df)}")
    print(f"   - Tamaño promedio: {df['particles'].mean():.1f} ± {df['particles'].std():.1f}")
    print(f"   - Tamaño mediano: {df['particles'].median():.1f}")
    print(f"   - Rango: {df['particles'].min():.0f} - {df['particles'].max():.0f}")

    # Estadísticas adicionales
    large_avalanches = df[df['particles'] > df['particles'].mean() + df['particles'].std()]
    print(f"   - Avalanchas grandes (>1σ): {len(large_avalanches)} ({len(large_avalanches)/len(df)*100:.1f}%)")

    return df

def create_simple_plot(flow_df, avalanche_df, sim_dir):
    """Crear gráfico simple de resultados"""
    try:
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

        # Gráfico de flujo
        if flow_df is not None:
            ax1.plot(flow_df['time'], flow_df['particles_per_second'], 'b-', alpha=0.7)
            ax1.set_ylabel('Partículas/segundo')
            ax1.set_title(f'Flujo de Partículas - {sim_dir.name}')
            ax1.grid(True, alpha=0.3)

        # Histograma de avalanchas
        if avalanche_df is not None and len(avalanche_df) > 0:
            ax2.hist(avalanche_df['particles'], bins=20, alpha=0.7, edgecolor='black')
            ax2.set_xlabel('Tamaño de Avalancha (partículas)')
            ax2.set_ylabel('Frecuencia')
            ax2.set_title('Distribución de Tamaños de Avalancha')
            ax2.grid(True, alpha=0.3)

        plt.tight_layout()

        # Guardar plot
        plot_file = sim_dir / "analysis_plot.png"
        plt.savefig(plot_file, dpi=150, bbox_inches='tight')
        print(f"\nGráfico guardado: {plot_file}")

        return plot_file

    except ImportError:
        print("matplotlib no disponible - saltando visualización")
        return None
    except Exception as e:
        print(f"Error creando gráfico: {e}")
        return None

def main():
    """Función principal"""
    print("ANÁLISIS BÁSICO DE SIMULACIÓN")
    print("================================")

    # Encontrar simulación
    sim_dir = find_latest_simulation()
    if sim_dir is None:
        print("\nEjecuta primero una simulación:")
        print("   cd examples && ./quick_circle_simulation.sh")
        return

    print(f"Analizando: {sim_dir.name}")

    # Análisis
    flow_df = analyze_flow_data(sim_dir)
    avalanche_df = analyze_avalanche_data(sim_dir)

    # Visualización
    if flow_df is not None or avalanche_df is not None:
        create_simple_plot(flow_df, avalanche_df, sim_dir)

    print(f"\nAnálisis completado!")
    print(f"Archivos en: {sim_dir}")

if __name__ == "__main__":
    main()

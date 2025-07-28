#!/usr/bin/env python3
"""
Script para generar la distribución de tamaños de avalanchas ns(D) a partir de avalanche_data.csv
donde D es la relación entre el radio del orificio y el base_radius.

Este script lee las avalanchas directamente del archivo avalanche_data.csv generado por el
simulador C++, que contiene las avalanchas detectadas con la misma lógica que usa la simulación.

Uso:
    python avalanche_size_distribution.py <directorio_simulacion> [--output <archivo_salida>] [--plot]

Ejemplo:
    python avalanche_size_distribution.py sim_2000_chi0.20_ratio0.40_br0.065_lg0_sm0_poly2000_sides5_outlet0.52
"""

import pandas as pd
import numpy as np
import argparse
import os
import sys
import re
from pathlib import Path

# Importar matplotlib solo si se necesita
try:
    import matplotlib.pyplot as plt
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

def extract_parameters_from_dirname(dirname):
    """
    Extrae parámetros de simulación del nombre del directorio.
    Formato esperado: sim_<particles>_chi<chi>_ratio<ratio>_br<base_radius>_lg<large>_sm<small>_poly<polygons>_sides<sides>_outlet<outlet>
    """
    pattern = r'sim_(\d+)_chi([\d.]+)_ratio([\d.]+)_br([\d.]+)_lg(\d+)_sm(\d+)_poly(\d+)_sides(\d+)_outlet([\d.]+)'
    match = re.match(pattern, dirname)

    if not match:
        raise ValueError(f"No se pudo extraer parámetros del nombre del directorio: {dirname}")

    params = {
        'total_particles': int(match.group(1)),
        'chi': float(match.group(2)),
        'size_ratio': float(match.group(3)),
        'base_radius': float(match.group(4)),
        'num_large_circles': int(match.group(5)),
        'num_small_circles': int(match.group(6)),
        'num_polygon_particles': int(match.group(7)),
        'num_sides': int(match.group(8)),
        'outlet_width': float(match.group(9))
    }

    return params

def read_avalanches_from_cpp_output(avalanche_file):
    """
    Lee las avalanchas directamente del archivo avalanche_data.csv generado por el código C++.

    Args:
        avalanche_file: Ruta al archivo avalanche_data.csv

    Returns:
        Lista de diccionarios con información de cada avalancha
    """
    avalanches = []

    try:
        # Leer el archivo línea por línea para manejar comentarios al final
        with open(avalanche_file, 'r') as f:
            lines = f.readlines()

        for line in lines:
            line = line.strip()
            # Saltar líneas vacías y comentarios
            if not line or line.startswith('#') or line.startswith('==='):
                continue

            # Parsear líneas de avalanchas
            # Formato: "Avalancha X,start_time,end_time,duration,particles"
            if line.startswith('Avalancha'):
                parts = line.split(',')
                if len(parts) >= 5:
                    try:
                        avalanche_id = parts[0].strip()
                        start_time = float(parts[1].strip())
                        end_time = float(parts[2].strip())
                        duration = float(parts[3].strip())
                        particles = int(float(parts[4].strip()))  # Convertir a int por si es float

                        avalanches.append({
                            'id': avalanche_id,
                            'start_time': start_time,
                            'end_time': end_time,
                            'duration': duration,
                            'particles': particles
                        })
                    except (ValueError, IndexError) as e:
                        print(f"Advertencia: Error procesando línea '{line}': {e}")
                        continue

        print(f"Avalanchas leídas del archivo C++: {len(avalanches)}")
        return avalanches

    except FileNotFoundError:
        print(f"Error: No se encontró el archivo {avalanche_file}")
        return []
    except Exception as e:
        print(f"Error leyendo archivo de avalanchas: {e}")
        return []

def calculate_avalanche_size_distribution(avalanches, outlet_radius, base_radius):
    """
    Calcula la distribución de tamaños de avalanchas ns(D) y estadísticas adicionales.

    Args:
        avalanches: Lista de avalanchas con información de partículas
        outlet_radius: Radio del orificio
        base_radius: Radio base de referencia

    Returns:
        Tuple con (DataFrame de distribución, DataFrame de estadísticas)
    """
    if not avalanches:
        print("Advertencia: No se encontraron avalanchas")
        return pd.DataFrame(columns=['D', 'avalanche_size', 'ns_D']), pd.DataFrame()

    # Calcular D (relación de radios)
    D = outlet_radius / base_radius

    # Extraer tamaños de avalancha (número de partículas)
    sizes = [av['particles'] for av in avalanches]
    durations = [av['duration'] for av in avalanches]

    # Estadísticas básicas
    stats = {
        'D': D,
        'total_avalanches': len(avalanches),
        'min_size': min(sizes),
        'max_size': max(sizes),
        'mean_size': np.mean(sizes),
        'median_size': np.median(sizes),
        'std_size': np.std(sizes),
        'total_particles_in_avalanches': sum(sizes),
        'mean_duration': np.mean(durations),
        'median_duration': np.median(durations),
        'total_avalanche_time': sum(durations)
    }

    # Crear bins para la distribución
    if len(sizes) > 1:
        min_size = min(sizes)
        max_size = max(sizes)

        # Para pocos datos, usar bins lineales simples
        if len(sizes) <= 10:
            # Crear bins que capturen cada avalancha individual o grupos pequeños
            unique_sizes = sorted(set(sizes))
            if len(unique_sizes) <= 5:
                # Bins centrados en cada tamaño único
                bins = []
                for i, size in enumerate(unique_sizes):
                    if i == 0:
                        lower = size - 0.5
                    else:
                        lower = (unique_sizes[i-1] + size) / 2

                    if i == len(unique_sizes) - 1:
                        upper = size + 0.5
                    else:
                        upper = (size + unique_sizes[i+1]) / 2

                    bins.extend([lower, upper])
                bins = sorted(set(bins))
            else:
                # Bins lineales tradicionales
                num_bins = min(len(sizes), 8)
                bins = np.linspace(min_size - 0.5, max_size + 0.5, num_bins + 1)
        else:
            # Para más datos, usar estrategia logarítmica si hay suficiente rango
            if max_size / min_size > 10:
                num_bins = min(50, int(np.sqrt(len(sizes))))
                bins = np.logspace(np.log10(max(min_size, 1)), np.log10(max_size), num_bins)
            else:
                num_bins = min(30, int(np.sqrt(len(sizes))))
                bins = np.linspace(min_size, max_size, num_bins)
    else:
        bins = [sizes[0] - 0.5, sizes[0] + 0.5]

    # Calcular histograma
    counts, bin_edges = np.histogram(sizes, bins=bins)
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    bin_widths = bin_edges[1:] - bin_edges[:-1]

    # Normalizar para obtener densidad de probabilidad
    total_avalanches = len(sizes)
    ns_D = counts / (total_avalanches * bin_widths)

    # Crear DataFrame con resultados
    distribution = pd.DataFrame({
        'D': D,
        'avalanche_size': bin_centers,
        'ns_D': ns_D,
        'count': counts,
        'bin_width': bin_widths,
        'probability': counts / total_avalanches
    })

    # Filtrar bins vacíos
    distribution = distribution[distribution['count'] > 0]

    # DataFrame de estadísticas
    stats_df = pd.DataFrame([stats])

    return distribution, stats_df

def plot_distribution(distribution, params, output_file=None):
    """
    Crea gráfico de la distribución de tamaños de avalanchas.
    """
    if not MATPLOTLIB_AVAILABLE:
        print("Error: matplotlib no está disponible. Instálalo con: pip install matplotlib")
        return

    if distribution.empty:
        print("No hay datos para graficar")
        return

    plt.figure(figsize=(10, 8))

    # Gráfico principal
    plt.loglog(distribution['avalanche_size'], distribution['ns_D'], 'bo-',
               markersize=8, linewidth=2, label='Datos')

    plt.xlabel('Tamaño de Avalancha (número de partículas)', fontsize=14)
    plt.ylabel('ns(D) - Densidad de Probabilidad', fontsize=14)
    plt.title(f'Distribución de Tamaños de Avalanchas\n'
              f'D = {distribution["D"].iloc[0]:.2f} (outlet_radius/base_radius)\n'
              f'Partículas: {params["total_particles"]}, Outlet: {params["outlet_width"]:.3f}m',
              fontsize=16)

    plt.grid(True, alpha=0.7)
    plt.legend(fontsize=12)

    # Añadir información estadística
    total_avalanches = distribution['count'].sum()
    mean_size = np.average(distribution['avalanche_size'], weights=distribution['count'])

    plt.text(0.02, 0.98, f'Total avalanchas: {int(total_avalanches)}\n'
                          f'Tamaño promedio: {mean_size:.1f} partículas\n'
                          f'Base radius: {params["base_radius"]:.3f}m\n'
                          f'Chi: {params["chi"]:.2f}',
             transform=plt.gca().transAxes, fontsize=11,
             verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Gráfico guardado en: {output_file}")
    else:
        plt.show()

def main():
    parser = argparse.ArgumentParser(description='Generar distribución de tamaños de avalanchas ns(D)')
    parser.add_argument('sim_dir', help='Directorio de simulación')
    parser.add_argument('--output', '-o', help='Archivo de salida para los datos CSV')
    parser.add_argument('--plot', action='store_true', help='Mostrar/guardar gráfico')
    parser.add_argument('--plot-output', help='Archivo de salida para el gráfico')

    args = parser.parse_args()

    # Verificar que el directorio existe
    sim_path = Path(args.sim_dir)
    if not sim_path.exists():
        print(f"Error: El directorio {args.sim_dir} no existe")
        sys.exit(1)

    # Buscar archivos necesarios
    avalanche_data_file = sim_path / 'avalanche_data.csv'
    if not avalanche_data_file.exists():
        print(f"Error: No se encontró {avalanche_data_file}")
        sys.exit(1)

    try:
        # Extraer parámetros del nombre del directorio
        params = extract_parameters_from_dirname(sim_path.name)
        print(f"Parámetros extraídos:")
        for key, value in params.items():
            print(f"  {key}: {value}")

        # Calcular radio del orificio (asumiendo orificio circular)
        outlet_radius = params['outlet_width'] / 2.0

        print(f"\nD = outlet_radius / base_radius = {outlet_radius:.4f} / {params['base_radius']:.4f} = {outlet_radius/params['base_radius']:.3f}")

        # Leer avalanchas directamente del archivo generado por el código C++
        print(f"\nLeyendo avalanchas desde: {avalanche_data_file}")
        avalanches = read_avalanches_from_cpp_output(avalanche_data_file)

        print(f"Avalanchas detectadas: {len(avalanches)}")

        if avalanches:
            for i, av in enumerate(avalanches):
                print(f"  {av['id']}: {av['particles']} partículas, {av['duration']:.2f}s ({av['start_time']:.2f}-{av['end_time']:.2f}s)")

        # Calcular distribución
        distribution, stats = calculate_avalanche_size_distribution(
            avalanches, outlet_radius, params['base_radius']
        )

        if not distribution.empty:
            print(f"\nEstadísticas de avalanchas:")
            print(f"  Total de avalanchas: {stats.iloc[0]['total_avalanches']}")
            print(f"  Tamaño mínimo: {stats.iloc[0]['min_size']} partículas")
            print(f"  Tamaño máximo: {stats.iloc[0]['max_size']} partículas")
            print(f"  Tamaño promedio: {stats.iloc[0]['mean_size']:.2f} ± {stats.iloc[0]['std_size']:.2f} partículas")
            print(f"  Tamaño mediana: {stats.iloc[0]['median_size']:.1f} partículas")
            print(f"  Total partículas en avalanchas: {stats.iloc[0]['total_particles_in_avalanches']}")
            print(f"  Duración promedio: {stats.iloc[0]['mean_duration']:.2f}s")
            print(f"  Tiempo total en avalanchas: {stats.iloc[0]['total_avalanche_time']:.2f}s")

            print(f"\nDistribución ns(D):")
            print(distribution[['avalanche_size', 'ns_D', 'count', 'probability']].to_string(index=False))

            # Guardar datos si se especifica
            if args.output:
                distribution.to_csv(args.output, index=False)
                # Guardar estadísticas también
                stats_file = args.output.replace('.csv', '_stats.csv')
                stats.to_csv(stats_file, index=False)
                print(f"\nDatos guardados en: {args.output}")
                print(f"Estadísticas guardadas en: {stats_file}")

            # Crear gráfico si se solicita
            if args.plot:
                if not MATPLOTLIB_AVAILABLE:
                    print("Advertencia: matplotlib no disponible. Instálalo para generar gráficos.")
                else:
                    plot_output = args.plot_output
                    if not plot_output and args.output:
                        plot_output = args.output.replace('.csv', '_plot.png')
                    elif not plot_output:
                        plot_output = sim_path / 'avalanche_size_distribution.png'

                    plot_distribution(distribution, params, plot_output)
        else:
            print("No se pudo calcular la distribución (sin avalanchas detectadas)")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

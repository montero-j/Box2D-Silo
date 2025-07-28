#!/usr/bin/env python3
"""
Script para combinar distribuciones de tamaños de avalanchas de múltiples simulaciones
y generar una distribución agregada ns(D).

Uso:
    python combine_avalanche_distributions.py <patrón_directorios> [--output <archivo_salida>] [--plot]

Ejemplo:
    python combine_avalanche_distributions.py "sim_*_chi0.20_*" --output combined_distribution.csv --plot
"""

import pandas as pd
import numpy as np
import argparse
import os
import sys
import re
import glob
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
    """
    pattern = r'sim_(\d+)_chi([\d.]+)_ratio([\d.]+)_br([\d.]+)_lg(\d+)_sm(\d+)_poly(\d+)_sides(\d+)_outlet([\d.]+)'
    match = re.match(pattern, dirname)

    if not match:
        return None

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
    """
    avalanches = []

    try:
        with open(avalanche_file, 'r') as f:
            lines = f.readlines()

        for line in lines:
            line = line.strip()
            if not line or line.startswith('#') or line.startswith('==='):
                continue

            if line.startswith('Avalancha'):
                parts = line.split(',')
                if len(parts) >= 5:
                    try:
                        avalanche_id = parts[0].strip()
                        start_time = float(parts[1].strip())
                        end_time = float(parts[2].strip())
                        duration = float(parts[3].strip())
                        particles = int(float(parts[4].strip()))

                        avalanches.append({
                            'id': avalanche_id,
                            'start_time': start_time,
                            'end_time': end_time,
                            'duration': duration,
                            'particles': particles
                        })
                    except (ValueError, IndexError):
                        continue

        return avalanches

    except FileNotFoundError:
        return []
    except Exception:
        return []

def combine_avalanche_distributions(sim_dirs):
    """
    Combina distribuciones de múltiples simulaciones.
    """
    all_avalanches = []
    all_params = []
    sim_count = 0

    for sim_dir in sim_dirs:
        sim_path = Path(sim_dir)
        if not sim_path.exists():
            continue

        # Extraer parámetros
        params = extract_parameters_from_dirname(sim_path.name)
        if params is None:
            print(f"Advertencia: No se pudieron extraer parámetros de {sim_path.name}")
            continue

        # Leer avalanchas
        avalanche_file = sim_path / 'avalanche_data.csv'
        if not avalanche_file.exists():
            print(f"Advertencia: No se encontró {avalanche_file}")
            continue

        avalanches = read_avalanches_from_cpp_output(avalanche_file)
        if avalanches:
            all_avalanches.extend(avalanches)
            all_params.append(params)
            sim_count += 1
            print(f"Simulación {sim_count}: {sim_path.name} - {len(avalanches)} avalanchas")

    if not all_avalanches:
        return None, None, None

    # Verificar consistencia de parámetros
    base_params = all_params[0]
    D = (base_params['outlet_width'] / 2.0) / base_params['base_radius']

    # Calcular distribución combinada
    sizes = [av['particles'] for av in all_avalanches]
    durations = [av['duration'] for av in all_avalanches]

    # Estadísticas combinadas
    combined_stats = {
        'D': D,
        'total_simulations': sim_count,
        'total_avalanches': len(all_avalanches),
        'min_size': min(sizes),
        'max_size': max(sizes),
        'mean_size': np.mean(sizes),
        'median_size': np.median(sizes),
        'std_size': np.std(sizes),
        'total_particles_in_avalanches': sum(sizes),
        'mean_duration': np.mean(durations),
        'median_duration': np.median(durations),
        'total_avalanche_time': sum(durations),
        'avalanches_per_simulation': len(all_avalanches) / sim_count
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

    return distribution, combined_stats, base_params

def plot_combined_distribution(distribution, stats, params, output_file=None):
    """
    Crea gráfico de la distribución combinada de tamaños de avalanchas.
    """
    if not MATPLOTLIB_AVAILABLE:
        print("Error: matplotlib no está disponible. Instálalo con: pip install matplotlib")
        return

    if distribution.empty:
        print("No hay datos para graficar")
        return

    plt.figure(figsize=(12, 9))

    # Gráfico principal
    plt.loglog(distribution['avalanche_size'], distribution['ns_D'], 'bo-',
               markersize=8, linewidth=2, label='Distribución Combinada')

    plt.xlabel('Tamaño de Avalancha (número de partículas)', fontsize=14)
    plt.ylabel('ns(D) - Densidad de Probabilidad', fontsize=14)
    plt.title(f'Distribución Combinada de Tamaños de Avalanchas\n'
              f'D = {distribution["D"].iloc[0]:.2f} - {stats["total_simulations"]} simulaciones\n'
              f'Total: {stats["total_avalanches"]} avalanchas',
              fontsize=16)

    plt.grid(True, alpha=0.7)
    plt.legend(fontsize=12)

    # Añadir información estadística
    plt.text(0.02, 0.98,
             f'Simulaciones: {stats["total_simulations"]}\n'
             f'Total avalanchas: {stats["total_avalanches"]}\n'
             f'Avalanchas/sim: {stats["avalanches_per_simulation"]:.1f}\n'
             f'Tamaño promedio: {stats["mean_size"]:.1f} ± {stats["std_size"]:.1f}\n'
             f'Rango: {stats["min_size"]}-{stats["max_size"]} partículas\n'
             f'Base radius: {params["base_radius"]:.3f}m\n'
             f'Outlet: {params["outlet_width"]:.3f}m',
             transform=plt.gca().transAxes, fontsize=11,
             verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Gráfico guardado en: {output_file}")
    else:
        plt.show()

def main():
    parser = argparse.ArgumentParser(description='Combinar distribuciones de avalanchas de múltiples simulaciones')
    parser.add_argument('pattern', help='Patrón de directorios de simulación (ej: "sim_*_chi0.20_*")')
    parser.add_argument('--output', '-o', help='Archivo de salida para los datos CSV')
    parser.add_argument('--plot', action='store_true', help='Mostrar/guardar gráfico')
    parser.add_argument('--plot-output', help='Archivo de salida para el gráfico')

    args = parser.parse_args()

    # Buscar directorios que coincidan con el patrón
    sim_dirs = glob.glob(args.pattern)
    sim_dirs = [d for d in sim_dirs if os.path.isdir(d)]

    if not sim_dirs:
        print(f"Error: No se encontraron directorios que coincidan con el patrón '{args.pattern}'")
        sys.exit(1)

    print(f"Encontrados {len(sim_dirs)} directorios de simulación")

    try:
        # Combinar distribuciones
        distribution, stats, params = combine_avalanche_distributions(sim_dirs)

        if distribution is None:
            print("Error: No se pudieron combinar las distribuciones")
            sys.exit(1)

        print(f"\nEstadísticas combinadas:")
        print(f"  Simulaciones procesadas: {stats['total_simulations']}")
        print(f"  Total de avalanchas: {stats['total_avalanches']}")
        print(f"  Avalanchas por simulación: {stats['avalanches_per_simulation']:.2f}")
        print(f"  D = {stats['D']:.3f}")
        print(f"  Tamaño promedio: {stats['mean_size']:.2f} ± {stats['std_size']:.2f} partículas")
        print(f"  Rango de tamaños: {stats['min_size']}-{stats['max_size']} partículas")
        print(f"  Duración promedio: {stats['mean_duration']:.2f}s")

        print(f"\nDistribución combinada ns(D) ({len(distribution)} bins):")
        print(distribution[['avalanche_size', 'ns_D', 'count', 'probability']].head(10).to_string(index=False))
        if len(distribution) > 10:
            print("  ...")

        # Guardar datos si se especifica
        if args.output:
            distribution.to_csv(args.output, index=False)
            # Guardar estadísticas también
            stats_file = args.output.replace('.csv', '_combined_stats.csv')
            stats_df = pd.DataFrame([stats])
            stats_df.to_csv(stats_file, index=False)
            print(f"\nDatos guardados en: {args.output}")
            print(f"Estadísticas guardadas en: {stats_file}")

        # Crear gráfico si se solicita
        if args.plot:
            if not MATPLOTLIB_AVAILABLE:
                print("Advertencia: matplotlib no disponible. Instálalo para generar gráficos.")
            else:
                plot_output = args.plot_output
                if not plot_output and args.output:
                    plot_output = args.output.replace('.csv', '_combined_plot.png')
                elif not plot_output:
                    plot_output = 'combined_avalanche_distribution.png'

                plot_combined_distribution(distribution, stats, params, plot_output)

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Script para verificar que los tamaños de avalanchas están correctamente calculados
comparando con los datos de flow_data.csv
"""

import pandas as pd
import numpy as np
import sys
from pathlib import Path

def verify_avalanche_calculations(sim_dir):
    """Verifica los cálculos de avalanchas comparando avalanche_data.csv con flow_data.csv"""

    sim_path = Path(sim_dir)
    avalanche_file = sim_path / 'avalanche_data.csv'
    flow_file = sim_path / 'flow_data.csv'

    print(f"\n=== Verificando simulación: {sim_path.name} ===")

    # Leer avalanchas
    avalanches = []
    with open(avalanche_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('Avalancha'):
                parts = line.split(',')
                if len(parts) >= 5:
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
                        'particles_reported': particles
                    })

    print(f"Avalanchas encontradas: {len(avalanches)}")

    # Leer datos de flujo
    flow_data = pd.read_csv(flow_file)
    print(f"Datos de flujo: {len(flow_data)} puntos")
    print(f"Rango temporal: {flow_data['Time'].min():.3f} - {flow_data['Time'].max():.3f}s")
    print(f"Partículas totales al final: {flow_data['NoPTotal'].iloc[-1]}")

    # Verificar cada avalancha
    for i, av in enumerate(avalanches):
        print(f"\n--- {av['id']} ---")
        print(f"Tiempo: {av['start_time']:.3f} - {av['end_time']:.3f}s (duración: {av['duration']:.3f}s)")
        print(f"Partículas reportadas: {av['particles_reported']}")

        # Encontrar datos de flujo en el rango de tiempo
        mask = (flow_data['Time'] >= av['start_time']) & (flow_data['Time'] <= av['end_time'])
        avalanche_flow = flow_data[mask].copy()

        if len(avalanche_flow) == 0:
            print("  No se encontraron datos de flujo en este rango temporal")
            continue

        # Calcular partículas en esta avalancha desde flow_data
        start_particles = 0
        end_particles = 0

        # Buscar el punto más cercano al inicio
        start_idx = flow_data['Time'].searchsorted(av['start_time'])
        if start_idx > 0:
            start_particles = flow_data.iloc[start_idx-1]['NoPTotal']

        # Buscar el punto más cercano al final
        end_idx = flow_data['Time'].searchsorted(av['end_time'])
        if end_idx < len(flow_data):
            end_particles = flow_data.iloc[end_idx]['NoPTotal']
        else:
            end_particles = flow_data.iloc[-1]['NoPTotal']

        calculated_particles = end_particles - start_particles

        print(f"  Partículas al inicio: {start_particles}")
        print(f"  Partículas al final: {end_particles}")
        print(f"  Partículas calculadas: {calculated_particles}")
        print(f"  Diferencia: {av['particles_reported'] - calculated_particles}")

        if abs(av['particles_reported'] - calculated_particles) <= 5:  # Tolerancia pequeña
            print("  Cálculo correcto")
        else:
            print("  Posible error en el cálculo")

        # Mostrar algunas estadísticas del flujo durante la avalancha
        if len(avalanche_flow) > 0:
            max_flow_rate = avalanche_flow['NoPFlowRate'].max()
            avg_flow_rate = avalanche_flow['NoPFlowRate'].mean()
            print(f"  Flujo máximo: {max_flow_rate:.2f} partículas/s")
            print(f"  Flujo promedio: {avg_flow_rate:.2f} partículas/s")

def main():
    if len(sys.argv) < 2:
        print("Uso: python verify_avalanche_calculations.py <directorio_simulacion> [<otro_directorio>...]")
        sys.exit(1)

    for sim_dir in sys.argv[1:]:
        try:
            verify_avalanche_calculations(sim_dir)
        except Exception as e:
            print(f"Error verificando {sim_dir}: {e}")

if __name__ == "__main__":
    main()

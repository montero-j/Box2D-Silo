import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator

# Configuración
BASE_DIR = "../resultados"
TARGET_R = "0.4"
CHI_VALUES = [f"{i/10:.1f}" for i in range(3, 11)]  # chi de 0.3 a 1.0
N_SIMULATIONS = 50  # Añadido para consistencia con el script de análisis

# Cargar datos estadísticos
stats_file = os.path.join(BASE_DIR, f"flow_stats_r_{TARGET_R}.csv")
if os.path.exists(stats_file):
    stats = pd.read_csv(stats_file)
else:
    raise FileNotFoundError("Primero ejecute el script de análisis (average-flow.py) para generar los datos")

# Configuración de gráfico mejorada
plt.figure(figsize=(12, 7), dpi=120)  # Tamaño ligeramente mayor
ax = plt.gca()

# Personalización de colores
main_color = '#2c7bb6'  # Azul más profesional
error_color = '#d7191c'  # Rojo para contrastar
bg_color = '#f7f7f7'     # Fondo gris claro

# Fondo del gráfico
plt.gcf().set_facecolor(bg_color)
ax.set_facecolor(bg_color)

# Gráfico principal con mejor formato
(line, caps, bars) = ax.errorbar(
    x=stats['Chi'].astype(float),
    y=stats['Mean_Flow'],
    yerr=stats['Std_Flow'],
    fmt='o-',
    markersize=8,
    linewidth=2,
    color=main_color,
    ecolor=error_color,
    capsize=6,
    capthick=2,
    elinewidth=2,
    label='Flujo promedio ± desviación estándar'
)

# Mejorar las barras de error
plt.setp(caps, color=error_color, alpha=0.8)
plt.setp(bars, color=error_color, alpha=0.5)

# Detalles del gráfico mejorados
title = (
    f"Análisis de Flujo en Silo - Radio = {TARGET_R}\n"
    f"Promedio de {N_SIMULATIONS} simulaciones por valor de χ"
)
ax.set_title(title, pad=20, fontsize=14, fontweight='bold')
ax.set_xlabel("Fracción de partículas grandes (χ)", fontsize=12, labelpad=10)
ax.set_ylabel("Flujo promedio (partículas/segundo)", fontsize=12, labelpad=10)

# Configuración de ejes
ax.xaxis.set_major_locator(MultipleLocator(0.1))
ax.xaxis.set_minor_locator(MultipleLocator(0.05))
ax.yaxis.grid(True, which='major', linestyle='--', alpha=0.7)
ax.xaxis.grid(True, which='major', linestyle=':', alpha=0.5)

# Añadir información adicional mejorada
for i, row in stats.iterrows():
    if not pd.isna(row['Mean_Flow']):
        ax.annotate(
            f"n={int(row['N_Sims'])}", 
            xy=(float(row['Chi']), row['Mean_Flow'] + row['Std_Flow']),
            xytext=(0, 8),
            textcoords="offset points",
            ha='center',
            va='bottom',
            fontsize=9,
            bbox=dict(boxstyle='round,pad=0.2', fc='white', alpha=0.7)
        )

# Leyenda y ajustes finales
plt.legend(
    loc='upper right',
    frameon=True,
    framealpha=0.9,
    facecolor='white'
)

# Ajustar márgenes y diseño
plt.tight_layout(pad=2.0)

# Guardar gráfico en alta calidad
output_path = os.path.join(BASE_DIR, f"flow_analysis_r_{TARGET_R}.png")
plt.savefig(
    output_path,
    bbox_inches='tight',
    dpi=300,  # Mayor resolución
    transparent=False
)

# Mostrar información de guardado
print(f"\nGráfico guardado en: {output_path}")
print(f"Tamaño del gráfico: {plt.gcf().get_size_inches()[0]:.1f}x{plt.gcf().get_size_inches()[1]:.1f} pulgadas")
print(f"Resolución: 300 dpi")

plt.show()
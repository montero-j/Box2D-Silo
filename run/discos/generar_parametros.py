import os
import glob
import argparse # 💡 ¡NUEVO! Importamos la librería para manejar argumentos

def generar_parametros(base_radius, size_ratio, chi, side, total_sims, output_dir='param_files'):
    """
    Genera archivos de parámetros para silo_simulator.
    La numeración de los archivos continúa donde se quedó la ejecución anterior.

    Args:
        base_radius (float): Radio base de partículas.
        size_ratio (float): Proporción de tamaños.
        chi (float): Parámetro de mezcla.
        total_sims (int): Número de simulaciones A GENERAR en esta ejecución.
        output_dir (str): Carpeta donde se guardarán los archivos.
    """

    # Crear carpeta si no existe
    os.makedirs(output_dir, exist_ok=True)

    # ----------------------------------------------------------------------
    # 1. Encontrar el número de simulación inicial (el más alto existente)
    # ----------------------------------------------------------------------

    existing_files = glob.glob(os.path.join(output_dir, "parametros_*.txt"))
    start_sim_num = 1

    if existing_files:
        sim_numbers = []
        for filepath in existing_files:
            try:
                # Extrae el número del nombre (ej: "parametros_5.txt" -> 5)
                filename = os.path.basename(filepath)
                num_str = filename.replace("parametros_", "").replace(".txt", "")
                sim_numbers.append(int(num_str))
            except ValueError:
                continue

        if sim_numbers:
            # El siguiente número a usar es el máximo encontrado + 1
            start_sim_num = max(sim_numbers) + 1

    print(f"La numeración de archivos comenzará en: {start_sim_num}")

    # ----------------------------------------------------------------------
    # 2. Generar los nuevos archivos
    # ----------------------------------------------------------------------

    # Calcular dimensiones escaladas
    silo_height = 240.0 * base_radius
    silo_width  = 40.2 * base_radius
    outlet_width = 11.2 * base_radius

    for sim_counter in range(total_sims):
        sim_num = start_sim_num + sim_counter

        filename = os.path.join(output_dir, f"parametros_{sim_num}.txt")

        with open(filename, "w") as f:
            f.write("# Archivo de parámetros para silo_simulator\n")
            f.write("# Líneas que empiezan con # son comentarios\n")
            f.write("# Formato: PARAMETRO=VALOR\n\n")

            # Parámetros básicos
            f.write("# Parámetros básicos de partículas\n")
            f.write(f"BASE_RADIUS={base_radius:.3f}\n")
            f.write(f"SIZE-RATIO={size_ratio:.3f}\n")
            f.write(f"CHI={chi:.3f}\n")
            f.write("TOTAL_PARTICLES=2000\n")
            f.write("NUM_LARGE_CIRCLES=0\n")
            f.write("NUM_SMALL_CIRCLES=0\n")
            f.write("NUM_POLYGON_PARTICLES=0\n")
            f.write(f"NUM_SIDES={side:.3f}\n")

            # Parámetros de simulación
            f.write("# Parámetros de simulación\n")
            f.write(f"CURRENT_SIM={sim_num}\n")
            f.write(f"TOTAL_SIMS={total_sims}\n")
            f.write("SAVE_SIM_DATA=0\n\n")

            # Parámetros adicionales
            f.write("# Parámetros adicionales opcionales (descomentarlos si se necesitan)\n")
            f.write(f"SILO_HEIGHT={silo_height:.1f}\n")
            f.write(f"SILO_WIDTH={silo_width:.1f}\n")
            f.write(f"OUTLET_WIDTH={outlet_width:.1f}\n")
            f.write("MIN_TIME=-30.0\n")

        print(f"Archivo generado: {filename}")

# ----------------------------------------------------------------------
# 3. Lectura de argumentos desde la terminal (BLOQUE CORREGIDO)
# ----------------------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Genera archivos de parámetros para silo_simulator.")

    # Define los argumentos esperados, igualando los nombres de la función
    parser.add_argument('--base_radius', type=float, required=True, help='Radio base de partículas.')
    parser.add_argument('--size_ratio', type=float, required=True, help='Proporción de tamaños.')
    parser.add_argument('--chi', type=float, required=True, help='Parámetro de mezcla.')
    parser.add_argument('--side', type=int, required=True, help='Número de lados de las partículas (e.g., 3 para triángulos, 4 para cuadrados).')
    parser.add_argument('--total_sims', type=int, required=True, help='Número de simulaciones a generar en esta ejecución.')
    parser.add_argument('--output_dir', type=str, default='param_files', help='Carpeta donde se guardarán los archivos.')

    args = parser.parse_args()

    # Llama a la función usando los valores leídos de la terminal
    generar_parametros(
        base_radius=args.base_radius,
        size_ratio=args.size_ratio,
        chi=args.chi,
        side=args.side,
        total_sims=args.total_sims,
        output_dir=args.output_dir
    )
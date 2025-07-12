import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import os
import re
from tqdm import tqdm
import argparse
import subprocess

def main():
    # Configurar argumentos de línea de comandos
    parser = argparse.ArgumentParser(description='Generar video a partir de datos de simulación de silo')
    parser.add_argument('--data-path', type=str, required=True, 
                        help='Ruta al archivo simulation_data.txt')
    parser.add_argument('--output-dir', type=str, default='video_frames',
                        help='Directorio para guardar los frames (predeterminado: video_frames)')
    parser.add_argument('--video-output', type=str, default='silo_simulation.mp4',
                        help='Nombre del archivo de video de salida (predeterminado: silo_simulation.mp4)')
    parser.add_argument('--frame-step', type=int, default=1,
                        help='Paso para muestrear frames (predeterminado: 1)')
    parser.add_argument('--show-progress', action='store_true',
                        help='Mostrar barra de progreso')
    parser.add_argument('--cpp-path', type=str, default='../src/silo_simulator.cpp',
                        help='Ruta al código fuente C++ para extraer FPS (predeterminado: ../src/silo_simulator.cpp)')
    
    args = parser.parse_args()

    # Extraer FPS del código C++
    def extract_cpp_constant(filename, constant_name):
        with open(filename, 'r') as f:
            content = f.read()
        match = re.search(rf"const\s+float\s+{constant_name}\s*=\s*([0-9.]+)\s*/\s*([0-9.]+)\s*;", content)
        if match:
            numerator = float(match.group(1))
            denominator = float(match.group(2))
            return numerator / denominator
        return None

    # Extraer constantes importantes del código C++
    try:
        time_step = extract_cpp_constant(args.cpp_path, "TIME_STEP")
        sub_step_count = extract_cpp_constant(args.cpp_path, "SUB_STEP_COUNT")
        save_interval = 10  # Por defecto (frameCounter % 10)

        # Calcular FPS efectivo
        if time_step and sub_step_count:
            # Tiempo real por frame de simulación
            real_time_per_frame = time_step * sub_step_count
            
            # FPS efectivo para frames guardados (considerando que se guarda cada 10 frames)
            sim_fps = 1 / (real_time_per_frame * save_interval)
            print(f"FPS detectados en simulación: {sim_fps:.2f} (basado en TIME_STEP={time_step}, SUB_STEP_COUNT={sub_step_count})")
        else:
            sim_fps = 60
            print("⚠️ No se pudieron extraer constantes del código C++. Usando FPS=60 por defecto")
    except Exception as e:
        sim_fps = 60
        print(f"⚠️ Error al extraer constantes del código C++: {str(e)}. Usando FPS=60 por defecto")

    # Crear directorio para las imágenes
    os.makedirs(args.output_dir, exist_ok=True)

    # Leer todo el archivo de datos
    with open(args.data_path, 'r') as f:
        lines = f.readlines()

    # Lista para guardar los datos de cada frame
    frames = []
    current_frame = {"walls": None, "particles": []}
    particle_sizes = []  # Para detectar automáticamente tamaños de partículas
    
    for line in lines:
        line = line.strip()
        if line.startswith("Walls"):
            # Reiniciar el frame actual
            if current_frame["particles"]:
                frames.append(current_frame)
            current_frame = {"walls": None, "particles": []}
            # Parsear la línea de las paredes
            parts = line.split()
            # Las coordenadas de las paredes: 8 valores: groundLeft, groundRight, leftWall, rightWall
            coords = list(map(float, parts[1:]))
            current_frame["walls"] = coords
        elif line == "EndFrame":
            # Guardar el frame actual y reiniciar
            if current_frame["particles"] or current_frame["walls"] is not None:
                frames.append(current_frame)
            current_frame = {"walls": None, "particles": []}
        else:
            # Es una partícula
            parts = line.split()
            if len(parts) < 5:
                continue
            shape_type = int(parts[0])
            x = float(parts[1])
            y = float(parts[2])
            angle = float(parts[3])
            size = float(parts[4])
            
            # Registrar tamaño para detección automática
            if shape_type == 0:  # Solo círculos
                particle_sizes.append(size)
            
            current_frame["particles"].append({
                "type": shape_type,
                "x": x,
                "y": y,
                "angle": angle,
                "size": size
            })

    # Detectar automáticamente tamaños de partículas
    if particle_sizes:
        unique_sizes = np.unique(particle_sizes)
        print(f"Tamaños de partículas detectados: {unique_sizes}")
        
        # Determinar umbral para grandes/pequeñas
        if len(unique_sizes) > 1:
            size_threshold = np.mean(unique_sizes)
        else:
            size_threshold = unique_sizes[0] * 0.8  # Valor arbitrario si solo hay un tamaño
    else:
        size_threshold = 0.2  # Valor por defecto

    print(f"Umbral de tamaño para colores: {size_threshold:.4f}")

    # Configuración de la figura
    fig, ax = plt.subplots(figsize=(6, 8))
    ax.set_xlim(-6, 6)
    ax.set_ylim(-1, 19)
    ax.set_aspect('equal')
    ax.set_facecolor('white')
    plt.tight_layout()

    # Función para dibujar un frame
    def draw_frame(frame_data):
        ax.clear()
        ax.set_xlim(-6, 6)
        ax.set_ylim(-1, 19)
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_facecolor('#f0f0f0')
        
        # Dibujar paredes
        walls = frame_data["walls"]
        if walls is not None:
            # Ground left
            groundLeft_x, groundLeft_y = walls[0], walls[1]
            rect_left = patches.Rectangle((groundLeft_x - 2.3, groundLeft_y - 0.5), 4.6, 1.0, 
                                         linewidth=1, edgecolor='#333333', facecolor='#333333', alpha=0.8)
            ax.add_patch(rect_left)
            
            # Ground right
            groundRight_x, groundRight_y = walls[2], walls[3]
            rect_right = patches.Rectangle((groundRight_x - 2.3, groundRight_y - 0.5), 4.6, 1.0, 
                                          linewidth=1, edgecolor='#333333', facecolor='#333333', alpha=0.8)
            ax.add_patch(rect_right)
            
            # Left wall
            leftWall_x, leftWall_y = walls[4], walls[5]
            rect_left_wall = patches.Rectangle((leftWall_x - 0.5, leftWall_y - 15.0), 1.0, 30.0, 
                                              linewidth=1, edgecolor='#555555', facecolor='#555555', alpha=0.8)
            ax.add_patch(rect_left_wall)
            
            # Right wall
            rightWall_x, rightWall_y = walls[6], walls[7]
            rect_right_wall = patches.Rectangle((rightWall_x - 0.5, rightWall_y - 15.0), 1.0, 30.0, 
                                               linewidth=1, edgecolor='#555555', facecolor='#555555', alpha=0.8)
            ax.add_patch(rect_right_wall)
        
        # Dibujar partículas
        for particle in frame_data["particles"]:
            x = particle["x"]
            y = particle["y"]
            size = particle["size"]
            
            if particle["type"] == 0:  # CIRCLE
                # Determinar color basado en tamaño
                if size > size_threshold:
                    color = '#1f77b4'  # Azul para grandes
                else:
                    color = '#ff7f0e'  # Naranja para pequeñas
                
                circle = patches.Circle((x, y), size, edgecolor='black', linewidth=0.5, facecolor=color, alpha=0.9)
                ax.add_patch(circle)
                
                # Añadir textura para diferenciar
                ax.plot(x, y, 'o', markersize=size*0.7, color='white', alpha=0.3)
                
            elif particle["type"] == 1:  # POLYGON
                # Hexágono (6 lados)
                polygon = patches.RegularPolygon(
                    (x, y),        # Centro
                    6,              # Número de lados
                    radius=size,    # Radio (tamaño)
                    orientation=particle["angle"],  # Orientación
                    edgecolor='black',
                    linewidth=0.5,
                    facecolor='#2ca02c',  # Verde para polígonos
                    alpha=0.9
                )
                ax.add_patch(polygon)
                
                # Añadir punto central para diferenciar
                ax.plot(x, y, 'o', markersize=size*0.5, color='white', alpha=0.4)

    # Generar imágenes para los frames seleccionados
    total_frames = len(frames)
    selected_frames = frames[::args.frame_step]
    num_selected = len(selected_frames)
    
    print(f"\nTotal de frames en simulación: {total_frames}")
    print(f"Frames a procesar: {num_selected} (step={args.frame_step})")
    print(f"FPS de video: {sim_fps:.2f} (calculados a partir del código C++)")
    
    # Configurar barra de progreso si se solicita
    iterator = range(num_selected)
    if args.show_progress:
        iterator = tqdm(iterator, desc="Generando imágenes")
    
    for idx in iterator:
        frame_idx = idx * args.frame_step
        frame = selected_frames[idx]
        draw_frame(frame)
        plt.savefig(f"{args.output_dir}/frame_{frame_idx:05d}.png", dpi=100, bbox_inches='tight', pad_inches=0.1)
        plt.cla()  # Limpiar el eje para el próximo frame

    plt.close()

    # Crear el video con ffmpeg
    print("\nCreando video...")
    cmd = [
        'ffmpeg',
        '-y',  # Sobrescribir si existe
        '-framerate', str(sim_fps),
        '-i', f'{args.output_dir}/frame_%05d.png',
        '-c:v', 'libx264',
        '-pix_fmt', 'yuv420p',
        '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',  # Asegurar dimensiones pares
        '-preset', 'slow',
        '-crf', '20',
        args.video_output
    ]
    
    try:
        subprocess.run(cmd, check=True)
        print(f"\n✅ Video guardado como {args.video_output}")
        
        # Opcional: eliminar frames temporales
        if input("\n¿Eliminar frames temporales? [y/N]: ").lower() == 'y':
            for file in os.listdir(args.output_dir):
                if file.endswith('.png'):
                    os.remove(os.path.join(args.output_dir, file))
            print("Frames temporales eliminados.")
    except subprocess.CalledProcessError as e:
        print(f"\n❌ Error al crear el video: {e}")
    except FileNotFoundError:
        print("\n❌ Error: ffmpeg no encontrado. Asegúrese de tener instalado ffmpeg.")

if __name__ == "__main__":
    main()
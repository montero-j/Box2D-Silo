import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from PIL import Image
import os
import argparse
import subprocess
from tqdm import tqdm
import traceback
import math

class SiloRenderer:
    def __init__(self, width=1920, height=2560,
                 base_radius=0.5, size_ratio=0.4,
                 num_large_circles=0, num_small_circles=0, num_polygon_particles=0,
                 silo_height=11.70, silo_width=2.6, outlet_width=0.3056,
                 high_quality=False):
        self.width = width
        self.height = height
        self.high_quality = high_quality
        
        if high_quality:
            self.particle_scale = 200
            self.min_particle_size = 8
            self.particle_border = 3
            self.circle_segments = 100
            self.line_width = 3.0
            self.dpi = 100  # Aumentar DPI para mejor calidad
        else:
            self.particle_scale = 150
            self.min_particle_size = 5
            self.particle_border = 2
            self.circle_segments = 50
            self.line_width = 2.0
            self.dpi = 72

        self.large_particle_radius = base_radius
        self.small_particle_radius = base_radius * size_ratio
        self.num_large_circles = num_large_circles
        self.num_small_circles = num_small_circles
        self.num_polygon_particles = num_polygon_particles
        self.total_particles = num_large_circles + num_small_circles + num_polygon_particles
        self.debug_mode = True
        self.silo_height = silo_height
        self.silo_width = silo_width
        self.wall_thickness = 0.1
        self.ground_level_y = 0.0
        self.outlet_x_half_width = outlet_width / 2.0
        
        self._init_matplotlib()

    def _init_matplotlib(self):
        try:
            plt.ioff()
            self.fig, self.ax = plt.subplots(1, 1, figsize=(self.width / self.dpi, self.height / self.dpi), dpi=self.dpi)
            self.fig.patch.set_facecolor([0.95, 0.95, 0.95])
            self.ax.set_facecolor([0.95, 0.95, 0.95])
            self.ax.set_aspect('equal', adjustable='box')
            self.ax.set_axis_off()
            plt.subplots_adjust(left=0, right=1, top=1, bottom=0)
            
        except Exception as e:
            print(f"Error al inicializar Matplotlib: {str(e)}")
            traceback.print_exc()
            raise

    def render_frame(self, frame_data):
        try:
            self.ax.clear()
            self.ax.set_axis_off()
            self.ax.set_aspect('equal', adjustable='box')

            target_world_left = -(self.silo_width / 2.0) - self.wall_thickness
            target_world_right = (self.silo_width / 2.0) + self.wall_thickness
            target_world_bottom = self.ground_level_y - self.wall_thickness
            target_world_top = self.silo_height + self.wall_thickness
            
            margin_particle = max(self.large_particle_radius, self.small_particle_radius,
                                self.large_particle_radius * 1.5)
            extra_margin_x = 0.2
            extra_margin_y_top = 0.5
            extra_margin_y_bottom = 0.5

            view_left = target_world_left - margin_particle - extra_margin_x
            view_right = target_world_right + margin_particle + extra_margin_x
            view_bottom = target_world_bottom - margin_particle - extra_margin_y_bottom
            view_top = target_world_top + margin_particle + extra_margin_y_top

            world_view_width = view_right - view_left
            world_view_height = view_top - view_bottom

            screen_aspect = float(self.width) / self.height
            world_aspect = world_view_width / world_view_height

            if screen_aspect > world_aspect:
                adjusted_world_width = world_view_height * screen_aspect
                x_center = (view_left + view_right) / 2.0
                left_bound = x_center - adjusted_world_width / 2.0
                right_bound = x_center + adjusted_world_width / 2.0
                bottom_bound = view_bottom
                top_bound = view_top
            else:
                adjusted_world_height = world_view_width / screen_aspect
                y_center = (view_bottom + view_top) / 2.0
                bottom_bound = y_center - adjusted_world_height / 2.0
                top_bound = y_center + adjusted_world_height / 2.0
                left_bound = view_left
                right_bound = view_right

            self.ax.set_xlim(left_bound, right_bound)
            self.ax.set_ylim(bottom_bound, top_bound)

            self._draw_walls()

            if 'rays' in frame_data:
                self._draw_rays(frame_data['rays'])

            if 'particles' in frame_data and frame_data['particles']['positions'].size > 0:
                self._draw_particles(frame_data['particles'])

            if self.debug_mode:
                self._draw_debug_info()

            image = self._capture_image_matplotlib()
            return image

        except Exception as e:
            print(f"Error en render_frame: {str(e)}")
            traceback.print_exc()
            return None

    def _draw_walls(self):
        try:
            wall_color = [0.4, 0.4, 0.4]
            ground_color = [0.3, 0.3, 0.3]

            self.ax.add_patch(
                patches.Rectangle(
                    (-(self.silo_width / 2.0) - self.wall_thickness, self.ground_level_y),
                    self.wall_thickness,
                    self.silo_height,
                    facecolor=wall_color,
                    edgecolor=wall_color,
                    zorder=2
                )
            )

            self.ax.add_patch(
                patches.Rectangle(
                    (self.silo_width / 2.0, self.ground_level_y),
                    self.wall_thickness,
                    self.silo_height,
                    facecolor=wall_color,
                    edgecolor=wall_color,
                    zorder=2
                )
            )

            self.ax.add_patch(
                patches.Rectangle(
                    (-self.silo_width / 2.0, self.ground_level_y - self.wall_thickness),
                    (self.silo_width / 2.0 - self.outlet_x_half_width),
                    self.wall_thickness,
                    facecolor=ground_color,
                    edgecolor=ground_color,
                    zorder=2
                )
            )

            self.ax.add_patch(
                patches.Rectangle(
                    (self.outlet_x_half_width, self.ground_level_y - self.wall_thickness),
                    (self.silo_width / 2.0 - self.outlet_x_half_width),
                    self.wall_thickness,
                    facecolor=ground_color,
                    edgecolor=ground_color,
                    zorder=2
                )
            )

        except Exception as e:
            print(f"Error dibujando paredes: {str(e)}")
            traceback.print_exc()

    def _draw_particles(self, particles_data):
        try:
            positions = particles_data['positions']
            types = particles_data['types']
            sizes = particles_data['sizes']
            num_sides_array = particles_data['num_sides']
            
            for idx in range(len(positions)):
                pos = positions[idx]
                p_type = types[idx]
                p_size = sizes[idx]
                p_num_sides = int(num_sides_array[idx])

                if p_type == 0:  # CIRCLE
                    color = [0.2, 0.4, 1.0, 0.9]
                    self._draw_circle_with_border(pos, p_size, color)
                elif p_type == 1:  # POLYGON
                    color = [0.0, 0.7, 0.3, 0.9]
                    self._draw_polygon_with_border(pos, p_size, p_num_sides, color)

        except Exception as e:
            print(f"Error dibujando partículas: {str(e)}")
            traceback.print_exc()

    def _draw_rays(self, rays):
        if not rays:
            return
        
        for ray in rays:
            self.ax.plot([ray[0][0], ray[1][0]], [ray[0][1], ray[1][1]],
                         color='red', linewidth=1.0, alpha=0.3, zorder=1)

    def _draw_circle_with_border(self, pos, radius, color):
        # Dibujar borde negro (círculo más grande)
        border_thickness_ratio = 0.05 if self.high_quality else 0.08
        border_radius = radius * (1 + border_thickness_ratio)
        border_circle = patches.Circle(pos, border_radius, facecolor='black', zorder=1)
        self.ax.add_patch(border_circle)

        # Dibujar interior de la partícula
        if self.high_quality:
            # Matplotlib no tiene gradientes nativos para parches. Podemos simular con un gradiente
            # o simplemente usar un color más claro para el centro.
            # Aquí, solo usaremos el color principal para simplificar y mantener la consistencia.
            pass
        
        # Dibujar círculo interior
        circle = patches.Circle(pos, radius, facecolor=color, edgecolor=None, zorder=2)
        self.ax.add_patch(circle)

    def _draw_polygon_with_border(self, pos, circum_radius, num_sides, color):
        if num_sides < 3:
            return
        
        # Calcular los vértices del polígono
        vertices = []
        for i in range(num_sides):
            angle = 2.0 * math.pi * i / num_sides
            x = pos[0] + circum_radius * math.cos(angle)
            y = pos[1] + circum_radius * math.sin(angle)
            vertices.append((x, y))

        # Dibujar el polígono interior
        polygon = patches.Polygon(vertices, facecolor=color, edgecolor='black', linewidth=self.line_width, zorder=2)
        self.ax.add_patch(polygon)
    
    def _draw_debug_info(self):
        self.ax.axhline(0, color='red', linestyle='--', linewidth=1, alpha=0.5, zorder=0)
        self.ax.axvline(0, color='green', linestyle='--', linewidth=1, alpha=0.5, zorder=0)

    def _capture_image_matplotlib(self):
        self.fig.canvas.draw()
        img_buffer = self.fig.canvas.buffer_rgba()
        image = Image.frombuffer("RGBA", (self.width, self.height), img_buffer, 'raw', "RGBA", 0, 1)
        return image.transpose(Image.FLIP_TOP_BOTTOM)

def load_simulation_data(file_path, min_time=-1.0, max_time=float('inf'), frame_step=1, total_particles=250):
    frames = []
    frame_count = 0

    with open(file_path, 'r') as f:
        header_skipped = False
        
        for line in f:
            if not header_skipped:
                header_skipped = True
                continue

            parts = line.strip().split(',')
            if not parts:
                continue

            try:
                current_time = float(parts[0])
            except ValueError:
                continue

            if current_time < min_time:
                continue
            if current_time > max_time:
                break

            if frame_count % frame_step != 0:
                frame_count += 1
                continue

            particle_positions = []
            particle_types = []
            particle_sizes = []
            particle_num_sides = []
            rays = []

            particle_data_end = 1 + total_particles * 5
            for i in range(1, particle_data_end, 5):
                if i + 4 >= len(parts):
                    break

                try:
                    x = float(parts[i])
                    y = float(parts[i+1])
                    p_type = int(float(parts[i+2]))
                    size = float(parts[i+3])
                    sides = int(float(parts[i+4]))
                except (ValueError, IndexError):
                    continue
                
                particle_positions.append([x, y])
                particle_types.append(p_type)
                particle_sizes.append(size)
                particle_num_sides.append(sides)

            if "rays_begin" in parts:
                try:
                    rays_begin_idx = parts.index("rays_begin")
                    rays_end_idx = parts.index("rays_end")
                    ray_data = parts[rays_begin_idx+1:rays_end_idx]

                    for i in range(0, len(ray_data), 4):
                        try:
                            x1 = float(ray_data[i])
                            y1 = float(ray_data[i+1])
                            x2 = float(ray_data[i+2])
                            y2 = float(ray_data[i+3])
                            rays.append([[x1, y1], [x2, y2]])
                        except (ValueError, IndexError):
                            continue
                except ValueError:
                    pass

            if particle_positions or rays:
                frame_data = {
                    'time': current_time,
                    'particles': {
                        'positions': np.array(particle_positions, dtype=np.float32),
                        'types': np.array(particle_types, dtype=np.int32),
                        'sizes': np.array(particle_sizes, dtype=np.float32),
                        'num_sides': np.array(particle_num_sides, dtype=np.int32)
                    },
                    'rays': rays
                }
                frames.append(frame_data)
            
            frame_count += 1

    return frames

def main():
    parser = argparse.ArgumentParser(description="Renderizador de simulación de silo con visualización de rayos")
    parser.add_argument('--min-time', type=float, default=-1.0,
                       help='Tiempo inicial a renderizar (segundos, usar -1.0 para incluir sacudida)')
    parser.add_argument('--max-time', type=float, default=float('inf'),
                       help='Tiempo final a renderizar (segundos)')
    parser.add_argument('--frame-step', type=int, default=1,
                       help='Saltar frames (ej. 2 para renderizar cada 2 frames)')
    parser.add_argument('--target-video-duration', type=float, default=None,
                       help='Duración objetivo del video en segundos (calcula automáticamente frame-step)')
    parser.add_argument('--data-path', required=True,
                       help='Archivo CSV con datos de simulación')
    parser.add_argument('--output-dir', default='output_frames',
                       help='Directorio para guardar frames PNG')
    parser.add_argument('--video-output', default='simulation.mp4',
                       help='Nombre del archivo de video de salida')
    parser.add_argument('--fps', type=int, default=60,
                       help='Cuadros por segundo para el video')
    parser.add_argument('--base-radius', type=float, default=0.5,
                       help='Radio base de partículas grandes (metros)')
    parser.add_argument('--size-ratio', type=float, default=0.4,
                       help='Radio pequeño/radio grande (0.0-1.0)')
    parser.add_argument('--chi', type=float, default=0.2,
                       help='Fracción de partículas pequeñas (0.0-1.0)')
    parser.add_argument('--num-large-circles', type=int, default=0,
                       help='Número exacto de partículas circulares grandes')
    parser.add_argument('--num-small-circles', type=int, default=0,
                       help='Número exacto de partículas circulares pequeñas')
    parser.add_argument('--num-polygon-particles', type=int, default=0,
                       help='Número exacto de partículas poligonales')
    parser.add_argument('--total-particles', type=int, default=1000,
                       help='Número total de partículas (usado cuando no se especifican conteos explícitos)')
    parser.add_argument('--silo-height', type=float, default=11.70,
                       help='Altura del silo (metros)')
    parser.add_argument('--silo-width', type=float, default=2.6,
                       help='Ancho del silo (metros)')
    parser.add_argument('--outlet-width', type=float, default=0.3056,
                       help='Ancho total del outlet (metros)')
    parser.add_argument('--width', type=int, default=1920,
                       help='Ancho de la imagen en píxeles (resolución horizontal)')
    parser.add_argument('--height', type=int, default=2560,
                       help='Alto de la imagen en píxeles (resolución vertical)')
    parser.add_argument('--hd', action='store_true',
                       help='Usar resolución HD (1280x1024)')
    parser.add_argument('--full-hd', action='store_true',
                       help='Usar resolución Full HD (1920x1536)')
    parser.add_argument('--4k', action='store_true',
                       help='Usar resolución 4K (3840x3072)')
    parser.add_argument('--8k', action='store_true',
                       help='Usar resolución 8K (7680x6144)')
    parser.add_argument('--high-quality', action='store_true',
                       help='Habilitar renderizado de alta calidad (antialiasing mejorado, más segmentos)')
    parser.add_argument('--debug', action='store_true',
                       help='Habilitar modo depuración')

    args = parser.parse_args()

    if args.hd:
        args.width, args.height = 1280, 1024
        print("Usando resolución HD: 1280x1024")
    elif args.full_hd:
        args.width, args.height = 1920, 1536
        print("Usando resolución Full HD: 1920x1536")
    elif getattr(args, '4k', False):
        args.width, args.height = 3840, 3072
        print("Usando resolución 4K: 3840x3072")
    elif getattr(args, '8k', False):
        args.width, args.height = 7680, 6144
        print("Usando resolución 8K: 7680x6144")
    else:
        print(f"Usando resolución personalizada: {args.width}x{args.height}")
    
    if args.high_quality:
        print("Modo de alta calidad activado: antialiasing mejorado, más segmentos")
    else:
        print("Modo de calidad estándar")

    try:
        os.makedirs(args.output_dir, exist_ok=True)
        print(f"Cargando datos entre {args.min_time}s y {args.max_time}s...")

        if args.target_video_duration is not None:
            total_frames = 0
            sim_start_time = None
            sim_end_time = None

            with open(args.data_path, 'r') as f:
                header_skipped = False
                for line in f:
                    if not header_skipped:
                        header_skipped = True
                        continue

                    parts = line.strip().split(',')
                    if not parts:
                        continue

                    try:
                        current_time = float(parts[0])
                    except ValueError:
                        continue

                    if current_time < args.min_time:
                        continue
                    if current_time > args.max_time:
                        break

                    if sim_start_time is None:
                        sim_start_time = current_time
                    sim_end_time = current_time
                    total_frames += 1

            if total_frames > 0 and sim_start_time is not None and sim_end_time is not None:
                sim_duration = sim_end_time - sim_start_time
                target_frames_in_video = args.target_video_duration * args.fps
                calculated_frame_step = max(1, int(total_frames / target_frames_in_video))

                print(f"Estadísticas de la simulación:")
                print(f"   - Duración de simulación: {sim_duration:.1f} segundos")
                print(f"   - Total de frames disponibles: {total_frames:,}")
                print(f"   - FPS objetivo del video: {args.fps}")
                print(f"   - Duración objetivo del video: {args.target_video_duration:.1f} segundos")
                print(f"   - Frame-step calculado: {calculated_frame_step} (renderizará cada {calculated_frame_step} frames)")
                print(f"   - Frames que se renderizarán: ~{total_frames // calculated_frame_step:,}")
                print(f"   - Duración estimada del video: {(total_frames // calculated_frame_step) / args.fps:.1f} segundos")
                args.frame_step = calculated_frame_step
            else:
                print("No se pudo calcular frame_step automáticamente, usando valor por defecto")

        if args.num_large_circles > 0 or args.num_small_circles > 0 or args.num_polygon_particles > 0:
            total_particles = args.num_large_circles + args.num_small_circles + args.num_polygon_particles
            print(f"Usando conteos explícitos: Grandes={args.num_large_circles}, Pequeñas={args.num_small_circles}, Polígonos={args.num_polygon_particles}")
        else:
            args.num_large_circles = int((1.0 - args.chi) * args.total_particles)
            args.num_small_circles = int(args.chi * args.total_particles)
            total_particles = args.num_large_circles + args.num_small_circles
            print(f"Usando parámetros de mezcla: Grandes={args.num_large_circles}, Pequeñas={args.num_small_circles}")

        frames = load_simulation_data(
            file_path=args.data_path,
            min_time=args.min_time,
            max_time=args.max_time,
            frame_step=args.frame_step,
            total_particles=total_particles
        )
        print(f"Se cargaron {len(frames)} frames en el rango especificado")

        renderer = SiloRenderer(
            width=args.width,
            height=args.height,
            base_radius=args.base_radius,
            size_ratio=args.size_ratio,
            num_large_circles=args.num_large_circles,
            num_small_circles=args.num_small_circles,
            num_polygon_particles=args.num_polygon_particles,
            silo_height=args.silo_height,
            silo_width=args.silo_width,
            outlet_width=args.outlet_width,
            high_quality=args.high_quality
        )
        renderer.debug_mode = args.debug

        print("Renderizando frames...")
        for i, frame in enumerate(tqdm(frames, disable=not args.debug)):
            if frame['particles']['positions'].size > 0 or frame['rays']:
                image = renderer.render_frame(frame)
                if image is not None:
                    image.save(f"{args.output_dir}/frame_{i:05d}.png")
            elif args.debug:
                print(f"Frame {i}: Sin partículas visibles")
        
        plt.close(renderer.fig)

        subprocess.run(['find', args.output_dir, '-type', 'f', '-size', '0c', '-delete'], check=False)

        if len(frames) > 0:
            subprocess.run([
                'ffmpeg', '-y', '-framerate', str(args.fps),
                '-i', f'{args.output_dir}/frame_%05d.png',
                '-c:v', 'libopenh264', '-pix_fmt', 'yuv420p',
                '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',
                args.video_output
            ], check=True)
            print(f"Video generado: {args.video_output}")
        else:
            print("No hay frames para generar video")

    except Exception as e:
        print(f"Error: {str(e)}")
        traceback.print_exc()

if __name__ == "__main__":
    main()
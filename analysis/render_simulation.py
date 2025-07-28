import cupy as cp
import numpy as np
import OpenGL.GL as gl
import OpenGL.GLUT as glut
import OpenGL.GLU as glu
from PIL import Image
import os
import argparse
import subprocess
from tqdm import tqdm
import traceback
import math

class SiloRenderer:
    def __init__(self, width=1200, height=1600,
                 base_radius=0.065, size_ratio=0.4,
                 num_large_circles=0, num_small_circles=0, num_polygon_particles=0,
                 silo_height=11.70, silo_width=2.6, outlet_width=0.3056):
        self.width = width
        self.height = height
        self.particle_scale = 150
        self.min_particle_size = 5
        self.large_particle_radius = base_radius
        self.small_particle_radius = base_radius * size_ratio
        self.num_large_circles = num_large_circles
        self.num_small_circles = num_small_circles
        self.num_polygon_particles = num_polygon_particles
        self.total_particles = num_large_circles + num_small_circles + num_polygon_particles
        self.debug_mode = True
        self.particle_border = 2
        self.silo_height = silo_height
        self.silo_width = silo_width
        self.wall_thickness = 0.1
        self.ground_level_y = 0.0
        self.outlet_x_half_width = outlet_width / 2.0
        self._glut_initialized = False
        self._init_opengl()

    def _init_opengl(self):
        try:
            if not self._glut_initialized:
                glut.glutInit()
                self._glut_initialized = True

            glut.glutInitDisplayMode(glut.GLUT_DOUBLE | glut.GLUT_RGBA)
            glut.glutInitWindowSize(self.width, self.height)
            if hasattr(self, 'window') and self.window:
                glut.glutDestroyWindow(self.window)
            self.window = glut.glutCreateWindow(b"Silo Simulation")
            glut.glutHideWindow()

            self.framebuffer = gl.glGenFramebuffers(1)
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)

            self.texture = gl.glGenTextures(1)
            gl.glBindTexture(gl.GL_TEXTURE_2D, self.texture)
            gl.glTexImage2D(
                gl.GL_TEXTURE_2D, 0, gl.GL_RGBA,
                self.width, self.height, 0,
                gl.GL_RGBA, gl.GL_UNSIGNED_BYTE, None
            )
            gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_LINEAR)
            gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_LINEAR)

            gl.glFramebufferTexture2D(
                gl.GL_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0,
                gl.GL_TEXTURE_2D, self.texture, 0
            )

            if gl.glCheckFramebufferStatus(gl.GL_FRAMEBUFFER) != gl.GL_FRAMEBUFFER_COMPLETE:
                raise RuntimeError("Framebuffer incompleto")

            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)

        except Exception as e:
            print(f"Error al inicializar OpenGL: {str(e)}")
            traceback.print_exc()
            raise

    def render_frame(self, frame_data):
        try:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)
            gl.glViewport(0, 0, self.width, self.height)

            gl.glClearColor(0.95, 0.95, 0.95, 1.0)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)

            gl.glMatrixMode(gl.GL_PROJECTION)
            gl.glLoadIdentity()

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

            glu.gluOrtho2D(left_bound, right_bound, bottom_bound, top_bound)

            gl.glMatrixMode(gl.GL_MODELVIEW)
            gl.glLoadIdentity()

            self._draw_walls()

            if 'rays' in frame_data:
                self._draw_rays(frame_data['rays'])

            if 'particles' in frame_data and frame_data['particles']['positions'].size > 0:
                self._draw_particles(frame_data['particles'])

            if self.debug_mode:
                self._draw_debug_info()

            image = self._capture_image()
            return image

        except Exception as e:
            print(f"Error en render_frame: {str(e)}")
            traceback.print_exc()
            return None
        finally:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)

    def _draw_walls(self):
        try:
            wall_color = [0.4, 0.4, 0.4, 0.9]
            ground_color = [0.3, 0.3, 0.3, 0.9]

            self._draw_rectangle(
                -(self.silo_width / 2.0) - self.wall_thickness,
                self.ground_level_y,
                self.wall_thickness,
                self.silo_height,
                wall_color
            )

            self._draw_rectangle(
                self.silo_width / 2.0,
                self.ground_level_y,
                self.wall_thickness,
                self.silo_height,
                wall_color
            )

            self._draw_rectangle(
                -self.silo_width / 2.0,
                self.ground_level_y - self.wall_thickness,
                (self.silo_width / 2.0 - self.outlet_x_half_width),
                self.wall_thickness,
                ground_color
            )

            self._draw_rectangle(
                self.outlet_x_half_width,
                self.ground_level_y - self.wall_thickness,
                (self.silo_width / 2.0 - self.outlet_x_half_width),
                self.wall_thickness,
                ground_color
            )

        except Exception as e:
            print(f"Error dibujando paredes: {str(e)}")
            traceback.print_exc()

    def _draw_particles(self, particles_data):
        try:
            positions = particles_data['positions'].get()
            types = particles_data['types'].get()
            sizes = particles_data['sizes'].get()
            num_sides_array = particles_data['num_sides'].get()

            gl.glEnable(gl.GL_BLEND)
            gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
            gl.glEnable(gl.GL_POINT_SMOOTH)

            visible_indices = np.where(positions[:, 1] > -1.5)[0]

            for idx in visible_indices:
                pos = positions[idx]
                p_type = types[idx]
                p_size = sizes[idx]
                p_num_sides = int(num_sides_array[idx])

                if p_type == 0:  # CIRCLE
                    if abs(p_size - self.large_particle_radius) < 1e-6:
                        color = [0.2, 0.4, 1.0, 0.9]
                    elif abs(p_size - self.small_particle_radius) < 1e-6:
                        color = [1.0, 0.5, 0.2, 0.9]
                    else:
                        color = [0.5, 0.5, 0.5, 0.9]

                    self._draw_circle_with_border(pos, p_size, color)

                elif p_type == 1:  # POLYGON
                    gl.glDisable(gl.GL_POINT_SMOOTH)
                    color = [0.0, 0.7, 0.3, 0.9]
                    self._draw_polygon_with_border(pos, p_size, p_num_sides, color)
                    gl.glEnable(gl.GL_POINT_SMOOTH)

        except Exception as e:
            print(f"Error dibujando partículas: {str(e)}")
            traceback.print_exc()
        finally:
            gl.glDisable(gl.GL_BLEND)
            gl.glDisable(gl.GL_POINT_SMOOTH)

    def _draw_rays(self, rays):
        if not rays:
            return

        gl.glEnable(gl.GL_BLEND)
        gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
        gl.glColor4f(1.0, 0.0, 0.0, 0.3)
        gl.glLineWidth(1.0)

        gl.glBegin(gl.GL_LINES)
        for ray in rays:
            gl.glVertex2f(ray[0][0], ray[0][1])
            gl.glVertex2f(ray[1][0], ray[1][1])
        gl.glEnd()

        gl.glDisable(gl.GL_BLEND)

    def _draw_circle_with_border(self, pos, radius, color, num_segments=30):
        # Borde visual mucho más delgado para evitar apariencia de superposición
        border_thickness_world_units = 0.001  # Reducido de 0.005 a 0.001 (80% menos)

        gl.glColor4f(0, 0, 0, 0.8)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_segments + 1):
            theta = 2.0 * math.pi * i / num_segments
            x = pos[0] + (radius + border_thickness_world_units) * math.cos(theta)
            y = pos[1] + (radius + border_thickness_world_units) * math.sin(theta)
            gl.glVertex2f(x, y)
        gl.glEnd()

        gl.glColor4f(*color)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_segments + 1):
            theta = 2.0 * math.pi * i / num_segments
            x = pos[0] + radius * math.cos(theta)
            y = pos[1] + radius * math.sin(theta)
            gl.glVertex2f(x, y)
        gl.glEnd()

    def _draw_polygon_with_border(self, pos, circum_radius, num_sides, color, border_thickness_world_units=0.001):
        if num_sides < 3:
            return

        gl.glColor4f(0, 0, 0, 0.8)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_sides + 1):
            angle = 2.0 * math.pi * i / num_sides
            x = pos[0] + (circum_radius + border_thickness_world_units) * math.cos(angle)
            y = pos[1] + (circum_radius + border_thickness_world_units) * math.sin(angle)
            gl.glVertex2f(x, y)
        gl.glEnd()

        gl.glColor4f(*color)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_sides + 1):
            angle = 2.0 * math.pi * i / num_sides
            x = pos[0] + circum_radius * math.cos(angle)
            y = pos[1] + circum_radius * math.sin(angle)
            gl.glVertex2f(x, y)
        gl.glEnd()

    def _draw_rectangle(self, x, y, width, height, color):
        gl.glBegin(gl.GL_QUADS)
        gl.glColor4f(*color)
        gl.glVertex2f(x, y)
        gl.glVertex2f(x + width, y)
        gl.glVertex2f(x + width, y + height)
        gl.glVertex2f(x, y + height)
        gl.glEnd()

    def _draw_debug_info(self):
        gl.glBegin(gl.GL_LINES)
        gl.glColor4f(1, 0, 0, 0.5)
        gl.glVertex2f(-50, 0)
        gl.glVertex2f(50, 0)
        gl.glColor4f(0, 1, 0, 0.5)
        gl.glVertex2f(0, -50)
        gl.glVertex2f(0, 50)
        gl.glEnd()

    def _capture_image(self):
        gl.glReadBuffer(gl.GL_COLOR_ATTACHMENT0)
        data = gl.glReadPixels(0, 0, self.width, self.height,
                             gl.GL_RGBA, gl.GL_UNSIGNED_BYTE)
        image = Image.frombytes("RGBA", (self.width, self.height), data)
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

            if particle_positions or rays:
                frame_data = {
                    'time': current_time,
                    'particles': {
                        'positions': cp.array(particle_positions, dtype=cp.float32),
                        'types': cp.array(particle_types, dtype=cp.int32),
                        'sizes': cp.array(particle_sizes, dtype=cp.float32),
                        'num_sides': cp.array(particle_num_sides, dtype=cp.int32)
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
    parser.add_argument('--base-radius', type=float, default=0.065,
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
    parser.add_argument('--debug', action='store_true',
                       help='Habilitar modo depuración')

    args = parser.parse_args()

    try:
        os.makedirs(args.output_dir, exist_ok=True)
        print(f"Cargando datos entre {args.min_time}s y {args.max_time}s...")

        # Calcular frame_step automáticamente si se especifica target_video_duration
        if args.target_video_duration is not None:
            # Primero necesitamos contar cuántos frames hay en total
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

                print(f"📊 Estadísticas de la simulación:")
                print(f"   • Duración de simulación: {sim_duration:.1f} segundos")
                print(f"   • Total de frames disponibles: {total_frames:,}")
                print(f"   • FPS objetivo del video: {args.fps}")
                print(f"   • Duración objetivo del video: {args.target_video_duration:.1f} segundos")
                print(f"   • Frame-step calculado: {calculated_frame_step} (renderizará cada {calculated_frame_step} frames)")
                print(f"   • Frames que se renderizarán: ~{total_frames // calculated_frame_step:,}")
                print(f"   • Duración estimada del video: {(total_frames // calculated_frame_step) / args.fps:.1f} segundos")

                args.frame_step = calculated_frame_step
            else:
                print("⚠️ No se pudo calcular frame_step automáticamente, usando valor por defecto")

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
            base_radius=args.base_radius,
            size_ratio=args.size_ratio,
            num_large_circles=args.num_large_circles,
            num_small_circles=args.num_small_circles,
            num_polygon_particles=args.num_polygon_particles,
            silo_height=args.silo_height,
            silo_width=args.silo_width,
            outlet_width=args.outlet_width
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

        subprocess.run(['find', args.output_dir, '-type', 'f', '-size', '0c', '-delete'], check=False)

        if len(frames) > 0:
            subprocess.run([
                'ffmpeg', '-y', '-framerate', str(args.fps),
                '-i', f'{args.output_dir}/frame_%05d.png',
                '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
                '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',
                '-preset', 'slow', '-crf', '18',
                args.video_output
            ], check=True)
            print(f"✅ Video generado: {args.video_output}")
        else:
            print("⚠️ No hay frames para generar video")

    except Exception as e:
        print(f"❌ Error: {str(e)}")
        traceback.print_exc()

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import math
import argparse
import subprocess
import traceback
from tqdm import tqdm

# --- Numpy / CuPy backend -----------------------------------------------------
def get_array_backend(force_cpu: bool = False):
    """
    Devuelve (xp, using_cupy) donde xp es numpy o cupy, según disponibilidad y flags.
    """
    if force_cpu:
        import numpy as np
        return np, False
    try:
        import cupy as cp
        return cp, True
    except Exception:
        import numpy as np
        return np, False

def to_numpy(arr):
    """
    Convierte un arreglo xp (numpy o cupy) a numpy.ndarray para dibujar con PyOpenGL.
    """
    import numpy as np
    try:
        import cupy as cp  # puede no existir
        if isinstance(arr, cp.ndarray):
            return cp.asnumpy(arr)
    except Exception:
        pass
    return np.asarray(arr)

# --- OpenGL / PIL -------------------------------------------------------------
import OpenGL.GL as gl
import OpenGL.GLUT as glut
import OpenGL.GLU as glu
from PIL import Image

# ------------------------------------------------------------------------------
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
        else:
            self.particle_scale = 150
            self.min_particle_size = 5
            self.particle_border = 2
            self.circle_segments = 50

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
        self._glut_initialized = False
        self._init_opengl()

    def _init_opengl(self):
        try:
            if not self._glut_initialized:
                glut.glutInit()
                self._glut_initialized = True

            display_mode = glut.GLUT_DOUBLE | glut.GLUT_RGBA
            if self.high_quality:
                try:
                    display_mode |= glut.GLUT_MULTISAMPLE
                except Exception:
                    pass

            glut.glutInitDisplayMode(display_mode)
            glut.glutInitWindowSize(self.width, self.height)
            if hasattr(self, 'window') and self.window:
                try:
                    glut.glutDestroyWindow(self.window)
                except Exception:
                    pass
            self.window = glut.glutCreateWindow(b"Silo Simulation")
            try:
                glut.glutHideWindow()
            except Exception:
                pass

            self.framebuffer = gl.glGenFramebuffers(1)
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)

            self.texture = gl.glGenTextures(1)
            gl.glBindTexture(gl.GL_TEXTURE_2D, self.texture)
            gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, gl.GL_RGBA8,
                            self.width, self.height, 0,
                            gl.GL_RGBA, gl.GL_UNSIGNED_BYTE, None)

            if self.high_quality:
                gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_LINEAR_MIPMAP_LINEAR)
                gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_LINEAR)
                gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
            else:
                gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_LINEAR)
                gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_LINEAR)

            gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_S, gl.GL_CLAMP_TO_EDGE)
            gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_T, gl.GL_CLAMP_TO_EDGE)

            gl.glFramebufferTexture2D(gl.GL_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0,
                                      gl.GL_TEXTURE_2D, self.texture, 0)

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

            if self.high_quality:
                gl.glEnable(gl.GL_MULTISAMPLE)
                gl.glEnable(gl.GL_LINE_SMOOTH)
                gl.glEnable(gl.GL_POLYGON_SMOOTH)
                gl.glEnable(gl.GL_BLEND)
                gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
                gl.glHint(gl.GL_LINE_SMOOTH_HINT, gl.GL_NICEST)
                gl.glHint(gl.GL_POLYGON_SMOOTH_HINT, gl.GL_NICEST)
                gl.glHint(gl.GL_PERSPECTIVE_CORRECTION_HINT, gl.GL_NICEST)
                gl.glLineWidth(2.0)
            else:
                gl.glEnable(gl.GL_MULTISAMPLE)
                gl.glEnable(gl.GL_LINE_SMOOTH)
                gl.glEnable(gl.GL_BLEND)
                gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
                gl.glHint(gl.GL_LINE_SMOOTH_HINT, gl.GL_NICEST)

            gl.glClearColor(0.95, 0.95, 0.95, 1.0)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)

            gl.glMatrixMode(gl.GL_PROJECTION)
            gl.glLoadIdentity()

            # Ajuste de bounds y aspect
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
            positions = to_numpy(particles_data['positions'])
            types = to_numpy(particles_data['types'])
            sizes = to_numpy(particles_data['sizes'])
            num_sides_array = to_numpy(particles_data['num_sides'])
            angles = particles_data.get('angles', None)
            angles = to_numpy(angles) if angles is not None else None
            if angles is None or len(angles) != len(positions):
                import numpy as np
                angles = np.zeros(len(positions), dtype=np.float32)

            gl.glEnable(gl.GL_BLEND)
            gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
            gl.glEnable(gl.GL_POINT_SMOOTH)

            for idx in range(len(positions)):
                pos = positions[idx]
                p_type = int(types[idx])
                p_size = float(sizes[idx])
                p_num_sides = int(num_sides_array[idx])
                p_angle = float(angles[idx])

                if p_type == 0:  # CIRCLE
                    color = [0.2, 0.4, 1.0, 0.9]
                    self._draw_circle_with_border(pos, p_size, color, self.circle_segments)
                elif p_type == 1:  # POLYGON
                    gl.glDisable(gl.GL_POINT_SMOOTH)
                    color = [0.0, 0.7, 0.3, 0.9]
                    self._draw_polygon_with_border(pos, p_size, p_num_sides, p_angle, color)
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

    def _draw_circle_with_border(self, pos, radius, color, num_segments=50):
        border_line_width = 3.0 if self.high_quality else 2.0
        gl.glLineWidth(border_line_width)
        gl.glColor4f(0, 0, 0, 1.0)
        gl.glBegin(gl.GL_LINE_LOOP)
        for i in range(num_segments):
            theta = 2.0 * math.pi * i / num_segments
            x = pos[0] + radius * math.cos(theta)
            y = pos[1] + radius * math.sin(theta)
            gl.glVertex2f(x, y)
        gl.glEnd()

        if self.high_quality:
            center_color = [min(1.0, color[0] * 1.1), min(1.0, color[1] * 1.1), min(1.0, color[2] * 1.1), color[3]]
            edge_color = [max(0.0, color[0] * 0.8), max(0.0, color[1] * 0.8), max(0.0, color[2] * 0.8), color[3]]
            gl.glBegin(gl.GL_TRIANGLE_FAN)
            gl.glColor4f(*center_color)
            gl.glVertex2f(pos[0], pos[1])
            gl.glColor4f(*edge_color)
            for i in range(num_segments + 1):
                theta = 2.0 * math.pi * i / num_segments
                x = pos[0] + radius * math.cos(theta)
                y = pos[1] + radius * math.sin(theta)
                gl.glVertex2f(x, y)
            gl.glEnd()
        else:
            gl.glColor4f(*color)
            gl.glBegin(gl.GL_TRIANGLE_FAN)
            gl.glVertex2f(pos[0], pos[1])
            for i in range(num_segments + 1):
                theta = 2.0 * math.pi * i / num_segments
                x = pos[0] + radius * math.cos(theta)
                y = pos[1] + radius * math.sin(theta)
                gl.glVertex2f(x, y)
            gl.glEnd()

    def _draw_polygon_with_border(self, pos, circum_radius, num_sides, angle, color):
        if num_sides < 3:
            return
        adjusted_radius = circum_radius

        gl.glLineWidth(2.0)
        gl.glColor4f(0, 0, 0, 1.0)
        gl.glBegin(gl.GL_LINE_LOOP)
        for i in range(num_sides):
            vertex_angle = 2.0 * math.pi * i / num_sides + angle
            x = pos[0] + adjusted_radius * math.cos(vertex_angle)
            y = pos[1] + adjusted_radius * math.sin(vertex_angle)
            gl.glVertex2f(x, y)
        gl.glEnd()

        gl.glColor4f(*color)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_sides + 1):
            vertex_angle = 2.0 * math.pi * i / num_sides + angle
            x = pos[0] + adjusted_radius * math.cos(vertex_angle)
            y = pos[1] + adjusted_radius * math.sin(vertex_angle)
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
        data = gl.glReadPixels(0, 0, self.width, self.height, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE)
        image = Image.frombytes("RGBA", (self.width, self.height), data)
        return image.transpose(Image.FLIP_TOP_BOTTOM)

# ------------------------------------------------------------------------------
def parse_intervals(intervals_str):
    """
    'a:b, c:d, e:f' -> lista de tuplas [(a,b), (c,d), (e,f)] con a<b.
    Soporta espacios; ignora entradas vacías.
    """
    spans = []
    if not intervals_str:
        return spans
    for chunk in intervals_str.split(','):
        chunk = chunk.strip()
        if not chunk:
            continue
        if ':' not in chunk:
            raise ValueError(f"Intervalo inválido: '{chunk}'. Debe ser 'ini:fin'.")
        a, b = chunk.split(':', 1)
        a = float(a.strip()); b = float(b.strip())
        if b < a:
            a, b = b, a
        spans.append((a, b))
    return spans

def time_in_intervals(t, spans):
    """Retorna True si el tiempo t está en alguno de los intervalos [a,b]."""
    for a, b in spans:
        if a <= t <= b:
            return True
    return False

# ------------------------------------------------------------------------------
def load_simulation_data(file_path, xp, min_time=-1.0, max_time=float('inf'),
                         frame_step=1, total_particles=250,
                         intervals=None):
    """
    Lectura robusta del CSV. Ahora permite:
      - Filtrar por min/max_time o por múltiples intervalos (si intervals no es None)
      - Backend xp: numpy (CPU) o cupy (GPU); si es numpy, no usa GPU.
    """
    frames = []
    frame_count = 0
    use_intervals = intervals is not None and len(intervals) > 0

    with open(file_path, 'r') as f:
        header_line = f.readline()
        header_parts = header_line.strip().split(',')
        indices = [int(m.group(1)) for part in header_parts
                   for m in [re.search(r"p(\d+)_x", part)] if m]
        header_particle_count = (max(indices) + 1) if indices else total_particles

        for line in f:
            parts = line.strip().split(',')
            if not parts:
                continue

            try:
                current_time = float(parts[0])
            except ValueError:
                continue

            # Filtrado por tiempo
            if use_intervals:
                if not time_in_intervals(current_time, intervals):
                    continue
            else:
                if current_time < min_time:
                    continue
                if current_time > max_time:
                    break

            if frame_count % frame_step != 0:
                frame_count += 1
                continue

            # Detectar rayos
            has_rays = ("rays_begin" in parts and "rays_end" in parts)
            if has_rays:
                rays_begin_idx = parts.index("rays_begin")
                rays_end_idx = parts.index("rays_end")
                fields_before_rays = rays_begin_idx
            else:
                rays_begin_idx = rays_end_idx = None
                fields_before_rays = len(parts)

            # Intentar determinar per_particle
            per_particle = None
            if header_particle_count > 0 and (fields_before_rays - 1) % header_particle_count == 0:
                per_particle = (fields_before_rays - 1) // header_particle_count
            else:
                if (fields_before_rays - 1) % 6 == 0:
                    per_particle = 6
                    header_particle_count = (fields_before_rays - 1) // 6
                elif (fields_before_rays - 1) % 5 == 0:
                    per_particle = 5
                    header_particle_count = (fields_before_rays - 1) // 5
                else:
                    per_particle = 6  # por defecto

            particle_positions = []
            particle_types = []
            particle_sizes = []
            particle_num_sides = []
            particle_angles = []

            for p in range(header_particle_count):
                base = 1 + p * per_particle
                if base + (per_particle - 1) >= fields_before_rays:
                    break
                try:
                    x = float(parts[base]); y = float(parts[base + 1])
                except (ValueError, IndexError):
                    break

                p_type = int(float(parts[base + 2])) if per_particle >= 3 else 0
                size = float(parts[base + 3]) if per_particle >= 4 else 0.0
                sides = int(float(parts[base + 4])) if per_particle >= 5 else 0
                angle = float(parts[base + 5]) if per_particle >= 6 else 0.0

                particle_positions.append([x, y])
                particle_types.append(p_type)
                particle_sizes.append(size)
                particle_num_sides.append(sides)
                particle_angles.append(angle)

            rays = []
            if has_rays and (rays_end_idx > rays_begin_idx + 1):
                ray_data = parts[rays_begin_idx + 1:rays_end_idx]
                for i in range(0, len(ray_data), 4):
                    try:
                        x1 = float(ray_data[i]); y1 = float(ray_data[i + 1])
                        x2 = float(ray_data[i + 2]); y2 = float(ray_data[i + 3])
                        rays.append([[x1, y1], [x2, y2]])
                    except (ValueError, IndexError):
                        continue

            if particle_positions or rays:
                frame_data = {
                    'time': current_time,
                    'particles': {
                        'positions': xp.array(particle_positions, dtype=xp.float32),
                        'types': xp.array(particle_types, dtype=xp.int32),
                        'sizes': xp.array(particle_sizes, dtype=xp.float32),
                        'num_sides': xp.array(particle_num_sides, dtype=xp.int32),
                        'angles': xp.array(particle_angles, dtype=xp.float32)
                    },
                    'rays': rays
                }
                frames.append(frame_data)

            frame_count += 1

    return frames

# ------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Renderizador de simulación de silo (CPU o GPU CuPy) con intervalos de tiempo")
    # NUEVO: control CPU/GPU e intervalos
    parser.add_argument('--cpu', action='store_true', help='Forzar modo CPU (usar numpy). Si no se pasa, intenta CuPy y cae a numpy si no hay GPU.')
    parser.add_argument('--intervals', type=str, default=None,
                        help='Lista de intervalos de tiempo "ini:fin, ini2:fin2, ...". Reemplaza a min/max.')
    parser.add_argument('--intervals-file', type=str, default=None,
                        help='Archivo de texto con intervalos "ini:fin" uno por línea. Reemplaza a min/max.')
    # Rango simple
    parser.add_argument('--min-time', type=float, default=-1.0,
                       help='Tiempo inicial a renderizar (s)')
    parser.add_argument('--max-time', type=float, default=float('inf'),
                       help='Tiempo final a renderizar (s)')
    parser.add_argument('--frame-step', type=int, default=1,
                       help='Saltar frames (ej. 2 para cada 2 frames)')
    parser.add_argument('--target-video-duration', type=float, default=None,
                       help='Duración objetivo del video en s (calcula frame-step)')
    parser.add_argument('--data-path', required=True,
                       help='Archivo CSV con datos de simulación')
    parser.add_argument('--output-dir', default='output_frames',
                       help='Directorio para guardar frames PNG')
    parser.add_argument('--video-output', default='simulation.mp4',
                       help='Nombre del archivo de video de salida')
    parser.add_argument('--no-video', action='store_true',
                       help='No generar el video, solo los frames')
    parser.add_argument('--fps', type=int, default=60,
                       help='FPS del video')
    parser.add_argument('--base-radius', type=float, default=0.5,
                       help='Radio base partículas grandes (m)')
    parser.add_argument('--size-ratio', type=float, default=0.4,
                       help='Radio pequeño/Radio grande (0-1)')
    parser.add_argument('--chi', type=float, default=0.2,
                       help='Fracción de partículas pequeñas (0-1)')
    parser.add_argument('--num-large-circles', type=int, default=0,
                       help='Número de partículas circulares grandes')
    parser.add_argument('--num-small-circles', type=int, default=0,
                       help='Número de partículas circulares pequeñas')
    parser.add_argument('--num-polygon-particles', type=int, default=0,
                       help='Número de partículas poligonales')
    parser.add_argument('--total-particles', type=int, default=1000,
                       help='Total de partículas si no hay conteos explícitos')
    parser.add_argument('--silo-height', type=float, default=11.70,
                       help='Altura del silo (m)')
    parser.add_argument('--silo-width', type=float, default=2.6,
                       help='Ancho del silo (m)')
    parser.add_argument('--outlet-width', type=float, default=0.3056,
                       help='Ancho total del outlet (m)')
    parser.add_argument('--width', type=int, default=1920,
                       help='Ancho px')
    parser.add_argument('--height', type=int, default=2560,
                       help='Alto px')
    parser.add_argument('--hd', action='store_true', help='1280x1024')
    parser.add_argument('--full-hd', action='store_true', help='1920x1536')
    parser.add_argument('--4k', action='store_true', help='3840x3072')
    parser.add_argument('--8k', action='store_true', help='7680x6144')
    parser.add_argument('--high-quality', action='store_true',
                       help='Antialiasing mejorado, más segmentos')
    parser.add_argument('--debug', action='store_true', help='Modo depuración')

    args = parser.parse_args()

    xp, using_cupy = get_array_backend(force_cpu=args.cpu)
    print(f"Backend arrays: {'CuPy (GPU)' if using_cupy else 'NumPy (CPU)'}")

    if args.hd:
        args.width, args.height = 1280, 1024
        print("Resolución: 1280x1024")
    elif args.full_hd:
        args.width, args.height = 1920, 1536
        print("Resolución: 1920x1536")
    elif getattr(args, '4k', False):
        args.width, args.height = 3840, 3072
        print("Resolución: 3840x3072 (4K)")
    elif getattr(args, '8k', False):
        args.width, args.height = 7680, 6144
        print("Resolución: 7680x6144 (8K)")
    else:
        print(f"Resolución personalizada: {args.width}x{args.height}")

    if args.high_quality:
        print("Alta calidad activada")
    else:
        print("Calidad estándar")

    # Preparar intervalos
    intervals = []
    if args.intervals_file:
        with open(args.intervals_file, 'r') as finv:
            lines = [ln.strip() for ln in finv if ln.strip()]
        intervals = parse_intervals(','.join(lines))
    elif args.intervals:
        intervals = parse_intervals(args.intervals)

    if intervals:
        print(f"Usando {len(intervals)} intervalos de tiempo:")
        for a, b in intervals:
            print(f"  - [{a}, {b}] s")
    else:
        print(f"Rango simple: {args.min_time} s -> {args.max_time} s")

    try:
        os.makedirs(args.output_dir, exist_ok=True)

        # Cálculo automático de frame-step si se pidió y NO hay intervalos
        if args.target_video_duration is not None and not intervals:
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
                calculated_frame_step = max(1, int(total_frames / max(1, int(target_frames_in_video))))
                print("Estadísticas:")
                print(f"  - Duración sim.: {sim_duration:.3f} s")
                print(f"  - Frames totales: {total_frames}")
                print(f"  - FPS objetivo: {args.fps}")
                print(f"  - Duración objetivo video: {args.target_video_duration:.3f} s")
                print(f"  - frame-step calculado: {calculated_frame_step}")
                args.frame_step = calculated_frame_step
            else:
                print("No se pudo calcular frame_step automáticamente (usando el provisto).")

        # Mezcla de partículas
        if args.num_large_circles > 0 or args.num_small_circles > 0 or args.num_polygon_particles > 0:
            total_particles = args.num_large_circles + args.num_small_circles + args.num_polygon_particles
            print(f"Conteos explícitos: Grandes={args.num_large_circles}, Pequeñas={args.num_small_circles}, Polígonos={args.num_polygon_particles}")
        else:
            args.num_large_circles = int((1.0 - args.chi) * args.total_particles)
            args.num_small_circles = int(args.chi * args.total_particles)
            total_particles = args.num_large_circles + args.num_small_circles
            print(f"Mezcla por chi: Grandes={args.num_large_circles}, Pequeñas={args.num_small_circles}")

        # Carga de frames con filtros / intervalos
        frames = load_simulation_data(
            file_path=args.data_path,
            xp=xp,
            min_time=args.min_time,
            max_time=args.max_time,
            frame_step=args.frame_step,
            total_particles=total_particles,
            intervals=intervals
        )
        print(f"Frames cargados: {len(frames)}")

        # Render
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

        # Limpieza de archivos vacíos (por si acaso)
        subprocess.run(['find', args.output_dir, '-type', 'f', '-size', '0c', '-delete'], check=False)

        if not args.no_video and len(frames) > 0:
            subprocess.run([
                'ffmpeg', '-y', '-framerate', str(args.fps),
                '-i', f'{args.output_dir}/frame_%05d.png',
                '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
                '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',
                args.video_output
            ], check=True)
            print(f"Video generado: {args.video_output}")
        elif len(frames) == 0:
            print("No hay frames para generar video")
        else:
            print("Se omitió la generación de video por --no-video.")

    except Exception as e:
        print(f"Error: {str(e)}")
        traceback.print_exc()

if __name__ == "__main__":
    main()

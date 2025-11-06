#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import math
import argparse
import subprocess
import traceback
from tqdm import tqdm

# --- backend arrays -----------------------------------------------------------
def get_array_backend(force_cpu: bool = False):
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
    import numpy as np
    try:
        import cupy as cp
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
                 silo_height=11.70, silo_width=2.6, outlet_width=0.3056,
                 high_quality=False):
        self.width = width
        self.height = height
        self.high_quality = high_quality

        if high_quality:
            self.circle_segments = 100
        else:
            self.circle_segments = 50

        self.large_particle_radius = base_radius
        self.small_particle_radius = base_radius * size_ratio
        self.debug_mode = True
        self.silo_height = silo_height
        self.silo_width = silo_width
        self.wall_thickness = 0.1
        self.ground_level_y = 0.0
        self.outlet_x_half_width = outlet_width / 2.0
        self._glut_initialized = False
        self._init_opengl()

    def _init_opengl(self):
        if not self._glut_initialized:
            glut.glutInit()
            self._glut_initialized = True

        display_mode = glut.GLUT_DOUBLE | glut.GLUT_RGBA
        try:
            if self.high_quality:
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

        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_LINEAR)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_LINEAR)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_S, gl.GL_CLAMP_TO_EDGE)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_T, gl.GL_CLAMP_TO_EDGE)

        gl.glFramebufferTexture2D(gl.GL_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0,
                                  gl.GL_TEXTURE_2D, self.texture, 0)

        if gl.glCheckFramebufferStatus(gl.GL_FRAMEBUFFER) != gl.GL_FRAMEBUFFER_COMPLETE:
            raise RuntimeError("Framebuffer incompleto")

        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)

    def render_frame(self, frame_data):
        try:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)
            gl.glViewport(0, 0, self.width, self.height)

            gl.glEnable(gl.GL_MULTISAMPLE)
            gl.glEnable(gl.GL_LINE_SMOOTH)
            gl.glEnable(gl.GL_BLEND)
            gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)

            gl.glClearColor(0.95, 0.95, 0.95, 1.0)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)

            gl.glMatrixMode(gl.GL_PROJECTION)
            gl.glLoadIdentity()

            # límites y aspecto
            target_world_left = -(self.silo_width / 2.0) - self.wall_thickness
            target_world_right = (self.silo_width / 2.0) + self.wall_thickness
            target_world_bottom = self.ground_level_y - self.wall_thickness
            target_world_top = self.silo_height + self.wall_thickness

            margin_particle = max(self.large_particle_radius,
                                  self.small_particle_radius,
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

            # círculos y polígonos
            if 'circles' in frame_data and frame_data['circles'].size > 0:
                self._draw_circles(frame_data['circles'])
            if 'polygons' in frame_data and len(frame_data['polygons']) > 0:
                self._draw_polygons(frame_data['polygons'])

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

    def _draw_circles(self, circles_np):
        circles = to_numpy(circles_np)
        gl.glEnable(gl.GL_BLEND)
        gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
        for cx, cy, r in circles:
            color = [0.2, 0.4, 1.0, 0.9]
            self._draw_circle_with_border((float(cx), float(cy)), float(r), color, self.circle_segments)
        gl.glDisable(gl.GL_BLEND)

    def _draw_polygons(self, polygons_list):
        gl.glEnable(gl.GL_BLEND)
        gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)

        for poly_np in polygons_list:
            poly = to_numpy(poly_np)
            if poly.shape[0] < 3:  # se necesitan al menos 3 vértices
                continue
            # borde
            gl.glLineWidth(2.0)
            gl.glColor4f(0, 0, 0, 1.0)
            gl.glBegin(gl.GL_LINE_LOOP)
            for x, y in poly:
                gl.glVertex2f(float(x), float(y))
            gl.glEnd()
            # relleno
            gl.glColor4f(0.0, 0.7, 0.3, 0.9)
            gl.glBegin(gl.GL_TRIANGLE_FAN)
            cx = float(poly[:,0].mean()); cy = float(poly[:,1].mean())
            gl.glVertex2f(cx, cy)
            for x, y in poly:
                gl.glVertex2f(float(x), float(y))
            x0, y0 = poly[0]
            gl.glVertex2f(float(x0), float(y0))
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

        gl.glColor4f(*color)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_segments + 1):
            theta = 2.0 * math.pi * i / num_segments
            x = pos[0] + radius * math.cos(theta)
            y = pos[1] + radius * math.sin(theta)
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
    for a, b in spans:
        if a <= t <= b:
            return True
    return False

# ------------------------------------------------------------------------------
import csv

def load_simulation_data(file_path, xp,
                         min_time=-1.0, max_time=float('inf'),
                         frame_step=1, intervals=None,
                         poly_num_sides=None,
                         num_circles=None, num_polygons=None):
    """
    Soporta:
    - Formato NUEVO con marcadores SOLO en el header:
        Header: Time,circles_begin,circles_end,polygons_begin,polygons_end
        Row:    t, cx,cy,r, cx,cy,r, ..., [vacío], x1,y1,..., xN,yN, x1,y1,..., [vacío]
      En este modo usamos los contadores --num-circles/--num-polygons para partir la fila.
    - (Fallback) Formato VIEJO por partícula: x,y,type,size,sides,angle ... (convierte a vértices).

    Requisitos:
      * Para polígonos en el nuevo formato se necesita --poly-sides = N (≥3).
      * Si los marcadores están solo en el header, **pasá** --num-circles y --num-polygons.
    """

    def time_in_ranges(t):
        if intervals:
            for a, b in intervals:
                if a <= t <= b:
                    return True
            return False
        return (t >= min_time) and (t <= max_time)

    frames = []
    idx = 0

    with open(file_path, 'r', newline='') as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header is None:
            return frames

        header = [h.strip() for h in header]
        has_header_markers = all(tok in header for tok in
                                 ['Time', 'circles_begin', 'circles_end', 'polygons_begin', 'polygons_end'])

        # Indices del header (si existen)
        if has_header_markers:
            h_time = header.index('Time')
            h_cb   = header.index('circles_begin')
            h_ce   = header.index('circles_end')
            h_pb   = header.index('polygons_begin')
            h_pe   = header.index('polygons_end')

        for row in reader:
            # NO eliminamos '' deliberadamente; mantenemos alineación
            parts = [p.strip() for p in row]

            # --- Intento 1: Formato NUEVO con marcadores solo en HEADER ---
            if has_header_markers and num_circles is not None and num_polygons is not None:
                try:
                    t = float(parts[h_time])
                except (ValueError, IndexError):
                    continue

                if not time_in_ranges(t):
                    if t > max_time:
                        break
                    else:
                        continue

                if idx % frame_step != 0:
                    idx += 1
                    continue

                # Layout asumido:
                # [0] Time
                # [1 .. 1+3*num_circles-1] triples de círculos
                # luego polígonos en pares (2*N por polígono)
                # + hay celdas '' “de separación” según exporte el C++

                # Extraer círculos (tres valores por círculo) justo después de Time
                start_c = 1
                end_c   = start_c + 3 * num_circles
                csec = parts[start_c:end_c]

                circles = []
                for i in range(0, len(csec), 3):
                    try:
                        cx = float(csec[i]); cy = float(csec[i+1]); r = float(csec[i+2])
                        circles.append((cx, cy, r))
                    except (ValueError, IndexError):
                        break

                # El resto (ignorando separadores vacíos iniciales) son vértices de polígonos
                psec = parts[end_c:]
                # Si hay separadores '' al principio, saltarlos
                while psec and psec[0] == '':
                    psec = psec[1:]

                polys = []
                if num_polygons and poly_num_sides and poly_num_sides >= 3:
                    stride = 2 * poly_num_sides
                    usable = (len(psec) // stride) * stride
                    for j in range(0, usable, stride):
                        chunk = psec[j:j+stride]
                        verts = []
                        ok = True
                        for k in range(0, stride, 2):
                            try:
                                x = float(chunk[k]); y = float(chunk[k+1])
                            except (ValueError, IndexError):
                                ok = False; break
                            verts.append((x, y))
                        if ok and len(verts) >= 3:
                            polys.append(verts)
                else:
                    # Sin info suficiente para reconstruir polígonos
                    polys = []

                frames.append({
                    'time': t,
                    'circles': xp.array(circles, dtype=xp.float32) if circles else xp.zeros((0,3), dtype=xp.float32),
                    'polygons': [xp.array(poly, dtype=xp.float32) for poly in polys],
                })
                idx += 1
                continue

            # --- Intento 2: Formato NUEVO con marcadores en cada fila (raro, pero lo soportamos) ---
            try:
                i_cb = parts.index('circles_begin')
                i_ce = parts.index('circles_end')
                i_pb = parts.index('polygons_begin')
                i_pe = parts.index('polygons_end')

                t = float(parts[0])
                if not time_in_ranges(t):
                    if t > max_time:
                        break
                    else:
                        continue
                if idx % frame_step != 0:
                    idx += 1
                    continue

                csec = parts[i_cb+1:i_ce]
                psec = parts[i_pb+1:i_pe]

                circles = []
                for i in range(0, len(csec), 3):
                    try:
                        cx = float(csec[i]); cy = float(csec[i+1]); r = float(csec[i+2])
                        circles.append((cx, cy, r))
                    except (ValueError, IndexError):
                        break

                polys = []
                if poly_num_sides and poly_num_sides >= 3:
                    stride = 2 * poly_num_sides
                    usable = (len(psec) // stride) * stride
                    for j in range(0, usable, stride):
                        chunk = psec[j:j+stride]
                        verts = []
                        ok = True
                        for k in range(0, stride, 2):
                            try:
                                x = float(chunk[k]); y = float(chunk[k+1])
                            except (ValueError, IndexError):
                                ok = False; break
                            verts.append((x, y))
                        if ok and len(verts) >= 3:
                            polys.append(verts)

                frames.append({
                    'time': t,
                    'circles': xp.array(circles, dtype=xp.float32) if circles else xp.zeros((0,3), dtype=xp.float32),
                    'polygons': [xp.array(poly, dtype=xp.float32) for poly in polys],
                })
                idx += 1
                continue

            except ValueError:
                # No hay tokens en la fila → probamos formato viejo
                pass

            # --- Intento 3: Fallback Formato VIEJO (por partícula) ---
            try:
                t = float(parts[0])
            except ValueError:
                continue
            if not time_in_ranges(t):
                if t > max_time:
                    break
                else:
                    continue
            if idx % frame_step != 0:
                idx += 1
                continue

            # Inferir bloques por partícula (6,5,4)
            per_particle = None
            for cand in (6, 5, 4):
                if (len(parts) - 1) % cand == 0:
                    per_particle = cand
                    break
            if per_particle is None:
                idx += 1
                continue

            count = (len(parts) - 1) // per_particle
            circles = []
            polys = []
            for p in range(count):
                base = 1 + p * per_particle
                try:
                    x = float(parts[base]); y = float(parts[base + 1])
                    p_type = int(float(parts[base + 2])) if per_particle >= 3 else 0
                    size   = float(parts[base + 3])      if per_particle >= 4 else 0.0
                    sides  = int(float(parts[base + 4])) if per_particle >= 5 else 0
                    angle  = float(parts[base + 5])      if per_particle >= 6 else 0.0
                except (ValueError, IndexError):
                    break

                if p_type == 0:
                    circles.append((x, y, size))
                else:
                    if sides < 3 or size <= 0.0:
                        continue
                    verts = []
                    for i in range(sides):
                        theta = 2.0 * math.pi * i / sides + angle
                        vx = x + size * math.cos(theta)
                        vy = y + size * math.sin(theta)
                        verts.append((vx, vy))
                    polys.append(verts)

            frames.append({
                'time': t,
                'circles': xp.array(circles, dtype=xp.float32) if circles else xp.zeros((0,3), dtype=xp.float32),
                'polygons': [xp.array(poly, dtype=xp.float32) for poly in polys],
            })
            idx += 1

    return frames



# ------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Renderizador de silo: círculos (cx,cy,r) y polígonos por vértices")
    parser.add_argument('--cpu', action='store_true', help='Forzar NumPy.')
    parser.add_argument('--intervals', type=str, default=None, help='Intervalos "ini:fin, ..."')
    parser.add_argument('--intervals-file', type=str, default=None, help='Archivo con intervalos uno por línea')
    parser.add_argument('--min-time', type=float, default=-1.0)
    parser.add_argument('--max-time', type=float, default=float('inf'))
    parser.add_argument('--frame-step', type=int, default=1)
    parser.add_argument('--target-video-duration', type=float, default=None)

    parser.add_argument('--data-path', required=True)
    parser.add_argument('--output-dir', default='output_frames')
    parser.add_argument('--video-output', default='simulation.mp4')
    parser.add_argument('--no-video', action='store_true')
    parser.add_argument('--fps', type=int, default=60)

    parser.add_argument('--base-radius', type=float, default=0.5)
    parser.add_argument('--size-ratio', type=float, default=0.4)
    parser.add_argument('--silo-height', type=float, default=11.70)
    parser.add_argument('--silo-width', type=float, default=2.6)
    parser.add_argument('--outlet-width', type=float, default=0.3056)

    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=2560)
    parser.add_argument('--hd', action='store_true')
    parser.add_argument('--full-hd', action='store_true')
    parser.add_argument('--4k', action='store_true')
    parser.add_argument('--8k', action='store_true')
    parser.add_argument('--high-quality', action='store_true')
    parser.add_argument('--debug', action='store_true')

    parser.add_argument('--poly-sides', type=int, default=None, help='N lados por polígono (requerido).')
    parser.add_argument('--num-circles', type=int, default=None,
                    help='Cantidad de círculos por frame (si los marcadores están solo en el header).')
    parser.add_argument('--num-polygons', type=int, default=None,
                    help='Cantidad de polígonos por frame (si los marcadores están solo en el header).')


    args = parser.parse_args()

    xp, using_cupy = get_array_backend(force_cpu=args.cpu)
    print(f"Backend arrays: {'CuPy (GPU)' if using_cupy else 'NumPy (CPU)'}")

    if args.hd:
        args.width, args.height = 1280, 1024
    elif args.full_hd:
        args.width, args.height = 1920, 1536
    elif getattr(args, '4k', False):
        args.width, args.height = 3840, 3072
    elif getattr(args, '8k', False):
        args.width, args.height = 7680, 6144
    print(f"Resolución: {args.width}x{args.height}")
    print("Alta calidad" if args.high_quality else "Calidad estándar")

    intervals = []
    if args.intervals_file:
        with open(args.intervals_file, 'r') as finv:
            lines = [ln.strip() for ln in finv if ln.strip()]
        intervals = parse_intervals(','.join(lines))
    elif args.intervals:
        intervals = parse_intervals(args.intervals)

    if intervals:
        print(f"Intervalos: {intervals}")
    else:
        print(f"Rango: {args.min_time} -> {args.max_time} s")

    try:
        os.makedirs(args.output_dir, exist_ok=True)

        # Cálculo rápido de frame-step si se pidió
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
                print("No se pudo calcular frame_step automáticamente (se usa el provisto).")

        frames = load_simulation_data(
            file_path=args.data_path,
            xp=xp,
            min_time=args.min_time,
            max_time=args.max_time,
            frame_step=args.frame_step,
            intervals=intervals,
            poly_num_sides=args.poly_sides,
            num_circles=args.num_circles,
            num_polygons=args.num_polygons
        )

        print(f"Frames cargados: {len(frames)}")

        renderer = SiloRenderer(
            width=args.width,
            height=args.height,
            base_radius=args.base_radius,
            size_ratio=args.size_ratio,
            silo_height=args.silo_height,
            silo_width=args.silo_width,
            outlet_width=args.outlet_width,
            high_quality=args.high_quality
        )
        renderer.debug_mode = args.debug

        print("Renderizando frames...")
        for i, frame in enumerate(tqdm(frames, disable=not args.debug)):
            image = renderer.render_frame(frame)
            if image is not None:
                image.save(f"{args.output_dir}/frame_{i:05d}.png")

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

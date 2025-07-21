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
                 total_particles=250, chi=0.2):
        """Inicializa el renderizador con configuración básica y parámetros de simulación."""
        self.width = width
        self.height = height
        self.particle_scale = 150  # Factor de escala para partículas (no usado directamente en el dibujo de círculos)
        self.min_particle_size = 5  # Tamaño mínimo en píxeles (no usado directamente en el dibujo de círculos)

        # Parámetros de partículas de la simulación C++
        self.large_particle_radius = base_radius
        self.small_particle_radius = base_radius * size_ratio
        self.num_small_particles = int(chi * total_particles)
        self.num_large_particles = total_particles - self.num_small_particles

        self.debug_mode = True
        self.particle_border = 2 # Este valor ahora es menos relevante con el nuevo método de dibujo

        # Constantes de geometría del silo (de silo_simulator.cpp)
        self.silo_height = 6.36
        self.silo_width = 1.0
        self.wall_thickness = 0.01
        self.ground_level_y = 0.0
        # CORRECCIÓN IMPORTANTE: Alinear con el valor de silo_simulator.cpp
        self.outlet_x_half_width = 2.3 * 0.065 # Esto es 0.1495, el valor exacto de silo_simulator.cpp

        # Bandera para asegurar que glut.glutInit() se llama solo una vez
        self._glut_initialized = False

        # Inicialización de OpenGL
        self._init_opengl()

    def _init_opengl(self):
        """Configuración inicial de OpenGL y framebuffer"""
        try:
            # Inicialización GLUT
            if not self._glut_initialized:
                glut.glutInit()
                self._glut_initialized = True

            glut.glutInitDisplayMode(glut.GLUT_DOUBLE | glut.GLUT_RGBA)
            glut.glutInitWindowSize(self.width, self.height)
            if hasattr(self, 'window') and self.window:
                glut.glutDestroyWindow(self.window)
            self.window = glut.glutCreateWindow(b"Silo Simulation")
            glut.glutHideWindow()  # Modo sin ventana

            # Configuración de framebuffer
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

    def render_frame(self, particles):
        """Renderiza un frame completo con paredes y partículas"""
        try:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)
            gl.glViewport(0, 0, self.width, self.height)

            gl.glClearColor(0.95, 0.95, 0.95, 1.0)  # Fondo gris claro
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)

            gl.glMatrixMode(gl.GL_PROJECTION)
            gl.glLoadIdentity()

            # --- CÁLCULO DE LA PROYECCIÓN CON ASPECT RATIO CORREGIDO ---
            # Calcular el rango de mundo deseado para el contenido principal (el silo)
            target_world_left = -(self.silo_width / 2.0) - self.wall_thickness # Borde exterior de la pared izquierda
            target_world_right = (self.silo_width / 2.0) + self.wall_thickness # Borde exterior de la pared derecha
            target_world_bottom = self.ground_level_y - self.wall_thickness # Base del suelo
            target_world_top = self.silo_height + self.wall_thickness # Parte superior de las paredes

            # Añadir márgenes para asegurar que las partículas no se corten en los bordes y espacio de visualización
            margin_particle = max(self.large_particle_radius, self.small_particle_radius)
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

            # Ajustar los límites de la proyección para mantener la relación de aspecto
            if screen_aspect > world_aspect:
                # La pantalla es relativamente más ancha, ajustar el ancho del mundo
                adjusted_world_width = world_view_height * screen_aspect
                x_center = (view_left + view_right) / 2.0
                left_bound = x_center - adjusted_world_width / 2.0
                right_bound = x_center + adjusted_world_width / 2.0
                bottom_bound = view_bottom
                top_bound = view_top
            else:
                # La pantalla es relativamente más alta, ajustar la altura del mundo
                adjusted_world_height = world_view_width / screen_aspect
                y_center = (view_bottom + view_top) / 2.0
                bottom_bound = y_center - adjusted_world_height / 2.0
                top_bound = y_center + adjusted_world_height / 2.0
                left_bound = view_left
                right_bound = view_right

            glu.gluOrtho2D(left_bound, right_bound, bottom_bound, top_bound)
            # --- FIN DEL CÁLCULO DE PROYECCIÓN ---

            gl.glMatrixMode(gl.GL_MODELVIEW)
            gl.glLoadIdentity()

            # Dibujar elementos
            self._draw_walls()

            if particles is not None:
                self._draw_particles(particles)

            if self.debug_mode:
                self._draw_debug_info()

            # Capturar imagen
            image = self._capture_image()
            return image

        except Exception as e:
            print(f"Error en render_frame: {str(e)}")
            traceback.print_exc()
            return None

        finally:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)

    def _draw_walls(self):
        """Renderiza las paredes del silo de fondo plano con constantes de C++."""
        try:
            wall_color = [0.4, 0.4, 0.4, 0.9] # Color de las paredes
            ground_color = [0.3, 0.3, 0.3, 0.9] # Color del suelo

            # Pared izquierda (vertical)
            # Su borde izquierdo real es -(silo_width / 2.0) - wall_thickness
            self._draw_rectangle(
                -(self.silo_width / 2.0) - self.wall_thickness, # X de inicio
                self.ground_level_y, # Y de inicio (la base de la pared está en ground_level_y)
                self.wall_thickness, # Ancho real
                self.silo_height,    # Alto real
                wall_color
            )

            # Pared derecha (vertical)
            # Su borde izquierdo real es (silo_width / 2.0)
            self._draw_rectangle(
                self.silo_width / 2.0, # X de inicio
                self.ground_level_y, # Y de inicio
                self.wall_thickness, # Ancho real
                self.silo_height,    # Alto real
                wall_color
            )

            # Fondo izquierdo (parte del suelo a la izquierda de la abertura)
            # Su borde izquierdo real es -self.silo_width / 2.0
            # Su ancho real es (self.silo_width / 2.0 - self.outlet_x_half_width)
            self._draw_rectangle(
                -self.silo_width / 2.0, # X de inicio
                self.ground_level_y - self.wall_thickness, # Y de inicio (dibuja la base hacia abajo desde ground_level_y)
                (self.silo_width / 2.0 - self.outlet_x_half_width), # Ancho real
                self.wall_thickness, # Alto real
                ground_color
            )

            # Fondo derecho (parte del suelo a la derecha de la abertura)
            # Su borde izquierdo real es self.outlet_x_half_width
            # Su ancho real es (self.silo_width / 2.0 - self.outlet_x_half_width)
            self._draw_rectangle(
                self.outlet_x_half_width, # X de inicio
                self.ground_level_y - self.wall_thickness, # Y de inicio
                (self.silo_width / 2.0 - self.outlet_x_half_width), # Ancho real
                self.wall_thickness, # Alto real
                ground_color
            )

        except Exception as e:
            print(f"Error dibujando paredes: {str(e)}")
            traceback.print_exc()

    def _draw_particles(self, particles_data):
        """Dibuja partículas como círculos con tamaño físico exacto y borde."""
        try:
            # Asegurarse de que particles_data['positions'] sea un numpy array para el procesamiento en CPU
            positions = particles_data['positions'].get() if hasattr(particles_data['positions'], 'get') else particles_data['positions']

            gl.glEnable(gl.GL_BLEND)
            gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
            gl.glEnable(gl.GL_POINT_SMOOTH)

            valid_mask = positions[:,1] > -1.5
            positions = positions[valid_mask]

            if self.debug_mode:
                print(f"Total partículas visibles: {len(positions)}")

            for i, pos in enumerate(positions):
                if i < self.num_large_particles:
                    current_radius = self.large_particle_radius
                    color = [0.2, 0.4, 1.0, 0.9]  # Azul
                else:
                    current_radius = self.small_particle_radius
                    color = [1.0, 0.5, 0.2, 0.9]  # Naranja

                self._draw_circle_with_border(pos, current_radius, color)

        except Exception as e:
            print(f"Error dibujando partículas: {str(e)}")
            traceback.print_exc()
        finally:
            gl.glDisable(gl.GL_BLEND)

    def _draw_circle_with_border(self, pos, radius, color, num_segments=30):
        """Dibuja un círculo con un borde negro para una partícula."""
        border_thickness_world_units = 0.005

        # Dibujar el círculo del borde (ligeramente más grande y negro)
        gl.glColor4f(0, 0, 0, 0.8)
        gl.glBegin(gl.GL_TRIANGLE_FAN)
        gl.glVertex2f(pos[0], pos[1])
        for i in range(num_segments + 1):
            theta = 2.0 * math.pi * i / num_segments
            x = pos[0] + (radius + border_thickness_world_units) * math.cos(theta)
            y = pos[1] + (radius + border_thickness_world_units) * math.sin(theta)
            gl.glVertex2f(x, y)
        gl.glEnd()

        # Dibujar el círculo de la partícula (color principal)
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
        """Dibuja un rectángulo simple"""
        gl.glBegin(gl.GL_QUADS)
        gl.glColor4f(*color)
        gl.glVertex2f(x, y)
        gl.glVertex2f(x + width, y)
        gl.glVertex2f(x + width, y + height)
        gl.glVertex2f(x, y + height)
        gl.glEnd()

    def _draw_debug_info(self):
        """Dibuja información de debug en la escena"""
        # Ejes coordenados
        gl.glBegin(gl.GL_LINES)
        gl.glColor4f(1, 0, 0, 0.5)  # Eje X (rojo)
        gl.glVertex2f(-50, 0)
        gl.glVertex2f(50, 0)
        gl.glColor4f(0, 1, 0, 0.5)  # Eje Y (verde)
        gl.glVertex2f(0, -50)
        gl.glVertex2f(0, 50)
        gl.glEnd()

    def _capture_image(self):
        """Captura la imagen del framebuffer"""
        gl.glReadBuffer(gl.GL_COLOR_ATTACHMENT0)
        data = gl.glReadPixels(0, 0, self.width, self.height,
                                 gl.GL_RGBA, gl.GL_UNSIGNED_BYTE)
        image = Image.frombytes("RGBA", (self.width, self.height), data)
        return image.transpose(Image.FLIP_TOP_BOTTOM)

def load_simulation_data(file_path):
    """Carga los datos de simulación desde el archivo CSV con el nuevo formato."""
    frames = []

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            parts = list(map(float, line.split(',')))

            if len(parts) < 1:
                continue

            current_time = parts[0]

            particle_positions = []
            for i in range(1, len(parts), 2):
                if i + 1 < len(parts):
                    particle_positions.append([parts[i], parts[i+1]])

            if particle_positions:
                frames.append({
                    'time': current_time,
                    'particles': {
                        'positions': cp.array(particle_positions),
                    }
                })
            else:
                frames.append({
                    'time': current_time,
                    'particles': {
                        'positions': cp.array([]),
                    }
                })

    return frames

def main():
    parser = argparse.ArgumentParser(description="Renderizador de simulación de silo")
    parser.add_argument('--data-path', required=True, help='Archivo de datos de simulación')
    parser.add_argument('--output-dir', default='output_frames', help='Directorio para frames')
    parser.add_argument('--video-output', default='simulation.mp4', help='Archivo de video de salida')
    parser.add_argument('--frame-step', type=int, default=1, help='Saltar frames')
    parser.add_argument('--fps', type=int, default=60, help='Cuadros por segundo para el video')
    parser.add_argument('--debug', action='store_true', help='Habilitar modo debug')
    parser.add_argument('--max-duration', type=float, default=15.0, help='Duración máxima de la simulación a renderizar en segundos') # Nuevo argumento

    # Argumentos para parámetros de simulación
    parser.add_argument('--base-radius', type=float, default=0.065, help='Radio base de las partículas grandes')
    parser.add_argument('--size-ratio', type=float, default=0.4, help='Relación de tamaño (radio pequeño / radio grande)')
    parser.add_argument('--total-particles', type=int, default=250, help='Número total de partículas')
    parser.add_argument('--chi', type=float, default=0.2, help='Fracción de partículas pequeñas (CHI)')

    args = parser.parse_args()

    try:
        os.makedirs(args.output_dir, exist_ok=True)
        print("Cargando datos de simulación...")
        frames = load_simulation_data(args.data_path)
        print(f"Se cargaron {len(frames)} frames")

        # Filtrar frames para renderizar solo hasta la duración máxima
        # Se asegura de que se considere el frame_step y la duración máxima.
        # Primero, se aplica el frame_step para reducir el número de frames a iterar.
        # Luego, se filtra por tiempo.
        selected_frames_for_duration = []
        for i in range(0, len(frames), args.frame_step):
            frame = frames[i]
            if frame['time'] <= args.max_duration:
                selected_frames_for_duration.append(frame)
            else:
                # Una vez que superamos la duración máxima, no necesitamos verificar más frames
                break

        print(f"Se renderizarán {len(selected_frames_for_duration)} frames hasta {args.max_duration} segundos.")

        # Pasar los parámetros al renderizador
        renderer = SiloRenderer(
            base_radius=args.base_radius,
            size_ratio=args.size_ratio,
            total_particles=args.total_particles,
            chi=args.chi
        )
        renderer.debug_mode = args.debug

        print("Renderizando frames...")
        # Iterar sobre los frames ya filtrados
        for i, frame in enumerate(tqdm(selected_frames_for_duration, disable=not args.debug)):
            if frame['particles']['positions'].size > 0:
                image = renderer.render_frame(frame['particles'])
                if image is not None:
                    image.save(f"{args.output_dir}/frame_{i:05d}.png")
            elif args.debug:
                print(f"Frame {i}: No hay partículas para renderizar. Saltando.")

        clean_command = [
            'find', args.output_dir, '-type', 'f', '-size', '0c', '-delete'
        ]
        subprocess.run(clean_command, check=False)

        print("Generando video...")
        subprocess.run([
            'ffmpeg', '-y', '-framerate', str(args.fps),
            '-i', f'{args.output_dir}/frame_%05d.png',
            '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
            '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',
            '-preset', 'slow', '-crf', '18',
            args.video_output
        ], check=True)

        print(f"✅ Simulación renderizada correctamente en {args.video_output}")

    except Exception as e:
        print(f"❌ Error: {str(e)}")
        traceback.print_exc()

if __name__ == "__main__":
    main()
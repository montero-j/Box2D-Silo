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
    def __init__(self, width=1200, height=1600, large_particle_size=None, small_particle_size=None):
        """Inicializa el renderizador con configuración básica"""
        self.width = width
        self.height = height
        self.particle_scale = 150  # Factor de escala para partículas
        self.min_particle_size = 5  # Tamaño mínimo en píxeles
        self.large_particle_size=0.25
        self.small_particle_size=0.1
        self.debug_mode = True
        self.particle_border = 2
        
        # Inicialización de OpenGL
        self._init_opengl()
        
    def _init_opengl(self):
        """Configuración inicial de OpenGL y framebuffer"""
        try:
            # Inicialización GLUT
            glut.glutInit()
            glut.glutInitDisplayMode(glut.GLUT_DOUBLE | glut.GLUT_RGBA)
            glut.glutInitWindowSize(self.width, self.height)
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

    def render_frame(self, walls, particles):
        """Renderiza un frame completo con paredes y partículas"""
        try:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.framebuffer)
            gl.glViewport(0, 0, self.width, self.height)
            
            # Configuración de vista
            gl.glClearColor(0.95, 0.95, 0.95, 1.0)  # Fondo gris claro
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)
            
            gl.glMatrixMode(gl.GL_PROJECTION)
            gl.glLoadIdentity()
            glu.gluOrtho2D(-6, 6, -1, 20)  # Área visible ajustada
            gl.glMatrixMode(gl.GL_MODELVIEW)
            gl.glLoadIdentity()
            
            # Dibujar elementos
            if walls is not None:
                self._draw_walls(walls)
            
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

    def _draw_walls(self, walls):
        """Renderiza las paredes del silo"""
        try:
            walls_cpu = walls.get() if hasattr(walls, 'get') else walls
            
            # Plataforma izquierda
            self._draw_rectangle(
                walls_cpu[0] - 2.3, walls_cpu[1] - 0.5, 4.6, 1.0,
                [0.3, 0.3, 0.3, 0.9]
            )
            
            # Plataforma derecha
            self._draw_rectangle(
                walls_cpu[2] - 2.3, walls_cpu[3] - 0.5, 4.6, 1.0,
                [0.3, 0.3, 0.3, 0.9]
            )
            
            # Pared izquierda
            self._draw_rectangle(
                walls_cpu[4] - 0.5, walls_cpu[5] - 15.0, 1.0, 30.0,
                [0.4, 0.4, 0.4, 0.9]
            )
            
            # Pared derecha
            self._draw_rectangle(
                walls_cpu[6] - 0.5, walls_cpu[7] - 15.0, 1.0, 30.0,
                [0.4, 0.4, 0.4, 0.9]
            )
            
        except Exception as e:
            print(f"Error dibujando paredes: {str(e)}")
            traceback.print_exc()

    def _draw_particles(self, particles):
        try:
            types = particles['types'].get() if hasattr(particles['types'], 'get') else particles['types']
            positions = particles['positions'].get() if hasattr(particles['positions'], 'get') else particles['positions']
            sizes = particles['sizes'].get() if hasattr(particles['sizes'], 'get') else particles['sizes']
            
            gl.glEnable(gl.GL_BLEND)
            gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
            gl.glEnable(gl.GL_POINT_SMOOTH)
            
            # Filtrar partículas visibles
            valid_mask = positions[:,1] > -0.25
            positions = positions[valid_mask]
            sizes = sizes[valid_mask]
            
            if self.debug_mode:
                unique_sizes = np.unique(sizes)
                print(f"\nTamaños únicos: {unique_sizes}")
                print(f"Rango de tamaños: {np.min(sizes)} a {np.max(sizes)}")
                print(f"Total partículas visibles: {len(positions)}")
            
            # Definir umbral para grandes/pequeñas
            size_threshold = np.median(sizes)  # Usamos la mediana como umbral
            
            # Dibujar partículas grandes (azules)
            large_mask = sizes > size_threshold
            if np.any(large_mask):
                self._draw_particle_batch(
                    positions[large_mask],
                    sizes[large_mask],
                    [0.2, 0.4, 1.0, 0.9]  # Azul
                )
            
            # Dibujar partículas pequeñas (naranjas)
            small_mask = sizes <= size_threshold
            if np.any(small_mask):
                self._draw_particle_batch(
                    positions[small_mask],
                    sizes[small_mask],
                    [1.0, 0.5, 0.2, 0.9]  # Naranja
                )
                
        except Exception as e:
            print(f"Error dibujando partículas: {str(e)}")
            traceback.print_exc()
        finally:
            gl.glDisable(gl.GL_BLEND)
        

    def _draw_particle_batch(self, positions, sizes, color):
        """Dibuja partículas como círculos con tamaño físico exacto y borde."""
        num_segments = 30

        # Disminuye este valor. Un rango de 0.005 a 0.01 podría ser mejor.
        border_thickness_world_units = 0.01  # <-- ¡PRUEBA CON ESTE VALOR!

        for pos, size in zip(positions, sizes):
            display_radius = max(size, 0.005) # Mantén este mínimo si quieres

            # --- Dibujar el círculo del borde (ligeramente más grande y negro) ---
            gl.glColor4f(0, 0, 0, 0.8) # Color negro para el borde
            gl.glBegin(gl.GL_TRIANGLE_FAN)
            gl.glVertex2f(pos[0], pos[1]) # Centro
            for i in range(num_segments + 1):
                theta = 2.0 * math.pi * i / num_segments
                x = pos[0] + (display_radius + border_thickness_world_units) * math.cos(theta)
                y = pos[1] + (display_radius + border_thickness_world_units) * math.sin(theta)
                gl.glVertex2f(x, y)
            gl.glEnd()

            # --- Dibujar el círculo de la partícula (color principal) ---
            gl.glColor4f(*color) # Color de la partícula
            gl.glBegin(gl.GL_TRIANGLE_FAN)
            gl.glVertex2f(pos[0], pos[1]) # Centro
            for i in range(num_segments + 1):
                theta = 2.0 * math.pi * i / num_segments
                x = pos[0] + display_radius * math.cos(theta)
                y = pos[1] + display_radius * math.sin(theta)
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
    """Carga los datos de simulación desde el archivo"""
    frames = []
    current_frame = None
    
    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            
            if line.startswith("Walls"):
                if current_frame is not None:
                    frames.append(current_frame)
                parts = list(map(float, line.split()[1:]))
                current_frame = {
                    'walls': cp.array(parts),
                    'particles': []
                }
            elif line == "EndFrame":
                if current_frame is not None:
                    frames.append(current_frame)
                current_frame = None
            elif current_frame is not None and line:
                parts = line.split()
                if len(parts) >= 5:
                    current_frame['particles'].append({
                        'type': int(parts[0]),
                        'position': [float(parts[1]), float(parts[2])],
                        'angle': float(parts[3]),
                        'size': float(parts[4])
                    })
    
    # Convertir partículas a arrays al final
    for frame in frames:
        if frame['particles']:
            frame['particles'] = {
                'types': cp.array([p['type'] for p in frame['particles']]),
                'positions': cp.array([p['position'] for p in frame['particles']]),
                'angles': cp.array([p['angle'] for p in frame['particles']]),
                'sizes': cp.array([p['size'] for p in frame['particles']])
            }
    
    return frames

def main():
    parser = argparse.ArgumentParser(description="Renderizador de simulación de silo")
    parser.add_argument('--data-path', required=True, help='Archivo de datos de simulación')
    parser.add_argument('--output-dir', default='output_frames', help='Directorio para frames')
    parser.add_argument('--video-output', default='simulation.mp4', help='Archivo de video de salida')
    parser.add_argument('--frame-step', type=int, default=1, help='Saltar frames')
    parser.add_argument('--fps', type=int, default=60, help='Cuadros por segundo para el video')
    parser.add_argument('--debug', action='store_true', help='Habilitar modo debug')
    
    args = parser.parse_args()
    
    try:
        os.makedirs(args.output_dir, exist_ok=True)
        print("Cargando datos de simulación...")
        frames = load_simulation_data(args.data_path)
        print(f"Se cargaron {len(frames)} frames")
        
        renderer = SiloRenderer()
        renderer.debug_mode = args.debug
        
        print("Renderizando frames...")
        selected_frames = frames[::args.frame_step]
        
        for i, frame in enumerate(tqdm(selected_frames, disable=not args.debug)):
            if frame['particles']:
                image = renderer.render_frame(frame['walls'], frame['particles'])
                if image is not None:
                    image.save(f"{args.output_dir}/frame_{i:05d}.png")
        
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
        
    except Exception as e:  # Asegúrate de que este bloque except existe
        print(f"❌ Error: {str(e)}")
        traceback.print_exc()

if __name__ == "__main__":
    main()

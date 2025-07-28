#!/usr/bin/env python3
"""
Script para ejecutar estudios de avalanchas por forma geométrica específica.
Guarda datos completos para renderizado y visualización.

Autor: GitHub Copilot
Fecha: 2025-07-25
"""

import argparse
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
import pandas as pd
import glob


class ShapeSpecificRunner:
    """Ejecutor de estudios de avalanchas para forma específica con datos para renderizado"""
    
    def __init__(self, shape_name, base_radius=0.065, outlet_width=0.26, target_avalanches=500, 
                 simulation_time=150, particles=2000):
        self.shape_name = shape_name.lower()
        self.base_radius = base_radius
        self.outlet_width = outlet_width
        self.target_avalanches = target_avalanches
        self.simulation_time = simulation_time
        self.particles = particles
        
        # Mapeo de formas a parámetros del simulador
        self.shape_params = {
            "circles": {"--polygon": 0, "--sides": 0},
            "triangles": {"--polygon": 1, "--sides": 3}, 
            "squares": {"--polygon": 1, "--sides": 4},
            "pentagons": {"--polygon": 1, "--sides": 5},
            "hexagons": {"--polygon": 1, "--sides": 6}
        }
        
        if self.shape_name not in self.shape_params:
            raise ValueError(f"Forma no soportada: {self.shape_name}")
        
        # Estado
        self.current_avalanches = 0
        self.simulation_count = 0
        self.start_time = time.time()
        
        # Directorio específico para esta forma
        self.results_dir = Path(f"shape_study_results_{self.shape_name}")
        self.results_dir.mkdir(exist_ok=True)
        
        # Archivos de estado y logs
        self.state_file = self.results_dir / "study_state.json"
        self.log_file = self.results_dir / "study_log.txt"
        self.progress_file = self.results_dir / "progress_log.csv"
        self.consolidated_flow_file = self.results_dir / "consolidated_flow_data.csv"
        self.consolidated_avalanche_file = self.results_dir / "consolidated_avalanche_data.csv"
        self.render_data_file = self.results_dir / "render_simulation_data.json"
        
        # Parámetros base para el simulador
        self.base_params = {
            "--base-radius": self.base_radius,
            "--outlet-width": self.outlet_width,
            "--particles": self.particles,
            "--time": self.simulation_time,
            "--output-interval": 1.0,
            "--save-flow": 1,
            "--save-avalanches": 1,
            "--save-particles": 1,  # ¡IMPORTANTE! Guardar posiciones para renderizado
            **self.shape_params[self.shape_name]
        }
        
        # Inicializar archivos
        self.initialize_files()
        
    def initialize_files(self):
        """Inicializar archivos de resultados y logs"""
        # Log principal
        if not self.log_file.exists():
            with open(self.log_file, "w") as f:
                f.write(f"=== ESTUDIO DE AVALANCHAS - {self.shape_name.upper()} ===\n")
                f.write(f"Iniciado: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Forma: {self.shape_name}\n")
                f.write(f"Base radius: {self.base_radius} m\n")
                f.write(f"Outlet width: {self.outlet_width} m\n")
                f.write(f"Tiempo por simulación: {self.simulation_time} s\n")
                f.write(f"Partículas: {self.particles}\n")
                f.write(f"Objetivo: {self.target_avalanches} avalanchas\n")
                f.write("="*50 + "\n\n")
        
        # Archivo de progreso
        if not self.progress_file.exists():
            with open(self.progress_file, "w") as f:
                f.write("Timestamp,SimulationNumber,AvalanchesThisSim,CumulativeAvalanches,ElapsedTimeHours,AvgAvalanchesPerSim\n")
        
        # Archivo de datos para renderizado
        if not self.render_data_file.exists():
            render_info = {
                "shape": self.shape_name,
                "base_radius": self.base_radius,
                "outlet_width": self.outlet_width,
                "target_avalanches": self.target_avalanches,
                "simulation_time": self.simulation_time,
                "particles": self.particles,
                "shape_parameters": self.shape_params[self.shape_name],
                "simulations_for_render": [],
                "created": time.strftime('%Y-%m-%d %H:%M:%S')
            }
            with open(self.render_data_file, 'w') as f:
                json.dump(render_info, f, indent=2)
        
    def log(self, message):
        """Registrar mensaje en log y consola"""
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        log_message = f"[{timestamp}] [{self.shape_name.upper()}] {message}"
        print(log_message)
        
        with open(self.log_file, "a") as f:
            f.write(log_message + "\n")
    
    def save_state(self):
        """Guardar estado actual para recuperación"""
        state = {
            "shape_name": self.shape_name,
            "target_avalanches": self.target_avalanches,
            "current_avalanches": self.current_avalanches,
            "simulation_count": self.simulation_count,
            "start_time": self.start_time,
            "base_radius": self.base_radius,
            "outlet_width": self.outlet_width,
            "simulation_time": self.simulation_time,
            "particles": self.particles
        }
        with open(self.state_file, "w") as f:
            json.dump(state, f, indent=2)
    
    def load_state(self):
        """Cargar estado desde archivo si existe"""
        if self.state_file.exists():
            try:
                with open(self.state_file, "r") as f:
                    state = json.load(f)
                    self.current_avalanches = state.get("current_avalanches", 0)
                    self.simulation_count = state.get("simulation_count", 0)
                    self.start_time = state.get("start_time", time.time())
                    self.log(f"Estado recuperado: {self.current_avalanches} avalanchas, {self.simulation_count} simulaciones")
                    return True
            except Exception as e:
                self.log(f"Error cargando estado: {e}")
        return False
    
    def count_avalanches_in_simulation(self, sim_dir):
        """Contar avalanchas en una simulación específica"""
        avalanche_file = Path(sim_dir) / "avalanche_data.csv"
        
        if not avalanche_file.exists():
            self.log(f"Advertencia: No se encontró {avalanche_file}")
            return 0
        
        try:
            # Contar líneas que empiecen con "Avalancha"
            with open(avalanche_file, 'r') as f:
                content = f.read()
                avalanche_count = content.count('Avalancha ')
                self.log(f"Archivo {avalanche_file.name}: {avalanche_count} avalanchas")
                return avalanche_count
        except Exception as e:
            self.log(f"Error leyendo avalanche_data.csv: {e}")
            return 0
    
    def run_single_simulation(self):
        """Ejecutar una simulación individual"""
        self.simulation_count += 1
        
        # Limpiar simulaciones anteriores
        sim_dirs = glob.glob("./simulations/sim_*")
        for d in sim_dirs:
            shutil.rmtree(d)
        
        # Construir comando
        cmd = ["./bin/silo_simulator"]
        for param, value in self.base_params.items():
            cmd.extend([param, str(value)])
        
        self.log(f"🚀 Simulación #{self.simulation_count} ({self.simulation_time}s)")
        self.log(f"🔧 Comando: {' '.join(cmd)}")
        
        try:
            # Ejecutar simulación
            start_time = time.time()
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=400)  # Más tiempo por partículas
            end_time = time.time()
            
            if result.returncode != 0:
                self.log(f"❌ Error: {result.stderr}")
                return 0
            
            self.log(f"✅ Completada en {end_time - start_time:.1f}s")
            
            # Encontrar directorio de simulación
            sim_dirs = glob.glob("./simulations/sim_*")
            if not sim_dirs:
                self.log("❌ No se encontraron directorios de simulación")
                return 0
            
            # Usar el directorio más reciente
            latest_dir = Path(max(sim_dirs, key=os.path.getctime))
            avalanches = self.count_avalanches_in_simulation(latest_dir)
            
            # Guardar archivos de esta simulación
            if avalanches > 0:
                self.save_simulation_results(latest_dir, avalanches)
            
            return avalanches
            
        except subprocess.TimeoutExpired:
            self.log(f"⏰ Simulación #{self.simulation_count} excedió tiempo límite")
            return 0
        except Exception as e:
            self.log(f"❌ Error: {e}")
            return 0
    
    def save_simulation_results(self, sim_dir, avalanches):
        """Guardar resultados de simulación con datos para renderizado"""
        try:
            # Crear directorio para esta simulación
            sim_backup_dir = self.results_dir / f"sim_{self.simulation_count:04d}"
            sim_backup_dir.mkdir(exist_ok=True)
            
            # Copiar TODOS los archivos para posible renderizado
            for file_pattern in ["*.csv", "*.txt", "*.json"]:
                for file_path in sim_dir.glob(file_pattern):
                    dest_path = sim_backup_dir / file_path.name
                    shutil.copy2(file_path, dest_path)
                    self.log(f"📁 Copiado: {file_path.name}")
            
            # Consolidar datos principales
            self.consolidate_simulation_data(sim_dir, avalanches)
            
            # Actualizar información de renderizado
            self.update_render_info(sim_backup_dir, avalanches)
            
        except Exception as e:
            self.log(f"❌ Error guardando resultados: {e}")
    
    def consolidate_simulation_data(self, sim_dir, avalanches):
        """Consolidar datos principales"""
        try:
            # Consolidar flow data
            flow_file = Path(sim_dir) / "flow_data.csv"
            if flow_file.exists():
                df_flow = pd.read_csv(flow_file)
                df_flow['simulation'] = self.simulation_count
                df_flow['shape'] = self.shape_name
                df_flow['base_radius'] = self.base_radius
                df_flow['outlet_width'] = self.outlet_width
                
                if self.consolidated_flow_file.exists():
                    df_flow.to_csv(self.consolidated_flow_file, mode='a', header=False, index=False)
                else:
                    df_flow.to_csv(self.consolidated_flow_file, index=False)
                
                self.log(f"📈 Flow data consolidado: {len(df_flow)} registros")
            
            # Consolidar avalanche data
            avalanche_file = Path(sim_dir) / "avalanche_data.csv"
            if avalanche_file.exists():
                df_avalanche = pd.read_csv(avalanche_file)
                df_avalanche['simulation'] = self.simulation_count
                df_avalanche['shape'] = self.shape_name
                df_avalanche['base_radius'] = self.base_radius
                df_avalanche['outlet_width'] = self.outlet_width
                
                if self.consolidated_avalanche_file.exists():
                    df_avalanche.to_csv(self.consolidated_avalanche_file, mode='a', header=False, index=False)
                else:
                    df_avalanche.to_csv(self.consolidated_avalanche_file, index=False)
                
                self.log(f"🏔️ Avalanche data consolidado: {len(df_avalanche)} registros")
            
            # Registrar progreso
            self.record_progress(avalanches)
            
        except Exception as e:
            self.log(f"❌ Error consolidando: {e}")
    
    def update_render_info(self, sim_backup_dir, avalanches):
        """Actualizar información para renderizado"""
        try:
            # Leer info actual
            with open(self.render_data_file, 'r') as f:
                render_info = json.load(f)
            
            # Agregar información de esta simulación
            sim_info = {
                "simulation_number": self.simulation_count,
                "avalanches": avalanches,
                "cumulative_avalanches": self.current_avalanches + avalanches,
                "timestamp": time.strftime('%Y-%m-%d %H:%M:%S'),
                "data_directory": str(sim_backup_dir.relative_to(self.results_dir)),
                "files_available": {
                    "flow_data": (sim_backup_dir / "flow_data.csv").exists(),
                    "avalanche_data": (sim_backup_dir / "avalanche_data.csv").exists(),
                    "simulation_data": (sim_backup_dir / "simulation_data.csv").exists()
                }
            }
            
            render_info["simulations_for_render"].append(sim_info)
            render_info["last_updated"] = time.strftime('%Y-%m-%d %H:%M:%S')
            render_info["total_avalanches"] = self.current_avalanches + avalanches
            render_info["total_simulations"] = self.simulation_count
            
            # Guardar info actualizada
            with open(self.render_data_file, 'w') as f:
                json.dump(render_info, f, indent=2)
                
        except Exception as e:
            self.log(f"❌ Error actualizando render info: {e}")
    
    def record_progress(self, avalanches_this_sim):
        """Registrar progreso en archivo CSV"""
        elapsed_hours = (time.time() - self.start_time) / 3600
        avg_avalanches = self.current_avalanches / self.simulation_count if self.simulation_count > 0 else 0
        
        with open(self.progress_file, "a") as f:
            f.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')},{self.simulation_count},{avalanches_this_sim},"
                   f"{self.current_avalanches},{elapsed_hours:.3f},{avg_avalanches:.3f}\n")
    
    def run_study(self):
        """Ejecutar estudio completo"""
        self.log("🚀 INICIANDO ESTUDIO DE AVALANCHAS")
        self.log(f"🎯 Forma: {self.shape_name.upper()}")
        self.log(f"🎯 Objetivo: {self.target_avalanches} avalanchas")
        self.log(f"⏱️ Tiempo por simulación: {self.simulation_time} segundos")
        self.log(f"🔧 Base radius: {self.base_radius} m")
        self.log(f"🕳️ Outlet width: {self.outlet_width} m")
        self.log(f"🔵 Partículas: {self.particles}")
        
        # Cargar estado previo si existe
        if self.load_state():
            self.log(f"🔄 Continuando desde: {self.current_avalanches}/{self.target_avalanches} avalanchas")
        
        try:
            while self.current_avalanches < self.target_avalanches:
                remaining = self.target_avalanches - self.current_avalanches
                self.log(f"\n📊 PROGRESO: {self.current_avalanches}/{self.target_avalanches} avalanchas ({remaining} restantes)")
                
                # Ejecutar simulación
                avalanches = self.run_single_simulation()
                
                # Actualizar contador
                self.current_avalanches += avalanches
                
                # Guardar estado
                self.save_state()
                
                elapsed_hours = (time.time() - self.start_time) / 3600
                self.log(f"📈 Total acumulado: {self.current_avalanches} avalanchas en {elapsed_hours:.2f}h")
                
                # Verificar si completamos
                if self.current_avalanches >= self.target_avalanches:
                    break
                
                time.sleep(1)  # Pausa breve
            
            # Resumen final
            total_time = (time.time() - self.start_time) / 3600
            self.log(f"\n🎉 ESTUDIO COMPLETADO - {self.shape_name.upper()}!")
            self.log(f"✅ {self.current_avalanches} avalanchas acumuladas")
            self.log(f"📊 {self.simulation_count} simulaciones ejecutadas")
            self.log(f"⏱️ Tiempo total: {total_time:.2f} horas")
            self.log(f"📈 Promedio: {self.current_avalanches/self.simulation_count:.1f} avalanchas/sim")
            self.log(f"📁 Resultados en: {self.results_dir.absolute()}")
            self.log(f"🎬 Datos para renderizado: {self.render_data_file}")
            
            return True
            
        except KeyboardInterrupt:
            self.log(f"\n⏸️ Interrumpido por usuario")
            self.save_state()
            return False
        except Exception as e:
            self.log(f"❌ Error: {e}")
            self.save_state()
            return False


def main():
    parser = argparse.ArgumentParser(description="Ejecutar estudio de avalanchas para forma específica")
    parser.add_argument("shape", choices=["circles", "triangles", "squares", "pentagons", "hexagons"],
                       help="Forma geométrica a simular")
    parser.add_argument("--base-radius", type=float, default=0.065, help="Radio base (m)")
    parser.add_argument("--outlet-width", type=float, default=0.26, help="Ancho de salida (m)")
    parser.add_argument("--target", type=int, default=500, help="Objetivo de avalanchas")
    parser.add_argument("--time", type=int, default=150, help="Tiempo por simulación (s)")
    parser.add_argument("--particles", type=int, default=2000, help="Número de partículas")
    parser.add_argument("--resume", action="store_true", help="Continuar estudio interrumpido")
    
    args = parser.parse_args()
    
    # Verificar que existe el ejecutable
    if not Path("bin/silo_simulator").exists():
        print("❌ ERROR: No se encontró bin/silo_simulator")
        return 1
    
    print(f"🚀 === ESTUDIO DE {args.shape.upper()} ===")
    print(f"🎯 Objetivo: {args.target} avalanchas")
    print(f"⏱️ Tiempo por simulación: {args.time} segundos")
    print(f"🔧 Base radius: {args.base_radius} m")
    print(f"🕳️ Outlet width: {args.outlet_width} m")
    print(f"🔵 Partículas: {args.particles}")
    print()
    
    # Crear y ejecutar runner
    runner = ShapeSpecificRunner(
        shape_name=args.shape,
        base_radius=args.base_radius,
        outlet_width=args.outlet_width,
        target_avalanches=args.target,
        simulation_time=args.time,
        particles=args.particles
    )
    
    success = runner.run_study()
    return 0 if success else 1


if __name__ == "__main__":
    exit(main())

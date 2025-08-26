# Box2D-Silo

Simulador de avalanchas granulares en silos usando Box2D. Permite estudiar cómo la forma de las partículas afecta el flujo y la formación de avalanchas.

## Estructura

```
├── bin/                # Ejecutable principal (silo_simulator)
├── src/                # Código fuente C++
├── box2d/              # Motor de física Box2D
├── analysis/           # Scripts Python para análisis y visualización
├── docs/               # Documentación y artículos
├── parametros_sims/    # Archivos de parámetros
```

1. Compila el simulador:
   ```bash
   make
   ```
2. Ejecuta una simulación básica:
   ```bash
   ./bin/silo_simulator --base-radius 0.065 --outlet-width 0.26 --particles 2000 --time 150
   ```
3. Ejecuta simulaciones usando archivos de parámetros:
   ```bash
   ./ejecutar_simulacion_1.sh parametros_ejemplo_1.txt
   ```
4. Analiza resultados:
   ```bash
   python3 analysis/avalanche_distribution.py --input simulations/sim_XXXX/
   ```


## Estructura del Proyecto

```
├── src/                     # Código fuente C++
│   └── silo_hexagono.cpp      # Simulador principal
├── bin/                     # Ejecutables compilados
│   └── silo_simulator         # Simulador principal
├── box2d/                   # Motor de física Box2D
├── scripts/                 # Scripts de automatización
│   ├── run_shape_study.py     # Estudios por forma específica
│   └── run_all_shapes_study.sh # Estudios completos
├── analysis/                # Scripts de análisis Python
│   ├── avalanche_distribution.py    # Análisis de distribuciones
│   ├── combine_distributions.py     # Combinación de datos
│   ├── render_simulation.py         # Renderización visual
│   └── verify_calculations.py       # Verificación de cálculos
├── data/                    # Datos y resultados
│   ├── simulations/           # Datos de simulaciones individuales
│   └── shape_study_results_*/ # Resultados consolidados
├── docs/                    # Documentación
│   ├── ARCHITECTURE.md        # Arquitectura del proyecto
│   ├── Goldberg-J.Stat.Mech.2018.pdf # Artículo de referencia
│   └── comandos.txt           # Comandos de ejemplo
├── ejecutar_simulacion.sh   # Sistema de ejecución con parámetros de archivo
├── parametros_ejemplo.txt    # Archivo de ejemplo de parámetros
├── simulacion_pequena.txt    # Configuración de simulación pequeña
└── simulacion_comandos.txt   # Configuración basada en comandos predefinidos
```


### Prerrequisitos

```bash
# Dependencias del sistema (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential cmake g++ python3 python3-pip

# Dependencias Python
pip3 install pandas numpy matplotlib opencv-python PyOpenGL PyOpenGL_accelerate
```

### Compilación

```bash
# Clonar el repositorio
git clone <repository-url>
cd Box2D-Silo-Polygons

# Compilar Box2D
cd box2d
./build.sh
cd ..

# Compilar el simulador
make clean
make

# Verificar compilación
ls bin/silo_simulator  # Debe existir
```

##  Uso del Simulador

### 1. Simulación Individual

```bash
# Simulación básica con círculos
./bin/silo_simulator --base-radius 0.065 --outlet-width 0.26 --particles 2000 --time 150

# Simulación con hexágonos
./bin/silo_simulator --polygon 1 --sides 6 --base-radius 0.065 --outlet-width 0.26 --particles 2000

# Parámetros principales:
#   --base-radius: Radio base de partículas (m)
#   --outlet-width: Ancho de salida del silo (m)
#   --particles: Número total de partículas
#   --time: Tiempo de simulación (s)
#   --polygon: 1 para polígonos, 0 para círculos
#   --sides: Número de lados del polígono
```

### 2. Estudios por Forma Específica

```bash
# Estudio de círculos (500 avalanchas)
python3 scripts/run_shape_study.py circles --target 500 --time 150

# Estudio de hexágonos con parámetros específicos
python3 scripts/run_shape_study.py hexagons --base-radius 0.05 --outlet-width 0.39 --target 1000

# Continuar estudio interrumpido
python3 scripts/run_shape_study.py squares --resume
```

**Parámetros disponibles:**
- `shape`: circles, triangles, squares, pentagons, hexagons
- `--target`: Número objetivo de avalanchas
- `--base-radius`: Radio base (m)
- `--outlet-width`: Ancho salida (m)
- `--particles`: Número de partículas
- `--time`: Tiempo por simulación (s)

### 3. Estudio Completo de Todas las Formas

```bash
# Ejecutar estudio completo (todas las formas)
./scripts/run_all_shapes_study.sh

# Esto ejecutará automáticamente:
# - circles: 500 avalanchas
# - triangles: 500 avalanchas
# - squares: 500 avalanchas
# - pentagons: 500 avalanchas
# - hexagons: 500 avalanchas
```

### 4. Ejemplos Rápidos

```bash
# Ejemplo rápido con Makefile
make example

# Test básico del sistema
make test

# Estudio específico de 100 avalanchas
make study-circles

# Análisis de resultados
make analyze
```

### 4. Replicación del Estudio Goldberg

```bash
# Replicar parámetros exactos de Goldberg et al. (2018)
python3 run_goldberg_study.py

# Parámetros Goldberg:
# - D/d = 3.9 (outlet_width = 0.39m, base_radius = 0.05m)
# - 2000 partículas
# - 3000 avalanchas objetivo
# - Expectativa: ~50 partículas/avalancha
```

## Sistema de Ejecución con Parámetros de Archivo

### Descripción

El sistema incluye un script de bash (`ejecutar_simulacion.sh`) que permite ejecutar el `silo_simulator` usando parámetros definidos en archivos de texto, facilitando la configuración y reutilización de configuraciones de simulación.

### Archivos del Sistema

- `ejecutar_simulacion.sh` - Script principal que compila y ejecuta las simulaciones
- `parametros_ejemplo.txt` - Archivo de ejemplo con todos los parámetros disponibles
- `simulacion_pequena.txt` - Configuración para una simulación pequeña de prueba
- `simulacion_comandos.txt` - Configuración basada en comandos predefinidos

### Uso del Sistema de Parámetros

#### Uso básico
```bash
# Usar archivo por defecto (parametros_ejemplo.txt)
./ejecutar_simulacion.sh

# Especificar archivo de parámetros específico
./ejecutar_simulacion.sh simulacion_pequena.txt

# Ver ayuda del sistema
./ejecutar_simulacion.sh --help
```

#### Formato del archivo de parámetros

Los archivos de parámetros usan el formato `PARAMETRO=VALOR`:

```
# Comentarios empiezan con #
BASE_RADIUS=0.5
TOTAL_PARTICLES=2000
NUM_LARGE_CIRCLES=2000
NUM_SMALL_CIRCLES=0
NUM_POLYGON_PARTICLES=10
NUM_SIDES=3
CURRENT_SIM=1
TOTAL_SIMS=1
SAVE_SIM_DATA=1

# Parámetros opcionales
# SILO_HEIGHT=120.0
# SILO_WIDTH=20.2
# OUTLET_WIDTH=3.9
# MIN_TIME=-30.0
```

#### Parámetros soportados

| Parámetro | Descripción | Requerido |
|-----------|-------------|-----------|
| `BASE_RADIUS` | Radio base de las partículas | Sí |
| `TOTAL_PARTICLES` | Número total de partículas | Sí |
| `NUM_LARGE_CIRCLES` | Número de círculos grandes | Sí |
| `NUM_SMALL_CIRCLES` | Número de círculos pequeños | Sí |
| `NUM_POLYGON_PARTICLES` | Número de partículas poligonales | Sí |
| `NUM_SIDES` | Número de lados de los polígonos | Sí |
| `CURRENT_SIM` | Simulación actual | Sí |
| `TOTAL_SIMS` | Total de simulaciones | Sí |
| `SAVE_SIM_DATA` | Guardar datos (1 o 0) | Sí |
| `SILO_HEIGHT` | Altura del silo | Opcional |
| `SILO_WIDTH` | Ancho del silo | Opcional |
| `OUTLET_WIDTH` | Ancho de la salida | Opcional |
| `MIN_TIME` | Tiempo mínimo | Opcional |

#### Funcionalidades del script

1. **Compilación automática**: Verifica y compila Box2D si es necesario
2. **Validación**: Verifica que los archivos de parámetros existan
3. **Flexibilidad**: Permite parámetros opcionales
4. **Output colorizado**: Facilita seguir el progreso
5. **Manejo de errores**: Se detiene si hay problemas en la compilación o ejecución

#### Ejemplos de uso

```bash
# Simulación de prueba rápida (100 partículas)
./ejecutar_simulacion.sh simulacion_pequena.txt

# Simulación con configuración predefinida
./ejecutar_simulacion.sh simulacion_comandos.txt

# Crear tu propia configuración
cp parametros_ejemplo.txt mi_simulacion.txt
# Editar mi_simulacion.txt según necesidades
./ejecutar_simulacion.sh mi_simulacion.txt
```

## Análisis de Resultados

### Gestión de Datos de Simulaciones

El simulador genera datos en diferentes ubicaciones según cómo se ejecute:

```
UBICACIONES DE DATOS:

1. Simulación directa:
   ./bin/silo_simulator → ./simulations/sim_XXXX/

2. Ejemplos y demos:
   make example → examples/simulations/sim_XXXX/

3. Tests del sistema:
   make test → tests/simulations/sim_XXXX/

4. Estudios organizados:
   scripts/run_shape_study.py → data/shape_study_results_FORMA/
   scripts/run_shape_study.py → data/simulations/sim_XXXX/
```

#### Limpieza de Datos

```bash
# Limpiar TODOS los datos de simulaciones
make clean-data

# Limpiar solo datos temporales (ejemplos, tests)
make clean-temp

# Limpiar código compilado Y datos
make clean-all

# Limpieza manual específica
rm -rf simulations/                    # Simulaciones directas
rm -rf examples/simulations/sim_*      # Datos de ejemplos
rm -rf tests/simulations/sim_*         # Datos de tests
rm -rf data/shape_study_results_*      # Estudios completos
```

### Estructura de Datos Generados

## Ejecución rápida de ejemplos

Para correr una simulación de ejemplo, simplemente ejecuta uno de los siguientes comandos desde la raíz del proyecto:

```bash
./ejecutar_simulacion_1.sh parametros_ejemplo_1.txt
```
o
```bash
./ejecutar_simulacion_2.sh parametros_ejemplo_2.txt
```

Los resultados se guardarán automáticamente en la carpeta correspondiente.


### Análisis Estadístico

```bash
# Análisis de distribuciones de avalanchas
python3 analysis/avalanche_distribution.py --input data/shape_study_results_circles/

# Combinar datos de múltiples estudios
python3 analysis/combine_distributions.py --input-dir data/ --output combined_results.csv

# Verificar cálculos y estadísticas
python3 analysis/verify_calculations.py --data data/shape_study_results_circles/consolidated_avalanche_data.csv
```

### Renderización Visual

```bash
# Crear video de simulación
python3 analysis/render_simulation.py \
    --data-path data/simulations/sim_XXXX/simulation_data.csv \
    --video-output avalancha_hexagons.mp4 \
    --target-video-duration 10 \
    --total-particles 2000
```

## Estudios de Formas Geométricas

### Parámetros Estándar

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Partículas | 2000 | Número total de partículas |
| Radio base | 0.065 m | Radio de partículas circulares |
| Ancho salida | 0.26 m | Ancho de salida (D/d ≈ 4.0) |
| Tiempo sim | 150 s | Duración por simulación |
| Objetivo | 500 | Avalanchas por forma |


## Métricas y Análisis

### Detección de Avalanchas

El simulador detecta avalanchas automáticamente basándose en:
- **Umbral de flujo**: < 0.1 partículas/segundo
- **Tiempo de bloqueo**: 5.0 segundos
- **Recuperación**: Detección de nuevo flujo

### Métricas Principales

- **Tamaño de avalancha**: Número de partículas por evento
- **Frecuencia**: Avalanchas por unidad de tiempo
- **Tiempo entre avalanchas**: Intervalo temporal
- **Tasa de flujo**: Partículas por segundo
- **Tiempo de bloqueo**: Duración de estancamiento


## Configuración

### Parámetros del Simulador

```cpp
// Parámetros físicos (src/silo_hexagono.cpp)
float density = 0.01f;         // Densidad kg/m²
float friction = 0.5f;         // Coeficiente fricción
float restitution = 0.05f;     // Coeficiente restitución
float gravity = 9.81f;         // Gravedad m/s²
```

### Modificar Geometría del Silo

```cpp
// Dimensiones del silo
float silo_width = 2.6f;       // Ancho interno (m)
float silo_height = 7.8f;      // Altura (m)
float wall_thickness = 0.1f;   // Grosor paredes (m)
```

### Personalizar Detección de Avalanchas

```cpp
// Umbrales de detección
float flow_threshold = 0.1f;           // part/s
float blocking_threshold = 5.0f;       // segundos
float output_interval = 1.0f;          // intervalo guardado
```

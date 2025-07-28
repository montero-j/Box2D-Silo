# Box2D Silo Polygons - Simulador de Avalanchas Granulares

## 📖 Descripción

Este proyecto es un simulador de silos granulares construido con Box2D que estudia el comportamiento de avalanchas de partículas con diferentes formas geométricas. El simulador permite analizar cómo la forma de las partículas (círculos, triángulos, cuadrados, pentágonos, hexágonos) afecta el flujo y la formación de avalanchas en un silo.

## 🎯 Características Principales

- **Simulación física realista** usando Box2D
- **Múltiples formas geométricas**: círculos, triángulos, cuadrados, pentágonos, hexágonos
- **Análisis de avalanchas** automático con detección inteligente
- **Estudios paramétricos** para diferentes configuraciones
- **Replicación de estudios Goldberg** et al. (2018)
- **Renderización visual** de simulaciones
- **Análisis estadístico** completo de resultados

## 🏗️ Estructura del Proyecto

```
├── 📁 src/                     # Código fuente C++
│   └── silo_hexagono.cpp      # Simulador principal
├── 📁 bin/                     # Ejecutables compilados
│   └── silo_simulator         # Simulador principal
├── 📁 box2d/                   # Motor de física Box2D
├── 📁 scripts/                 # Scripts de automatización
│   ├── run_shape_study.py     # Estudios por forma específica
│   └── run_all_shapes_study.sh # Estudios completos
├── 📁 analysis/                # Scripts de análisis Python
│   ├── avalanche_distribution.py    # Análisis de distribuciones
│   ├── combine_distributions.py     # Combinación de datos
│   ├── render_simulation.py         # Renderización visual
│   └── verify_calculations.py       # Verificación de cálculos
├── 📁 data/                    # Datos y resultados
│   ├── simulations/           # Datos de simulaciones individuales
│   └── shape_study_results_*/ # Resultados consolidados
├── 📁 docs/                    # Documentación
│   ├── ARCHITECTURE.md        # Arquitectura del proyecto
│   ├── Goldberg-J.Stat.Mech.2018.pdf # Artículo de referencia
│   └── comandos.txt           # Comandos de ejemplo
├── � examples/                # Ejemplos y demos
│   ├── quick_circle_simulation.sh  # Simulación rápida
│   └── basic_analysis.py      # Análisis básico
├── 📁 tests/                   # Tests del sistema
│   └── run_basic_tests.sh     # Tests básicos
├── 📄 Makefile                 # Sistema de compilación mejorado
├── 📄 LICENSE                  # Licencia MIT
└── 📄 CONTRIBUTING.md          # Guía de contribución
```

## 🚀 Instalación y Compilación

### ⚡ Inicio Rápido

```bash
# 1. Clonar el repositorio
git clone https://github.com/montero-j/Box2D-Silo.git
cd Box2D-Silo

# 2. Instalación automática
make install-deps    # Instalar dependencias Python
make                 # Compilar simulador
make test           # Verificar instalación

# 3. Ejemplo rápido
make example        # Simulación de 60 segundos
make analyze        # Análizar resultados
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

## 📊 Uso del Simulador

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

## 📈 Análisis de Resultados

### 🗂️ Gestión de Datos de Simulaciones

El simulador genera datos en diferentes ubicaciones según cómo se ejecute:

```
📂 UBICACIONES DE DATOS:

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

#### 🧹 Limpieza de Datos

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

Cada estudio genera:

```
data/shape_study_results_[forma]/
├── 📊 consolidated_avalanche_data.csv    # Todas las avalanchas
├── 📈 consolidated_flow_data.csv         # Datos de flujo temporal
├── 📋 progress_log.csv                   # Progreso del estudio
├── 🎬 render_simulation_data.json        # Datos para renderizado
├── 📝 study_log.txt                      # Log detallado
└── 📁 sim_XXXX/                          # Datos individuales por simulación
    ├── avalanche_data.csv             # Avalanchas de esta simulación
    ├── flow_data.csv                  # Flujo temporal
    ├── simulation_data.csv            # Posiciones de partículas
    └── simulation_log.txt             # Log de simulación
```

#### 📋 Descripción de Archivos de Datos

| Archivo | Contenido | Uso |
|---------|-----------|-----|
| `flow_data.csv` | Flujo de partículas vs tiempo | Análisis temporal |
| `avalanche_data.csv` | Tamaños y tiempos de avalanchas | Estadísticas de avalanchas |
| `simulation_data.csv` | Posiciones de todas las partículas | Renderización visual |
| `progress_log.csv` | Progreso de estudios largos | Monitoreo en tiempo real |
| `consolidated_*.csv` | Datos combinados de múltiples sims | Análisis estadístico |

#### 💾 Tamaños Típicos de Datos

| Simulación | Partículas | Tiempo | Tamaño Aprox |
|------------|------------|--------|-------------|
| Ejemplo rápido | 500 | 30s | ~1-5 MB |
| Simulación estándar | 2000 | 150s | ~10-50 MB |
| Estudio completo | 2000 x 100 sims | - | ~1-5 GB |

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

## 🔬 Estudios de Formas Geométricas

### Parámetros Estándar

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Partículas | 2000 | Número total de partículas |
| Radio base | 0.065 m | Radio de partículas circulares |
| Ancho salida | 0.26 m | Ancho de salida (D/d ≈ 4.0) |
| Tiempo sim | 150 s | Duración por simulación |
| Objetivo | 500 | Avalanchas por forma |

### Formas Soportadas

1. **Círculos** (`circles`)
   - Forma de referencia
   - Flujo más suave
   - Avalanchas frecuentes y pequeñas

2. **Triángulos** (`triangles`)
   - Mayor fricción
   - Tendencia a formar arcos
   - Avalanchas más grandes e irregulares

3. **Cuadrados** (`squares`)
   - Comportamiento intermedio
   - Empaquetamiento regular
   - Avalanchas medianas

4. **Pentágonos** (`pentagons`)
   - Equilibrio entre suavidad y irregularidad
   - Flujo moderadamente irregular

5. **Hexágonos** (`hexagons`)
   - Empaquetamiento más eficiente
   - Comportamiento más cercano a círculos
   - Flujo relativamente suave

## 📊 Métricas y Análisis

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

### Análisis Estadístico

- Distribución de tamaños de avalanchas
- Media, mediana, desviación estándar
- Análisis de ley de potencias
- Comparación entre formas geométricas
- Validación con literatura (Goldberg et al.)

## 🔧 Configuración Avanzada

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

## 🐛 Solución de Problemas

### Problemas Comunes

1. **Error de compilación Box2D**
   ```bash
   cd box2d
   rm -rf build
   ./build.sh
   ```

2. **Simulador no genera avalanchas**
   - Verificar que `outlet_width` no sea demasiado grande
   - Ajustar `flow_threshold` para detección más sensible
   - Aumentar tiempo de simulación

3. **Python ImportError**
   ```bash
   pip3 install --upgrade pandas numpy matplotlib
   ```

4. **Simulación muy lenta**
   - Reducir número de partículas
   - Compilar con `-O3` (ya incluido en Makefile)
   - Verificar que Box2D esté optimizado

### Logs y Depuración

```bash
# Ver logs detallados de estudio
tail -f data/shape_study_results_circles/study_log.txt

# Verificar progreso en tiempo real
watch -n 5 "tail -3 data/shape_study_results_circles/progress_log.csv"

# Depurar simulación individual
./bin/silo_simulator --help  # Ver todas las opciones

# Ejecutar tests del sistema
make test

# Limpiar datos de simulaciones
make clean-data
```

## 🛠️ Comandos del Makefile

```bash
# Compilación
make                    # Compilar simulador
make clean             # Limpiar archivos compilados
make clean-all         # Limpiar todo (código + datos)

# Gestión de datos
make clean-data        # Limpiar TODOS los datos de simulaciones
make clean-temp        # Limpiar solo datos temporales (ejemplos, tests)

# Instalación y verificación
make install-deps      # Instalar dependencias Python
make verify            # Verificar configuración del proyecto

# Tests y ejemplos
make test              # Ejecutar tests básicos
make example           # Ejemplo rápido de círculos
make analyze           # Analizar últimos resultados

# Estudios específicos (100 avalanchas cada uno)
make study-circles     # Estudio de círculos
make study-hexagons    # Estudio de hexágonos
make study-triangles   # Estudio de triángulos
make study-squares     # Estudio de cuadrados
make study-pentagons   # Estudio de pentágonos
```

### 🔍 Localización de Datos

```bash
# Ver dónde están todos los datos de simulación
find . -name "sim_*" -type d | head -10

# Ver tamaños de datos
du -sh data/ examples/simulations/ tests/simulations/ 2>/dev/null

# Listar simulaciones más recientes
ls -lat data/simulations/*/flow_data.csv | head -5

# Verificar datos de un estudio específico
ls -la data/shape_study_results_circles/
```

## � Buenas Prácticas para Gestión de Datos

### 📋 Recomendaciones de Uso

#### Para Trabajo de Investigación:
```bash
# 1. Usar scripts organizados (datos van a data/)
python3 scripts/run_shape_study.py circles --target 500

# 2. Respaldar estudios importantes
cp -r data/shape_study_results_circles/ backups/circles_$(date +%Y%m%d)/

# 3. Limpiar solo datos temporales regularmente
make clean-temp
```

#### Para Pruebas y Desarrollo:
```bash
# 1. Usar simulaciones directas para pruebas rápidas
./bin/silo_simulator --particles 500 --time 30

# 2. Usar ejemplos para demos
make example

# 3. Limpiar después de probar
make clean-temp
```

#### Para Repositorio Git:
```bash
# Solo versionar código, no datos
git add src/ scripts/ analysis/ docs/ examples/ tests/
git add README.md Makefile LICENSE .gitignore

# NO versionar datos (ya están en .gitignore)
# git add data/simulations/  # ❌ NO hacer esto
```

### ⚠️ Advertencias Importantes

- **Datos grandes**: Los estudios completos pueden generar varios GB
- **Respaldo**: Los datos NO están versionados en Git - hacer backups manuales
- **Limpieza**: `make clean-data` elimina TODO - usar con cuidado
- **Espacio**: Monitorear espacio en disco para estudios largos

## �📚 Referencias y Literatura

- **Goldberg et al. (2018)**: "Avalanche dynamics in a granular silo"
- **Box2D Documentation**: https://box2d.org/documentation/
- **Granular Flow Theory**: Jaeger & Nagel (1992)

## 🤝 Contribuciones

Para contribuir al proyecto:

1. Fork el repositorio
2. Crear rama feature (`git checkout -b feature/nueva-funcionalidad`)
3. Commit cambios (`git commit -am 'Agregar nueva funcionalidad'`)
4. Push a la rama (`git push origin feature/nueva-funcionalidad`)
5. Crear Pull Request

## 📄 Licencia

Este proyecto utiliza la licencia de Box2D. Ver `box2d/LICENSE` para detalles.

## 👥 Autores

- **Montero-J**: Implementación principal
- **GitHub Copilot**: Scripts de automatización y análisis

---

**¿Necesitas ayuda?** Abre un issue en el repositorio o revisa la documentación en `docs/`.
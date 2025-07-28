# Arquitectura del Proyecto

## 🏗️ Visión General

Box2D Silo Polygons está diseñado como un simulador modular de silos granulares con las siguientes capas:

```
┌─────────────────────────────────────────┐
│            Scripts de Control           │  ← scripts/
├─────────────────────────────────────────┤
│          Análisis y Visualización       │  ← analysis/
├─────────────────────────────────────────┤
│         Simulador Core (C++)            │  ← src/
├─────────────────────────────────────────┤
│           Motor Físico Box2D             │  ← box2d/
└─────────────────────────────────────────┘
```

## 📁 Estructura de Directorios

### Core del Simulador
- **`src/`**: Código fuente C++ del simulador principal
  - `silo_hexagono.cpp`: Simulador principal con física de partículas
  
### Scripts de Automatización  
- **`scripts/`**: Scripts Python para estudios automatizados
  - `run_shape_study.py`: Estudios por forma geométrica específica
  - `run_all_shapes_study.sh`: Ejecutor de estudios completos
  
### Análisis de Datos
- **`analysis/`**: Herramientas de análisis y visualización
  - `avalanche_distribution.py`: Análisis estadístico de avalanchas
  - `combine_distributions.py`: Combinación de múltiples estudios
  - `render_simulation.py`: Renderización visual de simulaciones
  - `verify_calculations.py`: Verificación de cálculos

### Datos y Resultados
- **`data/`**: Almacenamiento de resultados de simulaciones
  - `simulations/`: Datos de simulaciones individuales
  - `shape_study_results_*/`: Resultados consolidados por forma

### Documentación
- **`docs/`**: Documentación técnica y referencias
  - `Goldberg-J.Stat.Mech.2018.pdf`: Artículo de referencia
  - `comandos.txt`: Comandos de ejemplo

## 🔄 Flujo de Datos

```
Input Parameters → C++ Simulator → Raw Data → Python Analysis → Results
      ↓                ↓             ↓            ↓            ↓
   Geometry,        Box2D         CSV Files    Statistics,   Reports,
   Physics      Simulation        Positions    Distributions Videos
```

### 1. Simulación (C++)
- **Entrada**: Parámetros geométricos y físicos
- **Procesamiento**: Simulación física con Box2D
- **Salida**: Archivos CSV con posiciones y eventos

### 2. Detección de Eventos
- **Análisis de flujo**: Detección automática de avalanchas
- **Métricas**: Cálculo de tamaños y frecuencias
- **Consolidación**: Agregación de datos de múltiples simulaciones

### 3. Análisis Estadístico (Python)
- **Distribuciones**: Análisis de tamaños de avalanchas
- **Comparaciones**: Entre diferentes formas geométricas
- **Validación**: Contra literatura científica (Goldberg et al.)

### 4. Visualización
- **Renderizado**: Recreación visual de simulaciones
- **Gráficos**: Distribuciones y estadísticas
- **Videos**: Animaciones de avalanchas

## 🎛️ Parámetros del Sistema

### Físicos
```cpp
float density = 0.01f;        // kg/m²
float friction = 0.5f;        // Coef. fricción
float restitution = 0.05f;    // Coef. restitución  
float gravity = 9.81f;        // m/s²
```

### Geométricos
```cpp
float silo_width = 2.6f;      // Ancho interno (m)
float silo_height = 7.8f;     // Altura (m)
float outlet_width = 0.26f;   // Ancho salida (m)
float base_radius = 0.065f;   // Radio partículas (m)
```

### Detección de Avalanchas
```cpp
float flow_threshold = 0.1f;     // part/s
float blocking_time = 5.0f;      // segundos
float output_interval = 1.0f;    // intervalo datos
```

## 🔧 Extensibilidad

### Agregar Nueva Forma Geométrica

1. **Modificar `src/silo_hexagono.cpp`**:
   ```cpp
   // Agregar en función createParticle()
   case NEW_SHAPE:
       // Implementar creación de nueva forma
       break;
   ```

2. **Actualizar `scripts/run_shape_study.py`**:
   ```python
   self.shape_params = {
       # ...formas existentes...
       "new_shape": {"--polygon": 1, "--sides": N}
   }
   ```

### Agregar Nueva Métrica

1. **Extender análisis en `analysis/`**:
   ```python
   def calculate_new_metric(avalanche_data):
       # Implementar nueva métrica
       return metric_value
   ```

2. **Integrar en pipeline de análisis**

### Modificar Detección de Avalanchas

1. **Ajustar umbrales en simulador C++**
2. **Actualizar lógica de detección**
3. **Validar con datos existentes**

## 🧪 Testing

### Niveles de Test

1. **Unit Tests**: Funciones individuales
2. **Integration Tests**: Simulador completo
3. **Validation Tests**: Comparación con literatura

### Estrategia de Validación

1. **Reproducibilidad**: Mismos parámetros → mismos resultados
2. **Convergencia**: Más simulaciones → estadísticas estables
3. **Literatura**: Comparación con Goldberg et al.
4. **Física**: Conservación de momentum y energía

## 🚀 Performance

### Optimizaciones Actuales

- **Compilación**: `-O3` para máximo rendimiento
- **Box2D**: Motor optimizado para simulaciones 2D
- **Datos**: Escritura eficiente a CSV
- **Memoria**: Gestión automática de partículas

### Bottlenecks Identificados

1. **I/O**: Escritura frecuente de datos
2. **Colisiones**: Cálculo de contactos entre partículas
3. **Renderizado**: Generación de videos (opcional)

### Escalabilidad

- **Partículas**: Hasta ~5000 en hardware típico
- **Tiempo**: Simulaciones de 150-300 segundos
- **Formas**: 5 formas geométricas actualmente
- **Paralelización**: Múltiples simulaciones independientes

## 📚 Referencias de Diseño

- **Box2D Manual**: Principios de simulación física
- **Goldberg et al. (2018)**: Metodología experimental
- **Granular Flow Theory**: Base teórica para avalanchas

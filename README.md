
# Box2D-Silo

Simulador de avalanchas granulares en silos usando Box2D. Permite estudiar cómo la forma de las partículas afecta el flujo y la formación de avalanchas.

## Estructura

```
├── bin/                # Ejecutable principal (silo_simulator)
├── src/                # Código fuente C++
├── box2d/              # Motor de física Box2D
├── analysis/           # Scripts Python para análisis y visualización
├── docs/               # Documentación y artículos
├── parametros_sims/    # Ejemplos de archivos de parámetros
├── scripts/            # Scripts de automatización
├── simulations/        # Resultados de simulaciones
├── Makefile            # Compilación y automatización
├── ejecutar_simulacion_1.sh / ejecutar_simulacion_2.sh / ejecutar_x_simulaciones.sh
├── renderizar_simulacion.sh
├── parametros_ejemplo_1.txt / parametros_ejemplo_2.txt
```

## Uso rápido

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
   o
   ```bash
   ./ejecutar_simulacion_2.sh parametros_ejemplo_2.txt
   ```
4. Analiza resultados:
   ```bash
   python3 analysis/avalanche_distribution.py --input simulations/sim_XXXX/
   ```

## Parámetros principales
- `--base-radius`: Radio de partículas
- `--outlet-width`: Ancho de salida del silo
- `--particles`: Número de partículas
- `--time`: Tiempo de simulación
- `--polygon` y `--sides`: Para simular polígonos

## Limpieza y gestión de datos
- Limpiar datos: `make clean-data`
- Limpiar compilados: `make clean`
- Limpiar todo: `make clean-all`


# Box2D-Silo

Simulador de avalanchas granulares en silos usando Box2D. Permite estudiar cómo la forma de las partículas afecta el flujo y la formación de avalanchas.

## Estructura

```
├── bin/                # Ejecutable principal (silo_simulator)
├── src/                # Código fuente C++
├── box2d/              # Motor de física Box2D
├── analysis/           # Scripts Python para análisis y visualización
├── docs/               # Documentación y artículos
├── parametros_sims/    # Ejemplos de archivos de parámetros
├── scripts/            # Scripts de automatización
├── simulations/        # Resultados de simulaciones
├── Makefile            # Compilación y automatización
├── ejecutar_simulacion_1.sh / ejecutar_simulacion_2.sh
├── parametros_ejemplo_1.txt / parametros_ejemplo_2.txt
```
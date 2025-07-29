# Bitácora de Cambios - Código Fuente

## Formato de Entrada

Para cada cambio, usa el siguiente formato:

```markdown
## [Fecha] - [Tipo de Cambio]

### Archivos Modificados
- `archivo1.cpp`
- `archivo2.h`

### Descripción
Breve descripción del cambio realizado.

### Motivo
Por qué se realizó este cambio.

### Detalles Técnicos
- Detalle técnico 1
- Detalle técnico 2
- Impacto en el rendimiento/funcionalidad

### Pruebas Realizadas
- [ ] Compilación exitosa
- [ ] Test básico
- [ ] Test de funcionalidad específica

---
```

## Tipos de Cambio

- **NUEVA_FUNCIONALIDAD**: Nueva característica o capacidad
- **CORRECCIÓN**: Corrección de errores o bugs
- **OPTIMIZACIÓN**: Mejoras de rendimiento
- **REFACTORING**: Reestructuración de código sin cambio funcional
- **DOCUMENTACIÓN**: Actualización de comentarios o documentación
- **TESTING**: Adición o modificación de pruebas
- **ESTILO**: Cambios de formato, indentación, etc.
- **SEGURIDAD**: Correcciones relacionadas con seguridad

---

# Historial de Cambios

## [29/07/2025] - 🔧 OPTIMIZACIÓN

### Archivos Modificados
- `silo_hexagono.cpp`

### Descripción
Escalado del sistema para usar diámetro de partícula = 1 y densidad = 1.

### Motivo
Box2D tenía inconvenientes para resolver correctamente un sistema con dimensiones muy pequeñas, lo que causaba superposición de partículas y comportamiento físico inestable.

### Detalles Técnicos
- Modificado el diámetro de partícula de escala pequeña a 1.0 unidades
- Ajustada la densidad de partículas a 1.0 para mantener proporciones físicas
- Escalado proporcional de todas las dimensiones del silo
- Mejora significativa en la estabilidad de la simulación física
- Eliminación de superposiciones no físicas entre partículas

### Pruebas Realizadas
- [x] Compilación exitosa
- [x] Test básico sin superposición de partículas
- [x] Verificación de estabilidad en simulación prolongada
- [x] Validación de comportamiento físico realista

---

## [28/07/2025] - 📖 DOCUMENTACIÓN

### Archivos Modificados
- `CHANGELOG.md` (nuevo archivo)

### Descripción
Creación de bitácora para documentar cambios en el código fuente del simulador.

### Motivo
Necesidad de mantener un registro organizado de las modificaciones realizadas al código para facilitar el seguimiento y mantenimiento del proyecto.

### Detalles Técnicos
- Estructura estándar de changelog con formato markdown
- Plantilla para documentar cambios consistentemente
- Categorización por tipos de cambio
- Sección de pruebas para validar modificaciones

### Pruebas Realizadas
- [x] Compilación exitosa (sin cambios en código)
- [x] Verificación de formato markdown
- [ ] Test básico (pendiente para próximos cambios)

---

<!-- 
INSTRUCCIONES DE USO:

1. Cada vez que modifiques código, añade una entrada al principio de esta sección
2. Usa la fecha en formato DD/MM/AAAA
3. Selecciona el tipo de cambio más apropiado
4. Sé específico en la descripción pero conciso
5. Lista todos los archivos que modificaste
6. Explica el "por qué" además del "qué"
7. Documenta las pruebas que realizaste
8. Mantén las entradas ordenadas por fecha (más reciente primero)

EJEMPLO DE USO:

## [29/07/2025] - 🆕 NUEVA_FUNCIONALIDAD

### Archivos Modificados
- `silo_hexagono.cpp`

### Descripción
Agregado soporte para partículas octagonales en el simulador.

### Motivo
Extender el estudio a formas con 8 lados para completar el análisis geométrico.

### Detalles Técnicos
- Modificada función `createPolygon()` para soportar 8 lados
- Actualizado parámetro `--sides` para aceptar valor 8
- Ajustados cálculos de área para octágonos regulares
- Impacto mínimo en rendimiento

### Pruebas Realizadas
- [x] Compilación exitosa
- [x] Test básico con octágonos
- [x] Verificación de generación correcta de geometría
- [x] Simulación de 100 partículas por 30 segundos

-->

# Contribuir a Box2D Silo Polygons

¡Gracias por tu interés en contribuir a este proyecto! 

## 🤝 Cómo Contribuir

### Reportar Bugs

1. **Verifica** que el bug no haya sido reportado previamente
2. **Abre un issue** con una descripción clara del problema
3. **Incluye** pasos para reproducir el bug
4. **Proporciona** información del sistema (OS, versión de Python, etc.)

### Sugerir Mejoras

1. **Abre un issue** describiendo la mejora propuesta
2. **Explica** por qué sería útil para el proyecto
3. **Discute** la implementación si es compleja

### Enviar Pull Requests

1. **Fork** el repositorio
2. **Crea** una rama para tu feature: `git checkout -b feature/nueva-funcionalidad`
3. **Realiza** tus cambios siguiendo las guías de estilo
4. **Añade** tests para nueva funcionalidad
5. **Ejecuta** tests existentes: `make test`
6. **Commit** con mensajes descriptivos
7. **Push** a tu fork: `git push origin feature/nueva-funcionalidad`
8. **Abre** un Pull Request

## 📋 Guías de Estilo

### C++
- Usa 4 espacios para indentación
- Nombres de variables en `snake_case`
- Nombres de clases en `PascalCase`
- Comentarios descriptivos para funciones complejas

### Python
- Sigue PEP 8
- Usa 4 espacios para indentación
- Docstrings para todas las funciones públicas
- Type hints cuando sea apropiado

### Commits
- Mensajes en presente: "Add feature" no "Added feature"
- Primera línea < 50 caracteres
- Cuerpo explicativo si es necesario

## 🧪 Tests

```bash
# Ejecutar tests C++
make test

# Ejecutar tests Python
python -m pytest tests/

# Verificar simulación básica
./bin/silo_simulator --test-mode
```

## 🐛 Debugging

### Simulador C++
```bash
# Compilar con debug
make debug

# Ejecutar con gdb
gdb ./bin/silo_simulator
```

### Scripts Python
```bash
# Ejecutar con logging verbose
python scripts/run_shape_study.py circles --verbose

# Usar debugger
python -m pdb scripts/run_shape_study.py
```

## 📚 Recursos

- [Box2D Documentation](https://box2d.org/documentation/)
- [Granular Flow Literature](docs/Goldberg-J.Stat.Mech.2018.pdf)
- [Project Architecture](docs/ARCHITECTURE.md)

## ❓ ¿Necesitas Ayuda?

- 📧 Abre un issue para preguntas técnicas
- 💬 Usa Discussions para conversaciones generales
- 📖 Revisa la documentación en `docs/`

¡Gracias por contribuir! 🎉

# FunshiEngineGL

Motor y editor de juegos en tiempo real escrito en C++17, con interfaz Dear ImGui, renderizado OpenGL 3.3 Core y arquitectura modular (backends intercambiables).

**Licencia:** [Apache License 2.0](LICENSE).

## Compilación

```bash
cmake -S FunshiEngineGL -B FunshiEngineGL/build \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF -DFUNSHI_JAVA=ON
cmake --build FunshiEngineGL/build -j$(nproc)
```

## Ejecución

```bash
cd FunshiEngineGL && ./build/FunshiEngineGL
```

## Documentación

- [MANUAL_DE_USO.md](MANUAL_DE_USO.md) – Guía de uso del editor y scripting
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) – Arquitectura y estructura del proyecto
- [FLUJO_DE_RAMAS.md](FLUJO_DE_RAMAS.md) – Convención de ramas
- [DocuTecnicoBugs.md](DocuTecnicoBugs.md) – Registro técnico de bugs
- [TAREAS_PENDIENTES.md](TAREAS_PENDIENTES.md) – Sistemas que faltan en el motor
- [PLAN_SISTEMAS_NUEVOS.md](PLAN_SISTEMAS_NUEVOS.md) – Plan de integración de cada sistema

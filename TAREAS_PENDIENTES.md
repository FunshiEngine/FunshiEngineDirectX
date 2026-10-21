# Tareas pendientes

Lista de los sistemas que faltan en el motor, con el estado de cada uno y el
enlace a su plan de integración. Cada sistema tiene su plan en
[PLAN_SISTEMAS_NUEVOS.md](PLAN_SISTEMAS_NUEVOS.md), donde se explica qué
cambia, dónde vive el código, cómo se integra y cómo se valida.

| ID | Sistema | Área | Depende de | Plan |
|----|---------|------|-----------|------|
| S1 | Animación por máquina de estados | Animación | — | [S1](PLAN_SISTEMAS_NUEVOS.md#s1-animación-por-máquina-de-estados) |
| S2 | Editor visual de máquinas de estados (diagrama) | Editor / Animación | S1 | [S2](PLAN_SISTEMAS_NUEVOS.md#s2-editor-visual-de-máquinas-de-estados) |
| S3 | Animación con huesos (skeletal) | Animación / Render | S1, S4 | [S3](PLAN_SISTEMAS_NUEVOS.md#s3-animación-con-huesos-skeletal) |
| S4 | Materiales por regiones de un modelo 3D | Render / Assets | — | [S4](PLAN_SISTEMAS_NUEVOS.md#s4-materiales-por-regiones-de-un-modelo-3d) |
| S5 | Vista previa del material en esfera | Editor / Render | S4 | [S5](PLAN_SISTEMAS_NUEVOS.md#s5-vista-previa-del-material-en-esfera) |
| S6 | Icono de cada componente en el Inspector | Editor / GUI | — | [S6](PLAN_SISTEMAS_NUEVOS.md#s6-icono-de-cada-componente-en-el-inspector) |
| S7 | Sistema de partículas y efectos | Render | — | [S7](PLAN_SISTEMAS_NUEVOS.md#s7-sistema-de-partículas-y-efectos) |
| S8 | Sistema de construcción de terrenos | Render / Editor | S7 | [S8](PLAN_SISTEMAS_NUEVOS.md#s8-sistema-de-construcción-de-terrenos) |
| S9 | Interfaz de juego tradicional (sin web) | Interfaz | — | [S9](PLAN_SISTEMAS_NUEVOS.md#s9-interfaz-de-juego-tradicional-sin-web) |
| S10 | Arrastrar y pegar archivos del sistema operativo | Explorador | — | [S10](PLAN_SISTEMAS_NUEVOS.md#s10-arrastrar-y-pegar-archivos-del-sistema-operativo) |
| S11 | API de inspección de escena y CLI de construcción | Motor / CLI | — | [S11](PLAN_SISTEMAS_NUEVOS.md#s11-api-de-inspección-de-escena-y-cli-de-construcción) |

## Reparto entre repositorios

El motor es este repositorio (`FunshiEngineOpenGL`). La CLI es un proyecto
separado (`FunshiEngineCLI`) que consume el motor como submódulo: **el motor
publica la API, la CLI la usa**. Ningún sistema de esta lista exige tocar la
CLI, salvo S11, que es precisamente el que define ese reparto.

## Reglas que rigen todos los sistemas

- Un sistema se integra en commits pequeños y verificables; la documentación
  (estructura, manual de uso, documentación de bugs) va en el mismo commit que
  el código que la motiva.
- Todo lo nuevo tiene su prueba headless en `tests/`, con el patrón de
  `CHECK` y `TempPruebas::CarpetaPrueba`, y su target registrado en el
  `CMakeLists.txt` y en las listas de la CI.
- Ningún módulo nuevo depende de ImGui, de CEF ni del editor; el editor y el
  juego exportado son dos consumidores más del motor.
- Los contratos de los scripts (`ApiScriptGameObject`, `ScriptServices`) son
  append-only: los campos nuevos van al final, antes de `version`, y el
  número de versión sube.
- La CLI es un proyecto aparte: lo que se le expone va por un contrato con
  versión propia, no por tocar el motor desde fuera.

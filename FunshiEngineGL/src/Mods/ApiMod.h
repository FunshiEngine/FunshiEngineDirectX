/*
    FunshiEngineGL - Motor de juegos 3D con OpenGL e ImGui
    Copyright 2026 Gianfranco Ivan Enrique

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    SPDX-License-Identifier: Apache-2.0
*/

// API v0 para mods nativos del editor (fase B). La frontera es POD (sin STL)
// para evitar acoplar el .dll a la ABI de C++ del motor. Pattern append-only.
// La marca de exportacion sigue la misma logica que scripts (H-15).

#ifndef API_MOD_H
#define API_MOD_H

#include <cstddef>

// Marca de exportacion de la funcion de entrada del mod.
#if defined(_WIN32) && defined(_MSC_VER)
#define FUNSHI_MOD_EXPORT __declspec(dllexport)
#else
#define FUNSHI_MOD_EXPORT
#endif

namespace FunshiMods {

struct ContextoMod {
    // Contexto Grafico de ImGui (opaco). El mod debe hacer
    // ImGui::SetCurrentContext(ctxImgui) antes de dibujar, si decide dibujar.
    void* ctxImgui;
    // Tamaño de la ventana/draw area donde el panel pinta (opcional).
    float areaAncho;
    float areaAlto;
    // Tiempo delta (segundos).
    float deltaTime;
};

// Version actual de la API de mods. APPEND-ONLY.
constexpr int versionApiMod = 1;

// Tabla de servicios que el motor ofrece a los mods (POD, append-only).
struct ApiModServicios {
    // Reserva para futuros servicios; version al final.
    int version;
};

// Interfaz (vtable) que el mod debe implementar. Todas las funciones son
// opcionales (nullptr significa "no implementado").
struct IMod {
    // Inicializacion: recibe la tabla de servicios del motor. Llamado una vez
    // tras cargar el .dll, antes de cualquier dibujo/update.
    void (*init)(const ApiModServicios* servicios);
    // Actualizacion por frame (logica ligera). Opcional.
    void (*update)(float deltaTime);
    // Dibujo del panel del mod (dentro de su ventana/panel propio). Opcional.
    // El motor pasa contexto; el mod debe fijar ImGui::SetCurrentContext.
    void (*dibujar)(const ContextoMod* contexto);
    // Limpiar recursos. Llamado al descargar el mod (al cerrar el editor).
    void (*shutdown)();
    // Nombre visible del mod (para diagnosticos). Opcional.
    const char* (*nombre)();
    // Version del mod (string corto). Opcional.
    const char* (*versionStr)();
};

// Firma de la funcion de entrada que debe exportar el .dll del mod:
// extern "C" FUNSHI_MOD_EXPORT IMod* funshi_mod_api(int* version);
} // namespace FunshiMods

#endif // API_MOD_H

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

// API publica para mods nativos del editor. La frontera es POD (sin STL, sin
// objetos con destructor) para que el .dll/.so del mod NO tenga que enlazar
// contra el motor ni depender de su ABI de C++: el motor le pasa una tabla de
// punteros a funcion y el mod devuelve la suya.
//
// La tabla del motor es APPEND-ONLY: los campos nuevos van al final, antes
// del campo `version`, y `version` se incrementa al agregar funcionalidad.
//
// La version de la API que el motor ofrece es `versionApiMod`. Un mod declara
// con que version fue compilado: si declara una mayor, el motor lo rechaza
// (no puede ofrecer servicios que el mod no conoce); si declara una menor,
// el motor lo acepta y le ofrece la version actual.

#ifndef API_MOD_H
#define API_MOD_H

#include <cstddef>

// Marca de exportacion de la funcion de entrada del mod. En Windows (MSVC y
// MinGW) una funcion de una .dll NO se exporta sola: sin
// __declspec(dllexport) el binario compila pero su tabla de exportaciones no
// la tiene y GetProcAddress devuelve NULL. En Linux/macOS los simbolos se
// exportan por defecto.
#if defined(_WIN32)
#define FUNSHI_MOD_EXPORT __declspec(dllexport)
#else
#define FUNSHI_MOD_EXPORT
#endif

namespace FunshiMods {

// Contexto que el motor pasa al mod en cada dibujo. Todo lo que el mod
// necesita del motor entra por aca (o por la tabla de servicios): el mod no
// ve objetos del motor, solo datos.
struct ContextoMod {
    // Contexto grafico de ImGui (opaco). El mod que dibuja debe hacer
    // ImGui::SetCurrentContext(ctxImgui) antes de pintar, para que sus
    // llamadas a ImGui usen el contexto del motor y no el suyo.
    void* ctxImgui = nullptr;
    // Tamano en pixeles del area donde el mod dibuja.
    float areaAncho = 0.0f;
    float areaAlto = 0.0f;
    // Tiempo transcurrido desde el frame anterior, en segundos.
    float deltaTime = 0.0f;
};

// Version de la API de mods que ofrece esta build del motor.
constexpr int versionApiMod = 1;

// Tabla de servicios que el motor ofrece a los mods. APPEND-ONLY: los campos
// nuevos van antes de `version`.
struct ApiModServicios {
    // Version de la API que el motor ofrece (versionApiMod).
    int version = versionApiMod;
};

// Interfaz que el mod implementa. Es una tabla de punteros a funcion (no una
// clase con vtable): todos los campos son opcionales, y un nullptr significa
// "el mod no implementa esto".
struct IMod {
    // Inicializacion: el motor la llama una vez, tras cargar el binario y
    // antes de cualquier dibujo o actualizacion. Recibe la tabla de servicios.
    void (*init)(const ApiModServicios* servicios);
    // Actualizacion por frame (logica del mod). Opcional.
    void (*update)(float deltaTime);
    // Dibujo del mod, dentro de su propia ventana o panel. Opcional.
    void (*dibujar)(const ContextoMod* contexto);
    // Limpieza de recursos: el motor la llama al descargar el mod, antes de
    // cerrar el binario. Opcional.
    void (*shutdown)();
    // Nombre visible del mod (para el menu y los diagnosticos). Opcional.
    const char* (*nombre)();
    // Version del mod (texto corto). Opcional.
    const char* (*versionStr)();
};

// Tipo de la funcion de entrada que el motor busca en el binario del mod:
//   extern "C" FUNSHI_MOD_EXPORT int funshi_mod_api(FunshiMods::IMod** salida);
// Devuelve la version de la API con la que el mod fue compilado, o 0 si no
// puede trabajar con el motor (en ese caso el motor lo rechaza). Escribe la
// interfaz del mod en `salida`.
using FuncionEntradaMod = int (*)(IMod** salida);

} // namespace FunshiMods

#endif // API_MOD_H

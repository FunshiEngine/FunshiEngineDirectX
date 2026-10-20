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
#include "TypeUtils.h"

#include <typeinfo>

#if defined(__GNUG__)
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace {
// MSVC no "demanglea": typeid().name() ya viene en forma legible pero con el
// prefijo de verificacion de compilador ("class Transform", "struct XCoord").
// Esos prefijos no son plataforma-portables: en Linux el demangle produce el
// nombre pelado ("Transform") y la serializacion de la escena pierde consistencia
// en Windows (ComponentFactory no reconoce "class Transform" al deserializar).
void quitarPrefijoClassKey(std::string& nombre) {
    constexpr const char* prefijos[] = {"class ", "struct ", "union ", "enum "};
    for (const char* prefijo : prefijos) {
        const std::size_t len = std::char_traits<char>::length(prefijo);
        if (nombre.rfind(prefijo, 0) == 0) {
            nombre.erase(0, len);
            break;
        }
    }
}
} // namespace

std::string demangle(const char* name) {
#if defined(__GNUG__)
    int status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string result((status == 0 && demangled) ? demangled : name);
    std::free(demangled);
    return result;
#else
    // En MSVC no necesita demangling, pero si normalizacion del prefijo de tipo
    // para que los nombres serializados coincidan con los de GCC/Linux.
    std::string resultado = name ? name : "";
    quitarPrefijoClassKey(resultado);
    return resultado;
#endif
}

std::string nombreSimpleTipo(const std::string& nombre) {
    std::string base = demangle(nombre.c_str());
    // Un nombre persistido en un archivo puede traer el prefijo de class-key
    // aunque el demangle de la plataforma no lo toque: p. ej. "class Transform"
    // escrito por un build de Windows y leido en Linux.
    quitarPrefijoClassKey(base);
    // Namespace: se queda con el ultimo componente ("Objetos::Componentes::Color"
    // -> "Color"), que es la clave con la que se registran los componentes.
    const std::size_t separador = base.rfind("::");
    if (separador != std::string::npos) base = base.substr(separador + 2);
    return base;
}

std::string nombreSimpleTipo(const std::type_info& tipo) {
    return nombreSimpleTipo(std::string(tipo.name()));
}
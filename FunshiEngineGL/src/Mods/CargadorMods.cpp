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

#include "CargadorMods.h"

#include "CargaDinamica.h"
#include "LectorZip.h"
#include "../Configuracion/ProjectPaths.h"
#include "Diagnosticos/RegistroDiagnosticos.h"

#include <system_error>
#include <utility>

namespace {

constexpr const char* MODULO_MODS = "Mods";

// Simbolo que el motor busca en el binario del mod.
constexpr const char* SIMBOLO_ENTRADA = "funshi_mod_api";

// Extensiones de binario que el motor sabe cargar, por si el manifiesto
// declara el punto de entrada sin extension.
constexpr const char* const EXTENSIONES_BINARIO[] = {"dll", "so", "dylib"};

bool existeArchivo(const std::filesystem::path& ruta) {
    std::error_code ec;
    return std::filesystem::is_regular_file(ruta, ec);
}

// True si `hijo` esta dentro de `padre`. El punto de entrada que declara el
// manifiesto no puede sacar el binario de la carpeta de extraccion: un
// manifiesto con "../otro.dll" no debe alcanzar archivos de la cache.
bool estaDentro(const std::filesystem::path& hijo,
                 const std::filesystem::path& padre) {
    std::error_code ec;
    const std::filesystem::path relativo =
        std::filesystem::relative(hijo, padre, ec);
    if (ec || relativo.empty()) return false;
    for (const std::filesystem::path& parte : relativo) {
        if (parte == "..") return false;
    }
    return true;
}

// Localiza el binario del mod dentro de la carpeta extraida: primero el que
// declara el manifiesto; si no existe, el mismo nombre con las extensiones
// conocidas.
std::filesystem::path localizarBinario(
    const std::filesystem::path& carpetaExtraida,
    const std::string& puntoEntrada) {
    const std::filesystem::path declarado = carpetaExtraida / puntoEntrada;
    if (existeArchivo(declarado) && estaDentro(declarado, carpetaExtraida))
        return declarado;

    std::filesystem::path sinExtension = declarado;
    sinExtension.replace_extension();
    for (const char* extension : EXTENSIONES_BINARIO) {
        std::filesystem::path candidata = sinExtension;
        candidata += ".";
        candidata += extension;
        if (existeArchivo(candidata) && estaDentro(candidata, carpetaExtraida))
            return candidata;
    }
    return std::filesystem::path();
}

// La tabla de servicios que el motor entrega a los mods. Es la misma para
// todos: hoy solo lleva la version de la API.
const FunshiMods::ApiModServicios& tablaServicios() {
    static const FunshiMods::ApiModServicios servicios{};
    return servicios;
}

} // namespace

std::filesystem::path CargadorMods::directorioCacheMods() {
    return std::filesystem::path(ProjectPaths::directorioBase()) / "cache" /
           "mods";
}

std::string CargadorMods::sanitizarId(const std::string& id) {
    std::string salida;
    salida.reserve(id.size());
    for (const char caracter : id) {
        const bool permitido =
            (caracter >= 'a' && caracter <= 'z') ||
            (caracter >= 'A' && caracter <= 'Z') ||
            (caracter >= '0' && caracter <= '9') || caracter == '.' ||
            caracter == '-' || caracter == '_';
        salida += permitido ? caracter : '_';
    }
    if (salida.empty()) salida = "mod";
    return salida;
}

std::filesystem::path CargadorMods::carpetaCacheDelMod(
    const PaqueteMod& mod, const std::filesystem::path& directorioCache) {
    return directorioCache / sanitizarId(mod.manifiesto.id);
}

CargadorMods::ResultadoCarga CargadorMods::cargar(
    const GestorMods::Resultado& descubrimiento) {
    return cargar(descubrimiento, directorioCacheMods());
}

CargadorMods::ResultadoCarga CargadorMods::cargar(
    const GestorMods::Resultado& descubrimiento,
    const std::filesystem::path& directorioCache) {
    ResultadoCarga resultado;

    std::error_code ec;
    std::filesystem::create_directories(directorioCache, ec);
    if (ec) {
        resultado.fallos.push_back(
            {directorioCache.string(),
             "no se pudo crear la carpeta de cache (" + ec.message() + ")"});
        return resultado;
    }

    for (const PaqueteMod& paquete : descubrimiento.mods) {
        const ManifiestoMod& manifiesto = paquete.manifiesto;
        const std::string archivo = paquete.paquete.filename().string();
        const std::string etiqueta = manifiesto.id.empty() ? archivo : manifiesto.id;

        // El binario se busca por lo que declara el manifiesto.
        if (manifiesto.puntoEntrada.empty()) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": el manifiesto no declara 'puntoEntrada'"});
            continue;
        }

        // Carpeta de cache del mod. Se limpia antes de extraer para no
        // arrastrar el binario de una version anterior del mismo mod.
        const std::filesystem::path carpeta =
            carpetaCacheDelMod(paquete, directorioCache);
        std::filesystem::remove_all(carpeta, ec);
        ec.clear();

        std::string error;
        if (!LectorZip::extraerTodo(paquete.paquete.string(), carpeta.string(),
                                    error)) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": no se pudo extraer el paquete (" + error +
                            ")"});
            continue;
        }

        const std::filesystem::path binario =
            localizarBinario(carpeta, manifiesto.puntoEntrada);
        if (binario.empty()) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": el paquete no contiene el binario '" +
                            manifiesto.puntoEntrada + "'"});
            continue;
        }

        const std::string rutaBinario = binario.string();
        void* handle = FUNSHI_DLOPEN(rutaBinario);
        if (!handle) {
            std::string detalle;
#if !defined(_WIN32)
            const char* errorCarga = dlerror();
            if (errorCarga) detalle = errorCarga;
#endif
            resultado.fallos.push_back(
                {archivo, etiqueta + ": no se pudo cargar el binario (" +
                            (detalle.empty() ? rutaBinario : detalle) + ")"});
            continue;
        }

        const auto entrada = reinterpret_cast<FunshiMods::FuncionEntradaMod>(
            FUNSHI_DLSYM(handle, SIMBOLO_ENTRADA));
        if (!entrada) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": el binario no exporta '" +
                            std::string(SIMBOLO_ENTRADA) + "'"});
            FUNSHI_DLOPENCERRAR(handle);
            continue;
        }

        FunshiMods::IMod* interfaz = nullptr;
        const int versionApi = entrada(&interfaz);
        if (versionApi <= 0) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": el mod no acepta la API de mods"});
            FUNSHI_DLOPENCERRAR(handle);
            continue;
        }
        if (versionApi > FunshiMods::versionApiMod) {
            resultado.fallos.push_back(
                {archivo,
                 etiqueta + ": el mod requiere la API de mods v" +
                     std::to_string(versionApi) + " y el motor ofrece v" +
                     std::to_string(FunshiMods::versionApiMod)});
            FUNSHI_DLOPENCERRAR(handle);
            continue;
        }
        if (!interfaz) {
            resultado.fallos.push_back(
                {archivo, etiqueta + ": el mod no devolvio su interfaz"});
            FUNSHI_DLOPENCERRAR(handle);
            continue;
        }

        // El mod queda inicializado: a partir de aqui el motor lo llama cada
        // frame y lo descarga al salir.
        if (interfaz->init) interfaz->init(&tablaServicios());

        ModCargado cargado;
        cargado.id = manifiesto.id;
        cargado.nombre = manifiesto.nombre.empty() ? manifiesto.id
                                                    : manifiesto.nombre;
        cargado.version = manifiesto.version;
        cargado.rutaPaquete = paquete.paquete.string();
        cargado.directorioExtraido = carpeta;
        cargado.rutaBinario = binario;
        cargado.handleDll = handle;
        cargado.interfaz = interfaz;
        resultado.modsCargados.push_back(std::move(cargado));
    }

    return resultado;
}

void CargadorMods::descargar(std::vector<ModCargado>& modsCargados) {
    // Orden inverso al de carga: un mod que depende de otro se apaga antes
    // que el del que depende.
    for (auto iterador = modsCargados.rbegin();
         iterador != modsCargados.rend(); ++iterador) {
        ModCargado& mod = *iterador;
        if (mod.interfaz && mod.interfaz->shutdown) mod.interfaz->shutdown();
        mod.interfaz = nullptr;
        if (mod.handleDll) {
            FUNSHI_DLOPENCERRAR(mod.handleDll);
            mod.handleDll = nullptr;
        }
    }
    modsCargados.clear();
}

void CargadorMods::reportar(const ResultadoCarga& resultado,
                             RegistroDiagnosticos& registro) {
    for (const FalloMod& fallo : resultado.fallos) {
        EventoDiagnostico evento;
        evento.nivel = NivelDiagnostico::Error;
        evento.modulo = MODULO_MODS;
        evento.mensaje =
            "mod no cargado: " + fallo.paquete + " (" + fallo.causa + ")";
        registro.registrar(std::move(evento));
    }

    if (!resultado.modsCargados.empty()) {
        EventoDiagnostico resumen;
        resumen.nivel = NivelDiagnostico::Informacion;
        resumen.modulo = MODULO_MODS;
        resumen.mensaje = "mods nativos cargados: " +
                          std::to_string(resultado.modsCargados.size());
        registro.registrar(std::move(resumen));
    }
}

void CargadorMods::reportar(const ResultadoCarga& resultado) {
    reportar(resultado, RegistroDiagnosticos::global());
}

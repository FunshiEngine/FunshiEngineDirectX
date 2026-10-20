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
#include "MapaSombras.h"

#include "../../Iluminacion/MatrizLuz.h"

namespace Rendering {

ResultadoSeleccion seleccionarProyectores(const CandidatoProyector* candidatos,
                                           int cantidad, int limite,
                                           const glm::vec3& centroEscena,
                                           ProyectorSombra* salida) {
    ResultadoSeleccion resultado;
    // Sin candidatos no hay nada que elegir. Un limite de cero no se trata
    // como caso especial: cada candidato se cuenta como descartado por el
    // limite (que es lo que le pasa al que avisa de que hay luces con sombra
    // que no proyectan).
    if (!candidatos || !salida || cantidad <= 0) return resultado;

    for (int i = 0; i < cantidad; ++i) {
        const CandidatoProyector& candidato = candidatos[i];

        // Las luces puntuales proyectan en todas las direcciones: eso exige
        // un mapa por cara (o un mapa cubico), que el mapa de sombras todavia
        // no tiene. Se descartan y se cuentan aparte.
        if (candidato.tipo == TipoLuzProyector::Puntual) {
            ++resultado.noSoportados;
            continue;
        }

        if (resultado.elegidos >= limite) {
            ++resultado.porLimite;
            continue;
        }

        FocoLuz foco;
        if (candidato.tipo == TipoLuzProyector::Direccional) {
            // La luz direccional no esta en ningun sitio: su frustum se
            // coloca alrededor del centro de la escena.
            foco = focoDireccional(centroEscena, candidato.direccion,
                                   candidato.radio, candidato.distancia);
        } else {
            foco = focoPosicional(candidato.posicion, candidato.direccion,
                                  candidato.radio, candidato.distancia);
        }

        ProyectorSombra& proyector = salida[resultado.elegidos];
        proyector.matrizLuz = matrizLuz(foco);
        proyector.resolucion = candidato.resolucion;
        proyector.sesgo = candidato.sesgo;
        proyector.fuerza = candidato.fuerza;
        proyector.textura = Backend::kInvalidHandle;
        ++resultado.elegidos;
    }

    return resultado;
}

MapaSombras::MapaSombras(Backend::IRenderBackend& backend)
    : backend_(backend) {}

MapaSombras::~MapaSombras() {
    for (Entrada& entrada : entradas_) {
        if (entrada.target != Backend::kInvalidHandle) {
            backend_.destroyDepthTarget(entrada.target);
        }
    }
    entradas_.clear();
}

Backend::Handle MapaSombras::preparar(int indice, int resolucion) {
    if (indice < 0) return Backend::kInvalidHandle;
    if (indice < static_cast<int>(entradas_.size())) {
        Entrada& entrada = entradas_[indice];
        if (entrada.resolucion == resolucion &&
            entrada.target != Backend::kInvalidHandle) {
            return backend_.depthTargetTexture(entrada.target);
        }
        // La resolucion cambio: el mapa viejo ya no sirve.
        backend_.destroyDepthTarget(entrada.target);
        entrada.target = Backend::kInvalidHandle;
        entrada.resolucion = 0;
    } else {
        entradas_.resize(static_cast<std::size_t>(indice) + 1);
    }

    if (resolucion <= 0) return Backend::kInvalidHandle;

    const Backend::Handle target = backend_.createDepthTarget(resolucion);
    if (target == Backend::kInvalidHandle) return Backend::kInvalidHandle;

    Entrada& entrada = entradas_[static_cast<std::size_t>(indice)];
    entrada.target = target;
    entrada.resolucion = resolucion;
    return backend_.depthTargetTexture(target);
}

void MapaSombras::liberarDesde(int indice) {
    if (indice < 0) indice = 0;
    for (std::size_t i = static_cast<std::size_t>(indice);
         i < entradas_.size(); ++i) {
        if (entradas_[i].target != Backend::kInvalidHandle) {
            backend_.destroyDepthTarget(entradas_[i].target);
        }
    }
    if (static_cast<std::size_t>(indice) < entradas_.size()) {
        entradas_.resize(static_cast<std::size_t>(indice));
    }
}

}  // namespace Rendering

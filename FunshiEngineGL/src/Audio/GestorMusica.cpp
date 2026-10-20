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
#include "GestorMusica.h"

#include <algorithm>

namespace {

float limitar(float valor, float minimo, float maximo) {
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

} // namespace

GestorMusica::GestorMusica(IMusicaBackend& backend) : backend_(backend) {}

// --- catalogo ---

void GestorMusica::registrarPista(const std::string& nombre,
                                  const std::string& ruta) {
    if (nombre.empty() || ruta.empty()) return;
    const auto it = rutas_.find(nombre);
    if (it != rutas_.end()) {
        it->second = ruta;
        return;
    }
    rutas_[nombre] = ruta;
    orden_.push_back(nombre);
}

void GestorMusica::limpiarCatalogo() {
    rutas_.clear();
    orden_.clear();
}

std::vector<std::string> GestorMusica::nombresPistas() const {
    return orden_;
}

bool GestorMusica::tienePista(const std::string& nombre) const {
    return rutas_.find(nombre) != rutas_.end();
}

int GestorMusica::indiceDe(const std::string& nombre) const {
    for (std::size_t i = 0; i < orden_.size(); ++i) {
        if (orden_[i] == nombre) return static_cast<int>(i);
    }
    return -1;
}

std::string GestorMusica::nombreSiguienteDeCatalogo() const {
    if (orden_.empty()) return std::string();
    if (indiceActual_ < 0) return orden_.front();
    const std::size_t siguiente =
        (static_cast<std::size_t>(indiceActual_) + 1) % orden_.size();
    return orden_[siguiente];
}

// --- reproduccion ---

bool GestorMusica::reproducir(const std::string& nombre, float volumen,
                              float duracionFundido) {
    if (!tienePista(nombre)) return false;
    const float duracion = duracionFundido < 0.0f ? duracionFundido_ : duracionFundido;
    return iniciarCambio(nombre, volumen, duracion);
}

bool GestorMusica::reproducirSiguiente(float volumen, float duracionFundido) {
    const std::string siguiente = nombreSiguienteDeCatalogo();
    if (siguiente.empty()) return false;
    const float duracion = duracionFundido < 0.0f ? duracionFundido_ : duracionFundido;
    return iniciarCambio(siguiente, volumen, duracion);
}

// Cambia de pista: abre la nueva, la pone a volumen 0 y deja que el fundido
// la suba mientras baja la que sonaba. Si no habia ninguna, el fundido solo
// sube la nueva.
bool GestorMusica::iniciarCambio(const std::string& nombre, float volumen,
                                 float duracionFundido) {
    const auto it = rutas_.find(nombre);
    if (it == rutas_.end()) return false;

    // Si habia un fundido en curso, la pista que estaba entrando se retira:
    // el fundido en curso se cancela y empieza otro.
    cancelarEntrada();

    const int id = backend_.abrirPista(it->second);
    if (id == 0) return false;
    if (!backend_.reproducirPista(id, 0.0f)) {
        backend_.cerrarPista(id);
        return false;
    }

    fundido_.activo = true;
    fundido_.idSaliente = idActual_;
    fundido_.idEntrante = id;
    fundido_.duracion = std::max(0.0f, duracionFundido);
    fundido_.transcurrido = 0.0f;
    fundido_.progreso = 0.0f;
    fundido_.volumenSaliente = volumenActual_;
    fundido_.volumenEntrante = 0.0f;
    fundido_.volumenObjetivo = limitar(volumen, 0.0f, 1.0f);
    fundido_.indiceSaliente = indiceActual_;
    fundido_.indiceEntrante = indiceDe(nombre);

    idSiguiente_ = id;
    indiceSiguiente_ = fundido_.indiceEntrante;
    nombreSiguiente_ = nombre;

    // Sin fundido el cambio es inmediato: la pista nueva suena ya con su
    // volumen y la vieja se para.
    if (fundido_.duracion <= 0.0f) completarFundido();
    return true;
}

// Retira la pista que estaba entrando en un fundido (si la hay) y deja el
// reproductor como estaba antes de ese fundido.
void GestorMusica::cancelarEntrada() {
    if (!fundido_.activo) return;
    if (fundido_.idEntrante != 0) {
        backend_.detenerPista(fundido_.idEntrante);
        backend_.cerrarPista(fundido_.idEntrante);
    }
    fundido_ = Fundido();
    idSiguiente_ = 0;
    indiceSiguiente_ = -1;
    nombreSiguiente_.clear();
}

void GestorMusica::avanzarFundido(float deltaTime) {
    fundido_.transcurrido += deltaTime;
    float t = fundido_.duracion > 0.0f ? fundido_.transcurrido / fundido_.duracion
                                      : 1.0f;
    t = limitar(t, 0.0f, 1.0f);
    fundido_.progreso = t;

    const float volumenSaliente = fundido_.volumenSaliente * (1.0f - t);
    const float volumenEntrante =
        fundido_.volumenEntrante +
        (fundido_.volumenObjetivo - fundido_.volumenEntrante) * t;
    if (fundido_.idSaliente != 0)
        backend_.volumenPista(fundido_.idSaliente, volumenSaliente);
    if (fundido_.idEntrante != 0)
        backend_.volumenPista(fundido_.idEntrante, volumenEntrante);

    if (t >= 1.0f) completarFundido();
}

// El fundido termino: la pista que salia se para y se cierra, y la que
// entra pasa a ser la actual.
void GestorMusica::completarFundido() {
    if (fundido_.idSaliente != 0) {
        backend_.detenerPista(fundido_.idSaliente);
        backend_.cerrarPista(fundido_.idSaliente);
    }
    // La pista que entra se queda con el volumen final del fundido (si no
    // hubo fundido, con el volumen pedido al reproducirla).
    if (fundido_.idEntrante != 0)
        backend_.volumenPista(fundido_.idEntrante, fundido_.volumenObjetivo);
    idActual_ = fundido_.idEntrante;
    volumenActual_ = fundido_.volumenObjetivo;
    indiceActual_ = fundido_.indiceEntrante;
    nombreActual_ = nombreSiguiente_;
    idSiguiente_ = 0;
    indiceSiguiente_ = -1;
    nombreSiguiente_.clear();
    fundido_ = Fundido();
}

void GestorMusica::detener(float duracionFundido) {
    // La pista que estaba entrando se retira: el fundido en curso se anula.
    cancelarEntrada();

    if (idActual_ == 0) return;
    if (duracionFundido > 0.0f) {
        // Fundido de salida: el volumen baja hasta cero y despues se para.
        fundido_.activo = true;
        fundido_.idSaliente = idActual_;
        fundido_.idEntrante = 0;
        fundido_.duracion = duracionFundido;
        fundido_.transcurrido = 0.0f;
        fundido_.progreso = 0.0f;
        fundido_.volumenSaliente = volumenActual_;
        fundido_.volumenEntrante = 0.0f;
        fundido_.volumenObjetivo = 0.0f;
        fundido_.indiceSaliente = indiceActual_;
        fundido_.indiceEntrante = -1;
        return;
    }

    backend_.detenerPista(idActual_);
    backend_.cerrarPista(idActual_);
    idActual_ = 0;
    volumenActual_ = 0.0f;
    indiceActual_ = -1;
    nombreActual_.clear();
}

// --- bus ---

void GestorMusica::volumenBus(float volumen) {
    volumenBus_ = limitar(volumen, 0.0f, 1.0f);
    backend_.volumenBus(volumenBus_);
}

float GestorMusica::volumenBusActual() const { return volumenBus_; }

void GestorMusica::silenciarBus(bool silenciado) {
    busSilenciado_ = silenciado;
    backend_.silenciarBus(silenciado);
}

bool GestorMusica::busSilenciado() const { return busSilenciado_; }

void GestorMusica::duracionFundido(float segundos) {
    duracionFundido_ = std::max(0.0f, segundos);
}

float GestorMusica::duracionFundidoActual() const { return duracionFundido_; }

// --- avance ---

void GestorMusica::actualizar(float deltaTime) {
    if (deltaTime < 0.0f) deltaTime = 0.0f;

    if (fundido_.activo) {
        avanzarFundido(deltaTime);
        return;
    }

    if (idActual_ == 0) return;

    // La pista actual se acabo: se pasa a la siguiente del catalogo.
    if (backend_.estadoPista(idActual_) == EstadoPista::Terminada) {
        const std::string siguiente = nombreSiguienteDeCatalogo();
        if (siguiente.empty()) {
            detener(0.0f);
        } else {
            iniciarCambio(siguiente, volumenActual_, duracionFundido_);
        }
        return;
    }

    // Si a la pista actual le queda justo el fundido, se empieza el cruce
    // antes de que se acabe: asi una pista entra mientras la otra sale.
    if (duracionFundido_ <= 0.0f) return;
    const float duracion = backend_.duracionPista(idActual_);
    const float posicion = backend_.posicionPista(idActual_);
    if (duracion <= 0.0f || posicion < 0.0f) return;
    if ((duracion - posicion) > duracionFundido_) return;

    const std::string siguiente = nombreSiguienteDeCatalogo();
    if (siguiente.empty()) return;
    iniciarCambio(siguiente, volumenActual_, duracionFundido_);
}

// --- estado ---

const std::string& GestorMusica::pistaActual() const { return nombreActual_; }

const std::string& GestorMusica::pistaSiguiente() const {
    return nombreSiguiente_;
}

bool GestorMusica::sonando() const { return idActual_ != 0; }

bool GestorMusica::enFundido() const { return fundido_.activo; }

float GestorMusica::progresoFundido() const { return fundido_.progreso; }

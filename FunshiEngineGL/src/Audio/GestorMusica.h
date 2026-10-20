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
#ifndef GESTORMUSICA_H
#define GESTORMUSICA_H

#include <string>
#include <unordered_map>
#include <vector>

#include "IMusicaBackend.h"

// Reproductor de musica: que pista suena, cual es la siguiente, cuando hay
// que fundir una con otra y que volumen tiene el bus.
//
// No sabe nada de hilos ni del motor: recibe el backend con el que sonar y lo
// avanza con el deltaTime de la escena. Todo su estado vive aqui, asi que se
// puede probar entero sin audio (con un backend falso).
class GestorMusica {
public:
    explicit GestorMusica(IMusicaBackend& backend);

    // --- catalogo ---
    // Registra una pista del proyecto (nombre -> ruta). El orden de registro
    // es el orden del catalogo, que es el que se sigue al pasar a la pista
    // siguiente. Registrar un nombre que ya existe cambia su ruta.
    void registrarPista(const std::string& nombre, const std::string& ruta);
    void limpiarCatalogo();
    std::vector<std::string> nombresPistas() const;
    bool tienePista(const std::string& nombre) const;

    // --- reproduccion ---
    // Empieza a sonar una pista del catalogo. Si ya suena otra, se funden:
    // la que suena baja y la nueva sube durante el fundido. Con
    // `duracionFundido` negativa se usa la duracion por defecto del
    // reproductor; con 0 el cambio es inmediato.
    // Devuelve false si la pista no existe o el backend no pudo abrirla.
    bool reproducir(const std::string& nombre, float volumen = 1.0f,
                    float duracionFundido = -1.0f);
    // Pasa a la pista siguiente del catalogo (con fundido).
    bool reproducirSiguiente(float volumen = 1.0f, float duracionFundido = -1.0f);
    // Detiene la musica. Con un fundido > 0 el volumen baja a cero antes de
    // parar.
    void detener(float duracionFundido = 0.0f);

    // --- bus ---
    void volumenBus(float volumen);
    float volumenBusActual() const;
    void silenciarBus(bool silenciado);
    bool busSilenciado() const;

    // Duracion por defecto de los fundidos, en segundos.
    void duracionFundido(float segundos);
    float duracionFundidoActual() const;

    // --- avance ---
    // Avanza el reproductor: el fundido en curso y el paso automatico a la
    // pista siguiente cuando la actual se acaba (o cuando le queda justo el
    // fundido). Lo llama la escena con su deltaTime.
    void actualizar(float deltaTime);

    // --- estado ---
    // La pista que suena ahora (vacia si no suena ninguna).
    const std::string& pistaActual() const;
    // La pista que esta entrando en el fundido en curso (vacia si no hay).
    const std::string& pistaSiguiente() const;
    bool sonando() const;
    bool enFundido() const;
    // Progreso del fundido en curso, de 0 a 1 (0 si no hay fundido).
    float progresoFundido() const;

private:
    // Estado de un fundido en curso. `idSaliente` es la pista que se va (0 si
    // no hay ninguna: cuando solo entra una pista) e `idEntrante` la que
    // entra (0 si solo se va una).
    struct Fundido {
        bool activo = false;
        int idSaliente = 0;
        int idEntrante = 0;
        float duracion = 0.0f;
        float transcurrido = 0.0f;
        float progreso = 0.0f;
        float volumenSaliente = 1.0f;
        float volumenEntrante = 0.0f;
        float volumenObjetivo = 1.0f;
        int indiceSaliente = -1;
        int indiceEntrante = -1;
    };

    bool iniciarCambio(const std::string& nombre, float volumen,
                       float duracionFundido);
    void avanzarFundido(float deltaTime);
    void completarFundido();
    void cancelarEntrada();
    std::string nombreSiguienteDeCatalogo() const;
    int indiceDe(const std::string& nombre) const;

    IMusicaBackend& backend_;
    std::unordered_map<std::string, std::string> rutas_;
    std::vector<std::string> orden_;

    int idActual_ = 0;
    float volumenActual_ = 0.0f;
    int indiceActual_ = -1;
    std::string nombreActual_;

    int idSiguiente_ = 0;
    int indiceSiguiente_ = -1;
    std::string nombreSiguiente_;

    Fundido fundido_;
    float volumenBus_ = 1.0f;
    bool busSilenciado_ = false;
    float duracionFundido_ = 2.0f;
};

#endif // GESTORMUSICA_H

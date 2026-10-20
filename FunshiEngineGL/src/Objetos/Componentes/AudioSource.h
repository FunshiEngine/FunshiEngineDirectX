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
#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include "Component.h"

#include <string>
#include <utility>

class AudioEngine;

// Fuente de sonido como Component: se adjunta a un GameObject y reproduce un
// archivo de audio del proyecto (por RUTA). El usuario arrastra el archivo al
// componente (igual que con los scripts) y se guarda su ruta: el archivo puede
// estar en cualquier carpeta dentro de src<proyecto>/. El AudioEngine la
// inyecta la escena (solo en modo play). La reproduccion real corre en el
// hilo de audio del motor; aqui solo se guarda la ruta, el volumen, el loop y
// el estado del playback.
class AudioSource : public Component {
private:
    std::string rutaClip;           // ruta del archivo de audio
    float volume = 1.0f;
    bool loop = false;
    bool reproduccionAutomatica = false; // se reproduce al entrar en play
    int handleActual = -1;         // handle de AudioEngine::reproducir
    bool sonando = false;
    AudioEngine* motor = nullptr;  // inyectado por GameScene

protected:
    void serializeComponent(std::ofstream* file) override;
    void deserializeComponent(std::ifstream* file) override;

public:
    AudioSource() = default;

    void setMotor(AudioEngine* m) noexcept { motor = m; }
    AudioEngine* getMotor() const noexcept { return motor; }

    // Ruta del archivo de audio. En memoria es absoluta; al guardar la
    // escena se persiste relativa a la raiz de assets del proyecto, para que
    // la escena siga valida al mover o renombrar el proyecto.
    const std::string& getRutaClip() const noexcept { return rutaClip; }
    void setRutaClip(std::string ruta) noexcept { rutaClip = std::move(ruta); }

    float getVolume() const noexcept { return volume; }
    void setVolume(float v) noexcept { volume = v; }

    bool isLoop() const noexcept { return loop; }
    void setLoop(bool b) noexcept { loop = b; }

    bool isReproduccionAutomatica() const noexcept { return reproduccionAutomatica; }
    void setReproduccionAutomatica(bool b) noexcept { reproduccionAutomatica = b; }

    // Reprograma el handle si el clip cambio y el motor ya no tiene el anterior.
    void reproducir();
    void detener();
    bool isReproduciendo() const noexcept { return sonando; }
};

#endif // AUDIOSOURCE_H
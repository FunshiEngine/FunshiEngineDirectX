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
// Pruebas headless de la exportacion de juegos.
//
// El dialogo de exportacion es lo que el usuario toca para exportar. Lo que se
// comprueba aqui son sus reglas, que no dependen de la ventana:
//   - las precondiciones (proyecto elegido y con carpeta) dicen el motivo
//     cuando no se pueden cumplir,
//   - el estado de la exportacion conserva el resultado (exito o error) al
//     terminar, para que la interfaz pueda mostrarlo en vez de perderlo,
//   - la barra de menu le pasa al dialogo el proyecto activo.
//
// Patron de autoria (ver AGENTS.md): CHECK definido en este archivo, sin
// dependencias de GUI, y salida final "OK/FALLOS: N comprobaciones".

#include <filesystem>
#include <iostream>
#include <string>

#include "../FunshiEngineGL/src/Exportador/EstadoExportacion.h"
#include "../FunshiEngineGL/src/Exportador/PrecondicionesExportacion.h"
#include "../FunshiEngineGL/src/GUI/Export/ExportDialog.h"
#include "../FunshiEngineGL/src/GUI/SceneGUI/SceneMenuBarInterface.h"

int total = 0;
int fallos = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++total;                                                               \
        if (!(cond)) {                                                         \
            ++fallos;                                                          \
            std::cout << "  [FALLO] " << msg << std::endl;                     \
        }                                                                      \
    } while (0)

// Carpeta temporal que se borra al salir (RAII).
struct CarpetaTemporal {
    std::filesystem::path ruta;

    explicit CarpetaTemporal(const std::string& nombre) {
        ruta = std::filesystem::temp_directory_path() /
               ("funshi_exportacion_" + nombre);
        std::error_code ec;
        std::filesystem::remove_all(ruta, ec);
        std::filesystem::create_directories(ruta, ec);
    }

    ~CarpetaTemporal() {
        std::error_code ec;
        std::filesystem::remove_all(ruta, ec);
    }
};

void comprobarPrecondiciones() {
    std::string error;
    CHECK(!PrecondicionesExportacion::comprobar("", "", error),
          "sin proyecto no se puede exportar");
    CHECK(!error.empty(), "sin proyecto se explica el motivo");
    CHECK(error.find("proyecto") != std::string::npos,
          "el motivo menciona el proyecto que falta");

    error.clear();
    CHECK(!PrecondicionesExportacion::comprobar("MiProyecto",
                                                 "/no/existe/este/proyecto",
                                                 error),
          "una carpeta inexistente impide exportar");
    CHECK(error.find("no existe") != std::string::npos,
          "el motivo dice que el proyecto no existe");

    CarpetaTemporal carpeta("proyecto_valido");
    error.clear();
    CHECK(PrecondicionesExportacion::comprobar("MiProyecto",
                                                carpeta.ruta.string(), error),
          "un proyecto con carpeta se puede exportar");
    CHECK(error.empty(), "sin motivo de error cuando todo esta bien");
}

void comprobarEstado() {
    EstadoExportacion estado;
    CHECK(estado.fase() == EstadoExportacion::Fase::Configuracion,
          "el estado arranca en configuracion");
    CHECK(!estado.hayMensaje(), "sin mensaje antes de exportar");

    estado.iniciar();
    CHECK(estado.exportando(), "al iniciar esta exportando");
    CHECK(estado.progreso() == 0.0f, "el progreso vuelve a cero");
    CHECK(estado.resultado() == EstadoExportacion::Resultado::Pendiente,
          "todavia no hay resultado");

    estado.registrarProgreso(0.5f, "Compilando");
    CHECK(estado.progreso() == 0.5f, "el progreso se actualiza");
    CHECK(estado.etapa() == "Compilando", "la etapa se actualiza");

    estado.registrarLog("una linea de bitacora");
    CHECK(estado.mensaje() == "una linea de bitacora", "la bitacora se guarda");

    // El resultado no se pierde al terminar: es lo que le dice al usuario que
    // paso con lo que pidio.
    estado.finalizar(false, "Error generando CMake");
    CHECK(estado.terminado(), "al terminar queda en resultado");
    CHECK(estado.resultado() == EstadoExportacion::Resultado::Error,
          "un fallo se marca como error");
    CHECK(estado.hayMensaje(), "el motivo del fallo se conserva");
    CHECK(estado.mensaje() == "Error generando CMake",
          "el motivo del fallo es el que se dio");

    estado.reiniciar();
    CHECK(estado.fase() == EstadoExportacion::Fase::Configuracion,
          "reiniciar vuelve al formulario");
    CHECK(!estado.hayMensaje(), "reiniciar descarta el mensaje anterior");

    EstadoExportacion conExito;
    conExito.iniciar();
    conExito.finalizar(true, "Exportacion completada");
    CHECK(conExito.resultado() == EstadoExportacion::Resultado::Exito,
          "el exito se marca como exito");
    CHECK(conExito.progreso() == 1.0f, "con exito el progreso llega a uno");
    CHECK(conExito.mensaje() == "Exportacion completada",
          "el mensaje de exito se conserva");
}

void comprobarDialogo() {
    // Sin proyecto no hay nada que exportar: el dialogo tiene que decirlo, no
    // quedarse quieto como si el boton no hubiera reaccionado.
    ExportDialog sinProyecto("");
    sinProyecto.iniciarExportacion();
    CHECK(sinProyecto.vista() == ExportDialog::Vista::Resultado,
          "sin proyecto el dialogo muestra el resultado, no el formulario");
    CHECK(sinProyecto.estado().resultado() ==
              EstadoExportacion::Resultado::Error,
          "sin proyecto el resultado es un error");
    CHECK(sinProyecto.estado().hayMensaje(),
          "sin proyecto se conserva el motivo");

    // Un proyecto cuyo folder no existe tampoco se puede exportar.
    ExportDialog sinCarpeta("ProyectoQueNoExiste");
    sinCarpeta.iniciarExportacion();
    CHECK(sinCarpeta.vista() == ExportDialog::Vista::Resultado,
          "una carpeta inexistente deja el resultado a la vista");
    CHECK(sinCarpeta.estado().hayMensaje(),
          "una carpeta inexistente conserva el motivo");
}

void comprobarBarraDeMenu() {
    // La barra de menu refleja el proyecto activo en el dialogo: sin esto la
    // exportacion no sabe que proyecto exportar.
    SceneMenuBarInterface barra(true);
    barra.setProyectoActual("MiProyecto");
    barra.abrirDialogoExportacion();
    CHECK(barra.dialogoExportacion() != nullptr, "el dialogo se abre");
    CHECK(barra.dialogoExportacion()->proyecto() == "MiProyecto",
          "el dialogo recibe el proyecto activo");
    CHECK(barra.proyectoActual() == "MiProyecto",
          "la barra recuerda el proyecto activo");

    // Cambiar de proyecto con el dialogo ya abierto tambien lo actualiza.
    barra.setProyectoActual("OtroProyecto");
    CHECK(barra.dialogoExportacion()->proyecto() == "OtroProyecto",
          "el dialogo abierto sigue al proyecto activo");

    // Sin proyecto, el dialogo lo tiene vacio (y lo dira al exportar).
    SceneMenuBarInterface sinProyecto(true);
    sinProyecto.abrirDialogoExportacion();
    CHECK(sinProyecto.dialogoExportacion()->proyecto().empty(),
          "sin proyecto el dialogo lo tiene vacio");
}

int main() {
    comprobarPrecondiciones();
    comprobarEstado();
    comprobarDialogo();
    comprobarBarraDeMenu();

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}

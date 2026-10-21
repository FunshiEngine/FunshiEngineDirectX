# Plan de integración de los sistemas nuevos

Este documento explica, sistema por sistema, cómo se integra en el motor cada
una de las funcionalidades que faltan. Para cada una se detalla:

1. **Argumentos**: qué se pide, qué hace el motor hoy y por qué no alcanza.
2. **Arquitectura**: los contratos, las carpetas y los archivos, con el criterio
   de acoplamiento que se respeta.
3. **Documentación de arquitectura**: qué documentos del proyecto cambian y qué
   pasan a decir.
4. **Plan de integración**: el orden de los pasos, con el tamaño de cada uno y
   los riesgos que aparecen en cada paso.
5. **Análisis de resultados y testers**: qué se comprueba automáticamente, con
   qué suite headless y qué queda para la validación manual.

El índice y el estado de cada sistema están en
[TAREAS_PENDIENTES.md](TAREAS_PENDIENTES.md).

Las referencias a archivos y números de línea corresponden al estado del
repositorio en el momento de escribir este documento; si un archivo se mueve,
la cita es la que hay que actualizar.

---

## 0. Criterio común a todos los sistemas

### 0.1 Los patrones que el motor ya usa (y que el plan reutiliza)

| Patrón | Dónde está hoy | Cómo lo aplica un sistema nuevo |
|---|---|---|
| Contrato + implementación | `IRenderBackend`/`OpenGL3Backend`, `IPhysicsBackend`/`BulletPhysicsAdapter`, `IWebUIBackend`/`BackendCef`, `IMeshLoader`/`AssimpMeshLoader`, `ITextureLoader`/`StbImageLoader` | El sistema nuevo define su interfaz y su implementación, y el motor enlaza la implementación en el punto de arranque (`main.cpp`, `GameScene`). |
| DTO plano entre capas | `SpriteDraw` (`src/Rendering/SpriteRenderer.h:42`), `TilemapDraw` (`src/Rendering/TilemapRenderer.h:44`), `FrameContext` (`src/Rendering/SceneRenderer.h:83`) | La capa de entidades nunca se pasa al render: se pasan datos. Un sistema nuevo entrega sus datos al renderer igual. |
| Catálogo de componentes | `CatalogoComponentes` (`src/Objetos/Componentes/CatalogoComponentes.h:95`) | Un componente nuevo se registra en el catálogo y aparece solo en el inspector, en el menú y en la serialización. |
| Componente serializado por tipo | `Entity::serializeEntityComponents` (`src/Entity/Entity.cpp:311`), nombre del tipo como clave (`nombreSimpleTipo`) | Un componente nuevo es aditivo: no se tocan los existentes, porque el formato binario no tiene versionado. |
| Tabla de punteros a función para scripts | `ApiScriptGameObject`, `ScriptServices` (`src/Behaviour/ScriptGameObject.h:43`, `:130`) | Lo que un sistema ofrezca a los scripts entra por ahí, append-only, con su `version`. |
| CPU puro testeable | `LineBuilder` (`src/Rendering/LineBuilder.h:52`), `Primitivas2D::quad()` (`src/Assets/Primitivas2D.h:35`), `Tilemap::construirGeometria` (`src/Objetos/Componentes/Tilemap.cpp:171`) | La lógica de cada sistema (curvas, filtros, layout, terreno) vive fuera de la GUI y del render, y se prueba en headless. |

### 0.2 Reglas de acoplamiento

- Ningún módulo nuevo depende de ImGui, de CEF ni de `GameScene`.
- El render se inyecta (`setBackend`), no se busca en un global
  (`Rendering::Backend::activeBackend()` sigue existiendo, pero no se usa en
  código nuevo).
- La GUI no conoce el backend gráfico: recibe identificadores de textura ya
  resueltos (como hace hoy `backend.imguiTextureId`).
- Los contratos de scripts y de mods son append-only; los de la CLI y de la
  inspección llevan versión propia, para no invalidar la caché de scripts
  compilados.

### 0.3 Reglas de trabajo

- Cada sistema se integra en commits pequeños; el build y la suite quedan en
  verde en cada commit.
- La documentación del proyecto se actualiza en el mismo commit que el código.
- Cada sistema lleva su suite headless en `tests/`, con el patrón de `CHECK`
  y `TempPruebas::CarpetaPrueba`, y su target en el `CMakeLists.txt` y en las
  listas de la CI.
- Lo que no se puede comprobar sin ventana (lo visual) queda para la
  validación manual del usuario, y se dice explícitamente en cada sistema.

---

## 1. Orden de integración

El orden no es por dificultad, sino por dependencia: primero las bases de las
que cuelgan los demás, y al final lo que reescribe lo ya validado.

| Paso | Sistema | Por qué va aquí |
|---|---|---|
| 1 | S6 Iconos de componente | No depende de nada y desbloquea el patrón "el catálogo describe el componente" que reusan S4 y S5. |
| 2 | S4 Materiales por regiones | Cambia la carga de mallas y el contrato de dibujo; es la base de S5 y de S3. |
| 3 | S10 Arrastrar y pegar del sistema operativo | Independiente del resto; toca el explorador y la E/S del sistema. |
| 4 | S11 API de inspección | Independiente; define el contrato con la CLI y con el editor. |
| 5 | S5 Vista previa en esfera | Depende de S4 (necesita materiales por región) y del render a textura que ya existe. |
| 6 | S7 Partículas y efectos | Introduce el batch de geometría dinámica que reutiliza S8. |
| 7 | S9 Interfaz tradicional | Introduce el render en espacio de pantalla, que S5 no necesita pero que abre la puerta a HUD propio. |
| 8 | S1 Máquina de estados | Base de S2 y de S3. |
| 9 | S8 Terrenos | Reutiliza el patrón de geometría incremental de S7. |
| 10 | S2 Editor visual de máquinas | Necesita el modelo de S1 y el grafo de ImGuizmo. |
| 11 | S3 Animación con huesos | Lo más caro: toca carga de mallas, shader, pase de sombras y la pose. |

---

## S6. Icono de cada componente en el Inspector

### Argumentos

El Inspector lista los componentes de un objeto y los dibuja con un
`CollapsingHeader` que solo lleva el nombre
(`src/GUI/ObjetosGUI/SettingsObjectInterface.cpp:415`). Cada componente debería
tener su icono a la izquierda del nombre, como en la mayoría de los motores.

El motor ya tiene las imágenes: `FunshiEngineGL/Imagenes/GUI-Image/Component-Image/`
contiene el juego completo de iconos de componente (512×512, estilo Unity:
`Material/material-white.png`, `Model/model-white.png`,
`Iluminacion/comp-light-directional-white.png`,
`Fisicas/comp-collider-box-white.png`, `Interfaces/comp-ui-*.png`, y más de
veinticinco más). **Ningún archivo del código los referencia**: están
preparados y sin usar.

Falta además el dato que los relaciona con su componente: hoy no existe ningún
mapa tipo de componente → icono. Lo más cercano es
`IconosGUI::getIconoPorExtension` (`src/Herramientas/IconosGUI/IconosGUI.cpp:222`),
que es un mapa extensión de archivo → icono.

### Arquitectura

**El dato va en el catálogo de componentes.** `DescriptorComponente`
(`src/Objetos/Componentes/CatalogoComponentes.h:67`) ya declara todo lo que
el inspector necesita saber de un componente: nombre, etiqueta, alias,
dimensiones, submenú, prerequisito, creador y panel. Añadir un campo `icono`
es el punto de extensión natural: el inspector no necesita una segunda tabla
que pueda desincronizarse del catálogo.

```
DescriptorComponente
  nombre      "Transform"
  etiqueta    "Transform"
  dimensiones DosD|TresD|Ambas
  submenu     ""
  icono       "GUI-Image/Component-Image/Transform/comp-transform-white.png"   ← nuevo
  crear       std::function<std::unique_ptr<Component>(GameObject&)>
  crearPanel  std::function<...>
```

**El icono se deriva del tipo, nunca se persiste.** El nombre del tipo ya es
la clave persistida de la escena (`Entity::serializeEntityComponents`,
`src/Entity/Entity.cpp:335`), así que guardar además el icono duplicaría la
fuente de verdad y obligaría a migrar las escenas si el icono cambia.

**El inspector recibe un resolvedor, no el gestor de iconos.**
`IconosGUI` (`src/Herramientas/IconosGUI/IconosGUI.h:33`) es hoy a la vez
proveedor, caché y dueño de recursos GPU: 39 handles que destruye en su
destructor. Para que el Inspector dependa de una interfaz y no de esa clase:

```
src/Herramientas/IconosGUI/
  IReglaIcono.h        ← tabla tipo → ruta de icono (CPU puro, testeable en headless)
  TablaIconos.h/.cpp   ← implementación de la tabla
  IResolutorIconos.h   ← tipo → ImTextureID (la interfaz que consume el Inspector)
  CatalogoIconos.h/.cpp← implementación con GPU (envuelve a IconosGUI)
```

`TablaIconos` es CPU puro y se prueba en headless: que la tabla esté completa,
que no tenga duplicados, que todo componente del catálogo tenga icono y que
exista el archivo correspondiente en `Imagenes/`.

**El punto de dibujado** es `SettingsObjectInterface::contentGUI`, justo antes
del `CollapsingHeader` (`src/GUI/ObjetosGUI/SettingsObjectInterface.cpp:415`),
con el mismo patrón que ya usa el árbol de archivos
(`src/GUI/FileManagerGUI/TreeFilesInterface.cpp:142`): `ImGui::Image` y
`ImGui::SameLine`. En el inspector el tamaño razonable es 16×16 (el alto del
encabezado es de unos 19 px); en el árbol se usan 22×22.

**Inyección**: `ContextoPanel` (`src/Objetos/Componentes/CatalogoComponentes.h:60`)
lleva ya `editor`, `audio` y `dimension`; se le añade el resolvedor de iconos,
que `GUIManager::bindScene` inyecta igual que hace con el resto.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: el bloque `src/Herramientas/IconosGUI/` describe las
  cuatro piezas nuevas y su papel; el bloque del inspector menciona que cada
  componente se dibuja con su icono.
- `MANUAL_DE_USO.md`: la sección del inspector explica que cada componente
  muestra su icono y que el juego viene con el juego completo de iconos.
- `AGENTS.md`: no cambia ninguna regla.

### Plan de integración

1. `TablaIconos` con la tabla tipo → ruta, y su suite headless. No toca la GUI:
   se puede revisar entero en un commit.
2. `DescriptorComponente::icono` y el registro de los iconos existentes en el
   catálogo (un `icono` por entrada, con la ruta relativa a `Imagenes/`).
3. `IconosGUI` carga los iconos de componente (los de `Component-Image/`) y
   expone el lookup; `IResolutorIconos` + `CatalogoIconos` lo envuelven.
4. El Inspector dibuja el icono antes del encabezado; la inyección del
   resolvedor entra por `ContextoPanel`.
5. Comprobación de que todo componente del catálogo tiene icono (suite del
   catálogo ampliada).

Riesgo: la lista de iconos del motor y la tabla de tipos pueden divergir. Se
cubre con el test de integridad de la tabla, que falla si un componente del
catálogo no tiene icono.

### Análisis de resultados y testers

Suite nueva `tests/IconosComponentesTests.cpp` (CPU puro, sin GPU):

- La tabla tiene una entrada por cada componente registrado en el catálogo
  (`CatalogoComponentes::instancia().descriptores()`), sin entradas de más.
- No hay dos tipos con la misma ruta de icono, salvo los alias declarados.
- Toda ruta de la tabla corresponde a un archivo existente bajo
  `Imagenes/GUI-Image/Component-Image/`.
- El catálogo declara un icono no vacío para cada componente.
- Un tipo inexistente devuelve "sin icono" en vez de fallar.

Comprobación manual (la parte visual): abrir el inspector con un objeto que
tenga `Transform`, `Model`, `Material` y un `RigidBody`, y ver el icono a la
izquierda de cada nombre, alineado con el texto y del mismo tamaño.

---

## S4. Materiales por regiones de un modelo 3D

### Argumentos

Hoy un modelo tiene **un** material. `MeshRenderer::aplicarMaterial`
(`src/Rendering/MeshRenderer.cpp:225`) sube los uniforms del material del
objeto, y `aplicarTexturas` (`:284`) enlaza sus cuatro texturas: es un solo
juego de uniforms por objeto y un solo draw call.

La información para hacerlo mejor existe en el archivo y se pierde al cargar:
`AssimpMeshLoader::load` (`src/Assets/AssimpMeshLoader.cpp:38`) concatena todas
las sub-mallas en una sola malla y **nunca lee** `aiMesh::mMaterialIndex` ni
`aiScene::mMaterials[]`. Una malla de un `.fbx` con tres materiales llega al
motor como una geometría y un material.

Lo que se pide es poder asociar texturas a **certainas partes** de un modelo:
un cubo con material en una cara, un personaje con material por región.

### Arquitectura

**La malla conserva las regiones.** `struct Mesh`
(`src/Assets/Mesh.h:32`) suma un vector de sub-mallas, con datos puros:

```
struct SubMalla {
    std::string nombreMaterial;   // nombre del material en el archivo
    unsigned int indiceInicio;    // rango contiguo de índices
    unsigned int indiceCantidad;
};
```

**El componente `Material` sigue siendo el material único** (no se toca: su
formato binario `FMtB` ya está en escenas guardadas) y aparece un componente
nuevo `Materiales`, que es una lista de regiones con su material:

```
Materiales
  regiones: [{ nombreRegion, material }]
```

ElInspector muestra una fila por región: nombre, color y sus cuatro texturas.

**Un contrato de resolución**, para que el render no dependa de un tipo
concreto de componente:

```
src/Objetos/Componentes/
  IMaterial.h             ← vista de solo lectura de un material (colores y rutas)
  IResolutorMateriales.h  ← (objeto, índice de sub-malla) → IMaterial*
```

`IResolutorMateriales` tiene dos implementaciones: la del componente `Material`
actual (devuelve siempre el slot 0) y la del componente `Materiales` (devuelve
el material de la región que corresponde a la sub-malla). El render solo
conoce el contrato.

**El backend dibuja rangos de índices.** `IRenderBackend::drawMesh`
(`src/Rendering/Backend/IRenderBackend.h:88`) no admite rango; hace falta
`drawMesh(handle, primerIndice, cantidadIndices)`. Como el contrato tiene
implementaciones de prueba (`tests/ShadersTests.cpp:62` y otros), el método se
añade con un valor por defecto para no romper los dobles de los tests, o se
extiende el mismo grupo de fakes en el mismo commit.

**El shader no cambia**: el juego de uniforms sigue siendo uno; lo que cambia
es que `aplicarMaterial` y `aplicarTexturas` se ejecutan **dentro** del bucle
de sub-mallas, una vez por región, y el draw se hace con el rango de índices
de esa región.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: el bloque de `Assets/Mesh.h` describe las sub-mallas;
  el de `Objetos/Componentes/` describe `Materiales`, `IMaterial` e
  `IResolutorMateriales`; el de `Rendering/Backend/` anota el draw por rango.
- `MANUAL_DE_USO.md`: sección de materiales: un modelo puede traer varias
  regiones, cada una con su material, y cómo se editan en el Inspector.
- `DocuTecnicoBugs.md`: si al cargar un archivo con materiales múltiples la
  geometría sale mal, el patrón a registrar es "el cargador aplana la
  jerarquía del archivo y con ella la información de materiales".

### Plan de integración

1. `SubMalla` en `struct Mesh` y lectura en `AssimpMeshLoader` (índice de
   material y nombre por sub-malla). Es un cambio acotado y verificable: la
   suite de assets comprueba que una malla con dos materiales produce dos
   sub-mallas con rangos que no se solapan.
2. Draw por rango en `IRenderBackend` y en `MeshGPU`, con los fakes de los
   tests actualizados en el mismo commit.
3. `IMaterial` e `IResolutorMateriales`, con la implementación del material
   único actual: el render deja de incluir `Objetos/Componentes/Material.h`.
4. Componente `Materiales` (con su marca de formato propia) y su panel.
5. `MeshRenderer` recorre las sub-mallas: material y texturas por región.

Riesgo: los archivos con materiales mal declarados (mismo nombre, materiales
vacíos) producen sub-mallas sin material; el resolvedor devuelve el material
por defecto y el motor no falla.

### Análisis de resultados y testers

Suite `tests/MaterialesRegionesTests.cpp`:

- Una malla con dos materiales produce dos sub-mallas, con rangos de índices
  contiguos, sin solaparse y cuya suma es el total de índices.
- El resolvedor devuelve el material de la región correcta para cada índice de
  sub-malla, y el material por defecto cuando la región no tiene material.
- El componente `Materiales` hace round-trip binario (escritura, lectura,
  comparación) y el formato viejo se lee sin desalinear el stream (patrón de
  `Material.cpp:101`).
- Clonar un objeto con `Materiales` conserva sus regiones (el clonador usa
  el mismo formato de componente, `src/Objetos/ClonadorObjetos.cpp:37`).
- Renombrar una carpeta reescribe las rutas de textura de las regiones (lo
  cubre `tests/SceneSerializationTests.cpp:1119` con el material simple; el
  mismo caso con regiones).

Comprobación manual: importar un modelo con varios materiales y verlo con
texturas distintas por región en el viewport y en el juego exportado.


---

## S10. Arrastrar y pegar archivos del sistema operativo

### Argumentos

Hoy el explorador del editor mueve y copia archivos **dentro** del proyecto
(`src/GUI/FileManagerGUI/SoltarEnCarpeta.h:69`), pero no acepta archivos que
vienen del escritorio: no hay ninguna integración con el drop del sistema
operativo ni con el portapapeles del sistema.

Falta, en concreto:

- Poder arrastrar archivos desde el explorador de archivos del sistema hacia la
  ventana del editor y que se copien dentro de la carpeta que se está
  mostrando (la que corresponde al ShowFolder).
- Poder cortar y pegar desde el portapapeles del sistema hacia esa misma
  carpeta.
- Que al pegar se recargue el panel para que el archivo nuevo aparezca.

El motor ya tiene la pieza clave de la tercera parte: la invalidación
explícita de la cache del grid (`ContentFolderInterface::invalidarCache`), que
es el patrón documentado como R7 en `DocuTecnicoBugs.md` (no confiar en el
`mtime`).

### Arquitectura

**El sistema operativo se habla desde un solo sitio.** GLFW entrega el drop a
nivel de ventana (`glfwSetDropCallback`), no de panel: el backend del sistema
operativo no sabe de ImGui, y el explorador no debe saber de GLFW.

```
src/GestorDeArchivos/   (el módulo que ya posee la E/S de plataforma)
  IReceptorDrop.h           ← archivos soltados sobre la ventana (contrato)
  IServicioPortapapeles.h   ← archivos que hay en el portapapeles del sistema
  IServicioCopiaArchivos.h  ← copia desde fuera del proyecto al destino
  ReceptorDropGlfw.h/.cpp       ← implementación (glfwSetDropCallback)
  PortapapelesWindows.h/.cpp    ← CF_HDROP / DROPFILES
  PortapapelesLinux.h/.cpp      ← text/uri-list
```

El portapapeles del sistema **no** es texto: en Windows es una lista de
`HDROP`, y en Linux es `text/uri-list`. Por eso `glfwGetClipboardString` no
alcanza y hace falta un lector por plataforma.

**La lógica de "qué hacer con las rutas pegadas" es un helper puro.** Es el
mismo criterio que el motor ya aplicó con `SoltarEnCarpeta`, `CrearCarpeta` y
`RenombrarElemento`: la lógica vive en un header sin ImGui, y la GUI la llama.
Así se puede probar en headless, sin ventana.

```
src/GUI/FileManagerGUI/ImportarArchivos.h
  namespace ImportarArchivos {
    struct Resultado { int copiados; std::vector<std::string> errores; };
    Resultado pegar(FileManager&, const std::string& destino,
                    const std::vector<std::string>& rutas);
  }
```

**La carpeta destino es la carpeta visible, en el frame del drop.** El panel
calcula el destino cuando ocurre el drop (nunca se guarda entre frames: el
objeto de carpeta cuelga del árbol y cambia al navegar).

**El pegado no publica `ArchivosReubicados`:** una copia desde fuera del
proyecto no cambia ninguna ruta de la escena. Sí sube el contador de cambios
del árbol y **invalida la cache del grid** (`invalidarCache()`), que es lo que
hace que el archivo nuevo aparezca.

**Política de colisión: no pisar.** `FileManager::copiarArchivo` pisa
(`src/GestorDeArchivos/GestorDeArchivos.cpp:220`) y `mover` no (`:262`). Al
pegar desde el sistema conviene no pisar y generar un nombre único
(`archivo (1).txt`), que es lo que el usuario espera del sistema.

### Documentación de arquitectura

- `MANUAL_DE_USO.md`, sección del explorador: cómo arrastrar archivos del
  sistema, cómo pegar con Ctrl+V, qué pasa si el nombre ya existe.
- `PROJECT_STRUCTURE.md`: el bloque del explorador describe los tres
  servicios nuevos y el helper `ImportarArchivos`.
- `DocuTecnicoBugs.md`: el caso ya presente de "Copy Exist File"
  (`src/GUI/FileManagerGUI/ContentFolderInterface.cpp:362`) copia sin invalidar
  la cache: es la misma familia de R7 y se corrige en este trabajo.

### Plan de integración

1. `ImportarArchivos` (helper puro) y su suite, sin tocar la GUI. El test
   escribe un archivo fuera del proyecto, lo pega en una carpeta temporal y
   comprueba el resultado.
2. `IServicioCopiaArchivos` e implementación con `std::filesystem` (común a las
   dos plataformas), con la política de no pisar.
3. `IServicioPortapapeles` y las dos implementaciones de plataforma. Los
   parsers (`text/uri-list` y `DROPFILES`) son funciones puras y se prueban en
   headless en ambas plataformas.
4. `IReceptorDrop` y `ReceptorDropGlfw`, registrado en `main.cpp` después de
   crear la ventana (`src/main.cpp:324`); el juego exportado no lo registra
   (`src/Runtime/main_juego.cpp` no crea explorador).
5. El panel recibe los tres servicios por setter y los usa: drop sobre la
   ventana del folder, Ctrl+V con el patrón de `SceneObjectTree::manejarAtajos`
   (`src/GUI/SceneGUI/SceneObjectTree.cpp:462`), invalidación de la cache y
   subida del contador de cambios.
6. Corrección del caso "Copy Exist File" para que invalide la cache.

Riesgo: el portapapeles del sistema no existe en un servidor sin escritorio.
La degradación es explícita: el comando avisa que no hay portapapeles
disponible, sin fallar.

### Análisis de resultados y testers

Suite `tests/ImportarArchivosTests.cpp` (o ampliación de
`tests/FileManagerTests.cpp`):

- Pegar un archivo copia su contenido intacto en el destino.
- Pegar un archivo con nombre repetido **no pisa** el existente y devuelve el
  error con el motivo.
- Pegar una carpeta copia su contenido recursivamente.
- Pegar una ruta inexistente devuelve el error y no aborta el resto.
- Tras pegar, el contador de cambios del árbol sube y la cache del grid queda
  invalidada: la comprobación es sobre el valor de invalidación, no sobre el
  `mtime` (el `mtime` no es fiable en Windows con el CRT de MinGW; patrón R16
  de `DocuTecnicoBugs.md`).
- El parser de `text/uri-list` acepta rutas con espacios y no-ASCII, y
  descarta las líneas que no son URIs de archivo.
- El parser de `DROPFILES` de Windows devuelve las rutas en el orden del
  `HDROP`.

Comprobación manual: arrastrar un archivo desde el explorador del sistema
sobre la ventana del editor y verlo aparecer dentro de la carpeta mostrada;
copiar un archivo en el sistema, pegar con Ctrl+V sobre el panel y verlo
aparecer; comprobar que en Linux sin `xclip` el pegado avisa en vez de fallar.

---

## S11. API de inspección de escena y CLI de construcción

### Argumentos

El objetivo es poder desarrollar un juego entero desde la consola: listar
objetos con filtros (componente, tag, id, nombre, tipo de objeto), ver los
componentes de un objeto con sus variables y valores, crear componentes,
asignar variables y consultar posiciones.

**La CLI es un proyecto separado** (`FunshiEngineCLI`), no una parte de este
repositorio: el motor publica el contrato y la CLI lo consume. Hoy el motor
expone a los scripts una tabla de punteros a función con lo justo para
manipular objetos (`objetoPorNombre`, `objetoPorId`, `objetoPorEtiqueta`,
`crearObjeto`, `agregarColliderEsfera`… en
`src/Behaviour/ScriptGameObject.h:130`), pero **no** expone:

- Un filtro combinable sobre la colección de objetos.
- Una enumeración de los componentes de un objeto con sus variables y valores.
- Una forma de crear un componente por nombre de tipo.
- Una forma de asignar una variable de un componente.

La CLI, además, hoy **nunca guarda la escena**: carga, mira y descarta
(`FunshiEngineCLI/src/Escena/ContextoEscena.cpp:32`); sin persistencia no hay
"crear un juego por consola", solo "mirar un juego por consola".

### Arquitectura

**Un módulo de inspección en el motor, sin ImGui.** El catálogo de componentes
carga hoy las 24 cabeceras de los paneles
(`src/Objetos/Componentes/CatalogoComponentes.cpp:46`); la API de inspección no
debe vivir en esa unidad de traducción.

```
src/Inspeccion/
  ApiEscena.h                 ← contrato público (IApiEscena) + versión propia
  InspeccionEscena.h/.cpp     ← implementación sobre SceneRegistry y EditorController
  PropiedadesComponentes.h/.cpp ← registro tipo → propiedades (getter/setter)
  ValorCampoJson.h/.cpp       ← puente opcional a JSON (transporte de la CLI)
```

El contrato:

```
constexpr int versionApiEscena = 1;   // versión del contrato de inspección

struct FiltroObjetos {
    std::optional<std::string> componente;  // "RigidBody", "CameraComponent"…
    std::optional<std::string> etiqueta;    // tag
    std::optional<std::string> nombre;      // exacto o parcial
    bool nombreParcial = false;
    std::optional<int> id;
    std::optional<std::string> tipoObjeto;  // "GameObject", "Modelos3D"…
    std::size_t limite = 0;                 // 0 = sin límite
};

struct VistaObjeto { int id; std::string nombre; std::string etiqueta;
                     std::string tipoObjeto; int idPadre;
                     float posicionGlobal[3]; float posicionLocal[3];
                     std::size_t cantidadComponentes; };

struct VistaVariable { std::string nombre; std::string tipo; std::string valor; };
struct VistaComponente { std::string tipo; std::string etiqueta;
                         std::vector<VistaVariable> variables; };

class IApiEscena {
  virtual std::vector<VistaObjeto> listarObjetos(const FiltroObjetos&) = 0;
  virtual std::optional<VistaObjeto> objetoPorId(int) = 0;
  virtual std::vector<VistaComponente> componentesDe(int idObjeto) = 0;
  virtual bool crearComponente(int idObjeto, const std::string& tipo, std::string& error) = 0;
  virtual bool quitarComponente(int idObjeto, const std::string& tipo, std::string& error) = 0;
  virtual bool asignarVariable(int idObjeto, const std::string& tipoComponente,
                               const std::string& variable, const std::string& valor,
                               std::string& error) = 0;
  virtual bool posicionGlobal(int idObjeto, float xyz[3]) = 0;
  virtual std::vector<std::string> tiposDeComponente() = 0;
};
```

**Las propiedades se acceden por getter y setter, nunca por puntero crudo.**
`Transform` guarda dos copias del estado: la privada, que usa el render, y la
pública (`src/Objetos/Componentes/Transform.h:31`); escribir la copia pública
directamente desincroniza el render. Por eso `PropiedadesComponentes` declara
pares de funciones:

```
struct DefPropiedad {
    std::string nombre;
    std::string tipo;
    std::function<std::string(const void*)> leer;
    std::function<bool(void*, const std::string&)> escribir;
};
```

El motor ya tiene la maquinaria de tipos para leer y escribir valores:
`ReflejoScripts::TagTipo`, `ValorCampo`, `DefCampo`, `leerCampo` y
`escribirCampo` (`src/Behaviour/Reflection/BehaviourReflection.h:48`–`:132`).
Hoy solo la usan los scripts; este trabajo la reutiliza para los componentes.

**Cada mutación es un `IComando`, para que se pueda deshacer.** El motor ya
tiene esa regla escrita (`PROJECT_STRUCTURE.md`, sección de comandos): toda
operación de la GUI de edición pasa por un `IComando` con parámetros
explícitos, de modo que cada acción tiene equivalente invocable sin GUI.
Faltan tres: `AjustarVariableComando`, `CrearObjetoSimpleComando` (hoy
`CrearObjetoComando` solo acepta un `Modelos3D`) y `EstablecerTagComando`.

**El versionado es propio.** `versionRuntimeScript`
(`src/Behaviour/ScriptGameObject.h:42`) es la clave de la caché de scripts
compilados: subirlo por un cambio de la API de inspección invalidaría la caché
de todos los usuarios sin motivo. La API de inspección lleva su número
(`versionApiEscena`), que la CLI consulta al arrancar y compara con el suyo.

**Reparto entre repositorios.**

| Repositorio | Trabajo |
|---|---|
| Motor (este) | Módulo `src/Inspeccion/`, los `IComando` nuevos, el helper reutilizable de resolvers (hoy cableado en `GameScene::configurarResolverObjetosScripts`, `src/Scenes/GameScene.cpp:209`), su suite headless y la documentación. |
| CLI (`FunshiEngineCLI`) | Filtros en `scene.objects`, comandos `scene.components`, `scene.component.create/set/remove`, `scene.object.create`, `scene.object.setTag`, `scene.component.types`, `scene.save` y el modelo de sesión; sus tests. |

La CLI compila el motor desde el submódulo: el motor publica el contrato, la
CLI actualiza el puntero del submódulo y recompila.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: el árbol nuevo `src/Inspeccion/` con el contrato y
  sus piezas, y los comandos nuevos en `src/Comandos/`.
- `MANUAL_DE_USO.md`: sección de la CLI con los filtros y los comandos nuevos,
  y la sección de scripting que explica que la API de inspección y la de
  scripts son contratos distintos.
- `AGENTS.md`: se documenta que la CLI es un proyecto aparte y que el motor
  expone la API de inspección con versión propia.

### Plan de integración

Motor:

1. `PropiedadesComponentes`: registro de propiedades de `Transform`,
   `RigidBody`, colliders, `Color`, `Light`, `CameraComponent`, `Material` y
   `Model`, con getter y setter. Suite propia, CPU pura.
2. `IApiEscena` e `InspeccionEscena`: filtros, vistas y lectura. Los filtros
   usan `Entity::getComponentByName` (`src/Entity/Entity.cpp:158`, que ya
   normaliza el nombre y acepta alias) y `getGlobalTransform`
   (`src/Entity/Entity.h:106`).
3. Los `IComando` nuevos, con su suite en `tests/ComandosTests.cpp`.
4. `asignarVariable` y `crearComponente` sobre los comandos, para que la CLI
   pueda deshacer.
5. El helper de resolvers reutilizable, extraído de `GameScene`, para que la
   CLI no tenga que duplicar la resolución por nombre.

CLI (proyecto aparte, rama propia):

6. Sustituir los `dynamic_cast` a mano de
   `FunshiEngineCLI/src/Escena/SerializacionEscena.cpp:39` por la API.
7. Filtros y comandos nuevos.
8. `scene.save` y el modelo de sesión (por invocación, o sesión viva para la
   TUI, que es un proceso de larga vida).

Riesgo: el catálogo de componentes cambia de nombres de tipo con el tiempo; la
CLI normaliza siempre con `nombreSimpleTipo` (`src/Herramientas/TypeUtils.h:38`)
para que el mismo nombre funcione en MSVC y en GCC.

### Análisis de resultados y testers

Motor, suite `tests/InspeccionEscenaTests.cpp`:

- Filtro por componente, por tag, por id, por nombre (exacto y parcial), por
  tipo de objeto, y la intersección de varios filtros a la vez.
- Límite de resultados.
- Objeto inexistente: `std::nullopt`, sin fallo; `componentesDe` de un id
  inexistente devuelve vacío.
- `componentesDe` lista todos los componentes con sus variables y el valor
  actual.
- Posición global de un objeto hijo: la compuesta con el padre
  (`getGlobalTransform`), no la local.
- `crearComponente` con tipo válido lo añade y el deshacer lo quita; con tipo
  inexistente devuelve el motivo; con un **prerequisito incumplido** (un
  collider sin `Transform`) respeta el `prerequisito` del catálogo
  (`src/Objetos/Componentes/CatalogoComponentes.h:81`).
- `asignarVariable` escribe y se lee igual, y **el `Transform` queda
  sincronizado**: es el test que protege contra la doble copia de estado.
- `asignarVariable` con variable inexistente o tipo incompatible no escribe
  nada y devuelve el motivo.
- `version()` devuelve `versionApiEscena` y es distinto de
  `versionRuntimeScript` (documenta que son contratos independientes).

CLI (proyecto aparte), suite propia:

- Filtros combinados y `limit` sobre la respuesta JSON.
- Filtro sin resultados: `ok: true` con lista vacía (no es un error).
- Argumento inválido: `ok: false` con código estable y el código de salida de
  uso.
- `scene.components` devuelve el **valor** de cada variable (hoy la
  serialización solo emite nombre y tipo: el test falla antes del cambio).
- `scene.component.set` seguido de `scene.components` devuelve el valor nuevo.
- `scene.save` persiste: una invocación posterior ve el cambio.

Comprobación manual: crear por consola un objeto con un componente y una
variable, abrir el proyecto en el editor y ver que el cambio está ahí.

---

## S5. Vista previa del material en esfera

### Argumentos

En el panel de un material, el usuario ve hoy cuatro rutas de textura como
texto (`src/GUI/ObjetosGUI/Material/SettingsMaterial.cpp:93`): no hay ninguna
vista previa de cómo queda el material. La práctica habitual en los motores es
mostrar una esfera con el material aplicado, que informa de brillo, color y
textura de un vistazo.

El motor tiene las tres piezas que hacen falta:

- Render a textura: `RenderTarget` (`src/Rendering/RenderTarget.h:28`), con su
  FBO y su textura de color, ya en uso para las vistas previas de cámara
  (`src/Rendering/SceneRenderer.cpp:1185`).
- El dibujado de una malla arbitraria con matrices dadas:
  `MeshRenderer::intentarRenderMesh` (`src/Rendering/MeshRenderer.h:104`).
- La conversión de handle a identificador de ImGui:
  `backend.imguiTextureId` (`src/Rendering/Backend/IRenderBackend.h:171`), que
  el GUI ya usa para pintar las vistas previas de cámara
  (`src/Scenes/GameScene.cpp:936`).

Falta la malla de esfera, el servicio que la dibuja con el material y el punto
donde se pinta.

### Arquitectura

**El renderer de la vista previa no vive en `SceneRenderer`.** `SceneRenderer`
dibuja en espacio de mundo con las matrices de la cámara
(`src/Rendering/SceneRenderer.cpp:157`); una vista previa de producto es
espacio de pantalla. Además, `SceneRenderer` es grande y su responsabilidad es
la escena.

```
src/Rendering/Preview/
  IPreviewMaterial.h     ← contrato del servicio (lo consume la GUI)
  PreviewRenderer.h/.cpp ← FBO + malla de esfera + cámara de producto + luces fijas
src/Assets/
  Primitivas3D.h         ← esferaUV(), CPU pura, como Primitivas2D::quad()
```

**La GUI pide una textura; no sabe de FBO ni de mallas.** El contrato es
simple:

```
class IPreviewMaterial {
  virtual void invalidate() = 0;                    // marca el material como cambiado
  virtual unsigned int textura() const = 0;         // identificador para pintar
  virtual void anchoAlto(int& w, int& h) const = 0;
};
```

El servicio redibuja **solo cuando el material cambia** (un resumen de sus
cinco colores y sus cuatro rutas), no cada frame: un FBO por frame por panel
sería un desperdicio.

**La iluminación es propia del preview** (una luz principal y una de
contraluz), no la de la escena: el material tiene que verse igual en cualquier
escena. `MeshRenderer::setLuces` (`src/Rendering/MeshRenderer.h:85`) ya permite
inyectar luces fijas.

**La cámara de producto son matrices propias**, no un `GameObject` temporal:
`CameraComponent` deriva su vista del `Transform` de su dueño
(`src/Objetos/Componentes/CameraComponent.cpp:395`), lo que obligaría a crear
un objeto fuera de la escena solo para pintar una miniatura.

**La malla de esfera es CPU pura.** `Primitivas3D::esferaUV()` sigue el
molde de `Primitivas2D::quad()` (`src/Assets/Primitivas2D.h:35`), que ya se
prueba en headless (`tests/RenderingTests.cpp:783`).

**El servicio se inyecta.** `ContextoPanel`
(`src/Objetos/Componentes/CatalogoComponentes.h:60`) es el punto por donde la
GUI recibe lo que necesita; el preview llega por ahí, no por un global.

**Con S4 (materiales por regiones), la esfera muestra la región seleccionada.**
El panel de `Materiales` pinta una esfera por región, y el servicio recibe la
región que se está editando.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: los bloques `src/Rendering/Preview/` y
  `src/Assets/Primitivas3D.h` con su papel.
- `MANUAL_DE_USO.md`: la sección de materiales explica la esfera de vista
  previa y que se actualiza al cambiar el material.

### Plan de integración

1. `Primitivas3D::esferaUV()` con su test (vértices, UVs, índices, que la
   esfera no tenga vértices degenerados en los polos).
2. `PreviewRenderer` con el `RenderTarget` y la cámara de producto, usando un
   backend falso en el test para comprobar que emite los draws esperados.
3. `IPreviewMaterial` y su implementación, con la invalidación por cambio.
4. El panel del material lo pinta (`ImGui::Image` con la V invertida, como las
   vistas previas de cámara) y avisa cuando cambia.

Riesgo: el FBO del preview vive mientras el panel existe; hay que liberarlo
cuando el material se destruye, o se acumulan FBO por cada material editado.

### Análisis de resultados y testers

Suite `tests/PreviewMaterialTests.cpp` (con backend falso, como
`tests/ShadersTests.cpp:62`):

- La esfera tiene la cantidad de vértices e índices esperados, sus UVs están
  en [0,1] y sus normales son unitarias.
- El renderer del preview pide el programa al gestor de shaders por su
  nombre, igual que los demás renderers.
- La matriz de la cámara de producto es finita y `near < far`.
- Al cambiar el material (color o textura) se invalida la vista previa; sin
  cambio, no se vuelve a dibujar.
- La textura que devuelve el servicio es válida tras el primer dibujado.

Comprobación manual: abrir el panel de un material con textura y ver la
esfera con el material aplicado; cambiar el color y ver que la esfera cambia;
comprobar que la esfera se ve igual con y sin luces en la escena.

---

## S7. Sistema de partículas y efectos

### Argumentos

No existe nada de partículas ni de efectos en el motor: la búsqueda de
`Particula`, `Particle`, `VFX`, `Efecto`, `Emitter` en todo el árbol no
devuelve nada del motor (solo coincidencias de la librería de audio, de ImGui
y de comentarios). Faltan el emisor, la simulación de la vida de cada
partícula, el dibujado y el efecto.

El motor tiene las piezas para construirlo:

- El quad texturizado: `Primitivas2D::quad()` (`src/Assets/Primitivas2D.h:35`)
  y el shader de sprite (`src/Rendering/Shaders/ShaderSources.h:398`), que ya
  resuelve atlas por `uUvRect` y tinte por `uTint`.
- El pipeline de quads con blend: `SpriteRenderer`
  (`src/Rendering/SpriteRenderer.h`), que cachea texturas por identidad de la
  imagen y dibuja con blend activado y escritura de profundidad desactivada.
- La geometría dinámica en CPU: `LineBuilder` y `LineBatch`
  (`src/Rendering/LineBuilder.h:52`, `src/Rendering/LineBatch.h:33`), que es
  exactamente el patrón de "genero la geometría cada frame y la subo al mismo
  recurso".

### Arquitectura

**La simulación es CPU pura y no toca el motor gráfico.** El sistema de
partículas se puede probar entero en headless, que es la diferencia entre
poder validarlo y no poder.

```
src/Particulas/
  IParticula.h            ← contrato de una partícula (posición, vida, tamaño, color)
  EmisorParticulas.h/.cpp ← descripción del emisor (datos serializables)
  SistemaParticulas.h/.cpp← simulación: spawn, actualización, muerte, presupuesto
  PerfilParticula.h/.cpp  ← perfil de efecto (curvas de tamaño, color y vida)
  LectorPerfil.h/.cpp     ← carga del perfil (JSON, como Tileset)
src/Rendering/
  ParticulasRenderer.h/.cpp← dibuja el lote de partículas (cuads)
```

**El componente `EmisorParticulas` es un componente más**: se registra en el
catálogo y aparece solo en el Inspector, con su panel. Sus propiedades son
datos (cantidad, vida, velocidad, tamaño, textura, color); el estado de
ejecución (partículas vivas) no se serializa, igual que hace `Animador2D` con
su fotograma (`src/Objetos/Componentes/Animador2D.h:88`).

**El renderer recibe datos planos**, como los demás: un
`struct ParticulasDraw { const float* vertices; std::size_t cantidad; ... }`
con los vértices ya expandidos en CPU (posición, tamaño, UV y color por
vértice). El renderer no conoce el emisor ni el componente.

**El dibujado es un draw call**: los quads se expanden en un VBO
interleaving y se suben con el mismo recurso de cada frame, como hace
`LineBatch::update` (`src/Rendering/LineBatch.h`). El motor **no tiene
instancing**: `IRenderBackend` solo expone `drawMesh` y `drawLineBatch`
(`src/Rendering/Backend/IRenderBackend.h:88`). Añadir instancing al contrato
es el camino a medio plazo; para la primera versión, el patrón de lote único es
suficiente y no toca el backend.

**ElBlend aditivo no existe**: `OpenGL3Backend` solo hace
`glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
(`src/Rendering/OpenGL3Backend.cpp:820`). El fuego y el humo aditivos
necesitan ampliar el contrato del backend; como el contrato tiene muchos
dobles de prueba, se amplía en su propio commit, actualizando los dobles.

**El delta time se propaga por parámetro**, no se lee del reloj global: el
motor lo hace así (`GameScene::update` guarda el delta y lo mete en el
`FrameContext`, `src/Scenes/GameScene.cpp:1429`).

**La física no se toca**: las partículas son cinemáticas. Si una partícula
necesita rebotar, la consulta se hace contra el mundo por una interfaz, sin
meter Bullet en el sistema de partículas.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: los bloques `src/Particulas/` y el renderer nuevo.
- `MANUAL_DE_USO.md`: sección de partículas: cómo se añade un emisor a un
  objeto, qué es un perfil de efecto y cómo se edita.
- `DocuTecnicoBugs.md`: si el orden de dibujo de las partículas sale mal, el
  patrón a registrar es "el blend del backend no distingue el modo de
  mezcla".

### Plan de integración

1. `IParticula` y `SistemaParticulas` (CPU puro): vida, integración, muerte,
   presupuesto máximo de partículas vivas. Suite headless completa.
2. `PerfilParticula` y su lector (JSON, como `Tileset`).
3. `EmisorParticulas` como componente, con su panel y su serialización.
4. `ParticulasRenderer`: expansión de quads, subida del lote y dibujado.
5. Blend aditivo en el backend, con los dobles de prueba actualizados.
6. El sistema se engancha al update de la escena y al `FrameContext`, sin que
   el renderer de la escena conozca el emisor.

Riesgo: el coste por frame si el presupuesto no está acotado. El presupuesto
vive en el sistema y su límite es configurable y verificable en el test.

### Análisis de resultados y testers

Suite `tests/ParticulasTests.cpp` (CPU puro, sin GL):

- Una partícula nace viva, envejece con el delta time y muere al cumplir su
  vida; después de morir no se actualiza.
- El presupuesto de partículas vivas se respeta: con el límite puesto, nunca
  hay más partículas vivas que el límite.
- Las partículas no se mueven cuando el delta time es cero.
- Un emisor con cantidad cero no genera partículas.
- El perfil de efecto se carga desde JSON, y un perfil inválido devuelve el
  error sin dejar el emisor a medias.
- La expansión de quads produce 6 vértices por partícula (dos triángulos) con
  UV y color por vértice, y el rectángulo tiene el tamaño de la partícula.
- El componente `EmisorParticulas` hace round-trip binario y el estado de
  ejecución (partículas vivas) no se guarda.

Comprobación manual: añadir un emisor a un objeto, verlo en el viewport y en
el juego exportado, comprobar que el humo sube y se desvanece y que el fuego
se ve aditivo.

---

## S9. Interfaz de juego tradicional (sin web)

### Argumentos

El motor tiene hoy un único camino de interfaz de juego: el **web**, con
HTML/CSS/JS servido por CEF (`src/WebUI/`, `IWebUIBackend`,
`VistaWebUI`, `EstadoInterfaces`). Quien quiere una interfaz tradicional, como
la de un motor con editor visual, tiene que escribir HTML y depender de una
tecnología web.

El editor, además, no es reutilizable como interfaz de juego: `GeneralUserInterface`
incluye ImGui directamente (`src/GUI/GeneralUserInterface.h:23`) y el juego
exportado ni siquiera crea el `GUIManager` (`src/Runtime/main_juego.cpp:197`).

Faltan también dos capacidades que el sistema web no resuelve bien:

- **No hay texto.** No existe ningún renderizador de texto del motor: lo único
  que hay es el `stb_truetype` que ImGui ya trae dentro
  (`src/ImGui/imstb_truetype.h`), que además ata la UI a ImGui.
- **El HUD web no recibe input en el juego exportado**: `EditorInput` no
  reenvía ratón ni teclado al runtime web
  (`src/Runtime/main_juego.cpp:262`).

### Arquitectura

**Un contrato común de "interfaz de juego"**, del que el backend web y el
nativo son dos implementaciones. `IWebUIBackend`
(`src/WebUI/IWebUIBackend.h:39`) ya tiene esa forma (cargar, redimensionar,
actualizar, render, estado, eventos de entrada, foco), así que el backend
web puede **adaptarse** en lugar de reescribirse.

```
src/UI/                      (módulo nuevo, hermano de WebUI/)
  IInterfazJuego.h           ← contrato de una interfaz viva
  IWidget.h                  ← contrato de un widget (imagen, texto, botón, panel)
  IRenderizadorUI.h          ← dibujado en espacio de pantalla
  IFontLoader.h              ← carga y rasterizado de fuentes (Strategy)
  RectUI.h / LienzoUI.h      ← geometría CPU pura (headless)
  MotorUI.h/.cpp             ← fachada: registro, ciclo por frame, foco
  Widgets/                   ← ImagenUI, TextoUI, BotonUI, PanelUI
  Recursos/                  ← carga del recurso de UI (.ui) → árbol de widgets
src/Rendering/UI/
  RenderizadorUI.h/.cpp      ← quads y glifos en espacio de pantalla
  AtlasDeFuente.h/.cpp       ← rasterización de glifos → Image + uvRect
  ProyeccionPantalla.h/.cpp  ← glm::ortho(0, w, h, 0) (no existe hoy)
```

**El renderer dibuja en espacio de pantalla.** No hay hoy una proyección
ortográfica de pantalla: la única ortográfica es la de la cámara en unidades de
mundo (`src/Objetos/Componentes/CameraComponent.cpp:420`). Hace falta una
matriz nueva, con el origen arriba a la izquierda, que es la convención que ya
usa el HUD web (`src/WebUI/IWebUIBackend.h:97`).

**El GLSL no cambia**: el shader "sprite"
(`src/Rendering/Shaders/ShaderSources.h:398`) ya resuelve recorte por `uUvRect`
y color por `uTint`, que es exactamente lo que necesita un quad de imagen y un
quad de glifo. El atlas de la fuente es una `Image` más
(`src/Assets/Image.h:30`) y se pinta con `Primitivas2D::quad()`.

**La fuente es un subsistema nuevo.** El motor no tiene ninguno: hay que
elegir un rasterizador (`stb_truetype` o FreeType) y construir un atlas de
glifos con sus métricas. El loader es un Strategy, como `ITextureLoader`
(`src/Assets/TextureManager.h:32`), para que en los tests se pueda usar un
loader artificial.

**El widget no conoce el renderer y el renderer no conoce el widget**: el
mismo patrón de datos planos que `SpriteDraw` y `TilemapDraw`. La GUI del
editor puede ser un consumidor más del contrato: pintar una vista previa de un
`UIComponent` en el Inspector es el mismo `RenderizadorUI` sobre un FBO.

**El input llega por el contrato de estado de interfaz.** El motor ya lo tiene:
`IEstadoInterfaz` (`src/Input/IEstadoInterfaz.h:31`), que responde "captura
teclado" y "captura ratón" sin saber qué biblioteca dibuja la interfaz. La
implementación para la UI tradicional es distinta de la de ImGui, y el editor
la elige según cuál esté activa.

**El componente.** El nombre del tipo persistido es el formato de datos de la
escena, así que se decide explícitamente: `InterfaceComponent` (web) ya tiene
marca de formato (`src/Objetos/Componentes/InterfaceComponent.cpp:29`), y
extenderlo con un campo "tipo de interfaz" es aditivo; la alternativa es un
componente nuevo `UIComponent`, con su propia marca. La segunda es más limpia
(mixing dos mundos en un componente con formatos distintos) y es la recomendada.

**Un solo punto de composición.** `GameScene::gameScene`
(`src/Scenes/GameScene.cpp:1407`) decide hoy si pinta el HUD web; con los dos
sistemas, ese punto recolecta **todas** las interfaces, las ordena por `z` y
las dibuja con el mismo renderer.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: los bloques `src/UI/` y `src/Rendering/UI/`, y la
  descripción del contrato común con el backend web.
- `MANUAL_DE_USO.md`: sección de interfaces de juego: las dos formas (web y
  tradicional), cómo se crea un recurso de UI y cómo se expone a los scripts.
- `DocuTecnicoBugs.md`: el patrón a registrar si el HUD y la escena se
  mezclan es "dos sistemas de coordenadas y de proyección en la misma pasada".

### Plan de integración

1. `RectUI`, `LienzoUI` y `IWidget`: geometría, anclaje, hit-test y foco.
   CPU puro, con suite propia. Es la base de todo lo demás.
2. `IFontLoader` y `AtlasDeFuente`: rasterización, atlas y métricas, con
   loader artificial para los tests.
3. `ProyeccionPantalla` y `RenderizadorUI`: quads y glifos con el shader
   "sprite", con backend falso en el test.
4. `IInterfazJuego`, `MotorUI` y los widgets concretos.
5. `UIComponent` (o la ampliación de `InterfaceComponent`) y su panel.
6. Adaptador del backend web al contrato común, y composición por `z` en
   `GameScene`.
7. El canal de input de interfaz (que hoy falta para el HUD web en el juego
   exportado) lo usan los dos.

Riesgo: el atlas de la fuente necesita una dependencia (FreeType) o código
propio (`stb_truetype` fuera de ImGui). La decisión se toma en el primer
commit de este sistema y condiciona los siguientes.

### Análisis de resultados y testers

Suite `tests/InterfazTradicionalTests.cpp` (CPU pura para la mayor parte):

- Anclaje: las nueve posiciones sitúan el widget en el lugar esperado.
- Hit-test: el punto está dentro del widget solo cuando corresponde; el
  widget de arriba tiene prioridad sobre el de abajo.
- El foco pasa de un widget al siguiente con el tabulador.
- El atlas de la fuente: con un loader artificial, el atlas tiene el tamaño
  esperado, cada glifo tiene su rectángulo UV dentro del atlas y el avance
  del glifo es coherente con el tamaño de la fuente; un glifo inexistente no
  rompe.
- El renderer de UI emite los draws esperados (número de quads, UVs y
  tintes) con el backend falso.
- La proyección de pantalla es finita y mapea (0,0) a la esquina superior
  izquierda y (ancho, alto) a la inferior derecha.
- El recurso de UI hace round-trip y un recurso inválido devuelve el error.
- El componente hace round-trip binario, y el formato viejo se lee sin
  desalinear el stream (patrón de `tests/InterfaceComponentTests.cpp:109`).
- La API de scripts expone los eventos de la UI de forma append-only: la
  tabla de servicios suma los campos al final y sube su `version`.

Comprobación manual: una interfaz con un botón y un texto, que el botón
responda al clic, que el texto se lea y que el conjunto se vea igual en el
editor y en el juego exportado.

---

## S1. Animación por máquina de estados

### Argumentos

El motor solo tiene `Animador2D`
(`src/Objetos/Componentes/Animador2D.h:31`): un reproductor de planilla de
sprites que avanza fotogramas a fps fijos y escribe la región UV del
`SpriteRenderer` hermano. No hay clips, ni curvas, ni interpolación, ni
máquina de estados: la búsqueda de `Clip`, `Keyframe`, `Animation`, `Animator`
3D, `Blend`, `IK` y `bone` en `src/` no devuelve nada del motor.

Una máquina de estados es la forma habitual de organizar animaciones: cada
estado tiene su clip (o su conjunto de clips), y las transiciones dependen de
condiciones (velocidad, Contacto, un disparador del script).

### Arquitectura

**Un módulo de animación, independiente del render, de los assets y de los
objetos.**

```
src/Animacion/
  IAnimacion.h              ← contrato del reproductor
  ICurvaAnimacion.h         ← contrato de curva (muestreo, interpolación)
  IClipAnimacion.h          ← contrato de clip
  IMaquinaEstados.h         ← contrato de la máquina
  IEstadoAnimacion.h        ← contrato de estado
  IAvatar.h                 ← contrato de avatar (hueso → clip)
  CurvaAnimacion.h/.cpp     ← keyframes e interpolación
  ClipAnimacion.h/.cpp      ← curvas + duración + bucle
  MaquinaEstados.h/.cpp     ← estados, transiciones, condiciones
  EstadoAnimacion.h/.cpp
  CargadorClips.h/.cpp      ← Strategy de carga (como IMeshLoader)
  SerializacionAnimacion.h/.cpp
```

**El componente `Animador` es la pieza que une el módulo con la escena.**
Registrado en el catálogo, con su panel: reproduce el clip del estado actual
y escribe el resultado en el hermano que anima (un `Transform`, un
`SpriteRenderer` o, más adelante, una malla con huesos).

**El módulo no conoce el render.** El animador produce **datos** (posición,
rotación, escala de un hueso o del objeto) y quien dibuja los consume. Es el
mismo patrón que `SpriteDraw` y `TilemapDraw`.

**Las curvas son CPU puras.** `CurvaAnimacion` (keyframes, tangentes,
interpolación lineal/suave/paso) se prueba en headless, sin glm ni OpenGL:
es la parte con más lógica y la que más conviene testear.

**Los clips se cargan por una Strategy**, como las mallas y las texturas
(`IMeshLoader`, `ITextureLoader`), con su gestor
(`ClipManager`, gemelo de `AssetManager`) y su caché por ruta.

**Los assets se referencian por ruta**, como el audio y la música: el motor ya
lo decidió así y el manifiesto de assets de la escena
(`src/Scenes/ManifiestoAssets.h:28`) reescribe las rutas cuando se mueve o
renombra una carpeta.

**Las condiciones de transición son declarativas**, no código: un predicado
(velocidad por debajo de un valor, un bool del objeto, un disparador). Así la
máquina se serializa y se edita sin escribir código.

**El componente solo se actualiza en modo juego.** `GameObject::update` llama
a `Component::onUpdate` (`src/Objetos/GameObject.cpp:211`), y `GameScene` solo
recorre los objetos cuando la simulación está activa
(`src/Scenes/GameScene.cpp:1193`). En el editor, la animación no se ve salvo
que se añada una vista previa: es una limitación que conviene tener presente
al diseñar la máquina.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: el árbol `src/Animacion/` con el papel de cada
  contrato, y el componente `Animador` en el árbol de componentes.
- `MANUAL_DE_USO.md`: sección de animación: clips, curvas, estados,
  transiciones y condiciones; y qué se ve en el editor frente a lo que se ve
  en modo juego.
- `DocuTecnicoBugs.md`: el patrón a registrar si una animación se desincroniza
  al cambiar de estado es "el estado de ejecución de la animación no se
  serializa".

### Plan de integración

1. `ICurvaAnimacion` y `CurvaAnimacion`: keyframes, interpolación, muestreo
   por tiempo, límites. Suite headless.
2. `IClipAnimacion` y `ClipAnimacion`: varias curvas, duración, bucle,
   eventos. Suite headless.
3. `IMaquinaEstados`, `IEstadoAnimacion` y `MaquinaEstados`: estados,
   transiciones, condiciones, entrada y salida. Suite headless.
4. `CargadorClips` y el gestor de clips, con un archivo de clip en disco.
5. El componente `Animador` y su panel: elegir estado, ver el clip, verificar
   las transiciones.
6. La escena alimenta el animador: los parámetros de las condiciones salen de
   los componentes hermanos.

Riesgo: el formato del clip en disco. Empieza por un formato versionado por
archivo (con su marca), y no por una versión del motor: si el formato cambia,
el loader lo detecta.

### Análisis de resultados y testers

Suite `tests/AnimacionTests.cpp` (CPU pura en su mayor parte):

- Una curva con dos keyframes interpola el valor esperado en el punto medio.
- La interpolación de paso no interpola; la suave se acerca al valor medio.
- Muestrear fuera del rango devuelve el valor del extremo (no se sale del
  rango).
- Un clip con bucle devuelve el tiempo al empezar el segundo ciclo; sin bucle,
  se queda en el final.
- La máquina cambia de estado cuando la condición se cumple y **no** cambia
  cuando no se cumple.
- Entrar y salir de un estado llama a su entrada y su salida, y el estado
  actual es consultable.
- Una transición con condición mixta (dos condiciones en AND y en OR) se
  comporta como se declara.
- El clip se serializa y se lee igual; un archivo de clip con otra versión se
  rechaza con el motivo.
- El componente `Animador` hace round-trip binario; el estado de ejecución
  (estado actual, tiempo) no se guarda.

Comprobación manual: una escena con dos estados y una transición por
velocidad, en modo juego; y comprobar que al guardar y recargar la escena la
máquina queda igual.

---

## S8. Sistema de construcción de terrenos

### Argumentos

No hay nada de terreno en el motor: `Terreno`, `Terrain`, `Heightmap`,
`Heightfield`, `Splat` no aparecen en ningún archivo. Lo más parecido es la
grilla del editor (`Grid` + `GrillaRenderer`), que son líneas, no superficie.

Un sistema de construcción de terrenos necesita: una malla de alturas que se
pueda editar, un terreno que se dibuje con su material (texturas por región),
y la posibilidad de que la escena lo use como suelo con colisión.

### Arquitectura

**La geometría del terreno es CPU pura y se genera por revisión**, con el
molde del `Tilemap`: `revision()` + `construirGeometria(Mesh&)` + caché
(`src/Objetos/Componentes/Tilemap.h:83`, `:171`). La diferencia es que el
terreno se divide en **tiles** para no rehacerlo entero en cada pincelada.

```
src/Terreno/
  ITerreno.h                ← contrato: alturas, tamaño, material, revisión
  Terreno.h/.cpp            ← componente: alturas, filtros, pintura, colisión
  GeneradorMallaTerreno.h/.cpp ← tiles → Mesh (CPU puro)
  MallaTerreno.h/.cpp       ← teselado, normales,ruvado de limites
  ColliderTerreno.h/.cpp    ← colisión contra las alturas
src/Rendering/
  TerrenoRenderer.h/.cpp    ← dibuja los tiles visibles
```

**La edición es una herramienta de pincel**, no un componente: un pincel sube
o baja alturas dentro de un radio y con una fuerza, y escribe en la malla de
alturas. La herramienta vive en el editor (`src/GUI/ObjetosGUI/Terreno/`), la
lógica del pincel en un helper CPU puro (para poder probarla).

**El terreno se dibuja con tiles.** El patrón de streaming ya existe en el
motor, aunque sea con líneas: `GrillaRenderer` calcula una clave de
invalidación (posición de la cámara, radio, subdivisiones) y solo rearma la
geometría cuando la clave cambia (`src/Rendering/GrillaRenderer.cpp:146`). Para
el terreno, la clave es el origen del tile y la resolución.

**El motor no tiene subida parcial de mallas.** `IRenderBackend` expone
`createMesh` y `destroyMesh`, y `updateLineBatch` para líneas, pero no
`updateMesh` (`src/Rendering/Backend/IRenderBackend.h:86`–`:114`). El
streaming de tiles necesita una de dos: un VAO por tile (crear y destruir
buffers en runtime) o ampliar el contrato con `updateMesh`
(`glBufferSubData`). La segunda es el camino correcto, y el mismo contrato que
hay que ampliar para el instancing de partículas.

**El material del terreno mezcla varias texturas** según la altura o el
barniz: el motor ya tiene los cuatro slots de textura en `Material`
(`src/Objetos/Componentes/Material.h:38`) y el peso por región es lo que
describe S4.

**La colisión reutiliza la física por la fachada.** `PhysicsEngine` es la
fachada y `IPhysicsBackend` el contrato (`src/Fisicas/PhysicsEngine.h:29`); el
terreno se registra como un collider (como hace `TilemapCollider`), sin que
`src/Terreno/` incluya Bullet.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: los bloques `src/Terreno/` y el renderer del terreno.
- `MANUAL_DE_USO.md`: sección de terreno: la herramienta de pincel, los
  parámetros del terreno, el material por regiones y la colisión.

### Plan de integración

1. `GeneradorMallaTerreno`: de una malla de alturas a una `Mesh` con
   normales, en CPU. Suite headless (tamaños, normales, límites).
2. `Terreno` como componente: alturas, resolución, tamaño, material, revisión;
   serialización binaria (las alturas son un bloque de datos en el archivo).
3. El terreno dibujado: un renderer que sube un VAO por tile y lo dibuja con
   el material del terreno.
4. La herramienta de pincel en el editor, con su helper puro.
5. La colisión: un collider que consulta las alturas.
6. `updateMesh` en el contrato del backend, si el streaming lo necesita
   (con los dobles de prueba actualizados en el mismo commit).

Riesgo: el tamaño del archivo de escena con un terreno grande. Las alturas se
guardan comprimidas o en un asset aparte, no en el binario de la escena, si el
terreno es de edición (decisión a tomar en el primer commit).

### Análisis de resultados y testers

Suite `tests/TerrenoTests.cpp`:

- La malla generada tiene la cantidad de vértices e índices esperados para la
  resolución, y los vértices están en las posiciones que dan las alturas.
- Las normales son unitarias y apuntan hacia arriba en un terreno plano.
- Pintar con el pincel sube las alturas dentro del radio y **no** toca las de
  fuera; el borde del radio se degrada.
- La revisión sube al pintar, y la malla cacheada se reconstruye.
- El round-trip binario del componente conserva las alturas.
- El collider del terreno devuelve la altura correcta bajo un punto y no
  colisiona fuera de sus límites.

Comprobación manual: crear un terreno, pintar un valle, asignarle el material
por regiones, Walking por encima con un rigidbody y ver que el personaje
sigue la superficie.

---

## S2. Editor visual de máquinas de estados

### Argumentos

Una máquina de estados sin editor visual se configura a mano: en el panel del
Inspector, con listas de estados y transiciones. El pedido es poder **diseñar
la máquina con un diagrama**, al estilo del editor de animaciones de Unity:
nodos con nombre, flechas de transición y condiciones a la vista.

El motor ya tiene la pieza exacta: **ImGuizmo**, que está compilado
(`FunshiEngineGL/CMakeLists.txt:527`) y trae un editor de grafos
(`ImGuizmo/GraphEditor.h:34`) con nodos, enlaces y selección, un secuenciador
(`ImGuizmo/ImSequencer.h:32`) y un editor de curvas (`ImGuizmo/ImCurveEdit.h:32`).

### Arquitectura

**El diagrama es una vista del modelo, no el modelo.** La lógica de la máquina
(`MaquinaEstados`, de S1) vive en el módulo de animación, sin ImGui; el
diagrama la dibuja. Es el mismo criterio que el motor ya aplicó con
`DibujoModelo` (`src/Rendering/DibujoModelo.h:26`: "vive fuera de SceneRenderer
y no toca la pila gráfica, para poder verificarse headless").

```
src/GUI/Animaciones/
  EditorMaquinaEstados.h/.cpp   ← ventana del diagrama (ImGui)
  VistaNodo.h/.cpp              ← estado ↔ nodo del grafo (traducción)
  VistaTransicion.h/.cpp
```

**La traducción entre el modelo y el grafo es CPU pura** y por eso se prueba:
un estado es un nodo, una transición es un enlace con su condición. La
ventana solo mueve esa conversión.

**El editor es una ventana del editor**, registrada como las demás
(`GUIManager::registrarVentana`, `src/GUIManager/GUIManager.h:141`).

**Las curvas se editan con el editor de curvas de ImGuizmo**
(`ImGuizmo/ImCurveEdit.h`), que ya sabe mostrar y editar puntos.

**La edición se guarda en la escena**, a través del mismo camino que el resto
de las ediciones del inspector: el modelo de la máquina es un componente
serializado, y el editor lo modifica.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: el bloque `src/GUI/Animaciones/` y su relación con
  `src/Animacion/`.
- `MANUAL_DE_USO.md`: sección del editor de máquinas: cómo se abre, cómo se
  crean estados y transiciones, y cómo se editan las curvas.

### Plan de integración

1. `VistaNodo` y `VistaTransicion`: la traducción entre el modelo y el grafo,
   con su suite headless (crear un nodo, renombrarlo, conectar dos nodos,
   evitar una transición a sí mismo).
2. La ventana del diagrama sobre `GraphEditor`, con el listado de estados y
   el de transiciones.
3. El editor de curvas conectado a las curvas de los clips.
4. Guardado: la edición del diagrama escribe en el componente y se serializa
   con la escena.

Riesgo: `GraphEditor` es una biblioteca de terceros con su propia
convención (delegado, opciones, estado de vista). El trabajo consiste en
encajarla, no en modificarla.

### Análisis de resultados y testers

Suite `tests/VistaMaquinaEstadosTests.cpp` (CPU pura):

- La vista refleja los estados del modelo: un estado por nodo, con su nombre.
- Crear un nodo en la vista crea el estado en el modelo, y al revés.
- Conectar dos nodos crea la transición con su condición; quitarla la borra.
- El grafo no admite una transición de un estado a sí mismo.
- Renombrar un nodo renombra el estado y **no** rompe las transiciones que
  apuntan a él.
- El modelo serializado a través de la vista es idéntico al serializado
  directo.

Comprobación manual: abrir el editor del diagrama sobre un objeto con
`Animador`, crear estados, conectar transiciones, cambiar una condición y
comprobar que en modo juego la animación reacciona.

---

## S3. Animación con huesos

### Argumentos

Sin huesos, un personaje se anima como un objeto entero: no hay deformación de
la malla, ni esqueleto, ni skinning. El motor no tiene ni una coincidencia de
`bone`, `skeleton`, `skinned`, `Avatar` o `IK`.

Además, el cargador de mallas **descarta** justo lo que hace falta:
`AssimpMeshLoader::load` (`src/Assets/AssimpMeshLoader.cpp:38`) usa
`aiProcess_PreTransformVertices` (aplana la jerarquía de nodos) y
`aiProcess_JoinIdenticalVertices` (fusiona vértices con pesos distintos), y no
lee ni `aiMesh::mBones` ni `aiScene::mAnimations`.

### Arquitectura

**El esqueleto es un asset, como la malla.** El `AssetManager` ya es un
registro con Strategy (`IMeshLoader`, `src/Assets/AssetManager.h:32`), y su
comentario anticipa el caso: "las próximas ramas agreguen texturas, shaders y
clips de audio al mismo patrón". Los huesos siguen ese patrón:

```
src/Animacion/Huesos/
  IHueso.h                  ← contrato de hueso (nombre, padre, pose)
  Esqueleto.h/.cpp          ← jerarquía de huesos, pose de referencia, Inverse Bind
  Avatar.h/.cpp             ← association hueso ↔ clip
  CargadorEsqueletos.h/.cpp ← Strategy (como el de mallas)
src/Rendering/
  MeshRendererPiel.h/.cpp   ← renderer paralelo que sube la matriz de piel
```

**La malla lleva los pesos.** `struct Mesh` (`src/Assets/Mesh.h:32`) suma
`boneIndices` y `boneWeights` (cuatro bones por vértice), y `MeshData`
(`src/Rendering/Backend/IRenderBackend.h:38`) los sube a la GPU. Los slots de
atributo 5 y 6 están libres (`OpenGL3Backend::createMesh`,
`src/Rendering/Backend/OpenGL3Backend.cpp:174`, usa 0 a 4).

**El shader de piel es un programa más.** `GestorShaders::registrarIncorporados`
(`src/Rendering/Shaders/GestorShaders.cpp:33`) lo registra por nombre ("piel"),
y el GLSL va embebido en `ShaderSources.h` (que la CI valida con
`.github/scripts/validar-glsl.py`).

**El pase de sombras también tiene que deformar.** Si no, la sombra del
personaje sale de la malla sin deformar (`kDepthVertexShader`,
`src/Rendering/Shaders/ShaderSources.h:500`).

**Hay dos caminos técnicos y conviene decidir pronto:**

- *Matrices de hueso por uniform* (`uniform mat4 uBones[N]`): sencillo, pero
  exige ampliar el contrato del backend con un setter de arrays (hoy
  `ShaderProgram` solo tiene `setMat4`, `setVec3`, `setFloat`…,
  `src/Rendering/Shaders/ShaderProgram.h:72`) y tiene el límite de bones por
  personaje.
- *Vertex pulling con una textura de huesos*: no necesita arrays de uniforms,
  pero exige una textura de punto flotante, y el `Image2D` del backend es
  RGBA8 con bytes (`src/Rendering/Backend/IRenderBackend.h:54`).

La primera es la adecuada para empezar; la segunda, si el límite de huesos
resulta corto.

**El esqueleto manda sobre la máquina de estados** (S1): el estado actual
decide el clip, el clip produce la pose, la pose produce las matrices de hueso.
La cadena queda: `Animador` → máquina de estados → clip → curvas → pose →
matrices → renderer.

**El `Animador` del componente escribe la pose** en el esqueleto; el renderer
de piel la lee. Ninguno de los dos conoce al otro.

### Documentación de arquitectura

- `PROJECT_STRUCTURE.md`: los bloques de huesos, el renderer con piel y los
  campos nuevos de `Mesh` y `MeshData`.
- `MANUAL_DE_USO.md`: sección de animación con huesos: cómo se asigna un
  esqueleto a un modelo, cómo seAssociate un clip a un avatar y qué se puede
  animar.
- `DocuTecnicoBugs.md`: el patrón "el cargador aplana la jerarquía del
  archivo y con ella los huesos" (variante del R12 ya documentado en
  `DocuTecnicoBugs.md`).

### Plan de integración

1. El cargador deja de aplanar: se leen los huesos y las animaciones del
   archivo, con sus pesos. Es el paso que más riesgo tiene (cambia el
   comportamiento de la carga de todas las mallas) y va con su propia suite.
2. `Esqueleto`: jerarquía, pose de referencia, matrices de inverse bind y
   pose de enlace. CPU pura, con suite.
3. `Mesh` y `MeshData` con los pesos, y `MeshGPU` que los sube.
4. El shader de piel y el renderer que lo usa; el pase de sombras también.
5. `Avatar`: la asociación entre huesos y clips.
6. El componente `Animador` escribe la pose.

Riesgo: cambiar los flags de post-proceso de Assimp afecta a **todas** las
mallas que se cargan. El paso 1 tiene que comprobar que las mallas estáticas
siguen cargando igual (normales, tangentes, UVs) y que el cantidad de vértices
no cambia en los archivos sin huesos.

### Análisis de resultados y testers

Suite `tests/HuesosTests.cpp` (CPU pura):

- La jerarquía de huesos se lee con su padre correcto y la pose de referencia
  tiene la posición de reposo.
- Las matrices de inverse bind son la inversa de la de referencia.
- La pose de un hueso se compone con la de su padre.
- El peso de un vértice se normaliza y se reparte entre los huesos
  correspondientes.
- La matriz de piel de un vértice es la media ponderada de las matrices de
  sus huesos, y el cálculo coincide con el esperado.
- Un archivo sin huesos se carga como antes (misma cantidad de vértices, misma
  geometría): es el test que protege el resto del motor.
- Un archivo con huesos produce una malla con los pesos y un esqueleto con la
  jerarquía correcta.
- El clip de animación del archivo se puede leer y reproduce: la pose que
  produce a un tiempo dado es la esperada.

Comprobación manual: un modelo con esqueleto, en modo juego, moviendo un
hueso desde un script; y comprobar que su sombra sale deformada.

---

## 2. Decisiones que hay que tomar antes de empezar

Estas decisiones no se pueden resolver leyendo el código: cambian el diseño
de un sistema entero y conviene fijarlas antes de escribir la primera línea.

| Sistema | Decisión | Opciones | Efecto de la decisión |
|---|---|---|---|
| S3 | Cómo llegan las matrices de hueso a la GPU | uniform arrays (límite de huesos por personaje) o vertex pulling con textura (sin límite, más plumbing de backend) | Uniform arrays obliga a ampliar `ShaderProgram` y `IRenderBackend`; vertex pulling obliga a ampliar el tipo de imagen del backend (hoy es RGBA8). |
| S3 | Dónde viven los huesos | dentro del archivo de malla o en un asset aparte | Juntos simplifica la importación; separados permiten reusar un esqueleto entre varios modelos. |
| S9 | Qué rasterizador de fuentes | `stb_truetype` fuera de ImGui, o FreeType | `stb_truetype` evita una dependencia del sistema y se puede vendorizar; FreeType es más completo y hay que distribuirlo o pedirlo en la CI. |
| S9 | Componente nuevo o componente extendido | `UIComponent` nuevo o ampliar `InterfaceComponent` | El nombre del tipo persistido es el formato de datos: ampliar `InterfaceComponent` es una migración versionada de las escenas existentes. |
| S8 | Dónde se guardan las alturas | en el binario de la escena o en un asset aparte | En la escena, un terreno grande agranda el archivo de guardado; en un asset aparte, hay que versionarlo y elegir su formato. |
| S4 | Nombre de las regiones | el del archivo o uno del proyecto | El del archivo es automático pero cambia si el archivo se reexporta; uno del proyecto es estable y cuesta un paso extra. |
| S11 | Forma de la API de inspección | interfaz C++ con versión propia, o tabla de punteros a función | La interfaz C++ es cómoda y la CLI ya enlaza el motor; la tabla de punteros sirve si algún día un tercero consume la API sin enlazar el motor. |
| S7 | Instancing en el backend | lote único (sin tocar el backend) o `drawElementsInstanced` | El lote único es más simple y basta para empezar; el instancing multiplica el número de partículas possible y obliga a ampliar el contrato. |

## 3. Lo que queda fuera del alcance de estos sistemas

- La versión con Interfaz web no se toca: sigue siendo un backend más del
  contrato común, y sus defectos actuales (el HUD web no recibe input en el
  juego exportado, `webui-runtime-tests` está deshabilitado) se arreglan como
  parte de S9, no antes.
- El motor no adopta un motor de física externo: las partículas y el terreno
  usan la fachada `PhysicsEngine` que ya existe.
- El editor no se convierte en un motor de producción: los sistemas se
  diseñan para que el mismo código sirva en el editor y en el juego
  exportado.

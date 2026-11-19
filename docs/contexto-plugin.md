# Contexto de SideMK

Inspección del repositorio para empezar la ingeniería inversa del pump de Nicky Romero Kickstart dentro de este plugin.

Fecha de la inspección: 2026-10-28. Hechos tomados del árbol de trabajo. Lo que no está en un archivo se marca como inferencia, decisión de trabajo o por confirmar.

- Rama: `dsp-validation`
- Commit: `6a8f93b` — «Añade validación inicial del DSP» (Jose Andres Ureta Granda, 2026-10-25)
- Árbol de trabajo: limpio en el momento de la inspección

## Actualización 2026-11-19 (sin commit todavía)

Pasos 1 y 3 del método de la sección 6, hechos en el árbol de trabajo. **Nada de esto se ha compilado**: la máquina sigue sin `cl`, `clang++` ni `g++` (BuildTools 2022 y VS 18 vacíos, Docker parado).

- `processBlock` corregido: una sola lectura de cada parámetro, una sola aplicación de ganancia por muestra. Separado en `processCompressor` y `processPump`.
- Modo Pump: parámetros nuevos `mode` (Compressor/Pump), `rate` (1/16 … 1 Bar) y `depth` (0–100 %), con `ParameterID` versión 2. La curva vive en `PumpCurve.h` (sin JUCE): caída lineal de 1 ms + `1 − exp(−k·t)` normalizada, `k = 6,5`. Fase desde `getPpqPosition()` del host en cada bloque; `1 Bar` usa compás e inicio de compás del host. Sin transporte, corre libre al BPM del host o a 120.
- Makeup y depth suavizados con `SmoothedValue` (20 ms). Ahora es verdad lo que decía el README sobre la rampa.
- Editor: 648×300, selectores MODE/RATE en la cabecera, knob PUMP DEPTH, el knob «SIDECHAIN LEVEL» se llama ahora «MAKEUP». Los controles que el modo no usa se atenúan.
- Target `tests`: un solo `main` en `TestMain.cpp`; pruebas nuevas en `PumpModeTest.cpp` (playhead falso). Prueba pura de la curva en `tests/PumpCurveTests.cpp`. Valores esperados comprobados con un espejo en Python, no con el binario.
- README corregido: detector `0.5·(L+R)`, sin medidor de GR, sin `PLAN_ESCALADO_SIDEMK.md`.

Pendiente: compilar, correr `tests`, medir la envolvente de referencia (paso 2) y ajustar `k` con el null test (paso 5).

## 1. Proyecto y propósito

SideMK es un plugin de efecto de dinámica, estéreo, para producción musical. La identidad declarada es un compresor con sidechain externo opcional.

| Dato | Valor | Fuente |
|---|---|---|
| Nombre | SideMk | `JucePluginDefines.h`, `CMakeLists.txt` |
| Descripción | Sidechain Compressor | `JucePluginDefines.h` (`JucePlugin_Desc`), `CMakeLists.txt` |
| Fabricante | Lily | `JucePluginDefines.h`, `README.md` |
| Autor en el README | Jose Andrés Ureta Granda · Lily | `README.md` |
| Versión | 1.0.0 (`0x010000`) | `JucePluginDefines.h`, `project()` en `CMakeLists.txt` |
| Códigos | fabricante `Yrcm` (`0x5972636D`), plugin `Smk1` (`0x536D6B31`) | `JucePluginDefines.h`, `CMakeLists.txt` |
| Web / bundle | `https://example.com`, `com.lily.sidemk` | `JucePluginDefines.h` |
| Categoría VST3 | `Fx\|Dynamics` | `JucePluginDefines.h`, `CMakeLists.txt` |
| Formatos | VST3 y Standalone. AU declarado en 0 | `JucePluginDefines.h`, `FORMATS` en `CMakeLists.txt` |
| MIDI | no entra, no sale, no es instrumento ni efecto MIDI | `JucePluginDefines.h`, `PluginProcessor.h` |
| Canales | hasta 4 entradas (main L/R + sidechain L/R), 2 salidas | `JucePluginDefines.h` |

El README lo describe para insertarlo en la pista a comprimir (por ejemplo el bajo) y alimentar el bus de sidechain con una señal de control (por ejemplo el bombo).

## 2. Entorno y estado

- Lenguaje: C++17 (`CMAKE_CXX_STANDARD 17` en `CMakeLists.txt`). Framework: JUCE.
- CMake mínimo: 3.22.
- JUCE de referencia en CMake: submódulo `./JUCE` (`https://github.com/juce-framework/JUCE.git`, sin tag en `.gitmodules`). Ese checkout no tiene `CMakeLists.txt` ahora mismo.
- El configure que produjo `build/` no usó el submódulo. `build/CMakeCache.txt` apunta el código de JUCE a `build/_deps/juce-src`. Ese árbol declara `project(JUCE VERSION 8.0.13 ...)`, que es el tag de FetchContent en `CMakeLists.txt`.
- Generador grabado en la caché: Visual Studio 17 2022, plataforma x64, instancia `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools`.
- Esa carpeta de BuildTools no está en esta máquina (comprobado el 2026-10-28). `tests/README.md` (2026-10-25) ya decía que la compilación no pudo arrancar por eso, y que el entorno de entonces no tenía `cl`, `clang++` ni `g++`. No se ha vuelto a compilar en esta inspección.
- Artefactos de un build anterior, no del commit actual:
  - `build/SideMk_artefacts/Release/Standalone/SideMk.exe` — 4 de junio de 2026
  - `build/SideMk_artefacts/Release/VST3/SideMk.vst3` — 4 de junio de 2026
  - `tests/README.md` dice lo mismo: el Release existente es del 2026-06-04 y no valida el código actual.
- `COPY_PLUGIN_AFTER_BUILD` está en `FALSE`. El `.vst3` no se copia solo a la carpeta de plugins del sistema.
- `build/` está en `.gitignore`.

## 3. Funcionalidad y alcance

### Lo que el código hace

El procesador es `SidechainCompressorAudioProcessor` en `PluginProcessor.h` / `PluginProcessor.cpp`.

Buses (`PluginProcessor.cpp`, constructor):

- Entrada main, estéreo, obligatoria.
- Salida main, estéreo, obligatoria.
- Entrada sidechain, estéreo, opcional y desactivada por defecto.
- `isBusesLayoutSupported` rechaza cualquier main que no sea estéreo, y un sidechain que no sea estéreo o esté desactivado.

Parámetros en el APVTS, grupo `"Parameters"` (`createParameterLayout`):

| ID | Nombre | Rango | Defecto |
|---|---|---|---|
| `threshold` | Threshold | −60 … 0 dB, paso 0,1 | −12 dB |
| `ratio` | Ratio | 1 … 20, paso 0,1, skew 0,5 | 4 |
| `attack` | Attack | 0,1 … 500 ms, paso 0,1, skew 0,4 | 10 ms |
| `release` | Release | 1 … 2000 ms, paso 1, skew 0,4 | 100 ms |
| `gain` | Makeup Gain | −24 … +24 dB, paso 0,1 | 0 dB |

El estado se guarda como XML del APVTS (`getStateInformation` / `setStateInformation`).

DSP previsto, si `processBlock` llegara a compilar:

1. El compresor es `juce::dsp::Compressor<float>`, preparado a 1 canal (`prepareToPlay`).
2. Con 4 canales en el buffer, el detector es `0.5 * (sidechain L + sidechain R)`. Sin sidechain, el detector es solo el canal main L.
3. `compressor.processSample` devuelve ganancia × muestra. El procesador recupera la ganancia dividiendo, con piso `1.0e-6` (`kNoiseFloorThreshold`): por debajo, la ganancia de compresión es 1.
4. Esa ganancia, multiplicada por el makeup, se aplica a main L y main R. Los canales de sidechain no se escriben.
5. En JUCE 8.0.13, `juce::dsp::Compressor::processSample` (`build/_deps/juce-src/modules/juce_dsp/widgets/juce_Compressor.cpp`) rectifica con `BallisticsFilter` y aplica una curva hard-knee en amplitud lineal: por debajo del umbral la ganancia es 1; por encima es `(env / threshold) ^ (1/ratio - 1)`.

Interfaz (`PluginEditor.h`, `PluginEditor.cpp`):

- Ventana fija 540×300. Cabecera «SideMk» / «by Lily».
- Cinco knobs rotativos con `ChromePinkLookAndFeel`: THRESHOLD, RATIO, ATTACK, RELEASE, SIDECHAIN LEVEL. El último está unido al parámetro `gain` (makeup), no a un nivel de entrada del sidechain.
- Un timer a 10 Hz mira si el bus de sidechain está habilitado y pinta el LED «SIDECHAIN ON / OFF». No mide audio.
- No hay medidor de reducción de ganancia, ni curva dibujada, ni control de modo.

No hay lectura de `AudioPlayHead`, BPM, PPQ, posición de transporte ni fase musical en ningún `.h` / `.cpp` del plugin.

### Lo que el código no compila

`PluginProcessor.cpp`, dentro de `processBlock`, declara `const float makeupGain` tres veces (líneas 128, 140 y 169). Eso es C++ inválido: el procesador actual no puede construir un objeto de traducción.

En el mismo bloque la ganancia se aplica dos veces por muestra: la lambda `applyGain` (líneas 185-193) y, otra vez, el multiply de `mainL` / `mainR` (sidechain: líneas 211-217; sin sidechain: líneas 228-234). Aunque se dejara una sola variable `makeupGain`, el ducking quedaría al cuadrado.

El binario del 4 de junio de 2026 es anterior a este fuente. No sirve para escuchar el comportamiento del commit `6a8f93b`.

### Lo que la documentación afirma y el fuente no tiene

Estas frases están escritas y no tienen implementación en los `.h` / `.cpp` actuales:

| Afirmación | Dónde | Qué hay en el fuente |
|---|---|---|
| Detección enlazada por pico, `max(\|L\|, \|R\|)` | `README.md`, `tests/README.md` | Promedio `0.5 * (L + R)` del sidechain, o solo main L |
| Makeup con rampa de 20 ms | `README.md` | Conversión directa dB → ganancia, sin `SmoothedValue` |
| Medidor de GR de 24 dB | `README.md` | El editor no lo dibuja |
| `GainComputer.h`, curva pura testeable | `README.md`, `tests/GainComputerTests.cpp` | El header no está en el repositorio. El test no puede compilar |
| Buffer de ganancia segmentado y detector con estado entre segmentos | `tests/README.md` | No hay buffer de ganancia ni segmentación en `PluginProcessor.cpp` |
| Hoja de ruta `PLAN_ESCALADO_SIDEMK.md` | `README.md` | El archivo no está en el árbol ni en `git ls-tree HEAD` |
| Próximo hito: soft knee, dry/wet, filtro de sidechain, presets | `README.md` | Ninguno de esos controles existe |

`tests/GainComputerTests.cpp` espera `sidemk::computeGainReductionDb` con semántica hard-knee en dB (4:1, 12 dB sobre el umbral → −9 dB). Esa función no está, y además no es la curva de `juce::dsp::Compressor`, que opera en amplitud lineal.

### Pruebas y benchmarks

- `PluginProcessorTest.cpp`: arranque, layouts de buses, y silencio que sigue en silencio. Tiene `main`.
- `Tests.cpp`: solo layouts de buses. También tiene `main`.
- `CMakeLists.txt` mete los dos en el target `tests`. Dos `main` en un mismo ejecutable no enlazan.
- `tests/GainComputerTests.cpp` no está en ningún target de CMake. Pensado para `cl /std:c++17` suelto, y le falta el header.
- `benchmark.cpp` mide `drawRotarySlider`. `benchmark_dsp.cpp` llama `processBlock` 100 000 veces. Los dos son `juce_add_console_app` y los dos incluyen el procesador, así que tampoco compilan mientras `makeupGain` esté redeclarado.
- `tests/README.md` lista seis escuchas manuales todavía sin hacer. No hay prueba de escucha registrada.

### Fuera de este plugin

No hay sincronía al tempo, no hay disparo MIDI, no hay curva de volumen dibujada, no hay modo pump, y el standalone no tiene transporte propio. El ejecutable suelto no puede clavar nada a una rejilla: `getPlayHead()` solo lo rellena un host.

## 4. Estructura y convenciones

```
CMakeLists.txt          Target SideMk (VST3 + Standalone) y apps de consola
JucePluginDefines.h     Macros JucePlugin_*. Incluir antes de <JuceHeader.h>
PluginProcessor.h/.cpp  AudioProcessor, APVTS, processBlock
PluginEditor.h/.cpp     Editor, LookAndFeel, knobs, LED
PluginProcessorTest.cpp Pruebas JUCE del procesador (main propio)
Tests.cpp               Pruebas JUCE de buses (main propio)
tests/                  Prueba de curva y notas de validación, fuera del target
benchmark.cpp           Tiempo del knob
benchmark_dsp.cpp       Tiempo de processBlock
JUCE/                   Submódulo, sin checkout utilizable
build/                  Generado, ignorado. Contiene JUCE 8.0.13 y artefactos viejos
```

Convenciones que el código ya sigue y que un cambio de DSP tiene que respetar:

- Un solo `AudioProcessor`. Los parámetros nuevos entran en `createParameterLayout` con `ParameterID` y se leen en audio con `getRawParameterValue` (`std::atomic<float>`).
- `processBlock` no reserva memoria y no toma locks. El comentario de cabecera de la función lo pone como regla. El estado del compresor vive en el objeto, preparado en `prepareToPlay`.
- La GUI no calcula audio. El LED solo observa el bus. Un medidor o una fase de pump tendría que publicar un atómico desde el hilo de audio y leerlo en el `Timer`.
- Los knobs son `KnobGroup` (slider + label) más `SliderAttachment`. El layout es una fila de columnas iguales bajo una cabecera de 60 px.
- Paleta fija en `PluginEditor.cpp`: fondo `0xff121212`, acento `0xffff1493`, LED `0xff8a2be2`.
- Comentarios de DSP en inglés. El README y `tests/README.md` están en español.
- Identidad duplicada a propósito: un cambio de nombre o de código de plugin se hace en `JucePluginDefines.h` y en `CMakeLists.txt` a la vez (el header lo dice en el comentario inicial).
- `JucePluginDefines.h` usa `#ifndef` para no pelearse con los `-D` que inyecta `juce_add_plugin`.

## 5. Restricciones

Explícitas:

- Tiempo real de audio: sin heap y sin locks en `processBlock` (`PluginProcessor.cpp`).
- Main estéreo obligatorio. Sidechain estéreo o desactivado (`isBusesLayoutSupported`).
- C++17, CMake ≥ 3.22, JUCE 8.0.13 si no hay checkout local.
- VST3 y Standalone. AU apagado.
- El script de Windows que cita el README (`build_windows.ps1`) no está en `git ls-tree HEAD`. No forma parte del flujo real de este árbol. Lo mismo `Dockerfile` y `build_linux.sh`.
- `tests/README.md` pide no tocar el `CMakeLists.txt` de las pruebas de la curva, para no cambiar el flujo de build. El target `tests` actual, con dos `main`, ya no enlaza.

Por confirmar:

- Si en esta máquina se puede instalar Visual Studio 2022 BuildTools, o si el compile se hace en otra.
- Si el submódulo `JUCE` debe poblarse (`git submodule update --init`) o si se sigue con el FetchContent 8.0.13 de `build/_deps`.
- Presupuesto, fecha y si el resultado se publica. No están escritos.
- Comportamiento real del binario de junio: no corresponde a este fuente y no se ha abierto en un host durante esta inspección.

Riesgo inmediato para cualquier comparación de audio: el fuente del procesador no compila, y el `.vst3` / `.exe` de `build/SideMk_artefacts/Release` es de otra fecha.

## 6. Implicaciones para la ingeniería inversa

Objetivo de trabajo, decidido en la conversación y todavía no reflejado en el código: recrear el pump de Nicky Romero Kickstart clavado a la rejilla del DAW.

Kickstart, en ese uso, multiplica la señal por una curva de volumen que se repite cada división musical (el caso famoso es la negrita, 1 tiempo de negra). La fase sale del transporte del host, no del nivel del bombo. El compresor de SideMK hace otra cosa: estima una envolvente con `juce::dsp::Compressor` y de ahí saca una ganancia. Subir el ratio no produce esa curva ni la clava al compás.

Método de trabajo, para que el siguiente cambio tenga contra qué medirse:

1. Arreglar `processBlock` hasta que el árbol compile. Sin eso no hay null test.
2. Medir Kickstart por la salida de audio, con un seno constante o un DC, a un BPM conocido y rate 1/4. La envolvente de ese bounce es la referencia. No hace falta abrir el binario de Cableguys ni copiar sus tablas: la referencia es una curva medida, y la implementación de SideMK es una fórmula propia (el punto de partida acordado es `1 - exp(-k * fase)`, con `k` cerca de 6,5).
3. Añadir la curva como modo distinto del compresor. La fase es el PPQ del `AudioPlayHead` al inicio del bloque, más `i * (bpm / 60) / sampleRate` por muestra. La división por defecto es 1 negra (1,0 PPQ).
4. El VST3 dentro del DAW es el único sitio donde esa fase existe. El standalone no trae BPM ni PPQ.
5. Comprobar restando el bounce de SideMK contra el de Kickstart. Un pulso en cada beat significa fase corrida, no una curva mal ajustada.

Lo que no hay que reutilizar como especificación del pump:

- El README, donde el detector, el makeup y el medidor no coinciden con el editor ni con el procesador.
- `tests/README.md` y `GainComputerTests.cpp`, que describen un `GainComputer` y un enlace por pico que no están en el fuente.
- El `.vst3` del 4 de junio de 2026.

Punto de inserción cuando se implemente: parámetros nuevos en `createParameterLayout`, rama nueva en `processBlock` (la rama del compresor se queda, con una sola aplicación de ganancia), y controles en `PluginEditor` sin calcular audio en el hilo de interfaz.

# SideMk

Compresor estéreo con **sidechain** y modo **pump** sincronizado al tempo, para producción musical, construido con [JUCE 8](https://juce.com) y C++17.
Se compila como **VST3** y **Standalone** (AU opcional en macOS).

## Características

- **Sidechain externo opcional.** Si el bus de sidechain está activo, controla la compresión. Si no, el plugin funciona como un compresor feedforward normal sobre la señal principal.
- **Modo Pump**: curva de volumen que se repite en cada división musical (1/16 … 1 compás), clavada al PPQ del transporte del DAW. No depende del nivel del bombo ni del sidechain.
- **Detector** del compresor: mezcla mono `0.5 * (L + R)` del sidechain, o el canal main L si no hay sidechain.
- **Curva hard-knee** de `juce::dsp::Compressor` con envolvente de ataque/release (`juce::dsp::BallisticsFilter`).
- **Makeup gain y profundidad del pump suavizados** (rampa de 20 ms) para automatizar sin clics.
- **Indicador LED** del estado del bus de sidechain. (Todavía no hay medidor de reducción de ganancia.)
- **Procesado de audio seguro en tiempo real**: sin reservas de memoria, sin locks y con parámetros leídos desde atómicos.

### Controles

| Control | Rango | Por defecto | Modo |
|---|---|---|---|
| Mode | Compressor / Pump | Compressor | — |
| Threshold | −60 … 0 dB | −12 dB | Compressor |
| Ratio | 1 … 20 : 1 | 4 : 1 | Compressor |
| Attack | 0,1 … 500 ms | 10 ms | Compressor |
| Release | 1 … 2000 ms | 100 ms | Compressor |
| Pump Rate | 1/16, 1/8, 1/4, 1/2, 1 Bar | 1/4 | Pump |
| Pump Depth | 0 … 100 % | 100 % | Pump |
| Makeup | −24 … +24 dB | 0 dB | ambos |

La interfaz atenúa los controles que el modo activo no usa; siguen siendo automatizables.

## Modo Pump

La ganancia sigue una curva fija por ciclo (ver `PumpCurve.h`): una caída lineal de 1 ms hasta `1 − depth` (solo para evitar el clic) y una recuperación `1 − exp(−k·t)` normalizada, con `k = 6,5`. `k` es un valor de partida y se ajustará contra una envolvente de referencia medida.

- **Con el transporte del DAW en marcha**, la fase sale del PPQ que reporta el host en cada bloque, así que el dip cae en la rejilla. `1 Bar` usa el compás del host (por ejemplo, 3 negras en 3/4) y el inicio de compás que reporte.
- **Con el transporte parado o en el Standalone** (que no tiene transporte), la curva corre libre al tempo del host o, si no hay, a 120 BPM.

## Uso del sidechain

1. Inserta SideMk en la pista que quieres comprimir (por ejemplo, el bajo).
2. En tu DAW, envía la señal de control (por ejemplo, el bombo) a la **entrada sidechain** del plugin.
3. El LED superior derecho se enciende cuando el sidechain está activo.
4. Ajusta **Threshold** y **Ratio** hasta que el medidor muestre la reducción deseada.

Si no se conecta ningún sidechain, SideMk comprime con la propia señal de entrada.

## Compilación

### Requisitos

- CMake ≥ 3.22
- Un compilador C++17:
  - **Windows:** Visual Studio 2022 o Build Tools, con "Desarrollo de escritorio con C++"
  - **macOS:** Xcode
  - **Linux:** GCC o Clang, más las dependencias de JUCE (ver `Dockerfile`)
- JUCE: se usa el submódulo `./JUCE` o, si está vacío, CMake descarga JUCE 8.0.13 automáticamente.

### Pasos

```sh
git clone --recursive https://github.com/Jose14Andres/SideMK.git
cd SideMK
cmake -B build
cmake --build build --config Release
```

Targets individuales:

```sh
cmake --build build --config Debug --target SideMk_VST3
cmake --build build --config Debug --target SideMk_Standalone   # la forma más rápida de probar
```

### Pruebas

```sh
cmake --build build --config Debug --target tests
./build/tests_artefacts/Debug/tests        # devuelve 1 si falla alguna prueba
```

La curva del pump también se prueba sin JUCE (`tests/PumpCurveTests.cpp`, instrucciones en el propio archivo).

### Scripts de distribución

- **Windows:** `./build_windows.ps1` compila en Release y copia `SideMk.vst3` y `SideMk.exe` a `%USERPROFILE%\Desktop\dist`. ⚠️ El script **borra y recrea** esa carpeta en cada ejecución.
- **Linux:** `./build_linux.sh` compila en `build-linux/`.
- **Docker:** compila los binarios de Linux:
  ```sh
  docker build -t sidemk-build .
  docker run --rm -v "$(pwd)/dist:/workspace/out" sidemk-build
  ```

## Estructura

```
PluginProcessor.{h,cpp}   Motor de audio y parámetros
PumpCurve.h               Curva del modo pump (funciones puras, sin JUCE)
PluginEditor.{h,cpp}      Interfaz, LookAndFeel y medidor de GR
JucePluginDefines.h       Macros JucePlugin_* (incluir antes de <JuceHeader.h>)
CMakeLists.txt            Configuración de build y formatos
```

## Hoja de ruta

Próximo hito: compilar y ejecutar el target `tests`, medir una envolvente de referencia del pump (seno constante, BPM conocido, 1/4) y ajustar `k` con un null test. Después, una beta instalable.
Más adelante: medidor de reducción de ganancia, soft knee, mezcla dry/wet, filtro en el sidechain y presets.

## Autor

Jose Andrés Ureta Granda · Lily

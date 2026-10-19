# SideMk

Compresor estéreo con **sidechain** para producción musical, construido con [JUCE 8](https://juce.com) y C++17.
Se compila como **VST3** y **Standalone** (AU opcional en macOS).

## Características

- **Sidechain externo opcional.** Si el bus de sidechain está activo, controla la compresión. Si no, el plugin funciona como un compresor feedforward normal sobre la señal principal.
- **Detección estéreo enlazada** por pico (`max(|L|, |R|)`), sin las cancelaciones de fase de sumar L+R.
- **Curva hard-knee** con envolvente de ataque/release (`juce::dsp::BallisticsFilter`).
- **Makeup gain suavizado** (rampa de 20 ms) para automatizar sin clics.
- **Medidor de reducción de ganancia** (escala de 24 dB) e indicador LED del estado del sidechain.
- **Procesado de audio seguro en tiempo real**: sin reservas de memoria, sin locks y con parámetros leídos desde atómicos.

### Controles

| Control | Rango | Por defecto |
|---|---|---|
| Threshold | −60 … 0 dB | −12 dB |
| Ratio | 1 … 20 : 1 | 4 : 1 |
| Attack | 0,1 … 500 ms | 10 ms |
| Release | 1 … 2000 ms | 100 ms |
| Sidechain Level (makeup) | −24 … +24 dB | 0 dB |

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
GainComputer.h            Curva de compresión (función pura, testeable)
PluginEditor.{h,cpp}      Interfaz, LookAndFeel y medidor de GR
JucePluginDefines.h       Macros JucePlugin_* (incluir antes de <JuceHeader.h>)
CMakeLists.txt            Configuración de build y formatos
```

## Hoja de ruta

Ver [`PLAN_ESCALADO_SIDEMK.md`](PLAN_ESCALADO_SIDEMK.md). Próximo hito: validación del DSP y pruebas automatizadas; después, una beta instalable.
Más adelante: soft knee, mezcla dry/wet, filtro en el sidechain y presets.

## Autor

Jose Andrés Ureta Granda · Lily

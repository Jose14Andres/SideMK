cmake_minimum_required(VERSION 3.22)

project(SideMk
    VERSION     1.0.0
    DESCRIPTION "Sidechain Compressor VST3 Plugin"
    LANGUAGES   C CXX)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Compilación paralela en MSVC (sin costo, siempre conviene)
if(MSVC)
    add_compile_options(/MP)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# JUCE — dos opciones, en orden de preferencia:
#
#   Opción A (recomendada): clona JUCE dentro del proyecto y apunta aquí.
#     git clone https://github.com/juce-framework/JUCE.git
#
#   Opción B (automática):  si JUCE no existe localmente, FetchContent lo
#                            descarga en tiempo de configuración (~250 MB).
#
# Para forzar la Opción A, pasa -DJUCE_PATH=/ruta/a/JUCE al invocar cmake.
# ─────────────────────────────────────────────────────────────────────────────
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE"
    CACHE PATH "Ruta al checkout local de JUCE")

if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "[SideMk] Usando JUCE local en: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    message(STATUS "[SideMk] JUCE no encontrado en ${JUCE_PATH}.")
    message(STATUS "[SideMk] Descargando JUCE 8.0.13 via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Plugin target
# ─────────────────────────────────────────────────────────────────────────────
juce_add_plugin(SideMk

    # ── Identidad ──────────────────────────────────────────────────────────
    PRODUCT_NAME              "SideMk"           # nombre que muestra el host
    COMPANY_NAME              "Lily"              # ← cambia esto
    COMPANY_WEBSITE           "https://example.com"

    # Códigos de 4 caracteres obligatorios (OSType / Steinberg ID).
    # PLUGIN_MANUFACTURER_CODE debe ser único por empresa.
    # PLUGIN_CODE             debe ser único por plugin dentro de tu empresa.
    PLUGIN_MANUFACTURER_CODE  "Yrcm"
    PLUGIN_CODE               "Smk1"

    # ── Formatos ───────────────────────────────────────────────────────────
    # VST3:       plugin de producción (Windows / macOS / Linux)
    # Standalone: ejecutable independiente, útil para depuración
    # AU:         descomenta solo en macOS
    FORMATS                   VST3 Standalone
    # FORMATS                 VST3 AU Standalone   # ← macOS

    # ── Capacidades ────────────────────────────────────────────────────────
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # ── Metadatos VST3 ─────────────────────────────────────────────────────
    VST3_CATEGORIES           "Fx" "Dynamics"

    # ── Despliegue ─────────────────────────────────────────────────────────
    # TRUE copia el .vst3 a %CommonProgramFiles%\VST3\ tras compilar.
    # Requiere permisos de administrador en Windows.
    COPY_PLUGIN_AFTER_BUILD   FALSE
)

# ─────────────────────────────────────────────────────────────────────────────
# Fuentes
# ─────────────────────────────────────────────────────────────────────────────
target_sources(SideMk PRIVATE
    JucePluginDefines.h   # fuente canónica de macros JucePlugin_*; ver comentarios internos
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp)

# Asegura que #include "JucePluginDefines.h" se resuelva desde cualquier
# unidad de traducción del target (necesario para IDEs que no usan CMake).
target_include_directories(SideMk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# Genera JuceHeader.h en el directorio de build con los includes de módulos.
# juce_add_plugin() ya inyecta JucePlugin_Name etc. como flags de compilador;
# JucePluginDefines.h actúa como fallback para builds manuales / IntelliSense.
juce_generate_juce_header(SideMk)

# ─────────────────────────────────────────────────────────────────────────────
# Definiciones de compilación
# ─────────────────────────────────────────────────────────────────────────────
target_compile_definitions(SideMk
    PUBLIC
        # Elimina dependencias innecesarias del sistema
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0

        # El plugin no reemplaza VST2 (Steinberg ya no permite VST2)
        JUCE_VST3_CAN_REPLACE_VST2=0

        # Sin splash screen (licencia GPL o comercial)
        JUCE_DISPLAY_SPLASH_SCREEN=0

        # Silencia el aviso de assertion en builds Release
        $<$<CONFIG:Release>:JUCE_DISABLE_ASSERTIONS=1>)

# ─────────────────────────────────────────────────────────────────────────────
# Módulos JUCE requeridos
# ─────────────────────────────────────────────────────────────────────────────
target_link_libraries(SideMk
    PRIVATE
        juce::juce_audio_utils   # AudioProcessor, APVTS, AudioProcessorEditor
        juce::juce_dsp           # dsp::Compressor, AudioBlock, ProcessSpec
        juce::juce_gui_basics    # Component, Slider, Label, Timer
        juce::juce_gui_extra     # utilidades GUI adicionales
    PUBLIC
        # Flags de compilador recomendados por JUCE (optimizaciones, warnings)
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_add_console_app(benchmark)
target_sources(benchmark PRIVATE benchmark.cpp PluginEditor.cpp PluginProcessor.cpp)
target_include_directories(benchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
juce_generate_juce_header(benchmark)
target_link_libraries(benchmark PRIVATE juce::juce_gui_basics juce::juce_graphics juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)
target_compile_definitions(benchmark PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

juce_add_console_app(benchmark_dsp)
target_sources(benchmark_dsp PRIVATE benchmark_dsp.cpp PluginProcessor.cpp PluginEditor.cpp)
target_include_directories(benchmark_dsp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
juce_generate_juce_header(benchmark_dsp)
target_link_libraries(benchmark_dsp PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra)
target_compile_definitions(benchmark_dsp PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

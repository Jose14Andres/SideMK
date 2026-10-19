# ─────────────────────────────────────────────────────────────────────────────
# SideMk — Entorno de build headless para Linux (CI/CD)
#
# Compila el VST3 + Standalone de Linux. NO produce artefactos de Windows/macOS.
# JUCE se resuelve via FetchContent si ./JUCE está vacío (ver CMakeLists.txt).
#
# Uso:
#   docker build -t sidemk-build .
#   docker run --rm -v "$(pwd)/dist:/workspace/out" sidemk-build
#     → copia los artefactos compilados a ./dist en el host
# ─────────────────────────────────────────────────────────────────────────────
FROM ubuntu:24.04

# Evita prompts interactivos de apt durante el build de la imagen.
ENV DEBIAN_FRONTEND=noninteractive

# ── Dependencias de build de JUCE 8 (headless) ───────────────────────────────
# build-essential  → g++, make
# cmake, git, ca-certificates → configuración + FetchContent de JUCE
# lib*-dev         → ALSA, JACK, cURL, FreeType y X11 requeridos por juce_gui/audio
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        pkg-config \
        libasound2-dev \
        libjack-jackd2-dev \
        libcurl4-openssl-dev \
        libfreetype6-dev \
        libx11-dev \
        libxcomposite-dev \
        libxcursor-dev \
        libxext-dev \
        libxinerama-dev \
        libxrandr-dev \
        libxrender-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copia el árbol del proyecto dentro de la imagen.
COPY . /workspace

# El script de build es la fuente única de verdad de los pasos de compilación.
RUN chmod +x /workspace/build_linux.sh

# Por defecto, al arrancar el contenedor se ejecuta el build en Release.
ENTRYPOINT ["/workspace/build_linux.sh"]

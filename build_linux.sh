#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# SideMk — Build headless en Linux (Release)
#
# Se ejecuta dentro del contenedor Docker (ver Dockerfile) o en cualquier host
# Linux con las dependencias de JUCE 8 instaladas. Configura y compila el
# proyecto en Release y, si existe /workspace/out (montado como volumen),
# copia allí los artefactos generados.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

# Raíz del proyecto = carpeta que contiene este script.
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-linux"
CONFIG="Release"

# Núcleos disponibles para compilación paralela.
JOBS="$(nproc 2>/dev/null || echo 2)"

echo "[SideMk] Configurando (${CONFIG})..."
cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" -DCMAKE_BUILD_TYPE="${CONFIG}"

echo "[SideMk] Compilando con ${JOBS} jobs..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j "${JOBS}"

echo "[SideMk] Build completado."

# ── Recolección opcional de artefactos ────────────────────────────────────────
# Si se montó un volumen en /workspace/out, copiamos ahí los binarios.
ARTEFACTS="${BUILD_DIR}/SideMk_artefacts/${CONFIG}"
OUT_DIR="/workspace/out"

if [ -d "${OUT_DIR}" ]; then
    echo "[SideMk] Copiando artefactos a ${OUT_DIR}..."
    # VST3 (bundle) y Standalone de Linux.
    [ -d "${ARTEFACTS}/VST3" ]       && cp -r "${ARTEFACTS}/VST3/."       "${OUT_DIR}/" || true
    [ -d "${ARTEFACTS}/Standalone" ] && cp -r "${ARTEFACTS}/Standalone/." "${OUT_DIR}/" || true
    echo "[SideMk] Artefactos disponibles en ${OUT_DIR}."
else
    echo "[SideMk] (Sin volumen en ${OUT_DIR}; artefactos en ${ARTEFACTS})"
fi

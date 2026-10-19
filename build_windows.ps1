<#
.SYNOPSIS
    SideMk — Build de Windows (Release) + empaquetado para distribución.

.DESCRIPTION
    Automatiza la compilación del plugin en Windows:
      1. cmake -B build
      2. cmake --build build --config Release
      3. Copia el VST3 (bundle) y el Standalone (.exe) a una carpeta dist/
         creada en el Escritorio del usuario, lista para distribuir.

.NOTES
    Requiere Visual Studio 2022 + CMake en el PATH.
    Ejecutar desde la raíz del repo:  .\build_windows.ps1
#>

# Detiene el script ante el primer error no controlado.
$ErrorActionPreference = 'Stop'

# ── Rutas base ────────────────────────────────────────────────────────────────
# Raíz del proyecto = carpeta que contiene este script.
$ProjectRoot = $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot 'build'
$Config      = 'Release'

# Carpeta de distribución en el Escritorio del usuario.
$Desktop = [Environment]::GetFolderPath('Desktop')
$DistDir = Join-Path $Desktop 'dist'

Write-Host "[SideMk] Raiz del proyecto : $ProjectRoot"
Write-Host "[SideMk] Carpeta de build  : $BuildDir"
Write-Host "[SideMk] Carpeta dist      : $DistDir"

# ── 1. Configurar ─────────────────────────────────────────────────────────────
Write-Host "`n[SideMk] Configurando CMake..." -ForegroundColor Cyan
cmake -B $BuildDir -S $ProjectRoot
if ($LASTEXITCODE -ne 0) { throw "cmake configure fallo (codigo $LASTEXITCODE)" }

# ── 2. Compilar ───────────────────────────────────────────────────────────────
Write-Host "`n[SideMk] Compilando ($Config)..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "cmake build fallo (codigo $LASTEXITCODE)" }

# ── 3. Empaquetar en dist/ ────────────────────────────────────────────────────
$Artefacts = Join-Path $BuildDir "SideMk_artefacts\$Config"
$Vst3Src   = Join-Path $Artefacts 'VST3\SideMk.vst3'          # bundle (carpeta)
$ExeSrc    = Join-Path $Artefacts 'Standalone\SideMk.exe'

# (Re)crea una carpeta dist limpia.
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory -Path $DistDir | Out-Null

Write-Host "`n[SideMk] Copiando artefactos a dist..." -ForegroundColor Cyan

# El .vst3 es un bundle (carpeta) en Windows → copia recursiva.
if (Test-Path $Vst3Src) {
    Copy-Item $Vst3Src (Join-Path $DistDir 'SideMk.vst3') -Recurse -Force
    Write-Host "  + SideMk.vst3"
} else {
    Write-Warning "  No se encontro el VST3 en: $Vst3Src"
}

if (Test-Path $ExeSrc) {
    Copy-Item $ExeSrc $DistDir -Force
    Write-Host "  + SideMk.exe"
} else {
    Write-Warning "  No se encontro el Standalone en: $ExeSrc"
}

Write-Host "`n[SideMk] Listo. Artefactos en: $DistDir" -ForegroundColor Green

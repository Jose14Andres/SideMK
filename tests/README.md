# Validación DSP de SideMk

## Estado de esta revisión (2026-10-25)

- Revisión estática: la curva aplica la reducción esperada para una relación mayor
  que 1; la detección enlazada usa el máximo absoluto de ambos canales; el detector
  conserva estado entre segmentos y el buffer auxiliar limita cada segmento.
- Las pruebas de la curva están escritas, pero no ejecutadas: el entorno no tiene
  `cl`, `clang++` ni `g++`.
- El build de Release existente es del 2026-06-04 y no valida el código actual. La
  compilación actual no pudo empezar porque el generador configurado requiere Visual
  Studio 2022 BuildTools, que no está instalado.
- No se ha hecho una prueba de escucha en un host. Sidechain externo, tiempos de
  ataque/release y recuperación en silencio siguen pendientes de validación práctica.

## Curva de compresión

`GainComputerTests.cpp` comprueba el umbral hard-knee, la relación 1:1 y la pendiente
de la curva. Es independiente de JUCE y de la configuración del plugin. Compílalo
desde la raíz del repositorio con un compilador C++17:

```powershell
cl /nologo /std:c++17 /EHsc tests\GainComputerTests.cpp /Fe:GainComputerTests.exe
.\GainComputerTests.exe
```

## Comprobaciones manuales pendientes del procesador

Estas comprobaciones requieren cargar el plugin en un host de audio:

1. Con threshold en −12 dB, ratio 4:1 y ataque/release cortos, una senoide estable
   a −6 dBFS debe converger a aproximadamente −9 dB de reducción.
2. A nivel igual o inferior al threshold, el medidor debe permanecer en cero.
3. Con señal principal silenciosa y tono en el sidechain externo, debe haber
   reducción; al desactivar el bus, el detector debe volver a la señal principal.
4. Comparar canales principales idénticos y opuestos en fase: la detección enlazada
   por pico debe producir la misma reducción.
5. Al detener la señal, comprobar la recuperación según el release y que el medidor
   vuelva a cero sin valores no finitos.
6. Repetir con tamaños de bloque mayores que el indicado en `prepareToPlay` para
   confirmar que el procesamiento segmentado no altera el resultado ni se sale del
   buffer de ganancia.

Estas comprobaciones de integración aún no están automatizadas. No se modifica la
configuración CMake del proyecto para mantener intacto el flujo de compilación actual.

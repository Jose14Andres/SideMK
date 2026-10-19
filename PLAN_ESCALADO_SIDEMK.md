# Plan para escalar SideMk

## Contexto del proyecto

SideMk es un plugin de compresión estéreo con sidechain, construido con JUCE 8 y C++17. Actualmente genera formatos VST3 y Standalone mediante CMake.

La base funcional incluye:

- Procesamiento estéreo con sidechain externo opcional y detección desde la señal principal cuando el sidechain está deshabilitado.
- Controles de threshold, ratio, attack, release y makeup gain.
- Parámetros persistidos mediante AudioProcessorValueTreeState.
- Suavizado de makeup gain y medidor de reducción de ganancia con comunicación lock-free hacia la interfaz.
- Interfaz con cinco controles rotatorios e indicador de sidechain.

El procesamiento está principalmente en PluginProcessor.cpp/.h; la interfaz, en PluginEditor.cpp/.h. La configuración de formatos y dependencias está en CMakeLists.txt. El proyecto no tiene pruebas automatizadas según sus notas actuales. JUCE puede resolverse mediante FetchContent; el checkout local está vacío/no inicializado. El directorio build/ existente contiene una compilación generada.

## Próximos pasos priorizados

### 1. Asegurar la base antes de añadir funciones

Validar el comportamiento del compresor con señales conocidas: threshold, ratio, ataque, release, sidechain externo y entrada en silencio. Revisar el seguimiento de envolvente y la lectura de canales sidechain en distintos hosts.

**Resultado esperado:** comportamiento DSP predecible y documentado, con casos límite identificados.

### 2. Añadir pruebas reproducibles

Crear pruebas para la curva de compresión, la restauración de parámetros y los casos de canales habilitados o deshabilitados. Incorporar una comprobación de compilación para VST3 y Standalone en cada cambio relevante.

**Resultado esperado:** poder detectar regresiones antes de distribuir nuevas versiones.

### 3. Mejorar la medición y el control

Hacer más claros los valores numéricos, calibrar el medidor de reducción y evaluar controles de mezcla dry/wet y nivel de entrada. Al añadir parámetros, conservar la compatibilidad de los estados guardados.

**Resultado esperado:** más control y lectura del efecto sin perder sesiones existentes.

### 4. Pulir la experiencia de uso

Revisar legibilidad y escalado de la interfaz, añadir ayuda contextual y explicar cómo enrutar la señal al sidechain. Comprobar el editor en distintos tamaños y escalas de pantalla.

**Resultado esperado:** instalación, configuración y operación más sencillas para nuevos usuarios.

### 5. Preparar compatibilidad y distribución

Probar en los DAWs y sistemas operativos objetivo; definir instalador, firma, versionado y notas de lanzamiento. Actualmente CMake configura VST3 y Standalone. Confirmar también la estrategia para obtener JUCE en builds reproducibles.

**Resultado esperado:** una beta instalable que otros puedan probar de forma consistente.

### 6. Definir una dirección de producto

Precisar qué distingue a SideMk —por ejemplo, compresión sencilla con sidechain visual— y recoger comentarios de usuarios antes de ampliar el alcance. Posibles funciones futuras incluyen sidechain filtrado, knee ajustable y presets.

**Resultado esperado:** priorizar funciones según el uso real y mantener un alcance coherente.

## Primer hito recomendado

Cerrar la validación DSP y las pruebas básicas; después, preparar una beta instalable y recoger feedback. Antes de una refactorización amplia, revisar y preservar los cambios locales existentes en el repositorio.

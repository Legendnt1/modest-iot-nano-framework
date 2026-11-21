# Limitaciones y compatibilidad

## Compatibilidad

| Aspecto | Estado | Evidencia |
|---|---|---|
| ESP32 (`esp32:esp32:esp32`) | Compilación de ejemplos configurada en CI | `.github/workflows/compile-examples.yml` |
| ESP32-S3 (`esp32:esp32:esp32s3`) | Compilación de ejemplos configurada en CI | `.github/workflows/compile-examples.yml` |
| ESP32-C3, C6, H2 | Declarado en el README para BLE; **no** compilado en CI | README |
| Placas no ESP32 (AVR, Arduino Nano clásico, ESP8266…) | No soportadas | `architectures=esp32`; uso de FreeRTOS y APIs de Arduino-ESP32 |
| Core Arduino-ESP32 | Serie 3.x según el uso de `timerBegin(frecuencia)`; versión mínima no declarada | `src/Device.cpp`, README |
| Pruebas en hardware | No documentadas en el repositorio ni realizadas para este sitio | — |

!!! note "El nombre «Nano»"
    «Nano» forma parte del nombre del framework; no indica compatibilidad con la placa
    Arduino Nano. La única arquitectura declarada es `esp32`.

## Discrepancias detectadas

Encontradas al contrastar documentación existente, ejemplos y código durante la
preparación de este sitio. No se ha modificado el comportamiento del código.

| # | Discrepancia | Evidencia | Impacto |
|---|---|---|---|
| 1 | Los ejemplos llamaban a `initializeAsynchronousEngine()` **antes** de registrar sensores, pero el código rechaza cualquier registro posterior al arranque (`engineStarted`). | `src/Device.cpp` (`initializeAsynchronousEngine`, `appendSensorToScheduler`, `appendSensorToTriggeringScheduler`, `appendToBothSchedulers`); constructores de `examples/*/*.ino`; simulación de `StatusIndicator` en Wokwi (`Blinker registration failed`, LED sin parpadeo) | Corregido en los 22 ejemplos que registran sensores o eventos (y en el README): ahora registran antes de arrancar el motor. Comprobado en simulación solo con `StatusIndicator`, antes del cambio (el fallo). El código de `src/` no cambia. |
| 2 | El ejemplo mínimo del README declara el constructor de `StatusIndicator` en la sección `private:` y lo instancia con `new` desde `setup()`, lo que no compila; además difiere del sketch real. | README, sección *Usage*; `examples/StatusIndicator/StatusIndicator.ino` | El sitio usa el sketch real en lugar del fragmento del README. |
| 3 | El README presenta el registro de PlatformIO como trabajo futuro, pero la biblioteca ya figura en ese registro (1.1.2). | README, *Future Directions*; registro de PlatformIO consultado el 2026-11-20 | Solo documental. |
| 4 | La estructura de ejemplo del README menciona `TelemetryPackage.cpp`, que no existe (la clase es solo cabecera). | README, *Manual Installation*; `src/` | Solo documental. |
| 5 | `stopAsynchronousEngine()` llama a `timerDetachInterrupt()` después de `timerEnd()` sobre el mismo puntero. En Arduino-ESP32 3.x `timerEnd()` libera el temporizador, por lo que podría tratarse de un uso tras liberación. | `src/Device.cpp`, `stopAsynchronousEngine` | Posible defecto; **pendiente de confirmar** con el código del core. |
| 6 | `library.properties`/`library.json` anuncian colas de mensajes *thread-safe*; no hay prueba que lo verifique de forma concurrente. | Metadatos; `tests/` usa dobles sin concurrencia real | Tratar como afirmación del proyecto, no verificada aquí. |
| 7 | `.libraryignore` excluye `tests/`, `CMakeLists.txt` y documentos internos «del Library Manager», pero no es un mecanismo documentado del Library Manager de Arduino. | `.libraryignore` | Esos archivos podrían distribuirse igualmente. **Pendiente de confirmar.** |

## Límites de esta documentación

- **Sin ejecución en hardware:** ninguna ficha ni ejemplo incluye resultados observados.
- **Sin compilación local de ejemplos:** la compilación para ESP32 la realiza la CI; al
  generar este sitio no se ejecutó `arduino-cli`.
- **Pruebas host:** las fichas indican qué suites existen y qué casos describen, no si
  pasan. El resultado está en el workflow *Host Unit Tests*.
- **Descripciones en inglés:** los textos extraídos del código se muestran sin traducir.
- **Búsqueda separada:** el buscador del sitio no indexa la referencia de Doxygen (ver
  [Referencia de API](api.md#busqueda)).
- **Esquemas eléctricos:** no se documentan cableados, tensiones ni resistencias.

## Accesibilidad

El sitio aplica las prácticas de [ACCESSIBILITY.md](../ACCESSIBILITY.md): jerarquía de
encabezados, texto alternativo o descripción textual para cada diagrama, bloques de código
con lenguaje declarado, información no basada solo en el color y foco visible al navegar
con teclado. **No se ha realizado una evaluación formal** con lectores de pantalla ni frente a
un estándar (WCAG u otro); la referencia de Doxygen tampoco se ha revisado en este sentido.

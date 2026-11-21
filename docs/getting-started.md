# Primeros pasos

Esta guía usa solo datos comprobados en el repositorio (`library.properties`,
`library.json`, workflows de CI y ejemplos). Donde algo no está verificado se indica.

## Requisitos

| Requisito | Valor | Evidencia |
|---|---|---|
| Arquitectura | ESP32 | `architectures=esp32` en `library.properties`; `platforms: espressif32` en `library.json` |
| Entorno | Arduino (IDE 2.x o `arduino-cli`) o PlatformIO con framework `arduino` | `library.properties`, `library.json` |
| Core de placa | Arduino-ESP32 (`esp32:esp32`) | `compile-examples.yml` instala `esp32:esp32` sin fijar versión |
| ArduinoJson | `>=7.0.0` | `depends=` en `library.properties` (CI usa 7.4.3) |
| MQTT | `>=2.5.3` | `depends=` en `library.properties` (CI usa 2.5.3) |
| Otras bibliotecas | `WiFi`, `BLE`, `HTTPClient`, `Preferences`, `LittleFS`, `ArduinoOTA`, `Wire`, `SPI` | Incluidas por `src/`; las aporta el core Arduino-ESP32 (rutas referenciadas en `CMakeLists.txt`) |

!!! warning "Versión mínima del core"
    El código usa la API de temporizadores `timerBegin(frecuencia)` y
    `timerAttachInterruptArg`, propia de la serie 3.x del core Arduino-ESP32. El repositorio
    no declara una versión mínima exacta; el README recomienda «core 3.x».
    <!-- PENDIENTE: confirmar la versión mínima del core Arduino-ESP32 compilando con versiones concretas. -->

## Instalación

=== "Arduino Library Manager"

    1. Abre **Herramientas › Administrar bibliotecas…** en Arduino IDE 2.x.
    2. Busca **Modest-IoT-Nano-Framework** e instala la versión deseada.
    3. Acepta la instalación de las dependencias (`ArduinoJson`, `MQTT`).

    La biblioteca figura en el índice oficial del Library Manager con las versiones
    1.0.0, 1.1.0, 1.1.1 y 1.1.2 (comprobado el 2026-11-20).

=== "arduino-cli"

    ```sh
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    arduino-cli lib install "Modest-IoT-Nano-Framework"
    ```

    Para trabajar con una copia local del repositorio (como hace la CI):

    ```sh
    arduino-cli lib install "ArduinoJson@7.4.3" "MQTT@2.5.3"
    arduino-cli compile --fqbn esp32:esp32:esp32 --library /ruta/al/repositorio \
        /ruta/al/repositorio/examples/StatusIndicator/StatusIndicator.ino
    ```

=== "PlatformIO"

    El registro de PlatformIO publica `Modest-IoT-Nano-Framework` 1.1.2 bajo los
    propietarios `modest-iot` y `avelasquezn` (comprobado el 2026-11-20). Con el
    propietario declarado en `library.json`:

    ```ini
    [env:esp32]
    platform = espressif32
    framework = arduino
    board = esp32dev
    lib_deps = modest-iot/Modest-IoT-Nano-Framework@1.1.2
    ```

    `board = esp32dev` es solo un ejemplo de placa ESP32: elige la tuya.
    Esta configuración no se ha compilado durante la preparación de esta documentación.

=== "Copia manual"

    Copia o clona el repositorio dentro de la carpeta `libraries` de tu instalación de
    Arduino e instala `ArduinoJson` y `MQTT` desde el Library Manager.

## Primer sketch

Todos los dispositivos derivan de [`Device`](components/Device.md). El sketch mínimo del
repositorio es [`StatusIndicator`](examples/StatusIndicator.md):

1. **Incluye** la cabecera paraguas `#include <ModestIoT.h>`.
2. **Deriva** una clase de `Device` y declara como miembros sus componentes
   (aquí un [`Led`](components/Led.md) cuyo manejador padre es el propio dispositivo).
3. **Registra** fuentes de eventos con `appendSensorToScheduler(...)` y **arranca el motor**
   con `initializeAsynchronousEngine(...)` (ver la advertencia sobre el orden).
4. **Reacciona** a los eventos en `on(Event)` enviando comandos a los actuadores.
5. **Instancia** el dispositivo en `setup()`; `loop()` solo cede tiempo con `vTaskDelay`.

!!! danger "Discrepancia detectada: orden de registro y arranque"
    En `src/Device.cpp`, `initializeAsynchronousEngine()` marca `engineStarted = true` y todos
    los métodos de registro (`appendSensorToScheduler`, `appendSensorToTriggeringScheduler`,
    `appendToBothSchedulers`) devuelven `false` (o un manejador `0`) si el motor ya arrancó.
    **Registra siempre antes de llamar a `initializeAsynchronousEngine()`.**

    Con el orden inverso, una simulación en Wokwi de `StatusIndicator` imprimía
    `Blinker registration failed` y el LED no parpadeaba. Todos los ejemplos de
    `examples/` están ya corregidos para registrar antes de arrancar. Ver
    [Limitaciones](limitations.md#discrepancias-detectadas).

El sketch se reproduce tal como está en el repositorio:

```cpp title="examples/StatusIndicator/StatusIndicator.ino" linenums="1"
--8<-- "examples/StatusIndicator/StatusIndicator.ino"
```

### Compilar y cargar

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 examples/StatusIndicator
arduino-cli upload  --fqbn esp32:esp32:esp32 --port <PUERTO> examples/StatusIndicator
arduino-cli monitor --port <PUERTO> --config baudrate=115200
```

`<PUERTO>` depende de tu sistema. El sketch usa el pin 2 (`SYSTEM_DIAGNOSTIC_LED_PIN`),
que su comentario describe como LED integrado habitual de placas ESP32; comprueba el pin
del LED de tu placa.

| | |
|---|---|
| **Salida esperada** | Mensajes con el prefijo `[StatusIndicator]` cada 500 ms y el LED conmutando. Se deduce del código. |
| **Salida observada** | No registrada: el sketch no se ejecutó en hardware para esta documentación. |

## Siguientes pasos

- Entiende el modelo de ejecución en [Arquitectura](architecture.md).
- Explora los [componentes](components/index.md) y sus escenarios.
- Busca un [ejemplo](examples/index.md) parecido a tu dispositivo.
- Si algo falla, consulta [Resolución de problemas](troubleshooting-es.md).

## Limitaciones conocidas

- Solo ESP32: el código usa FreeRTOS, temporizadores y APIs de Arduino-ESP32.
- La CI compila los ejemplos para `esp32` y `esp32s3`; otras variantes no se compilan en CI.
- Más detalles en [Limitaciones y compatibilidad](limitations.md).

---
title: Inicio
---

# Modest-IoT Nano

**Modest IoT Nano Framework (C++ Edition)** es una biblioteca Arduino para ESP32 que ofrece
clases reutilizables para construir dispositivos IoT a partir de sensores, actuadores,
transportes de red y clientes de telemetría, coordinados por una clase mediadora `Device`.

La versión documentada es la que declara `library.properties` en este repositorio
(**1.1.2**). Todo lo que aparece en este sitio se ha extraído del código o se indica
expresamente cuando no está verificado.

<div class="grid cards" markdown>

-   **Comenzar**

    ---

    Requisitos, instalación y primer sketch real.

    [Primeros pasos](getting-started.md)

-   **Componentes**

    ---

    Catálogo de clases con su API, eventos, comandos, pruebas y ejemplos relacionados.

    [Ver catálogo](components/index.md)

-   **Ejemplos**

    ---

    Los sketches de `examples/`, con su código completo sincronizado con el repositorio.

    [Ver ejemplos](examples/index.md)

-   **API**

    ---

    Referencia generada por Doxygen: clases, miembros, archivos y código fuente enlazado.

    [Referencia de API](api.md)

</div>

## Capacidades verificadas en el código

Cada punto enlaza con la ficha del componente que lo implementa.

| Capacidad | Dónde está |
|---|---|
| Mediador `Device` con dos tareas FreeRTOS (muestreo y red) y temporizador hardware | [`Device`](components/Device.md) |
| Planificador de muestreo con periodo independiente por ranura (máximo 8 ranuras) | [`SamplingScheduler`](components/SamplingScheduler.md) |
| Sensores por interrupción GPIO (máximo 4 por dispositivo) | [`Device`](components/Device.md) |
| Comandos encolados desde otra tarea (`enqueueCommand`) | [`Device`](components/Device.md) |
| Cola de telemetría que descarta el paquete más antiguo cuando se llena | [`Device`](components/Device.md) |
| Serialización JSON con metadatos de sobre (marca de tiempo, secuencia, versiones) | [`TelemetryPackage`](components/TelemetryPackage.md) |
| Detección de fallos de sensor con umbral configurable (eventos 22 y 23) | [`Sensor`](components/Sensor.md) |
| Instantánea de diagnóstico (pilas, heap, cola, envíos, tiempo activo) | [`DiagnosticsSnapshot`](components/DiagnosticsSnapshot.md) |
| Modos de energía `Periodic`, `Tickless` y `DeepSleep` | [`Device`](components/Device.md) |
| Transporte Wi-Fi y BLE (Nordic UART Service) | [Conectividad](components/index.md#connectivity) |
| Envío de telemetría por MQTT, HTTP y BLE | [Gateways](components/index.md#gateways) |
| Configuración persistente (NVS), hora NTP, batería por ADC, búfer en LittleFS y OTA | [Servicios](components/index.md#services) |
| Registro con filtrado de nivel en compilación | [`ModestLog`](components/ModestLog.md) |

El framework apunta a **ESP32** (`architectures=esp32` en `library.properties`). Consulta
[Limitaciones y compatibilidad](limitations.md) antes de elegir placa.

## Ejemplo introductorio

El sketch más sencillo del repositorio, [`StatusIndicator`](examples/StatusIndicator.md),
deriva de `Device`, registra un evento periódico en el planificador y responde
conmutando un [`Led`](components/Led.md):

```cpp
// Fragmento de examples/StatusIndicator/StatusIndicator.ino (ver archivo completo abajo)
class StatusIndicator : public Device {
    Led statusLed;
public:
    static constexpr int TIMER_EVENT_IDENTIFIER = 300;

    StatusIndicator(int ledPin, unsigned long blinkIntervalInMilliseconds, uint8_t timerChannel = 0)
    : Device(blinkIntervalInMilliseconds, timerChannel), statusLed(ledPin, STATUS_LED_INITIAL_STATE, this) {
        appendSensorToScheduler(this, StatusIndicator::TIMER_EVENT_IDENTIFIER, blinkIntervalInMilliseconds,
                                NO_MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS);
        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH); // registrar siempre antes de arrancar
    }

    void on(Event event) override {
        if (event.identifier == StatusIndicator::TIMER_EVENT_IDENTIFIER) {
            statusLed.handle(Led::TOGGLE_LED_COMMAND);
        }
    }
};
```

??? example "Archivo completo: examples/StatusIndicator/StatusIndicator.ino"

    ```cpp
    --8<-- "examples/StatusIndicator/StatusIndicator.ino"
    ```

!!! note "Qué está verificado y qué no"
    El fragmento resume el archivo real (se han omitido comentarios, mensajes por `Serial`
    y la comprobación del valor de retorno de `appendSensorToScheduler`). El archivo completo se inserta desde el repositorio en cada
    construcción. La compilación para ESP32 está configurada en CI, pero **no** se ejecutó
    ni se probó en hardware al generar esta documentación.

## Proyecto

- Repositorio: [modest-iot/modest-iot-nano-framework](https://github.com/modest-iot/modest-iot-nano-framework)
- Licencia: Apache 2.0 ([LICENSE.md](../LICENSE.md), [NOTICE](../NOTICE))
- Cambios por versión: [CHANGELOG.md](../CHANGELOG.md)

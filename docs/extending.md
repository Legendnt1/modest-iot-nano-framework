# Extender el framework

Esta guía describe los puntos de extensión **que existen en el código**. No hay interfaces
adicionales del tipo `ISensor`, ni métodos `begin()`/`update()` comunes: cada contrato es
una clase base abstracta concreta.

## Puntos de extensión

| Quieres crear… | Deriva de | Debes implementar (virtuales puras) | Opcional |
|---|---|---|---|
| Un dispositivo | [`Device`](components/Device.md) | — (constructor protegido) | `on(Event)`, `handle(Command)`, `processQueuedTelemetryData()`, `maintainNetworkServices()` |
| Un sensor | [`Sensor`](components/Sensor.md) | `processEvent(Event&)`, `getDefaultSamplingIntervalInMilliseconds()`, `getMinimalSamplingIntervalInMilliseconds()` | — |
| Un actuador | [`Actuator`](components/Actuator.md) | `executeCommand(Command)` | — (`handle()` es `final`) |
| Un paquete de telemetría | [`TelemetryPackage`](components/TelemetryPackage.md) | `serialize(JsonDocument&) const` | — |
| Un gateway | [`GatewayClient`](components/GatewayClient.md) | `sendTelemetryRecord(const TelemetryPackage&)` | `service()`, `isConnected()`, `getStatistics()` |
| Un transporte | [`ConnectivityDriver`](components/ConnectivityDriver.md) | `connect()`, `isConnected()`, `transmit(target, data)`, `canTransmit()` | — |

## Patrón de un sensor

Los sensores existentes siguen el mismo patrón; [`SoilMoistureSensor`](components/SoilMoistureSensor.md)
es uno de los más simples:

1. El constructor llama a `Sensor(pin, parentHandler)` y configura el pin.
2. `processEvent()` actúa cuando recibe `MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER` (21),
   que envía el planificador.
3. Tras leer, llama a `reportReadSuccess(event)` o `reportReadFailure(event)` para alimentar
   la detección de fallos.
4. Si hay un dato nuevo, reemplaza el evento (p. ej. `Event(DATA_READ_EVENT_IDENTIFIER, pin)`);
   si no, pone `event.identifier = -1` para que `Sensor::on()` no lo propague.
5. Devuelve sus intervalos por defecto y mínimo desde constantes privadas
   `DEFAULT_SAMPLING_INTERVAL_IN_MILLISECONDS` y `MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS`.

```cpp title="src/SoilMoistureSensor.cpp (fragmento real)"
void SoilMoistureSensor::processEvent(Event& event) {
    if (event.identifier == MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER) {
        cachedMoistureLevel = analogRead(pin);

        if (std::fabs(static_cast<float>(cachedMoistureLevel) - static_cast<float>(lastNotifiedMoistureLevel)) > MOISTURE_CHANGE_EPSILON) {
            lastNotifiedMoistureLevel = cachedMoistureLevel;
            reportReadSuccess(event);
            event = Event(DATA_READ_EVENT_IDENTIFIER, pin);
            return;
        }

        reportReadSuccess(event);
        event.identifier = PROPAGATION_SUPPRESSION_IDENTIFIER;
    }
}
```

!!! warning "Eventos de recuperación"
    `reportReadSuccess()` puede cambiar el evento a `SENSOR_RECOVERED_EVENT_IDENTIFIER` (23).
    Si después lo sobrescribes con otro identificador, el evento de recuperación no se
    propaga. Decide de forma explícita qué evento debe llegar al dispositivo.

### Esqueleto ilustrativo

!!! note "Código ilustrativo, no compilado"
    Usa solo API real del framework, pero no forma parte del repositorio ni se ha compilado.
    `AnalogThresholdSensor` es un nombre de ejemplo.

```cpp
#include <ModestIoT.h>

class AnalogThresholdSensor : public Sensor {
public:
    // El README reserva los identificadores 1000+ para la aplicación.
    static constexpr int THRESHOLD_EXCEEDED_EVENT_IDENTIFIER = 1001;

    AnalogThresholdSensor(int analogPin, int threshold, EventHandler* parentHandler = nullptr)
        : Sensor(analogPin, parentHandler), threshold(threshold) {
        pinMode(pin, INPUT);
    }

protected:
    void processEvent(Event& event) override {
        if (event.identifier != MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER) {
            return;
        }
        const int value = analogRead(pin);
        reportReadSuccess(event);
        if (event.identifier == SENSOR_RECOVERED_EVENT_IDENTIFIER) {
            return; // deja pasar la recuperación
        }
        if (value > threshold) {
            event = Event(THRESHOLD_EXCEEDED_EVENT_IDENTIFIER, pin);
        } else {
            event.identifier = -1; // no propagar
        }
    }

    uint32_t getDefaultSamplingIntervalInMilliseconds() const override { return 1000; }
    uint32_t getMinimalSamplingIntervalInMilliseconds() const override { return 100; }

private:
    int threshold;
};
```

Se registra desde el constructor del dispositivo con
`appendSensorToScheduler(&sensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER)`.

## Patrón de un actuador

[`Led`](components/Led.md) muestra el patrón completo: constantes `*_COMMAND_IDENTIFIER`,
objetos `Command` precreados y un `executeCommand()` que compara y actúa.

```cpp title="src/Led.cpp (fragmento real)"
void Led::executeCommand(Command command) {
    if (command == TURN_ON_COMMAND) {
        active = true;
    }
    else if (command == TURN_OFF_COMMAND) {
        active = false;
    }
    else if (command == TOGGLE_LED_COMMAND) {
        active = !active;
    }
    digitalWrite(pin, active ? HIGH : LOW);
}
```

Para comandos con parámetro, `Command` tiene un `payload` de tipo `int32_t`
(ver `ServoMotor::DRIVE_TO_ANGLE_COMMAND_IDENTIFIER` en el [catálogo](components/ServoMotor.md)).

## Errores y valores inválidos

No existe un tipo de error común. Los mecanismos presentes son:

- **Sensores:** `reportReadFailure()` y el umbral de `setFaultThreshold()`; el evento con
  identificador `-1` suprime la propagación.
- **Registro en el planificador:** `appendSensorToScheduler()` devuelve `bool`
  (`false` si no hay capacidad, por ejemplo).
- **Telemetría y comandos:** `enqueueTelemetryPayload()` y `enqueueCommand()` devuelven `bool`.
- **Gateways:** `sendTelemetryRecord()` devuelve `bool`; `getStatistics()` cuenta envíos.

Consulta la ficha de cada clase para su comportamiento concreto.

## Si contribuyes un componente al framework

Además del código en `src/`:

1. **Comentarios Doxygen** en la cabecera: `@file`, `@brief`, `@ingroup` del módulo,
   `@param`, `@return` y `@runs_on{sampler|network}` (normas de `CONTRIBUTING.md`).
2. **Cabecera paraguas:** añade el `#include` a `src/ModestIoT.h`.
3. **Palabras clave:** añade la clase y sus constantes a `keywords.txt`.
4. **Pruebas host:** crea `tests/<Clase>/test.cpp` y regístralo en `tests/run_all.sh`
   (y, si usas CLion, en `CMakeLists.txt`). El catálogo detecta la suite por el nombre de la carpeta.
5. **Ejemplo:** si aporta valor, un sketch en `examples/<Nombre>/<Nombre>.ino` con la
   cabecera Doxygen que exige `CONTRIBUTING.md`. Sigue el estilo de `StatusIndicator`: sin
   números ni cadenas mágicas. Pines, intervalos, profundidad de cola, estados iniciales,
   versión de firmware, identificadores MQTT, coordenadas de pantalla y mensajes van en
   constantes `static const` documentadas y agrupadas con `@name` al inicio del sketch. Los
   formatos de `MODEST_LOG_*`/`MIOT_LOG_*` deben ser `#define`: en ESP32 esas macros
   concatenan literales en compilación y no aceptan una variable `const char*`.
   Registra los sensores **antes** de llamar a `initializeAsynchronousEngine()`.
6. **Catálogo:** si la clase no deriva de `Sensor`, `Actuator`, `ConnectivityDriver` ni
   `GatewayClient`, asígnale una categoría en `docs/components/catalog.yml`. La construcción
   estricta falla si una clase queda sin categoría.
7. **Identificadores:** elige valores que no colisionen; el
   [registro de identificadores](architecture.md#registro-de-identificadores) se genera solo.
8. **Changelog:** documenta el cambio en `CHANGELOG.md`.

La ficha del catálogo, la página del ejemplo y la referencia de API se generan
automáticamente: no hay que escribirlas a mano.

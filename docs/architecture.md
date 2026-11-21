# Arquitectura

Esta página describe el sistema **tal como está implementado** en `src/`. Las afirmaciones
remiten a clases concretas; los diagramas se generan en cada construcción del sitio.

## Organización del código

- `src/` es plano: una pareja `.h`/`.cpp` por clase (algunas clases son solo cabecera,
  como `TelemetryPackage`, `GatewayClient` o `ConnectivityDriver`).
- `src/ModestIoT.h` es la cabecera paraguas que incluye la API pública.
- `examples/` contiene un sketch por carpeta; `tests/` contiene una suite host por carpeta
  y los dobles de Arduino/ESP32 en `tests/fakes/`.
- No hay espacios de nombres públicos para las clases: los nombres son globales.

## Modelo: eventos hacia arriba, comandos hacia abajo

El diseño separa notificaciones (`Event`) de órdenes (`Command`):

- Un [`Sensor`](components/Sensor.md) es un [`EventHandler`](components/EventHandler.md).
  Su método `on(Event)` llama a `processEvent(Event&)` (implementado por la subclase) y,
  salvo que el identificador resultante sea `-1`, reenvía el evento a su manejador padre.
- Un [`Actuator`](components/Actuator.md) es un [`CommandHandler`](components/CommandHandler.md).
  Su `handle(Command)` es `final`: ejecuta `executeCommand(Command)` y después propaga el
  comando al manejador padre, si existe.
- [`Device`](components/Device.md) implementa ambas interfaces. La aplicación deriva de
  `Device`, declara sensores y actuadores como miembros y les pasa `this` como padre: así
  `Device` actúa como **mediador**.

<!-- DIAGRAMA:architecture-overview -->

*Descripción textual:* el sketch crea la subclase de `Device`. `Device` registra sensores en su
`SamplingScheduler`, que les envía el evento 21 (`MEASURE_DATA_REQUESTED`). Los sensores
reenvían eventos al `Device`, que envía comandos a los actuadores; estos propagan el comando
de vuelta al padre. Para telemetría, el `Device` crea un `TelemetryPackage` y lo encola; la
tarea de red lo entrega a `processQueuedTelemetryData()`, donde la subclase usa un
`GatewayClient`, que a su vez usa un `ConnectivityDriver`.

## Modelo de ejecución

`Device::initializeAsynchronousEngine()` crea dos colas FreeRTOS (telemetría y comandos), dos
tareas fijadas a núcleo con `xTaskCreatePinnedToCore` y un temporizador hardware a 1 MHz.

<!-- DIAGRAMA:engine-flow -->

*Descripción textual:* la ISR del temporizador, la ISR GPIO y `enqueueCommand()` despiertan a la
tarea de muestreo mediante bits de notificación. Esa tarea avanza el planificador, procesa
los sensores disparados por interrupción y ejecuta los comandos pendientes; los sensores
notifican al `Device`, que actúa sobre los actuadores y, si la aplicación lo hace, encola
telemetría. La tarea de red extrae paquetes (esperando hasta 100 ms), llama a
`processQueuedTelemetryData()`, libera el paquete y ejecuta `maintainNetworkServices()`.

| Contexto | Qué se ejecuta allí (según el código) |
|---|---|
| ISR del temporizador | `handleHardwareTimerInterrupt`: incrementa un contador atómico y notifica a la tarea de muestreo |
| ISR GPIO | `onSensorTrigger`: marca la entrada de la tabla de disparo y notifica a la tarea de muestreo |
| Tarea de muestreo `HardwareWorker` | `SamplingScheduler::advance`, `Sensor::on`, `Device::on` de la aplicación, `Actuator::handle`, `processPendingCommands`, encolado de telemetría |
| Tarea de red `NetworkWorker` | `processQueuedTelemetryData`, borrado del paquete, `maintainNetworkServices` |
| `setup()` / `loop()` de Arduino | Construcción del dispositivo y arranque del motor; los ejemplos dejan `loop()` cediendo con `vTaskDelay` |

Muchos métodos indican su contexto con la etiqueta `@runs_on` en el código; las fichas del
catálogo la muestran en la columna *Contexto documentado*.

!!! warning "Concurrencia"
    El README afirma que las dos tareas solo comparten la cola y que no se necesitan cerrojos
    en el núcleo. Esta documentación **no** ha verificado formalmente la seguridad entre
    tareas: no llames a sensores, actuadores ni `Wire` desde `loop()` u otras tareas; usa
    `enqueueCommand()` para enviar comandos desde la tarea de red.

### Configuración del motor

[`EngineConfiguration`](components/EngineConfiguration.md) permite ajustar las tareas. Valores
por defecto declarados en `src/Device.h`:

| Campo | Defecto |
|---|---|
| `samplerStackBytes` | 4096 |
| `networkStackBytes` | 8192 |
| `samplerPriority` | 3 |
| `networkPriority` | 1 |
| `samplerCore` / `networkCore` | 1 / 1 |
| `queueLength` | 10 |

`stopAsynchronousEngine()` detiene ambas tareas, vacía la cola y libera el temporizador.

### Planificación

- [`SamplingScheduler`](components/SamplingScheduler.md) admite hasta **8** ranuras
  (`MAXIMUM_SLOTS`). Cada ranura tiene su periodo; si una ranura vence varias veces entre
  dos avances se despacha **una sola vez** (no se reproducen muestras perdidas).
- Cada sensor declara un intervalo por defecto y uno mínimo; `appendSensorToScheduler`
  aplica el mínimo.
- Los sensores por interrupción se registran con `appendSensorToTriggeringScheduler`
  (máximo **4**, `MAX_TRIGGERED_SENSORS`).

### Modos de energía

`Device::PowerMode` define `Periodic` (alarma periódica, por defecto), `Tickless` (alarma de
un disparo re-armada y *light sleep*) y `DeepSleep` (apagado de CPU con despertar por
temporizador). Se selecciona con `setPowerMode()`.

## Telemetría

1. La aplicación define una subclase de [`TelemetryPackage`](components/TelemetryPackage.md)
   que implementa `serialize(JsonDocument&)`.
2. `enqueueTelemetryPayload()` añade metadatos de sobre (`timestamp`, `sequence` y,
   si se configuraron, `schemaVersion` y `firmwareVersion`) y encola el puntero.
   Si la cola está llena, descarta el paquete **más antiguo** y reintenta.
3. La tarea de red entrega el paquete a `processQueuedTelemetryData()` y lo libera después.
4. La subclase decide cómo enviarlo, normalmente con un
   [`GatewayClient`](components/GatewayClient.md).

La marca de tiempo procede de `TimeSynchronizationDriver::getCurrentTimestamp()` cuando se
compila para ESP32 (`ARDUINO_ARCH_ESP32`) y de `millis()` en otros casos, como las pruebas host.

## Salud de sensores y diagnóstico

- `Sensor::setFaultThreshold(n)` activa la detección: tras `n` fallos consecutivos el
  sensor emite `SENSOR_FAULT_EVENT_IDENTIFIER` (22) una vez, y al recuperarse
  `SENSOR_RECOVERED_EVENT_IDENTIFIER` (23). Con `0` (valor inicial) está desactivada.
- `Device::diagnostics()` devuelve un [`DiagnosticsSnapshot`](components/DiagnosticsSnapshot.md).

## Registro de identificadores

Tabla generada a partir de las constantes `*_IDENTIFIER` públicas de cada clase. Eventos y
comandos son espacios distintos: un mismo número puede aparecer en ambos.

<!-- IDENTIFICADORES -->

## Jerarquía de clases

Generada desde las relaciones de herencia del XML de Doxygen. Cada nodo enlaza a su ficha.

<!-- DIAGRAMA:class-hierarchy -->

*Descripción textual:* las flechas apuntan de la clase derivada a su base. Los sensores
derivan de `Sensor` (y `PthSensor` y `EnvironmentalGasSensor` de `BmeEnvironmentalSensor`),
los actuadores de `Actuator`, `Sensor` de `EventHandler`, `Actuator` de `CommandHandler` y
`Device` de ambas interfaces; los transportes derivan de `ConnectivityDriver` y los
gateways de `GatewayClient`.

Las decisiones de diseño y sus motivos están en
[Decisiones de arquitectura (inglés)](adrs.md).

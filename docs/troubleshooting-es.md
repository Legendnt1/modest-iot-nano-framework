# Resolución de problemas

Problemas cuyo origen se puede rastrear en el código. Para cuestiones de IDE, placa,
carga del firmware, Wi-Fi, BLE o MQTT, consulta también la
[guía original en inglés](troubleshooting.md).

## Un sensor no produce eventos

1. **Comprueba el valor de retorno del registro.** `appendSensorToScheduler()` devuelve
   `false` y `appendSensorToTriggeringScheduler()` devuelve `0` si:
    - el motor ya arrancó (`initializeAsynchronousEngine()` se llamó antes; ver la
      [discrepancia n.º 1](limitations.md#discrepancias-detectadas));
    - el puntero del sensor es nulo o el identificador de evento es `-1`;
    - no queda capacidad: 8 ranuras en el planificador, 4 sensores por interrupción.
2. **Comprueba que el sensor tiene padre.** El último argumento del constructor del sensor
   debe ser el `Device` (`this`), o llama después a `setHandler()`.
3. **Comprueba la supresión.** Muchos sensores solo propagan cuando el valor cambia; si no,
   ponen el identificador a `-1` y `Sensor::on()` descarta el evento. Revisa la ficha del
   sensor en el [catálogo](components/index.md).

## El periodo de muestreo no es el configurado

`appendSensorToScheduler()` aplica el intervalo mínimo del sensor (o el del temporizador del
`Device`) y emite un aviso `MODEST_LOG_W` «Sensor period clamped…». Los valores por defecto y
mínimos de cada sensor aparecen en su ficha (*Muestreo*).

## No veo mensajes de registro

- El nivel por defecto es `MODEST_LOG_LEVEL_WARN` (2): los mensajes `MODEST_LOG_I` y
  `MODEST_LOG_D` se eliminan en compilación. Define `MODEST_LOG_LEVEL` antes de incluir el
  framework o en los *build flags*.
- En ESP32 (sin `MODEST_LOG_CUSTOM_SINK`) los mensajes se envían a `log_e`/`log_w`/… de
  `esp32-hal-log.h`, que también dependen del nivel de depuración del core configurado en el IDE.
- Detalles en [`ModestLog`](components/ModestLog.md).

## Se pierden paquetes de telemetría

Si la cola está llena, `enqueueTelemetryPayload()` descarta el paquete más antiguo. Consulta
`packetsDropped`, `packetsDroppedOldest` y `queueHighWaterMark` en `Device::diagnostics()`
([`DiagnosticsSnapshot`](components/DiagnosticsSnapshot.md)) y aumenta `queueLength` en
[`EngineConfiguration`](components/EngineConfiguration.md) si es necesario.

## Desbordamiento de pila

`DiagnosticsSnapshot` informa del margen libre de cada tarea en palabras (`samplerStackFreeWords`,
`networkStackFreeWords`). Ajusta `samplerStackBytes` o `networkStackBytes` en `EngineConfiguration`.

## Problemas al generar la documentación

| Síntoma | Causa | Solución |
|---|---|---|
| `Doxygen no encontrado` | Falta Doxygen | Instálalo o define la variable `DOXYGEN` con la ruta al ejecutable |
| `Graphviz 'dot' no está disponible` | Falta Graphviz | Instálalo o define `GRAPHVIZ_DOT` |
| `No se encontró el XML de Doxygen` al usar `mkdocs` directamente | MkDocs se ejecutó sin generar antes la API | Ejecuta `python tools/docs/build_docs.py` (o `--skip-mkdocs` y después `mkdocs serve`) |
| `Componente sin categoría…` | Clase nueva no catalogada | Añádela en `docs/components/catalog.yml` |
| Más avisos de Doxygen que la línea base | Comentarios nuevos con errores | Corrige los avisos de `build/docs/doxygen-warnings.log` |
| Enlaces rotos | Página o ancla inexistente | Revisa el informe del paso de validación |

Más información en [Generar y validar la documentación](development.md).

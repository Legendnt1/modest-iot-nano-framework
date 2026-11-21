# Catálogo de componentes

Un recorrido por cada clase pública del framework, al estilo de un catálogo de componentes:
qué es, cómo se crea, qué eventos emite o qué comandos acepta, qué escenarios están
cubiertos por pruebas y en qué ejemplos aparece.

!!! info "De dónde sale cada dato"
    Las fichas se **generan en cada construcción** del sitio a partir de:

    - el XML de Doxygen extraído de `src/` (firmas, herencia, comentarios, constantes);
    - `examples/` (qué sketches usan cada clase y en qué líneas);
    - `tests/` y `tests/run_all.sh` (suites host y descripciones de sus casos).

    Las descripciones procedentes del código están en inglés. Ninguna ficha registra
    resultados de ejecución: el navegador no ejecuta firmware ni controla hardware.

**Cómo leer el estado de validación**

| Término | Significado |
|---|---|
| Verificado en código | La declaración existe y Doxygen la extrajo al construir el sitio. |
| Prueba host | Hay una suite en `tests/` que compila la clase con dobles (`tests/fakes`). La CI la ejecuta en *Host Unit Tests*. |
| Compilación para ESP32 | Algún ejemplo usa la clase y la CI (*Compile Examples*) lo compila para `esp32` y `esp32s3`. |
| Requiere hardware | El comportamiento físico solo puede observarse con el periférico real. No se ha validado aquí. |

<!-- CATALOGO -->

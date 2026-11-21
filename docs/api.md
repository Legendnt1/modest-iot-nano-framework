# Referencia de API

La referencia completa la genera **Doxygen** desde los comentarios de `src/` y se publica
dentro de este mismo sitio, en `api/reference/`.

[Abrir la referencia de API](api/reference/index.html){ .md-button .md-button--primary }
[Lista de clases](api/reference/annotated.html){ .md-button }
[Archivos](api/reference/files.html){ .md-button }
[Módulos](api/reference/topics.html){ .md-button }
[Ejemplos con enlaces](api/reference/examples.html){ .md-button }

## Qué contiene

| Sección | Contenido |
|---|---|
| Clases y estructuras | Miembros públicos y protegidos, herencia, diagramas de clases y colaboración |
| Archivos | Cada cabecera y fuente de `src/`, con el código navegable |
| Módulos | Agrupación definida en `docs/doxygen-groups.dox` (núcleo, sensores, actuadores, conectividad, gateway) |
| Ejemplos | Los sketches de `examples/` con los símbolos del framework enlazados a su documentación |
| Páginas | README, ADR, historias de usuario, mejoras y licencia, tal como los incluye `docs/Doxyfile` |

## Búsqueda

!!! note "Dos buscadores"
    El buscador de la cabecera de este sitio indexa las páginas editoriales, el catálogo y
    los ejemplos. **No** indexa la referencia de Doxygen. Para buscar símbolos usa el
    buscador propio de la referencia (cuadro *Search* en la esquina superior derecha de
    cualquier página de `api/reference/`). Ambos funcionan sin servidor.

## Cómo se relaciona con el catálogo

Cada ficha del [catálogo](components/index.md) enlaza sus métodos y constantes con el ancla
correspondiente de la referencia, y cada página de la referencia ofrece un enlace de vuelta
a este sitio.

## Convenciones de los comentarios

- `@brief`, `@param`, `@return` describen el comportamiento público estable.
- `@runs_on{contexto}` (alias definido en `docs/Doxyfile`) indica la tarea FreeRTOS en la
  que se ejecuta un método; aparece como «Runs on: …».
- Las cabeceras son la fuente de verdad de los contratos públicos.

## Limitaciones de la extracción

- Los sketches `.ino` se tratan como C++ (`EXTENSION_MAPPING = ino=C++`) y se publican como
  *ejemplos* de Doxygen. Doxygen no ejecuta el preprocesador de Arduino (no genera
  prototipos ni añade `#include <Arduino.h>`), así que su análisis no equivale a compilar.
- Los miembros privados no se documentan (`EXTRACT_PRIVATE = NO`).
- `EXTRACT_ALL = YES` publica también símbolos sin comentario; por eso Doxygen no avisa de
  todo lo que falta por documentar. La línea base de avisos se describe en
  [Generar y validar la documentación](development.md#avisos-de-doxygen).

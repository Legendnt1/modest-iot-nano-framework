# Generar y validar la documentación

## Arquitectura del sitio

| Pieza | Herramienta | Papel |
|---|---|---|
| Referencia de API | **Doxygen** con `docs/Doxyfile` | Extrae clases, miembros y comentarios de `src/`; publica los `.ino` como ejemplos enlazados; genera XML |
| Sitio editorial | **MkDocs** + **Material for MkDocs** (`mkdocs.yml`) | Navegación, búsqueda, guías en `docs/`, tema adaptable |
| Catálogo y ejemplos | Hooks propios (`tools/docs/`) | Generan en memoria las fichas desde el XML de Doxygen, `examples/` y `tests/` |
| Diagramas | **Graphviz** | Renderiza `docs/diagrams/*.dot` y la jerarquía de clases a SVG incrustado; Doxygen lo usa para sus grafos |
| Orquestación | `tools/docs/build_docs.py` | Comprueba dependencias, ejecuta todo, integra la API en `site/api/reference/` y valida |

Se eligió esta combinación porque el repositorio ya tenía una configuración de Doxygen
mantenida y comentarios Doxygen en todo `src/`; MkDocs Material aporta la navegación y la
búsqueda sin JavaScript propio. Se descartaron Compodoc y Storybook (no analizan C++),
Sphinx + Breathe/Exhale (más piezas para el mismo resultado) y un frontend a medida.

### Flujo

```text
src/ + docs/Doxyfile ──Doxygen──► build/docs/doxygen/{html,xml}
                                          │
docs/*.md + mkdocs.yml ──MkDocs + hooks───┤ (XML → fichas; examples/ y tests/ → páginas)
                                          ▼
                                       site/   ← sitio estático (index.html en la raíz)
                                       site/api/reference/  ← HTML de Doxygen
```

## Dependencias

| Herramienta | Versión fijada | Dónde |
|---|---|---|
| Python | ≥ 3.11 (requisito de Material) | — |
| MkDocs | 1.6.1 | `tools/docs/requirements.txt` |
| Material for MkDocs | 9.8.0 | `tools/docs/requirements.txt` |
| Doxygen | 1.18.0 (usada en CI y para generar este sitio) | `.github/workflows/docs.yml` |
| Graphviz | La de la distribución en CI (`apt`); 16.1.0 en la generación local | `.github/workflows/docs.yml` |

!!! warning "Mantenimiento de Material for MkDocs"
    Material for MkDocs 9.8 anuncia en su salida de construcción que llega al final de su vida
    el **5 de mayo de 2027** (correcciones críticas hasta esa fecha) y propone Zensical como
    sucesor compatible con `mkdocs.yml`. Antes de esa fecha conviene evaluar la migración;
    los hooks de `tools/docs/` usan la API de hooks de MkDocs 1.6 y deberán revisarse.
    El aviso de la consola se puede ocultar con `NO_MKDOCS_2_WARNING=1`.

Ninguna de estas herramientas forma parte de la biblioteca: `tools/` y `mkdocs.yml` no están
en `export.include` de `library.json`, y `site/` y `build/` están en `.gitignore`.

## Comandos

```sh
# 1. Instalar las dependencias Python (mejor en un entorno virtual)
python -m venv .venv-docs
. .venv-docs/bin/activate            # Windows: .venv-docs\Scripts\activate
python -m pip install -r tools/docs/requirements.txt

# 2. Construir todo (Doxygen + MkDocs estricto + validación)
python tools/docs/build_docs.py
#    Opcional: crea build/docs/modest-iot-nano-docs-site.zip con index.html en la raíz
python tools/docs/build_docs.py --zip

# 3. Previsualizar el sitio construido, servido bajo un subdirectorio como en GitHub Pages
python tools/docs/build_docs.py serve          # http://127.0.0.1:8000/modest-iot-nano-framework/

# 4. Edición con recarga en vivo (tras generar la API una vez)
python tools/docs/build_docs.py --skip-mkdocs
mkdocs serve

# 5. Revalidar site/ sin reconstruir
python tools/docs/build_docs.py check

# 6. Limpiar salidas generadas (solo site/ y build/docs/)
python tools/docs/build_docs.py clean
```

Si Doxygen o Graphviz no están en el `PATH`, indica su ruta con las variables `DOXYGEN` y
`GRAPHVIZ_DOT`. Para otro alojamiento cambia la URL canónica con `DOCS_SITE_URL`.

## Qué valida el build

| Validación | Cómo |
|---|---|
| Configuración de Doxygen y extracción | Doxygen debe terminar sin error y producir HTML y XML |
| Avisos de Doxygen | No pueden superar la línea base de `tools/docs/doxygen-warnings-baseline.txt` |
| Configuración de MkDocs, navegación y enlaces | `mkdocs build --strict`: cualquier aviso (página inexistente, enlace roto, componente sin categoría, fragmento incluido inexistente) detiene la construcción |
| Enlaces internos del sitio final | Recorre todos los HTML (incluida la API) y comprueba cada `href`/`src` relativo |
| Estructura | Portada, secciones, API, índice de búsqueda y una página por ejemplo |
| Rutas locales | Busca rutas del equipo de desarrollo y `file://` en la salida |
| Servido HTTP bajo subdirectorio | Levanta un servidor local en `/modest-iot-nano-framework/` y pide páginas y recursos |

El resultado se guarda en `build/docs/validation-report.md` y `.json`; los registros en
`build/docs/doxygen-warnings.log` y `build/docs/mkdocs.log`.

### Avisos de Doxygen

La línea base actual son los avisos de referencias `\ref` que Doxygen no puede resolver en
`README.md` (enlaces a otros archivos Markdown del repositorio). Como `EXTRACT_ALL = YES`,
Doxygen no avisa de miembros sin documentar; activar `EXTRACT_ALL = NO` mostraría esos huecos
y es una mejora gradual recomendada, no aplicada para no cambiar la referencia publicada.

## Cómo documentar un componente

1. Comenta la clase en su cabecera con `@brief` (una frase), detalles, `@ingroup` y, en cada
   método público, `@param`, `@return`, `@runs_on{…}`, `@note`/`@warning` si procede.
   No dupliques la documentación en el `.cpp`.
2. Si la clase no hereda de una base catalogada, asígnala en `docs/components/catalog.yml`.
3. Crea la suite `tests/<Clase>/test.cpp`; los mensajes `printf` de cada caso aparecen como
   escenarios en la ficha.
4. Ejecuta `python tools/docs/build_docs.py` y revisa la ficha generada.

## Publicación

El workflow `.github/workflows/docs.yml` construye y valida el sitio en cada *pull request*
y *push*, y conserva el sitio y los registros como artefactos. El despliegue en GitHub Pages
solo se ejecuta en `push` a `main` o manualmente, y requiere activar antes **Settings ›
Pages › Source: GitHub Actions** en el repositorio. A fecha de esta documentación, GitHub
Pages no está habilitado para el repositorio y el sitio **no** está publicado.

Para otro alojamiento estático basta con subir el contenido de `site/` (o el ZIP). Los
enlaces son relativos, por lo que el sitio funciona en la raíz de un dominio o en un
subdirectorio sin reconstruirlo; solo la URL canónica (`site_url`) y el `sitemap.xml`
dependen de `DOCS_SITE_URL`.

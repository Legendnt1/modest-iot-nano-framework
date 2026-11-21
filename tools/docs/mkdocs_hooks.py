"""Hooks de MkDocs para el sitio de Modest-IoT Nano.

Responsabilidades:

* Cargar el modelo del catálogo (XML de Doxygen + examples/ + tests/) al iniciar.
* Ampliar la navegación con una entrada por componente y por ejemplo.
* Generar en memoria las fichas de componentes y las páginas de ejemplos.
* Publicar la referencia HTML de Doxygen bajo ``api/reference/`` y el logotipo del proyecto.
* Sustituir marcadores ``<!-- CATALOGO -->``, ``<!-- EJEMPLOS -->`` y ``<!-- DIAGRAMA:nombre -->``.
* Reescribir enlaces relativos que salen de ``docs/`` (README, src/, examples/...) hacia el
  repositorio, para que el sitio publicado no contenga enlaces rotos.

Requisito: haber generado antes el XML/HTML de Doxygen (``python tools/docs/build_docs.py``).
"""

from __future__ import annotations

import logging
import posixpath
import re
import sys
from pathlib import Path

from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File, Files

sys.path.insert(0, str(Path(__file__).resolve().parent))

import catalog  # noqa: E402
import pages  # noqa: E402

log = logging.getLogger("mkdocs.plugins.modest_docs")

COMPONENTS_INDEX = "components/index.md"
EXAMPLES_INDEX = "examples/index.md"
DIAGRAMS_DIR = catalog.REPO_ROOT / "docs" / "diagrams"
LOGO_SOURCE = catalog.REPO_ROOT / "assets" / "branding" / "logo.png"
LOGO_DEST = "assets/branding/logo.png"

#: Documentos preexistentes en inglés que se publican sin traducir.
LEGACY_PAGES = {"quick-start.md", "troubleshooting.md", "migration.md", "adrs.md"}

MARKDOWN_LINK_RE = re.compile(r"(\]\()([^)\s]+)(\))")
DIAGRAM_RE = re.compile(r"<!--\s*DIAGRAMA:([\w-]+)\s*-->")

_state: dict = {}


def _model() -> catalog.Model:
    return _state["model"]


def on_config(config):
    try:
        model = catalog.load_model()
    except FileNotFoundError as exc:
        raise PluginError(str(exc)) from exc
    _state["model"] = model
    _state["dot"] = catalog.find_dot()
    _state["svg_cache"] = {}
    for warning in model.warnings:
        log.warning(warning)

    nav = config.get("nav") or []
    config["nav"] = [_expand_nav_item(item, model) for item in nav]
    return config


def _expand_nav_item(item, model: catalog.Model):
    if not isinstance(item, dict) or len(item) != 1:
        return item
    (title, value), = item.items()
    if value == COMPONENTS_INDEX:
        children: list = [COMPONENTS_INDEX]
        for cat in model.categories:
            comps = model.components_in(cat.id)
            if comps:
                children.append({cat.title: [{c.title or c.name: c.page} for c in comps]})
        return {title: children}
    if value == EXAMPLES_INDEX:
        children = [EXAMPLES_INDEX]
        children += [{ex.name: ex.page} for ex in sorted(model.examples.values(), key=lambda e: e.name.lower())]
        return {title: children}
    if isinstance(value, list):
        return {title: [_expand_nav_item(child, model) for child in value]}
    return item


def on_files(files: Files, config) -> Files:
    model = _model()
    for comp in model.components.values():
        files.append(File.generated(config, comp.page, content=pages.render_component(model, comp)))
    for ex in model.examples.values():
        files.append(File.generated(config, ex.page, content=pages.render_example(model, ex)))

    if not catalog.HTML_DIR.is_dir():
        raise PluginError(f"Falta la salida HTML de Doxygen en {catalog.HTML_DIR}. "
                          "Ejecuta: python tools/docs/build_docs.py")
    for path in sorted(catalog.HTML_DIR.rglob("*")):
        if path.is_file():
            rel = path.relative_to(catalog.HTML_DIR).as_posix()
            files.append(File.generated(config, f"{catalog.API_PREFIX}/{rel}", abs_src_path=str(path)))

    if LOGO_SOURCE.exists():
        files.append(File.generated(config, LOGO_DEST, abs_src_path=str(LOGO_SOURCE)))
    return files


def on_page_markdown(markdown: str, page, config, files: Files) -> str:
    model = _model()
    src_uri = page.file.src_uri

    if src_uri == COMPONENTS_INDEX:
        markdown = markdown.replace("<!-- CATALOGO -->", pages.render_components_index(model, ""))
    elif src_uri == EXAMPLES_INDEX:
        markdown = markdown.replace("<!-- EJEMPLOS -->", pages.render_examples_index(model, ""))
    if "<!-- IDENTIFICADORES -->" in markdown:
        if posixpath.dirname(src_uri):
            raise PluginError("<!-- IDENTIFICADORES --> solo se admite en páginas de la raíz de docs/")
        markdown = markdown.replace("<!-- IDENTIFICADORES -->", pages.render_identifier_registry(model))

    markdown = DIAGRAM_RE.sub(lambda m: _diagram_html(m.group(1), page), markdown)
    markdown = _rewrite_external_links(markdown, src_uri, files)

    if src_uri in LEGACY_PAGES:
        notice = ('!!! info "Documento original en inglés"\n'
                  "    Este documento forma parte de la documentación histórica del repositorio y se "
                  "publica sin traducir ni revisar línea a línea para este sitio. Ante cualquier "
                  "discrepancia prevalecen el código y la [referencia de API](api.md).\n\n")
        markdown = _insert_after_title(markdown, notice)
    return markdown


def _insert_after_title(markdown: str, block: str) -> str:
    lines = markdown.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.startswith("# "):
            return "".join(lines[: i + 1]) + "\n" + block + "".join(lines[i + 1:])
    return block + markdown


def _rewrite_external_links(markdown: str, src_uri: str, files: Files) -> str:
    """Convierte enlaces relativos que no existen en el sitio en enlaces al repositorio."""
    page_dir = posixpath.dirname(src_uri)
    in_fence = False
    out_lines = []
    for line in markdown.splitlines(keepends=True):
        if line.lstrip().startswith(("```", "~~~")):
            in_fence = not in_fence
        if in_fence:
            out_lines.append(line)
            continue

        def replace(match: re.Match) -> str:
            target = match.group(2)
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith(("#", "/")):
                return match.group(0)
            path, _, fragment = target.partition("#")
            if not path:
                return match.group(0)
            resolved = posixpath.normpath(posixpath.join(page_dir, path))
            if not resolved.startswith("..") and files.get_file_from_path(resolved) is not None:
                return match.group(0)
            if not resolved.startswith(".."):
                # Ruta dentro de docs/ pero no publicada (archivo excluido): enlazar al repositorio.
                repo_rel = posixpath.join("docs", resolved)
            else:
                repo_rel = posixpath.normpath(posixpath.join("docs", page_dir, path))
            repo_rel = repo_rel.rstrip("/")
            if repo_rel.startswith("..") or not (catalog.REPO_ROOT / repo_rel).exists():
                return match.group(0)  # que MkDocs informe del enlace roto
            url = catalog.source_url(repo_rel) + (f"#{fragment}" if fragment else "")
            return f"{match.group(1)}{url}{match.group(3)}"

        out_lines.append(MARKDOWN_LINK_RE.sub(replace, line))
    return "".join(out_lines)


def _diagram_html(name: str, page) -> str:
    cache = _state["svg_cache"]
    if name not in cache:
        dot_exe = _state.get("dot")
        if not dot_exe:
            raise PluginError("Graphviz 'dot' no está disponible (define GRAPHVIZ_DOT o añade dot al PATH).")
        if name == "class-hierarchy":
            source = pages.class_hierarchy_dot(_model())
        else:
            dot_file = DIAGRAMS_DIR / f"{name}.dot"
            if not dot_file.exists():
                raise PluginError(f"Diagrama no encontrado: docs/diagrams/{name}.dot")
            source = dot_file.read_text(encoding="utf-8")
        cache[name] = catalog.render_dot(source, dot_exe)
    svg = cache[name]
    # Bloque HTML sin líneas vacías para que Markdown lo deje intacto.
    svg = "\n".join(line for line in svg.splitlines() if line.strip())
    return f'<div class="mi-diagram" role="figure">\n{svg}\n</div>'

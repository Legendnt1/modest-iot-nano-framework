#!/usr/bin/env python3
"""Construye, valida y previsualiza el sitio de documentación de Modest-IoT Nano.

Uso (desde cualquier directorio):

    python tools/docs/build_docs.py            # build completo: Doxygen + MkDocs + validación
    python tools/docs/build_docs.py check      # vuelve a validar site/ (enlaces, rutas, HTTP)
    python tools/docs/build_docs.py serve      # sirve site/ por HTTP en un subdirectorio
    python tools/docs/build_docs.py clean      # borra site/ y build/docs/

Herramientas externas: Doxygen (variable DOXYGEN o PATH) y Graphviz dot (GRAPHVIZ_DOT o PATH).
Dependencias Python: tools/docs/requirements.txt.

Códigos de salida: 0 éxito, 1 fallo de construcción o validación, 2 dependencias ausentes.
"""

from __future__ import annotations

import argparse
import functools
import http.server
import importlib.metadata
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from html.parser import HTMLParser
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parents[1]
BUILD_DIR = REPO_ROOT / "build" / "docs"
DOXYGEN_OUT = BUILD_DIR / "doxygen"
EXAMPLES_DOX = BUILD_DIR / "doxygen-examples.dox"
DOXYGEN_LOG = BUILD_DIR / "doxygen-warnings.log"
MKDOCS_LOG = BUILD_DIR / "mkdocs.log"
REPORT_JSON = BUILD_DIR / "validation-report.json"
REPORT_MD = BUILD_DIR / "validation-report.md"
SITE_DIR = REPO_ROOT / "site"
ZIP_PATH = BUILD_DIR / "modest-iot-nano-docs-site.zip"
BASELINE_FILE = TOOLS_DIR / "doxygen-warnings-baseline.txt"
REQUIREMENTS = TOOLS_DIR / "requirements.txt"
DEFAULT_BASE_PATH = "/modest-iot-nano-framework/"

BACKLINK_MARKER = "mi-backlink"
BACKLINK_HTML = (
    '<div class="mi-backlink" style="padding:6px 12px;background:#3f51b5;'
    'font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif">'
    '<a href="../../index.html" style="color:#fff">&larr; Modest-IoT Nano: documentación</a>'
    ' <span style="color:#fff">&middot;</span> '
    '<a href="../../components/index.html" style="color:#fff">Catálogo de componentes</a>'
    ' <span style="color:#fff">&middot;</span> '
    '<a href="../../examples/index.html" style="color:#fff">Ejemplos</a></div>'
)


class BuildError(RuntimeError):
    pass


class MissingDependency(RuntimeError):
    pass


def log(message: str) -> None:
    print(message, flush=True)


def rel(path: Path) -> str:
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return str(path)


# --------------------------------------------------------------------------------------
# Dependencias
# --------------------------------------------------------------------------------------

def check_python_dependencies() -> list[str]:
    notes = []
    for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line or "==" not in line:
            continue
        name, wanted = (part.strip() for part in line.split("==", 1))
        try:
            found = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError as exc:
            raise MissingDependency(
                f"Falta el paquete Python '{name}'. Instala: "
                f"python -m pip install -r {rel(REQUIREMENTS)}") from exc
        if found != wanted:
            notes.append(f"{name} {found} instalado (requirements fija {wanted})")
    return notes


def find_tool(env_var: str, executable: str) -> str | None:
    candidate = os.environ.get(env_var)
    if candidate:
        return candidate if Path(candidate).exists() else None
    return shutil.which(executable)


def tool_version(executable: str, flag: str) -> str:
    try:
        out = subprocess.run([executable, flag], capture_output=True, text=True, encoding="utf-8",
                             errors="replace", check=False)
        return (out.stdout or out.stderr).strip().splitlines()[0]
    except (OSError, IndexError):
        return "desconocida"


# --------------------------------------------------------------------------------------
# Doxygen
# --------------------------------------------------------------------------------------

def generate_examples_dox() -> int:
    """Crea el índice @example para que Doxygen publique los sketches con enlaces a la API."""
    blocks = ["/**", " * @file doxygen-examples.dox",
              " * @brief Generado por tools/docs/build_docs.py. No editar.", " */", ""]
    count = 0
    for ino in sorted((REPO_ROOT / "examples").glob("*/*.ino")):
        text = ino.read_text(encoding="utf-8")
        brief = re.search(r"@brief\s+(.+)", text)
        name = ino.relative_to(REPO_ROOT / "examples").as_posix()
        blocks += ["/**", f" * @example {name}", f" * {brief.group(1).strip() if brief else ino.stem}", " */", ""]
        count += 1
    EXAMPLES_DOX.write_text("\n".join(blocks), encoding="utf-8")
    return count


def run_doxygen(doxygen: str, dot: str) -> dict:
    if DOXYGEN_OUT.exists():
        shutil.rmtree(DOXYGEN_OUT)
    DOXYGEN_OUT.mkdir(parents=True)
    examples = generate_examples_dox()
    base_config = (REPO_ROOT / "docs" / "Doxyfile").read_text(encoding="utf-8")
    overrides = "\n".join([
        "",
        "# --- Ajustes del sitio (tools/docs/build_docs.py) ---",
        f"OUTPUT_DIRECTORY = {rel(DOXYGEN_OUT)}",
        f"INPUT += {rel(EXAMPLES_DOX)}",
        "GENERATE_XML = YES",
        "XML_PROGRAMLISTING = NO",
        f"WARN_LOGFILE = {rel(DOXYGEN_LOG)}",
        "HAVE_DOT = YES",
        f'DOT_PATH = "{Path(dot).parent.as_posix()}"',
        "QUIET = YES",
        "",
    ])
    log(f"  · Doxygen ({tool_version(doxygen, '--version')}), {examples} ejemplos .ino indexados")
    result = subprocess.run([doxygen, "-"], input=base_config + overrides, cwd=REPO_ROOT, text=True,
                            encoding="utf-8", errors="replace", capture_output=True, check=False)
    if result.returncode != 0:
        raise BuildError("Doxygen terminó con error:\n" + result.stdout + result.stderr)
    if not (DOXYGEN_OUT / "html" / "index.html").exists() or not (DOXYGEN_OUT / "xml" / "index.xml").exists():
        raise BuildError("Doxygen no generó html/index.html o xml/index.xml")

    warnings = [line for line in DOXYGEN_LOG.read_text(encoding="utf-8", errors="replace").splitlines()
                if "warning:" in line] if DOXYGEN_LOG.exists() else []
    baseline = int(BASELINE_FILE.read_text(encoding="utf-8").split()[0])
    rewritten, neutralized = repair_doxygen_links()
    inject_backlinks()
    if rewritten or neutralized:
        log(f"  · enlaces de Doxygen sin destino local: {rewritten} redirigidos al repositorio, "
            f"{len(neutralized)} neutralizados")
    return {"warnings": len(warnings), "baseline": baseline, "examples": examples,
            "links_redirected_to_repository": rewritten, "links_neutralized": neutralized}


def _repository_url() -> str:
    data = json.loads((REPO_ROOT / "library.json").read_text(encoding="utf-8"))
    url = data.get("repository", {}).get("url", "")
    return url[:-4] if url.endswith(".git") else url


def repair_doxygen_links() -> tuple[int, list[str]]:
    """Corrige enlaces de Doxygen que apuntan a archivos inexistentes en la salida HTML.

    Las páginas Markdown que ``docs/Doxyfile`` incluye (README, historias de usuario...) enlazan
    a archivos del repositorio que Doxygen no publica. Se redirigen al repositorio si el archivo
    existe; si no, se elimina el ``href`` (el texto se conserva) y se registra en el informe.
    """
    import posixpath
    import xml.etree.ElementTree as ET

    html_dir, xml_dir = DOXYGEN_OUT / "html", DOXYGEN_OUT / "xml"
    locations: dict[str, str] = {}
    for xml_file in xml_dir.glob("*.xml"):
        if xml_file.name in ("index.xml", "Doxyfile.xml"):
            continue
        compound = ET.parse(xml_file).getroot().find("compounddef")
        location = compound.find("location") if compound is not None else None
        if location is not None and location.get("file"):
            locations[compound.get("id")] = location.get("file")
    repo = _repository_url()
    rewritten = 0
    neutralized: list[str] = []
    attr_re = re.compile(r'(\s(?:href|src)=")([^"]+)(")')

    for page in html_dir.glob("*.html"):
        text = page.read_text(encoding="utf-8", errors="replace")
        base_dir = posixpath.dirname(locations.get(page.stem, ""))

        def fix(match: re.Match) -> str:
            nonlocal rewritten
            value = match.group(2)
            parsed = urllib.parse.urlsplit(value)
            if parsed.scheme or parsed.netloc or value.startswith(("#", "javascript:", "data:")):
                return match.group(0)
            path = urllib.parse.unquote(parsed.path)
            if not path or (html_dir / path).exists():
                return match.group(0)
            target = None
            if path.endswith("_source.html") and path[: -len("_source.html")] in locations:
                target = locations[path[: -len("_source.html")]]
            else:
                target = posixpath.normpath(posixpath.join(base_dir, path)).rstrip("/")
            generated = target and target.split("/", 1)[0] in ("build", "site")
            if target and not generated and not target.startswith("..") and (REPO_ROOT / target).exists():
                if target.startswith("assets/branding/"):
                    new_value = "../../" + target  # el logotipo también se publica en el sitio
                else:
                    kind = "tree" if (REPO_ROOT / target).is_dir() else "blob"
                    new_value = f"{repo}/{kind}/main/{target}" + (f"#{parsed.fragment}" if parsed.fragment else "")
                rewritten += 1
                return f"{match.group(1)}{new_value}{match.group(3)}"
            neutralized.append(f"{page.name}: {value}")
            return f' data-unresolved="{value}"'

        new_text = attr_re.sub(fix, text)
        if new_text != text:
            page.write_text(new_text, encoding="utf-8")
    return rewritten, neutralized


def inject_backlinks() -> int:
    """Añade a cada página de Doxygen un enlace de vuelta al sitio editorial."""
    count = 0
    for page in (DOXYGEN_OUT / "html").glob("*.html"):
        text = page.read_text(encoding="utf-8", errors="replace")
        if BACKLINK_MARKER in text:
            continue
        new_text, n = re.subn(r"(<body[^>]*>)", r"\1" + BACKLINK_HTML.replace("\\", "\\\\"), text, count=1)
        if n:
            page.write_text(new_text, encoding="utf-8")
            count += 1
    return count


# --------------------------------------------------------------------------------------
# MkDocs
# --------------------------------------------------------------------------------------

def run_mkdocs(dot: str) -> None:
    env = dict(os.environ, GRAPHVIZ_DOT=dot, PYTHONIOENCODING="utf-8")
    cmd = [sys.executable, "-m", "mkdocs", "build", "--strict", "--clean",
           "-f", str(REPO_ROOT / "mkdocs.yml"), "-d", str(SITE_DIR)]
    result = subprocess.run(cmd, cwd=REPO_ROOT, env=env, text=True, encoding="utf-8", errors="replace",
                            capture_output=True, check=False)
    output = result.stdout + result.stderr
    MKDOCS_LOG.write_text(output, encoding="utf-8")
    for line in output.splitlines():
        if line.startswith(("WARNING", "ERROR")) or "Aborted" in line:
            log("    " + line)
    if result.returncode != 0:
        raise BuildError(f"mkdocs build --strict falló (detalles en {rel(MKDOCS_LOG)})")


# --------------------------------------------------------------------------------------
# Validación
# --------------------------------------------------------------------------------------

class _LinkCollector(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links: list[str] = []

    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if value and name in ("href", "src") and tag in ("a", "link", "script", "img", "iframe", "source"):
                self.links.append(value)


def check_internal_links(site: Path, base_path: str = DEFAULT_BASE_PATH) -> tuple[int, list[str]]:
    """Comprueba href/src relativos. Las rutas absolutas solo se aceptan bajo ``base_path``
    (MkDocs las usa en 404.html, que el alojamiento sirve desde rutas arbitrarias)."""
    broken: list[str] = []
    checked = 0
    for page in site.rglob("*.html"):
        parser = _LinkCollector()
        parser.feed(page.read_text(encoding="utf-8", errors="replace"))
        for link in parser.links:
            parsed = urllib.parse.urlsplit(link)
            if parsed.scheme or parsed.netloc or link.startswith(("#", "mailto:", "javascript:", "data:")):
                continue
            target_path = urllib.parse.unquote(parsed.path)
            if link.startswith("/"):
                if page.name != "404.html" or not target_path.startswith(base_path):
                    broken.append(f"{rel(page)} -> {link} (ruta absoluta fuera de 404.html o de {base_path})")
                    continue
                target = (site / target_path[len(base_path):]).resolve()
                if link.endswith("/") or target.is_dir():
                    target = target / "index.html"
                checked += 1
                if not target.exists():
                    broken.append(f"{rel(page)} -> {link}")
                continue
            if not target_path:
                continue
            target = (page.parent / target_path).resolve()
            if link.endswith("/") or target.is_dir():
                target = target / "index.html"
            checked += 1
            if not target.exists():
                broken.append(f"{rel(page)} -> {link}")
    return checked, broken


LOCAL_FILE_URL_RE = re.compile(r"file:///?(?:[A-Za-z]:[/\\]|home/|Users/|tmp/)")


def check_local_paths(site: Path) -> list[str]:
    """Busca rutas del equipo de construcción (repositorio, carpeta personal, URLs file:// locales)."""
    needles = {str(REPO_ROOT), REPO_ROOT.as_posix(), str(Path.home()), Path.home().as_posix()}
    needles = {n for n in needles if len(n) > 3}
    hits = []
    for path in site.rglob("*"):
        if path.suffix.lower() not in (".html", ".js", ".css", ".xml", ".svg", ".json", ".txt", ".map"):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for needle in needles:
            if needle in text:
                hits.append(f"{rel(path)} contiene una ruta local ('{needle}')")
        match = LOCAL_FILE_URL_RE.search(text)
        if match:
            hits.append(f"{rel(path)} contiene una URL local ('{match.group(0)}')")
    return hits


def check_structure(site: Path) -> list[str]:
    required = ["index.html", "getting-started/index.html", "architecture/index.html",
                "components/index.html", "examples/index.html", "api/index.html",
                "api/reference/index.html", "search/search_index.json", "sitemap.xml"]
    problems = [f"Falta {r}" for r in required if not (site / r).exists()]
    examples = sorted(p.parent.name for p in (REPO_ROOT / "examples").glob("*/*.ino"))
    for name in examples:
        if not (site / "examples" / name / "index.html").exists():
            problems.append(f"Falta la página del ejemplo {name}")
    return problems


class _BasePathHandler(http.server.SimpleHTTPRequestHandler):
    base_path = DEFAULT_BASE_PATH

    def translate_path(self, path):
        parsed = urllib.parse.urlsplit(path).path
        if parsed.startswith(self.base_path):
            parsed = "/" + parsed[len(self.base_path):]
        elif parsed not in ("/", ""):
            return str(Path(self.directory) / "__fuera_del_subdirectorio__")
        return super().translate_path(parsed)

    def do_GET(self):
        if urllib.parse.urlsplit(self.path).path in ("/", ""):
            self.send_response(302)
            self.send_header("Location", self.base_path)
            self.end_headers()
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        if getattr(self.server, "verbose", False):
            super().log_message(fmt, *args)


def _make_server(site: Path, port: int, base_path: str, verbose: bool):
    handler = type("Handler", (_BasePathHandler,), {"base_path": base_path})
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port),
                                             functools.partial(handler, directory=str(site)))
    server.verbose = verbose
    return server


def http_smoke_test(site: Path, base_path: str) -> tuple[list[str], list[str]]:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    server = _make_server(site, port, base_path, verbose=False)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    root = f"http://127.0.0.1:{port}{base_path}"
    paths = ["", "getting-started/", "architecture/", "components/", "components/Device/",
             "components/Led/", "examples/", "examples/StatusIndicator/", "api/",
             "api/reference/index.html", "api/reference/annotated.html", "assets/branding/logo.png",
             "search/search_index.json", "404.html"]
    index_parser = _LinkCollector()
    index_parser.feed((site / "index.html").read_text(encoding="utf-8"))
    paths += [l for l in index_parser.links if l.endswith((".css", ".js")) and not urllib.parse.urlsplit(l).scheme]
    ok, failed = [], []
    try:
        for path in dict.fromkeys(paths):
            url = urllib.parse.urljoin(root, path)
            try:
                with urllib.request.urlopen(url, timeout=10) as response:
                    (ok if response.status == 200 else failed).append(f"{response.status} {path or '/'}")
            except urllib.error.HTTPError as exc:
                failed.append(f"{exc.code} {path or '/'}")
    finally:
        server.shutdown()
    return ok, failed


def validate(site: Path, base_path: str) -> dict:
    if not (site / "index.html").exists():
        raise BuildError(f"No existe {rel(site)}/index.html: ejecuta primero el build")
    files = [p for p in site.rglob("*") if p.is_file()]
    checked, broken = check_internal_links(site, base_path)
    structure = check_structure(site)
    leaks = check_local_paths(site)
    http_ok, http_failed = http_smoke_test(site, base_path)
    return {
        "files": len(files),
        "html_pages": sum(1 for p in files if p.suffix == ".html"),
        "component_pages": len(list((site / "components").glob("*/index.html"))),
        "example_pages": len(list((site / "examples").glob("*/index.html"))),
        "api_pages": len(list((site / "api" / "reference").glob("*.html"))),
        "links_checked": checked, "broken_links": broken,
        "structure_problems": structure, "local_path_leaks": leaks,
        "http_base_path": base_path, "http_ok": http_ok, "http_failed": http_failed,
    }


def write_report(data: dict) -> None:
    REPORT_JSON.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    v = data.get("validation", {})
    d = data.get("doxygen", {})
    lines = ["# Informe de validación del sitio", "",
             "| Validación | Resultado |", "|---|---|"]
    if d:
        lines.append(f"| Avisos de Doxygen | {d['warnings']} (línea base {d['baseline']}) |")
        lines.append(f"| Enlaces de Doxygen sin destino local | {d['links_redirected_to_repository']} redirigidos "
                     f"al repositorio, {len(d['links_neutralized'])} neutralizados |")
    if v:
        lines += [
            f"| Archivos en site/ | {v['files']} ({v['html_pages']} HTML) |",
            f"| Fichas de componentes | {v['component_pages']} |",
            f"| Páginas de ejemplos | {v['example_pages']} |",
            f"| Páginas de la API | {v['api_pages']} |",
            f"| Enlaces internos comprobados | {v['links_checked']}, rotos: {len(v['broken_links'])} |",
            f"| Estructura | {'OK' if not v['structure_problems'] else '; '.join(v['structure_problems'])} |",
            f"| Rutas locales filtradas | {len(v['local_path_leaks'])} |",
            f"| HTTP bajo `{v['http_base_path']}` | {len(v['http_ok'])} OK, {len(v['http_failed'])} fallos |",
        ]
    lines += ["", "Detalle completo en validation-report.json."]
    REPORT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_zip() -> None:
    with zipfile.ZipFile(ZIP_PATH, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(SITE_DIR.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(SITE_DIR).as_posix())


# --------------------------------------------------------------------------------------
# Comandos
# --------------------------------------------------------------------------------------

def cmd_build(args) -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    report: dict = {}
    log("[1/4] Comprobando dependencias")
    for note in check_python_dependencies():
        log(f"  ! {note}")
    doxygen = find_tool("DOXYGEN", "doxygen")
    dot = find_tool("GRAPHVIZ_DOT", "dot")
    missing = [n for n, p in (("Doxygen (DOXYGEN)", doxygen), ("Graphviz dot (GRAPHVIZ_DOT)", dot)) if not p]
    if missing:
        raise MissingDependency("No encontrado: " + ", ".join(missing))
    log(f"  · dot: {tool_version(dot, '-V')}")

    if args.skip_doxygen:
        log("[2/4] Doxygen omitido (--skip-doxygen)")
        if not (DOXYGEN_OUT / "xml" / "index.xml").exists():
            raise BuildError("--skip-doxygen requiere una salida previa en build/docs/doxygen")
    else:
        log("[2/4] Generando la referencia de API con Doxygen")
        report["doxygen"] = run_doxygen(doxygen, dot)
        d = report["doxygen"]
        log(f"  · avisos: {d['warnings']} (línea base {d['baseline']}; detalle en {rel(DOXYGEN_LOG)})")
        if d["warnings"] > d["baseline"] and not args.allow_doxygen_warnings:
            raise BuildError("Los avisos de Doxygen superan la línea base. Corrígelos o, si están "
                             f"justificados, actualiza {rel(BASELINE_FILE)}.")
        if d["warnings"] < d["baseline"]:
            log(f"  · hay menos avisos que la línea base: puedes reducir {rel(BASELINE_FILE)} a {d['warnings']}")

    if args.skip_mkdocs:
        log("[3/4] MkDocs omitido (--skip-mkdocs). Ahora puedes usar: mkdocs serve")
        return 0
    log("[3/4] Construyendo el sitio con MkDocs (modo estricto)")
    run_mkdocs(dot)

    log("[4/4] Validando el sitio generado")
    report["validation"] = validate(SITE_DIR, args.base_path)
    write_report(report)
    failed = _print_validation(report["validation"])
    if args.zip and not failed:
        make_zip()
        log(f"  · ZIP: {rel(ZIP_PATH)}")
    log(f"Informe: {rel(REPORT_MD)}")
    if failed:
        return 1
    log(f"Sitio listo en {rel(SITE_DIR)}/ (index.html en la raíz)")
    return 0


def _print_validation(v: dict) -> bool:
    log(f"  · {v['files']} archivos, {v['component_pages']} fichas, {v['example_pages']} ejemplos, "
        f"{v['api_pages']} páginas de API")
    log(f"  · enlaces internos comprobados: {v['links_checked']}, rotos: {len(v['broken_links'])}")
    for item in v["broken_links"][:25]:
        log(f"    ✗ {item}")
    for item in v["structure_problems"]:
        log(f"    ✗ {item}")
    for item in v["local_path_leaks"][:25]:
        log(f"    ✗ {item}")
    log(f"  · HTTP bajo {v['http_base_path']}: {len(v['http_ok'])} OK, {len(v['http_failed'])} fallos")
    for item in v["http_failed"]:
        log(f"    ✗ {item}")
    return bool(v["broken_links"] or v["structure_problems"] or v["local_path_leaks"] or v["http_failed"])


def cmd_check(args) -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    validation = validate(SITE_DIR, args.base_path)
    write_report({"validation": validation})
    return 1 if _print_validation(validation) else 0


def cmd_serve(args) -> int:
    if not (SITE_DIR / "index.html").exists():
        raise BuildError("No existe site/index.html: ejecuta primero python tools/docs/build_docs.py")
    base = args.base_path
    server = _make_server(SITE_DIR, args.port, base, verbose=True)
    log(f"Sirviendo {rel(SITE_DIR)}/ en http://127.0.0.1:{args.port}{base} (Ctrl+C para detener)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


def cmd_clean(_args) -> int:
    for target in (SITE_DIR, BUILD_DIR):
        resolved = target.resolve()
        if REPO_ROOT not in resolved.parents:
            raise BuildError(f"Ruta fuera del repositorio, no se borra: {resolved}")
        if resolved.exists():
            shutil.rmtree(resolved)
            log(f"Eliminado {rel(resolved)}/")
    return 0


def main(argv: list[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command")

    def add_base_path(p):
        p.add_argument("--base-path", default=DEFAULT_BASE_PATH,
                       help=f"subdirectorio simulado del alojamiento (por defecto {DEFAULT_BASE_PATH})")

    build = sub.add_parser("build", help="construcción completa (por defecto)")
    for p in (parser, build):
        p.add_argument("--skip-doxygen", action="store_true", help="reutiliza la salida previa de Doxygen")
        p.add_argument("--skip-mkdocs", action="store_true", help="solo genera la API (para mkdocs serve)")
        p.add_argument("--allow-doxygen-warnings", action="store_true", help="no falla si se supera la línea base")
        p.add_argument("--zip", action="store_true", help="crea un ZIP del sitio en build/docs/")
        add_base_path(p)
    add_base_path(sub.add_parser("check", help="valida site/ (enlaces, rutas locales, HTTP)"))
    serve = sub.add_parser("serve", help="sirve site/ por HTTP")
    serve.add_argument("--port", type=int, default=8000)
    add_base_path(serve)
    sub.add_parser("clean", help="borra site/ y build/docs/")

    args = parser.parse_args(argv)
    base_path = getattr(args, "base_path", None)
    if base_path and not (base_path.startswith("/") and base_path.endswith("/")):
        parser.error("--base-path debe empezar y terminar con '/'")
    commands = {None: cmd_build, "build": cmd_build, "check": cmd_check, "serve": cmd_serve, "clean": cmd_clean}
    try:
        return commands[args.command](args)
    except MissingDependency as exc:
        log(f"ERROR de dependencias: {exc}")
        return 2
    except BuildError as exc:
        log(f"ERROR: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())

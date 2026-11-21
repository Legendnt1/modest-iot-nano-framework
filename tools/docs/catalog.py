"""Modelo del catálogo de Modest-IoT Nano construido a partir de evidencia del repositorio.

Fuentes (en este orden de autoridad):

1. XML generado por Doxygen a partir de ``src/`` (firmas, herencia, comentarios).
2. Cabeceras de ``src/`` (``#include``, constantes privadas de muestreo).
3. Sketches de ``examples/`` (qué clases usa cada ejemplo).
4. Pruebas de ``tests/`` y ``tests/run_all.sh`` (qué suites existen y qué casos describen).
5. Metadatos (``library.properties``, ``library.json``).

El módulo no inventa datos: si algo no puede extraerse, la página generada lo dice.
Lo utilizan ``mkdocs_hooks.py`` (generación de páginas) y ``build_docs.py`` (validación).
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = REPO_ROOT / "src"
EXAMPLES_DIR = REPO_ROOT / "examples"
TESTS_DIR = REPO_ROOT / "tests"
BUILD_DIR = REPO_ROOT / "build" / "docs"
DOXYGEN_DIR = BUILD_DIR / "doxygen"
XML_DIR = DOXYGEN_DIR / "xml"
HTML_DIR = DOXYGEN_DIR / "html"
CATALOG_CONFIG = REPO_ROOT / "docs" / "components" / "catalog.yml"

API_PREFIX = "api/reference"

#: Dependencias declaradas en library.properties (``depends=``), indexadas por cabecera.
DECLARED_LIBRARY_HEADERS = {"ArduinoJson.h": "ArduinoJson", "MQTT.h": "MQTT"}


# --------------------------------------------------------------------------------------
# Metadatos del proyecto
# --------------------------------------------------------------------------------------

def read_library_properties() -> dict[str, str]:
    props: dict[str, str] = {}
    for line in (REPO_ROOT / "library.properties").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            key, value = line.split("=", 1)
            props[key.strip()] = value.strip()
    return props


def repository_url() -> str:
    """URL del repositorio declarada en library.json (sin sufijo .git)."""
    data = json.loads((REPO_ROOT / "library.json").read_text(encoding="utf-8"))
    url = data.get("repository", {}).get("url", "")
    return url[:-4] if url.endswith(".git") else url


def source_url(rel_path: str, branch: str = "main") -> str:
    kind = "tree" if (REPO_ROOT / rel_path).is_dir() else "blob"
    return f"{repository_url()}/{kind}/{branch}/{rel_path}"


def find_dot() -> str | None:
    import os

    candidate = os.environ.get("GRAPHVIZ_DOT")
    if candidate and Path(candidate).exists():
        return candidate
    return shutil.which("dot")


# --------------------------------------------------------------------------------------
# Conversión de descripciones Doxygen (XML) a Markdown
# --------------------------------------------------------------------------------------

RUNS_ON_RE = re.compile(r"Runs on:\s*([A-Za-z_/ ,\-]+?)(?:\s*$|\s*\n|\.)", re.M)


def _text(elem: ET.Element | None) -> str:
    return "".join(elem.itertext()).strip() if elem is not None else ""


def _escape_cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ").strip()


class DescriptionRenderer:
    """Convierte el subárbol de descripción de Doxygen a Markdown sencillo.

    ``link_for_refid`` permite enlazar ``<ref>`` a páginas del catálogo o a la API.
    Las secciones simples (note, warning, see, pre, post) se recogen aparte para
    poder listarlas como restricciones.
    """

    def __init__(self, link_for_refid):
        self.link_for_refid = link_for_refid

    def inline(self, elem: ET.Element) -> str:
        parts: list[str] = [elem.text or ""]
        for child in elem:
            parts.append(self._inline_child(child))
            parts.append(child.tail or "")
        return "".join(parts)

    def _inline_child(self, child: ET.Element) -> str:
        tag = child.tag
        if tag == "computeroutput":
            inner = _text(child)
            return f"`{inner}`" if inner else ""
        if tag == "ref":
            label = _text(child)
            href = self.link_for_refid(child.get("refid", ""), child.get("kindref", ""))
            return f"[`{label}`]({href})" if href else f"`{label}`"
        if tag in ("bold",):
            return f"**{self.inline(child).strip()}**"
        if tag in ("emphasis",):
            return f"*{self.inline(child).strip()}*"
        if tag == "ulink":
            return f"[{self.inline(child).strip()}]({child.get('url', '')})"
        if tag in ("linebreak",):
            return " "
        if tag in ("simplesect", "parameterlist", "programlisting", "itemizedlist",
                   "orderedlist", "xrefsect", "table", "heading"):
            return ""  # bloques: se tratan en block()
        return self.inline(child)

    def block(self, elem: ET.Element | None) -> tuple[str, list[tuple[str, str]]]:
        """Devuelve (markdown, [(tipo_seccion, texto)])."""
        if elem is None:
            return "", []
        out: list[str] = []
        sections: list[tuple[str, str]] = []
        for para in elem.findall("para"):
            text = self.inline(para).strip()
            if text:
                out.append(text)
            for child in para:
                if child.tag == "simplesect":
                    kind = child.get("kind", "")
                    sect_text = " ".join(self.inline(p).strip() for p in child.findall("para"))
                    sections.append((kind, sect_text))
                elif child.tag in ("itemizedlist", "orderedlist"):
                    marker = "-" if child.tag == "itemizedlist" else "1."
                    for item in child.findall("listitem"):
                        item_text = " ".join(self.inline(p).strip() for p in item.findall("para"))
                        out.append(f"{marker} {item_text}")
                elif child.tag == "programlisting":
                    lines = []
                    for codeline in child.findall("codeline"):
                        lines.append("".join(codeline.itertext()))
                    out.append("```cpp\n" + "\n".join(lines) + "\n```")
        markdown = "\n\n".join(out)
        return markdown, sections


# --------------------------------------------------------------------------------------
# Estructuras del modelo
# --------------------------------------------------------------------------------------

@dataclass
class Member:
    name: str
    kind: str
    prot: str
    static: bool
    virt: str
    definition: str
    argsstring: str
    brief: str
    detail: str
    runs_on: str
    initializer: str
    refid: str
    enum_values: list[tuple[str, str, str]] = field(default_factory=list)
    sections: list[tuple[str, str]] = field(default_factory=list)

    @property
    def signature(self) -> str:
        sig = (self.definition + self.argsstring).strip()
        sig = re.sub(r"\s+", " ", sig)
        return sig

    @property
    def is_identifier_constant(self) -> bool:
        return (self.kind == "variable" and self.static and self.name.endswith("_IDENTIFIER")
                and bool(self.initializer))


@dataclass
class Component:
    name: str
    refid: str
    kind: str
    brief: str
    detail: str
    sections: list[tuple[str, str]]
    header: str
    bases: list[str]
    derived: list[str]
    members: list[Member]
    category: str = "uncategorized"
    examples: list[str] = field(default_factory=list)
    is_file_component: bool = False
    title: str = ""
    file_members: list[Member] = field(default_factory=list)

    @property
    def page(self) -> str:
        return f"components/{self.name}.md"

    @property
    def api_page(self) -> str:
        return f"{API_PREFIX}/{self.refid}.html"

    def by(self, prot: str, kinds: tuple[str, ...]) -> list[Member]:
        return [m for m in self.members if m.prot == prot and m.kind in kinds]


@dataclass
class Example:
    name: str
    path: str  # relativo al repositorio
    brief: str
    aspects: list[str]
    includes: list[str]
    code: str
    components: list[str] = field(default_factory=list)
    doxygen_page: str | None = None

    @property
    def page(self) -> str:
        return f"examples/{self.name}.md"


@dataclass
class TestSuite:
    name: str
    path: str
    in_runner: bool
    runner_sources: list[str]
    included_headers: list[str]
    cases: list[str]


@dataclass
class Category:
    id: str
    title: str
    description: str
    hardware: str
    derives_from: list[str]
    members: list[str]


@dataclass
class Model:
    components: dict[str, Component]
    categories: list[Category]
    examples: dict[str, Example]
    tests: dict[str, TestSuite]
    umbrella_headers: set[str]
    doxygen_index: dict[str, str]
    file_refids: dict[str, str]
    warnings: list[str]
    version: str

    def components_in(self, category_id: str) -> list[Component]:
        return sorted((c for c in self.components.values() if c.category == category_id),
                      key=lambda c: c.name.lower())

    def category(self, category_id: str) -> Category | None:
        return next((c for c in self.categories if c.id == category_id), None)

    def tests_for(self, component: Component) -> tuple[list[TestSuite], list[TestSuite]]:
        """(suites principales, suites que solo incluyen la cabecera)."""
        primary, secondary = [], []
        header_name = Path(component.header).name
        cpp_rel = f"src/{Path(component.header).stem}.cpp"
        for suite in self.tests.values():
            if suite.name == component.name:
                primary.append(suite)
            elif header_name in suite.included_headers or cpp_rel in suite.runner_sources:
                secondary.append(suite)
        return primary, secondary


# --------------------------------------------------------------------------------------
# Carga del modelo
# --------------------------------------------------------------------------------------

def _member_from_xml(md: ET.Element, renderer: DescriptionRenderer) -> Member:
    brief, _ = renderer.block(md.find("briefdescription"))
    detail, sections = renderer.block(md.find("detaileddescription"))
    runs_on = ""
    match = RUNS_ON_RE.search(detail)
    if match:
        runs_on = match.group(1).strip()
        detail = RUNS_ON_RE.sub("", detail).strip()
    enum_values = []
    for ev in md.findall("enumvalue"):
        ev_brief, _ = renderer.block(ev.find("briefdescription"))
        enum_values.append((_text(ev.find("name")), _text(ev.find("initializer")), ev_brief))
    return Member(
        name=_text(md.find("name")),
        kind=md.get("kind", ""),
        prot=md.get("prot", ""),
        static=md.get("static") == "yes",
        virt=md.get("virt", ""),
        definition=_text(md.find("definition")),
        argsstring=_text(md.find("argsstring")),
        brief=brief,
        detail=detail,
        runs_on=runs_on,
        initializer=_text(md.find("initializer")),
        refid=md.get("id", ""),
        enum_values=enum_values,
        sections=sections,
    )


MEMBER_ID_RE = re.compile(r"^(.*)_1([a-z][0-9a-f]{20,})$")


def api_href_for_refid(refid: str) -> str:
    """Ruta (relativa a la raíz del sitio) de la página Doxygen para un refid."""
    match = MEMBER_ID_RE.match(refid)
    if match:
        return f"{API_PREFIX}/{match.group(1)}.html#{match.group(2)}"
    return f"{API_PREFIX}/{refid}.html"


def _load_categories(warnings: list[str]) -> list[Category]:
    data = yaml.safe_load(CATALOG_CONFIG.read_text(encoding="utf-8"))
    cats = []
    for raw in data.get("categories", []):
        cats.append(Category(
            id=raw["id"], title=raw["title"], description=raw.get("description", ""),
            hardware=raw.get("hardware", ""), derives_from=raw.get("derives_from", []) or [],
            members=raw.get("members", []) or [],
        ))
    return cats


def _ancestors(name: str, components: dict[str, Component]) -> list[str]:
    seen: list[str] = []
    stack = list(components[name].bases) if name in components else []
    while stack:
        base = stack.pop()
        if base in seen:
            continue
        seen.append(base)
        if base in components:
            stack.extend(components[base].bases)
    return seen


def _strip_cpp_comments(code: str) -> str:
    code = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), code, flags=re.S)
    code = re.sub(r"//[^\n]*", "", code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    return code


def _parse_example(ino: Path) -> Example:
    code = ino.read_text(encoding="utf-8")
    header_match = re.search(r"/\*\*(.*?)\*/", code, re.S)
    brief, aspects = "", []
    if header_match:
        lines = [re.sub(r"^\s*\*\s?", "", l) for l in header_match.group(1).splitlines()]
        for i, line in enumerate(lines):
            if line.startswith("@brief"):
                brief = line[len("@brief"):].strip()
            if line.startswith("@par"):
                for follow in lines[i + 1:]:
                    if not follow.strip():
                        break
                    if follow.strip().startswith("-"):
                        aspects.append(follow.strip()[1:].strip())
                    elif aspects:
                        aspects[-1] += " " + follow.strip()
    includes = re.findall(r"^\s*#include\s*[<\"]([^>\"]+)[>\"]", code, re.M)
    return Example(name=ino.parent.name, path=ino.relative_to(REPO_ROOT).as_posix(), brief=brief,
                   aspects=aspects, includes=includes, code=code)


TEST_CASE_RE = re.compile(r'printf\(\s*"((?:\\.|[^"\\])*)"')
TEST_COMMENT_RE = re.compile(r"^\s*//\s*Test\s+\d+[a-z]?\s*:\s*(.+)$", re.M)


TEST_FUNC_RE = re.compile(r"^\s*(?:static\s+)?void\s+test_?(\w+)\s*\(\s*\)", re.M)


def _humanize_test_name(name: str) -> str:
    """testBeginReadOnly / test_begin_read_only -> 'Begin read only' (nombre de la función)."""
    words = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name).replace("_", " ").split()
    text = " ".join(w if w.isupper() and len(w) > 1 else w.lower() for w in words)
    return (text[:1].upper() + text[1:]) + " (nombre de la función de prueba)"


def _parse_runner() -> dict[str, list[str]]:
    runner = (TESTS_DIR / "run_all.sh").read_text(encoding="utf-8")
    suites: dict[str, list[str]] = {}
    for block in re.finditer(r'run_test\s+"(test_\w+)"((?:[^\n]*\\\n)*[^\n]*)', runner):
        sources = re.findall(r"(src/\w+\.cpp|tests/[\w/]+\.cpp)", block.group(2))
        suites[block.group(1)[len("test_"):]] = sources
    return suites


def _parse_tests() -> dict[str, TestSuite]:
    runner = _parse_runner()
    suites: dict[str, TestSuite] = {}
    for test_file in sorted(TESTS_DIR.glob("*/test.cpp")):
        name = test_file.parent.name
        code = test_file.read_text(encoding="utf-8")
        cases: list[str] = []
        for raw in TEST_CASE_RE.findall(code):
            text = raw.replace("\\n", " ").replace('\\"', '"').strip()
            text = re.sub(r"^\[(PASS|test|TEST)\]\s*", "", text)
            text = re.sub(r"\s*(\.\.\.|…)\s*(OK|PASS(ED)?)?$", "", text).strip()
            if (not text or "%" in text or "===" in text or "---" in text
                    or re.search(r"\b(all|passed|running|tests? complete|summary)\b", text, re.I)
                    or text.endswith(":")):
                continue
            cases.append(text)
        if not cases:
            cases = [c.strip() for c in TEST_COMMENT_RE.findall(code)]
        if not cases:
            cases = [_humanize_test_name(n) for n in TEST_FUNC_RE.findall(code)]
        includes = re.findall(r'^\s*#include\s*"([^"]+)"', code, re.M)
        suites[name] = TestSuite(
            name=name, path=test_file.relative_to(REPO_ROOT).as_posix(), in_runner=name in runner,
            runner_sources=runner.get(name, []), included_headers=[Path(i).name for i in includes],
            cases=list(dict.fromkeys(cases)),
        )
    return suites


def load_model() -> Model:
    if not (XML_DIR / "index.xml").exists():
        raise FileNotFoundError(
            f"No se encontró el XML de Doxygen en {XML_DIR.relative_to(REPO_ROOT)}. "
            "Ejecuta: python tools/docs/build_docs.py")

    warnings: list[str] = []
    index = ET.parse(XML_DIR / "index.xml").getroot()

    compound_names: dict[str, str] = {}
    doxygen_index: dict[str, str] = {}
    for comp in index.findall("compound"):
        compound_names[comp.get("refid", "")] = _text(comp.find("name"))
        if comp.get("kind") == "example":
            doxygen_index[_text(comp.find("name"))] = comp.get("refid", "")

    components: dict[str, Component] = {}

    def link_for_refid(refid: str, kindref: str) -> str:
        target = compound_names.get(refid)
        # Las descripciones se renderizan en páginas de docs/components/*.md
        if kindref == "compound" and target and "::" not in target and target in class_names:
            return f"{target}.md"  # enlace a otra ficha del catálogo
        return "../" + api_href_for_refid(refid)

    class_names = {_text(c.find("name")) for c in index.findall("compound")
                   if c.get("kind") in ("class", "struct")}
    renderer = DescriptionRenderer(link_for_refid)

    for comp in index.findall("compound"):
        if comp.get("kind") not in ("class", "struct"):
            continue
        name = _text(comp.find("name"))
        if "::" in name:
            continue  # tipos anidados: se muestran dentro de su clase
        refid = comp.get("refid", "")
        root = ET.parse(XML_DIR / f"{refid}.xml").getroot().find("compounddef")
        brief, _ = renderer.block(root.find("briefdescription"))
        detail, sections = renderer.block(root.find("detaileddescription"))
        location = root.find("location")
        header = location.get("file", "") if location is not None else ""
        members = [_member_from_xml(md, renderer) for md in root.iter("memberdef")]
        for inner in root.findall("innerclass"):
            # tipos anidados públicos (p. ej. GasSensor::GasCalibrationParameters)
            if inner.get("prot") == "public":
                inner_root = ET.parse(XML_DIR / f"{inner.get('refid')}.xml").getroot().find("compounddef")
                ib, _ = renderer.block(inner_root.find("briefdescription"))
                members.append(Member(
                    name=_text(inner).split("::")[-1], kind="nested", prot="public", static=False,
                    virt="", definition=f"{inner_root.get('kind')} {_text(inner)}", argsstring="",
                    brief=ib, detail="", runs_on="", initializer="", refid=inner.get("refid", "")))
        components[name] = Component(
            name=name, refid=refid, kind=comp.get("kind", ""), brief=brief, detail=detail,
            sections=sections, header=header,
            bases=[_text(b) for b in root.findall("basecompoundref")],
            derived=[_text(d) for d in root.findall("derivedcompoundref")],
            members=members,
        )

    categories = _load_categories(warnings)
    config = yaml.safe_load(CATALOG_CONFIG.read_text(encoding="utf-8"))

    # Componentes basados en archivo (p. ej. macros de ModestLog.h)
    for raw in config.get("file_components", []) or []:
        file_name = raw["file"]
        refid = next((rid for rid, nm in compound_names.items() if nm == file_name), None)
        if refid is None:
            warnings.append(f"catalog.yml: el archivo '{file_name}' no aparece en el XML de Doxygen")
            continue
        root = ET.parse(XML_DIR / f"{refid}.xml").getroot().find("compounddef")
        brief, _ = renderer.block(root.find("briefdescription"))
        detail, sections = renderer.block(root.find("detaileddescription"))
        members = [_member_from_xml(md, renderer) for md in root.iter("memberdef")]
        for m in members:
            m.prot = "public"
        comp = Component(
            name=raw["name"], refid=refid, kind="file", brief=brief, detail=detail, sections=sections,
            header=f"src/{file_name}", bases=[], derived=[], members=members,
            category=raw["category"], is_file_component=True, title=raw.get("title", raw["name"]))
        components[comp.name] = comp

    # Categorías
    for comp in components.values():
        if comp.is_file_component:
            continue
        explicit = [c for c in categories if comp.name in c.members]
        if explicit:
            comp.category = explicit[0].id
            continue
        ancestors = _ancestors(comp.name, components)
        derived = [c for c in categories if any(b in ancestors for b in c.derives_from)]
        if derived:
            comp.category = derived[0].id
        else:
            warnings.append(
                f"Componente sin categoría en docs/components/catalog.yml: {comp.name} "
                f"({comp.header})")

    # Símbolos a nivel de archivo declarados junto a una clase (enums libres, typedefs)
    file_refids = {nm: rid for rid, nm in compound_names.items()}
    for comp in components.values():
        if comp.is_file_component or not comp.header:
            continue
        rid = file_refids.get(Path(comp.header).name)
        if not rid:
            continue
        froot = ET.parse(XML_DIR / f"{rid}.xml").getroot().find("compounddef")
        for section in froot.findall("sectiondef"):
            if section.get("kind") in ("enum", "typedef", "func"):
                for md in section.findall("memberdef"):
                    member = _member_from_xml(md, renderer)
                    member.prot = "public"
                    comp.file_members.append(member)

    # Ejemplos
    examples: dict[str, Example] = {}
    for ino in sorted(EXAMPLES_DIR.glob("*/*.ino")):
        ex = _parse_example(ino)
        stripped = _strip_cpp_comments(ex.code)
        ex.components = sorted(
            name for name in components
            if not components[name].is_file_component and re.search(rf"\b{re.escape(name)}\b", stripped))
        rel_to_examples = ino.relative_to(EXAMPLES_DIR).as_posix()
        ex.doxygen_page = doxygen_index.get(rel_to_examples)
        examples[ex.name] = ex
        for comp_name in ex.components:
            components[comp_name].examples.append(ex.name)
    log_component = next((c for c in components.values() if c.is_file_component), None)
    if log_component is not None:
        for ex in examples.values():
            if re.search(r"\b(MODEST|MIOT)_LOG_[A-Z]\b", _strip_cpp_comments(ex.code)):
                log_component.examples.append(ex.name)

    # Cabeceras incluidas por la cabecera paraguas
    umbrella = set(re.findall(r'#include\s+"([^"]+)"', (SRC_DIR / "ModestIoT.h").read_text(encoding="utf-8")))

    version = read_library_properties().get("version", "")
    return Model(components=components, categories=categories, examples=examples,
                 tests=_parse_tests(), umbrella_headers=umbrella, doxygen_index=doxygen_index,
                 file_refids=file_refids, warnings=warnings, version=version)


# --------------------------------------------------------------------------------------
# Información adicional extraída de las cabeceras
# --------------------------------------------------------------------------------------

def header_includes(header_rel: str) -> list[str]:
    path = REPO_ROOT / header_rel
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    cpp = path.with_suffix(".cpp")
    if cpp.exists():
        text += "\n" + cpp.read_text(encoding="utf-8")
    return list(dict.fromkeys(re.findall(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", text, re.M)))


def sampling_defaults(header_rel: str) -> tuple[str | None, str | None]:
    path = REPO_ROOT / header_rel
    if not path.exists():
        return None, None
    text = path.read_text(encoding="utf-8")
    default = re.search(r"DEFAULT_SAMPLING_INTERVAL_IN_MILLISECONDS\s*=\s*(\d+)", text)
    minimum = re.search(r"MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS\s*=\s*(\d+)", text)
    return (default.group(1) if default else None, minimum.group(1) if minimum else None)


def render_dot(dot_source: str, dot_exe: str) -> str:
    result = subprocess.run([dot_exe, "-Tsvg"], input=dot_source.encode("utf-8"),
                            capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError("Graphviz falló: " + result.stderr.decode("utf-8", "replace"))
    svg = result.stdout.decode("utf-8")
    # Elimina el prólogo XML/DOCTYPE y comentarios con rutas o versiones del generador
    svg = re.sub(r"<\?xml.*?\?>\s*", "", svg, flags=re.S)
    svg = re.sub(r"<!DOCTYPE.*?>\s*", "", svg, flags=re.S)
    svg = re.sub(r"<!--.*?-->\s*", "", svg, flags=re.S)
    return svg

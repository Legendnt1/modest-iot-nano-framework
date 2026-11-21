"""Generación de páginas Markdown del catálogo y de los ejemplos.

Todas las páginas se generan en memoria durante ``mkdocs build``; no se guardan en ``docs/``.
"""

from __future__ import annotations

import re
from pathlib import Path

from catalog import (
    DECLARED_LIBRARY_HEADERS,
    SRC_DIR,
    Component,
    Example,
    Member,
    Model,
    _escape_cell,
    _strip_cpp_comments,
    api_href_for_refid,
    header_includes,
    sampling_defaults,
    source_url,
)

SECTION_LABELS = {
    "note": "Nota",
    "warning": "Advertencia",
    "attention": "Atención",
    "pre": "Precondición",
    "post": "Postcondición",
    "see": "Véase",
    "return": "Retorno",
    "remark": "Observación",
    "par": "Detalle",
}

KIND_LABELS = {"class": "Clase", "struct": "Estructura", "file": "Archivo de cabecera"}

HOST_TEST_NOTE = (
    "Las pruebas host compilan el código con dobles de Arduino/ESP32 (`tests/fakes`). "
    "Validan lógica, no hardware real."
)


def _api(path_from_root: str) -> str:
    """Enlace desde una página de components/ o examples/ a un recurso de la raíz del sitio."""
    return "../" + path_from_root


def _component_link(model: Model, name: str, from_dir: str = "components") -> str:
    comp = model.components.get(name)
    if comp is None:
        return f"`{name}`"
    target = f"{name}.md" if from_dir == "components" else f"../components/{name}.md"
    return f"[`{name}`]({target})"


def _example_link(name: str, from_dir: str = "components") -> str:
    target = f"../examples/{name}.md" if from_dir == "components" else f"{name}.md"
    return f"[{name}]({target})"


def _first_sentence(text: str) -> str:
    text = re.sub(r"\s+", " ", text).strip()
    match = re.match(r"(.+?\.)(\s|$)", text)
    return match.group(1) if match else text


# --------------------------------------------------------------------------------------
# Fichas de componentes
# --------------------------------------------------------------------------------------

def _dependency_rows(model: Model, comp: Component) -> tuple[list[str], list[str], list[str]]:
    framework, declared, platform = [], [], []
    src_headers = {p.name for p in SRC_DIR.glob("*.h")}
    own = Path(comp.header).name
    for inc in header_includes(comp.header):
        name = Path(inc).name
        if name == own:
            continue
        if name in src_headers:
            owners = [c.name for c in model.components.values()
                      if Path(c.header).name == name and not c.is_file_component]
            framework.append(", ".join(_component_link(model, o) for o in owners) if owners else f"`{name}`")
        elif name in DECLARED_LIBRARY_HEADERS:
            declared.append(f"`{name}` ({DECLARED_LIBRARY_HEADERS[name]})")
        else:
            platform.append(f"`{inc}`")
    return list(dict.fromkeys(framework)), declared, platform


def _member_table(members: list[Member], with_context: bool = True) -> list[str]:
    if with_context:
        lines = ["| Firma | Descripción | Contexto documentado |", "|---|---|---|"]
    else:
        lines = ["| Firma | Descripción |", "|---|---|"]
    for m in members:
        sig = _escape_cell(m.signature)
        link = _api(api_href_for_refid(m.refid))
        desc = _escape_cell(m.brief or _first_sentence(m.detail) or "—")
        row = f"| [`{sig}`]({link}) | {desc} |"
        if with_context:
            row += f" {_escape_cell(m.runs_on) or '—'} |"
        lines.append(row)
    return lines


def _enum_block(m: Member) -> list[str]:
    lines = [f"**[`{m.name}`]({_api(api_href_for_refid(m.refid))})** — {m.brief or 'sin descripción'}", ""]
    if m.enum_values:
        lines += ["| Valor | Inicializador | Descripción |", "|---|---|---|"]
        for name, init, brief in m.enum_values:
            init = init.lstrip("= ").strip()
            lines.append(f"| `{name}` | {('`' + init + '`') if init else '—'} | {_escape_cell(brief) or '—'} |")
        lines.append("")
    return lines


def render_component(model: Model, comp: Component) -> str:
    cat = model.category(comp.category)
    title = comp.title or comp.name
    out: list[str] = [f"# {title}", ""]
    if comp.brief:
        out += [f"> {comp.brief}", ""]
    else:
        out += ['!!! warning "Sin descripción en el código"',
                "    Esta clase no tiene comentario `@brief` en su declaración.", ""]

    header = comp.header
    cpp = Path(header).with_suffix(".cpp").as_posix()
    has_cpp = (SRC_DIR.parent / cpp).exists()
    file_refid = model.file_refids.get(Path(header).name)

    # ---- Ficha
    rows = [
        ("Nombre en el código", f"`{comp.name}`" if not comp.is_file_component else f"`{Path(header).name}`"),
        ("Tipo", KIND_LABELS.get(comp.kind, comp.kind)),
        ("Categoría", f"[{cat.title}](index.md#{cat.id})" if cat else "Sin categoría"),
    ]
    impl = f"Declarado en [`{header}`]({source_url(header)})"
    if has_cpp:
        impl += f"; definido en [`{cpp}`]({source_url(cpp)})"
    else:
        impl += " (implementación en la cabecera)"
    rows.append(("Estado de implementación", impl))
    header_name = Path(header).name
    if header_name in model.umbrella_headers:
        include = "`#include <ModestIoT.h>` (cabecera paraguas)"
    elif header_name in _umbrella_closure():
        include = (f"`#include <ModestIoT.h>` — `{header_name}` no figura en la cabecera paraguas, "
                   "pero se incluye de forma transitiva")
    else:
        include = f"`#include \"{header_name}\"` (no accesible desde `ModestIoT.h`)"
    rows.append(("Inclusión", include))
    if comp.bases:
        rows.append(("Hereda de", ", ".join(_component_link(model, b) for b in comp.bases)))
    if comp.derived:
        rows.append(("Especializaciones", ", ".join(_component_link(model, d) for d in comp.derived)))
    default_ms, min_ms = sampling_defaults(header)
    if default_ms or min_ms:
        rows.append(("Muestreo (constantes de la cabecera)",
                     f"por defecto {default_ms or '—'} ms · mínimo {min_ms or '—'} ms"))
    framework, declared, platform = _dependency_rows(model, comp)
    if framework:
        rows.append(("Depende de (framework)", ", ".join(framework)))
    if declared:
        rows.append(("Bibliotecas declaradas en `library.properties`", ", ".join(declared)))
    if platform:
        rows.append(("Cabeceras de plataforma/sistema", ", ".join(platform)))
    api_target = comp.api_page if not comp.is_file_component else f"api/reference/{comp.refid}.html"
    api_links = f"[Referencia completa]({_api(api_target)})"
    if file_refid:
        api_links += f" · [Código fuente de la cabecera]({_api(f'api/reference/{file_refid}_source.html')})"
    rows.append(("API (Doxygen)", api_links))
    if cat:
        rows.append(("Necesidad de hardware", cat.hardware))

    out += ["## Ficha", "", "| Campo | Valor |", "|---|---|"]
    out += [f"| {k} | {_escape_cell(v)} |" for k, v in rows]
    out += [""]

    # ---- Descripción
    if comp.detail:
        out += ["## Descripción", "", '!!! quote "Extraído de los comentarios del código (en inglés)"', ""]
        out += ["    " + line if line else "" for line in comp.detail.splitlines()]
        out += [""]

    # ---- Eventos y comandos
    identifiers = [m for m in comp.members if m.prot == "public" and m.is_identifier_constant]
    if identifiers:
        out += ["## Eventos y comandos", "",
                "Identificadores públicos declarados por la clase. Los eventos los emite el componente "
                "hacia su `EventHandler`; los comandos los recibe mediante `handle(Command)`.", "",
                "| Constante | Valor | Descripción |", "|---|---|---|"]
        for m in identifiers:
            value = m.initializer.lstrip("= ").strip()
            out.append(f"| [`{m.name}`]({_api(api_href_for_refid(m.refid))}) | `{value}` | "
                       f"{_escape_cell(m.brief) or '—'} |")
        out += [""]

    # ---- API pública
    funcs = [m for m in comp.by("public", ("function",))]
    if comp.is_file_component:
        funcs = [m for m in comp.members if m.kind in ("define", "function")]
    if funcs:
        out += ["## API pública", ""]
        out += _member_table(funcs, with_context=any(m.runs_on for m in funcs))
        out += [""]
    protected = comp.by("protected", ("function",))
    if protected:
        out += ["## Puntos de extensión (miembros protegidos)", "",
                "Métodos que una subclase puede sobrescribir o invocar.", ""]
        out += _member_table(protected, with_context=any(m.runs_on for m in protected))
        out += [""]

    types = [m for m in comp.members if m.prot == "public" and m.kind in ("enum", "typedef", "nested")]
    types += [m for m in comp.file_members if m.kind in ("enum", "typedef")]
    if types:
        out += ["## Tipos públicos", ""]
        for m in types:
            if m.kind == "enum":
                out += _enum_block(m)
            else:
                out += [f"- [`{_escape_cell(m.signature)}`]({_api(api_href_for_refid(m.refid))}) — "
                        f"{m.brief or 'sin descripción'}", ""]

    attrs = [m for m in comp.members
             if m.prot == "public" and m.kind == "variable" and not m.is_identifier_constant]
    if attrs:
        out += ["## Atributos y constantes públicas", "", "| Miembro | Valor | Descripción |", "|---|---|---|"]
        for m in attrs:
            value = m.initializer.lstrip("= {").rstrip("}").strip()
            out.append(f"| [`{_escape_cell(m.signature)}`]({_api(api_href_for_refid(m.refid))}) | "
                       f"{('`' + _escape_cell(value) + '`') if value else '—'} | {_escape_cell(m.brief) or '—'} |")
        out += [""]

    # ---- Escenarios
    out += ["## Escenarios", ""]
    primary, secondary = model.tests_for(comp)
    if primary:
        for suite in primary:
            out += [f"### Escenarios cubiertos por la suite host `{suite.name}`", "",
                    f"- **Preparación:** instancia con dobles de prueba en [`{suite.path}`]({source_url(suite.path)}).",
                    "- **Tipo de validación:** prueba host (simulación con dobles), sin hardware.",
                    "- **Resultado esperado:** el que fijan las aserciones `assert` de cada caso.",
                    "- **Resultado observado:** no se registra en este sitio; consulta el workflow "
                    "*Host Unit Tests* o ejecuta `./tests/run_all.sh`.", ""]
            if suite.cases:
                out += ["| # | Caso (texto tomado de la prueba, en inglés) |", "|---|---|"]
                out += [f"| {i} | {_escape_cell(case)} |" for i, case in enumerate(suite.cases, 1)]
            else:
                out += ["La suite no imprime descripciones de casos; revisa el código fuente de la prueba."]
            if not suite.in_runner:
                out += ["", '!!! warning "Suite fuera del ejecutor"',
                        "    Esta suite no aparece en `tests/run_all.sh`, por lo que el workflow no la ejecuta."]
            out += [""]
    else:
        out += ['!!! info "Sin suite host dedicada"',
                f"    No existe `tests/{comp.name}/test.cpp`. "
                + ("Otras suites incluyen su cabecera (ver abajo)." if secondary else
                   "No se encontró cobertura de pruebas host para este componente."), ""]
    if secondary:
        out += ["**Otras suites que lo compilan o incluyen:** "
                + ", ".join(f"[`{s.name}`]({source_url(s.path)})" for s in secondary), ""]

    if comp.examples:
        out += ["### Uso en ejemplos", "",
                "- **Tipo de validación:** compilación configurada en CI (`compile-examples.yml`); "
                "no ejecutada al generar este sitio.",
                "- **Requiere hardware** para observar el comportamiento.", ""]
        for ex_name in sorted(set(comp.examples)):
            ex = model.examples[ex_name]
            out += [f"**{_example_link(ex_name)}** — {ex.brief or 'sin descripción'}", ""]
            snippet = _usage_lines(ex, comp.name)
            if snippet:
                out += ["```cpp", *snippet, "```", ""]
    else:
        out += ["Ningún ejemplo de `examples/` usa este componente directamente.", ""]

    # ---- Restricciones
    notes = list(comp.sections)
    for m in comp.members:
        if m.prot in ("public", "protected"):
            notes += [(k, f"`{m.name}`: {t}") for k, t in m.sections if k in ("warning", "note", "attention", "pre", "post")]
    notes = [(k, t) for k, t in notes if k in ("warning", "note", "attention", "pre", "post") and t]
    if notes:
        out += ["## Restricciones y advertencias", "",
                "Recogidas de las etiquetas `@note`, `@warning`, `@pre` y `@post` del código.", ""]
        for kind, text in notes:
            out.append(f"- **{SECTION_LABELS.get(kind, kind)}:** {text}")
        out += [""]

    # ---- Evidencia
    out += ["## Validación y evidencia", "", "| Aspecto | Estado |", "|---|---|",
            "| Declaración | Verificado en código (extraído por Doxygen al construir el sitio) |"]
    if primary:
        out.append(f"| Prueba host | Existe la suite `{primary[0].name}`"
                   + (" (incluida en `tests/run_all.sh`)" if primary[0].in_runner else " (**no** incluida en `tests/run_all.sh`)")
                   + "; resultado no registrado aquí |")
    else:
        out.append("| Prueba host | Sin suite dedicada |")
    if comp.examples:
        out.append(f"| Compilación para ESP32 | Indirecta: {len(set(comp.examples))} ejemplo(s) lo usan y "
                   "`compile-examples.yml` los compila para `esp32` y `esp32s3`; no verificado localmente |")
    else:
        out.append("| Compilación para ESP32 | Sin ejemplo que lo use |")
    out.append("| Hardware | No validado por esta documentación |")
    out += ["", HOST_TEST_NOTE, ""]
    return "\n".join(out)


_CLOSURE: set[str] | None = None


def _umbrella_closure() -> set[str]:
    """Cabeceras de src/ alcanzables (transitivamente) desde ModestIoT.h."""
    global _CLOSURE
    if _CLOSURE is None:
        seen: set[str] = set()
        stack = ["ModestIoT.h"]
        while stack:
            name = stack.pop()
            path = SRC_DIR / name
            if name in seen or not path.exists():
                continue
            seen.add(name)
            stack += re.findall(r'^\s*#\s*include\s*"([^"]+)"', path.read_text(encoding="utf-8"), re.M)
        _CLOSURE = seen
    return _CLOSURE


def _usage_lines(ex: Example, name: str, limit: int = 4) -> list[str]:
    stripped = _strip_cpp_comments(ex.code).splitlines()
    original = ex.code.splitlines()
    picked = []
    for i, line in enumerate(stripped):
        if re.search(rf"\b{re.escape(name)}\b", line):
            picked.append(f"/* L{i + 1} */ {original[i].strip()}")
            if len(picked) >= limit:
                break
    return picked


# --------------------------------------------------------------------------------------
# Índice del catálogo
# --------------------------------------------------------------------------------------

def render_components_index(model: Model, intro: str) -> str:
    out = [intro.rstrip(), ""]
    total = sum(1 for c in model.components.values() if c.category != "uncategorized")
    with_tests = sum(1 for c in model.components.values() if model.tests_for(c)[0])
    with_examples = sum(1 for c in model.components.values() if c.examples)
    out += ['<div class="grid cards" markdown>', ""]
    out += [f"-   **{total}** componentes catalogados",
            f"-   **{with_tests}** con suite host dedicada",
            f"-   **{with_examples}** usados en ejemplos",
            f"-   **{len(model.examples)}** sketches en `examples/`", "", "</div>", ""]
    for cat in model.categories:
        comps = model.components_in(cat.id)
        if not comps:
            continue
        out += [f"## {cat.title} {{ #{cat.id} }}", "", cat.description, "",
                f"*Hardware:* {cat.hardware}", "",
                "| Componente | Descripción (del código) | Prueba host | Ejemplos |", "|---|---|---|---|"]
        for c in comps:
            primary, _ = model.tests_for(c)
            test_cell = "Sí" if primary else "—"
            ex_count = len(set(c.examples))
            out.append(f"| [`{c.title or c.name}`]({c.name}.md) | {_escape_cell(_first_sentence(c.brief)) or '—'} | "
                       f"{test_cell} | {ex_count or '—'} |")
        out += [""]
    orphans = model.components_in("uncategorized")
    if orphans:
        out += ["## Sin categoría", ""] + [f"- [`{c.name}`]({c.name}.md)" for c in orphans] + [""]
    return "\n".join(out)


# --------------------------------------------------------------------------------------
# Ejemplos
# --------------------------------------------------------------------------------------

PIN_RE = re.compile(r"^\s*(?:static\s+)?(?:constexpr\s+|const\s+)+(?:u?int\w*|int|uint8_t|gpio_num_t)\s+(\w*PIN\w*)\s*=\s*([^;]+);", re.M)


def render_example(model: Model, ex: Example) -> str:
    out = [f"# {ex.name}", ""]
    if ex.brief:
        out += [f"> {ex.brief}", ""]
    rows = [("Archivo", f"[`{ex.path}`]({source_url(ex.path)})")]
    if ex.components:
        rows.append(("Componentes del framework",
                     ", ".join(_component_link(model, c, from_dir="examples") for c in ex.components)))
    framework_headers = {p.name for p in SRC_DIR.glob("*.h")}
    deps_declared = [f"`{i}` ({DECLARED_LIBRARY_HEADERS[i]})" for i in ex.includes if i in DECLARED_LIBRARY_HEADERS]
    deps_platform = [f"`{i}`" for i in ex.includes
                     if i not in DECLARED_LIBRARY_HEADERS and Path(i).name not in framework_headers]
    deps_framework = [f"`{i}`" for i in ex.includes if Path(i).name in framework_headers]
    if deps_framework:
        rows.append(("Cabeceras del framework", ", ".join(deps_framework)))
    if deps_declared:
        rows.append(("Bibliotecas declaradas", ", ".join(deps_declared)))
    if deps_platform:
        rows.append(("Otras cabeceras (plataforma/sistema)", ", ".join(deps_platform)))
    if ex.doxygen_page:
        rows.append(("Código con enlaces a la API", f"[Página del ejemplo en Doxygen](../api/reference/{ex.doxygen_page}.html)"))
    rows.append(("Compilación", "Configurada en CI (`compile-examples.yml`, placas `esp32` y `esp32s3`); "
                                "no compilado al generar este sitio"))
    rows.append(("Hardware", "Necesario: ESP32 y los periféricos que usa el sketch"))
    out += ["## Ficha", "", "| Campo | Valor |", "|---|---|"]
    out += [f"| {k} | {_escape_cell(v)} |" for k, v in rows]
    out += [""]

    if ex.aspects:
        out += ["## Qué ilustra", "", "Según el comentario de cabecera del sketch (en inglés):", ""]
        out += [f"- {a}" for a in ex.aspects]
        out += [""]

    pins = PIN_RE.findall(ex.code)
    if pins:
        out += ["## Pines declarados en el sketch", "",
                '!!! warning "Valores del ejemplo, no un esquema verificado"',
                "    Son las constantes que define el código. Esta documentación no verifica cableado, "
                "tensiones, resistencias ni compatibilidad eléctrica con tu placa.", "",
                "| Constante | Valor |", "|---|---|"]
        out += [f"| `{name}` | `{_escape_cell(value.strip())}` |" for name, value in pins]
        out += [""]

    lowered = ex.code.lower()
    if any(k in lowered for k in ("ssid", "password", "token", "apikey", "api_key")):
        out += ['!!! danger "Credenciales"',
                "    El sketch contiene marcadores para credenciales (Wi-Fi, contraseñas o tokens). "
                "Sustitúyelos solo en tu copia local y no los publiques.", ""]

    out += ["## Salida", "",
            "- **Salida esperada:** la que producen las llamadas a `Serial` del propio código.",
            "- **Salida observada:** no registrada; este ejemplo no se ejecutó en hardware para esta documentación.", ""]

    out += ["## Código", "",
            "Se incluye el archivo completo, leído del repositorio al construir el sitio.", "",
            f'```cpp title="{ex.path}" linenums="1"', ex.code.rstrip("\n"), "```", ""]
    return "\n".join(out)


def render_examples_index(model: Model, intro: str) -> str:
    out = [intro.rstrip(), "",
           "| Ejemplo | Descripción (del sketch) | Componentes del framework |", "|---|---|---|"]
    for ex in sorted(model.examples.values(), key=lambda e: e.name.lower()):
        comps = ", ".join(_component_link(model, c, from_dir="examples") for c in ex.components[:6])
        if len(ex.components) > 6:
            comps += f" y {len(ex.components) - 6} más"
        out.append(f"| [{ex.name}]({ex.name}.md) | {_escape_cell(ex.brief) or '—'} | {comps or '—'} |")
    out += [""]
    return "\n".join(out)


def render_identifier_registry(model: Model) -> str:
    rows = []
    for comp in model.components.values():
        for m in comp.members:
            if m.prot == "public" and m.is_identifier_constant:
                value = m.initializer.lstrip("= ").strip()
                kind = "Comando" if "COMMAND" in m.name else ("Evento" if "EVENT" in m.name else "Otro")
                try:
                    sort_value = int(value)
                except ValueError:
                    sort_value = 1 << 30
                rows.append((kind, sort_value, value, comp.name, m))
    rows.sort(key=lambda r: (r[0], r[1], r[3]))
    out = ["| Espacio | Valor | Clase | Constante |", "|---|---|---|---|"]
    for kind, _, value, comp_name, m in rows:
        out.append(f"| {kind} | `{value}` | [`{comp_name}`](components/{comp_name}.md) | "
                   f"[`{m.name}`]({api_href_for_refid(m.refid)}) |")
    return "\n".join(out)


# --------------------------------------------------------------------------------------
# Diagramas generados desde el modelo
# --------------------------------------------------------------------------------------

CATEGORY_COLORS = {
    "core": "#dbe9f6", "sensors": "#e3f2e1", "actuators": "#fdebd3", "connectivity": "#efe3f6",
    "gateways": "#f6e3ea", "display-drivers": "#f1f1f1", "services": "#fff7cc", "types": "#ffffff",
}


def class_hierarchy_dot(model: Model) -> str:
    """Grafo de herencia (solo relaciones presentes en el XML de Doxygen)."""
    lines = [
        "digraph Hierarchy {",
        '  graph [rankdir=LR, bgcolor="white", fontname="Helvetica,Arial,sans-serif", nodesep=0.15, ranksep=0.6];',
        '  node [shape=box, style="rounded,filled", fontname="Helvetica,Arial,sans-serif", fontsize=10, color="#555555"];',
        '  edge [arrowhead=empty, color="#666666"];',
    ]
    involved = set()
    for c in model.components.values():
        for b in c.bases:
            if b in model.components:
                involved.update({c.name, b})
    for name in sorted(involved):
        comp = model.components[name]
        color = CATEGORY_COLORS.get(comp.category, "#ffffff")
        lines.append(f'  "{name}" [fillcolor="{color}", URL="../components/{name}/", target="_top"];')
    for c in sorted(model.components.values(), key=lambda x: x.name):
        for b in c.bases:
            if b in model.components:
                lines.append(f'  "{c.name}" -> "{b}";')
    lines.append("}")
    return "\n".join(lines)

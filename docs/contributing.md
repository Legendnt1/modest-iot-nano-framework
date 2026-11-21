# Contribuir

Las normas del proyecto están en los documentos de la raíz del repositorio; esta página
resume dónde encontrar cada una.

| Documento | Contenido |
|---|---|
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Flujo Git Flow, convenciones de nombres, normas de comentarios Doxygen y proceso de *pull request* |
| [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md) | Código de conducta (Contributor Covenant) |
| [SECURITY.md](../SECURITY.md) | Política de seguridad y notificación de vulnerabilidades |
| [ACCESSIBILITY.md](../ACCESSIBILITY.md) | Declaración de accesibilidad y cómo informar de problemas |
| [CHANGELOG.md](../CHANGELOG.md) | Historial de versiones |
| [LICENSE.md](../LICENSE.md) y [NOTICE](../NOTICE) | Licencia Apache 2.0 y atribuciones |

## Antes de abrir un *pull request*

1. **Pruebas host:** `./tests/run_all.sh` (requiere `g++` con soporte C++17; activa
   AddressSanitizer y UBSan por defecto, desactivables con `ENABLE_SANITIZERS=0`).
2. **Ejemplos:** la CI los compila con `arduino-cli` para `esp32` y `esp32s3`
   (`.github/workflows/compile-examples.yml`).
3. **Lint de biblioteca:** `arduino-lint` en modo estricto (`.github/workflows/lint.yml`).
4. **Documentación:** `python tools/docs/build_docs.py` (ver
   [Generar y validar la documentación](development.md)).

Para añadir un componente nuevo, sigue [Extender el framework](extending.md#si-contribuyes-un-componente-al-framework).

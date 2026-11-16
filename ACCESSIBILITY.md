# Accessibility Statement

## Our Commitment

The Modest IoT Nano Framework is committed to making the project accessible to all developers, regardless of ability. We aim to ensure that our documentation, code, and community spaces are usable by people with disabilities, including those who use screen readers, keyboard-only navigation, or other assistive technologies.

As an embedded IoT library, the framework itself runs on ESP32 microcontrollers and does not have a user interface. However, the project's documentation, codebase, and community participation channels are designed to be accessible.

## Documentation Accessibility

Our documentation follows these accessibility practices:

- **Heading structure**: All Markdown documents use a logical heading hierarchy (`#`, `##`, `###`) for screen reader navigation.
- **Alt text**: Images and diagrams include descriptive alt text where applicable.
- **Code examples**: All code snippets are properly formatted with language identifiers for syntax highlighting and screen reader compatibility.
- **Plain language**: We write documentation in clear, concise English, avoiding unnecessary jargon.
- **Color independence**: No information is conveyed by color alone in our documentation.

## Code Accessibility

Our code follows these accessibility-conscious practices:

- **Clear naming**: All public APIs use descriptive, self-documenting names (e.g., `getBatteryVoltageInMillivolts()` rather than `getV()`).
- **Doxygen comments**: Every public method has KDoc/Doxygen documentation with `@brief`, `@param`, `@return`, and `@note` tags, making the API navigable via IDE autocomplete and documentation generators.
- **Consistent conventions**: The project follows strict naming conventions (kebab-case for files, PascalCase for classes, camelCase for methods) so developers can predict and navigate the codebase.
- **No magic numbers**: All constants are named with descriptive suffixes, eliminating the need to decode numeric literals.

## Community Participation

- **Code of Conduct**: We follow the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md), which explicitly includes "visible or invisible disability" in our non-discrimination pledge.
- **Issue templates**: Our [bug report](.github/ISSUE_TEMPLATE/bug-report.md) and [feature request](.github/ISSUE_TEMPLATE/feature-request.md) templates are structured forms that are keyboard-navigable and screen-reader friendly.
- **Pull request template**: Our [PR template](.github/pull-request-template.md) includes a checklist that ensures contributions meet quality standards, including documentation requirements.

## Known Limitations

- The project has not undergone formal keyboard-only or screen-reader testing of its documentation.
- The PlantUML class diagram (`docs/class-diagram.puml`) is a text-based diagram format that requires rendering; the rendered image may not be fully accessible without alt text.
- The Doxygen-generated API reference is auto-generated and has not been reviewed for accessibility compliance.

## Reporting Accessibility Issues

If you encounter an accessibility barrier in our documentation, code, or community spaces, please let us know:

1. **Open an Accessibility Report** by navigating to the [Issues tab](https://github.com/modest-iot/modest-iot-nano-framework/issues/new/choose) of the repository and selecting "Accessibility Report".
2. **Include the following information**:
   - A description of the barrier you encountered
   - The affected page, file, or section
   - Your operating system and assistive technology (e.g., VoiceOver, NVDA, JAWS)
   - Steps to reproduce the issue, if applicable
   - Screenshots or recordings (optional)

We will address accessibility issues promptly and coordinate with you on the resolution.

## References

- [W3C Web Content Accessibility Guidelines (WCAG) 2.2](https://www.w3.org/WAI/standards-guidelines/wcag/)
- [W3C Developing an Accessibility Statement](https://www.w3.org/WAI/planning/statements/)
- [Open Source Guides: Accessibility Best Practices](https://opensource.guide/accessibility-best-practices-for-your-project/)
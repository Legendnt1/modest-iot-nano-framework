# Pull Request

## Description

A clear and concise description of what this pull request does.

## Type of Change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## Related Issue

Fixes #(issue number) or references #(issue number).

## Testing

- [ ] All tests pass: `bash tests/run_all.sh`
- [ ] Doxygen generates without warnings: `doxygen docs/Doxyfile`
- [ ] New tests added for new functionality
- [ ] Existing tests still pass

## Checklist

- [ ] Code follows [Framework Philosophy](CONTRIBUTING.md#framework-philosophy) (mediator pattern, CQRS, loop-less)
- [ ] Naming conventions are consistent ([Naming Conventions](CONTRIBUTING.md#naming-conventions))
- [ ] All public APIs have Doxygen comments with `@runs_on{task}` tags
- [ ] No magic numbers — all constants are named with descriptive suffixes
- [ ] CHANGELOG.md is updated under the appropriate section
- [ ] New components are added to `ModestIoT.h` umbrella header
- [ ] New components have entries in `keywords.txt`
- [ ] Documentation uses proper heading hierarchy and alt text for images ([Accessibility](ACCESSIBILITY.md))

## Branch Information

- **Source branch**: `feature/...` or `bugfix/...`
- **Target branch**: `develop`

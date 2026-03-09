# Semgrep Verification Inputs

This directory holds tracked Semgrep verification inputs for `static-check-add-semgrep`.

- `fixtures/` stores positive, negative, and exception examples using repo-style `src/` and `tests/` paths.
- `verify_semgrep_fixtures.py` copies those fixtures into a temporary workspace and runs the checked-in Semgrep rules against that mirrored tree.
- Mandatory no-fallback static checks for this change remain `pixi run semgrep` and `pixi run build -p quality`.
- `pixi run semgrep` now runs both the repository scan and the fixture verifier.

Run the fixture verifier with:

```bash
pixi run python tests/semgrep/verify_semgrep_fixtures.py
```

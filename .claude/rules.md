# Coding Rules for LiquidGov.US

- Follow naming.toml patterns strictly for new files (tests, deploy scripts, branches).
- New C++ tests go in new files; do not modify frozen committed tests.
- Never hardcode secrets; use .env.local.example patterns.
- Prefer modular additions under liquidgov-platform/libs/ or src/site/.
- Keep assets URLs stable for frozen icons/manifests.
- Update Persistence/notes.md with dated decisions for architecture or product changes.
- Run naming checks and relevant tests before deploying.
- Use C++17, clear includes, and link only needed libraries.
- For deploy scripts: follow the restart_allowed list; do not invent new restart units without update.
- Exclude large media and build artifacts from AI context via .claudeignore.

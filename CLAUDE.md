# LiquidGov.US

Civic platform for liquid democracy with natural law supremacy, pledge contracts for candidate accountability, maps, live rooms, community features, and more. Built as a custom C++ HTTP server with modular feature libraries and a JS/CSS frontend.

## Stack
- Language: C++17
- Build: CMake 3.16+
- Server: custom liquidgov_http (src/http_server.cpp + modular src/site/)
- Storage: custom file store (.lgp)
- Frontend: vanilla JS, CSS, HTML assets (liquidgov-platform/assets/)
- Testing: C++ unit tests (tests/*_test.cpp), shell e2e (tests/e2e/*.sh), Python helpers
- Deploy: shell scripts in deploy/ and liquidgov-platform/deploy/
- CI: GitHub Actions (.github/workflows/)

## Directory Structure
- `liquidgov-platform/` — main platform source, libs/, src/, assets/, tests/, CMakeLists.txt
- `liquidgov-core/` — core components (if separate)
- `commons/` / `commons-daemon/` — related commons/game components
- `deploy/` — deployment and remote restart scripts
- `scripts/` — build, check, and utility scripts
- `Persistence/` — shared notes, decisions, and Claude context files (notes.md, TheCommons/)
- `docs/` — documentation and archive
- `.github/workflows/` — CI pipelines
- `.githooks/` — local git hooks

## Commands
- Build: `cd liquidgov-platform && cmake -B build && cmake --build build`
- Run tests: build the *_test binaries and run them; e2e via bash tests/e2e/*.sh
- Deploy: individual feature scripts like `deploy/city-remote.sh` (see naming.toml for allowed restarts)
- Naming check: scripts related to naming.toml
- Pre-commit / hooks: .githooks/

## Conventions
- C++: modern C++17, clear modules under libs/ and src/site/
- Naming: enforced by naming.toml (e2e shells, test cpp, deploy scripts, branches)
- Tests: new tests in new files; committed tests stay frozen
- Assets: icons, manifests, media in liquidgov-platform/assets/ (some frozen URLs)
- Secrets: never commit .env.local or real stores (*.lgp)
- Persistence: update notes.md with dated decisions

## Important
- Never commit secrets, real account stores, or build outputs
- Follow naming.toml patterns for new files
- Run relevant tests and naming checks before deploy
- CLAUDE.md and .claude/ provide the map for AI-assisted development
- Persistence/ holds long-term decision history — consult before major changes

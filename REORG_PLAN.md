# LiquidGov.US Bottom-Up Reorganization Plan

Goal: Restructure the codebase for clarity, AI-assisted development (Claude Code / similar), better testing, and deployment organization **without changing runtime looks or functionality**.

## Phase 1: Claude Context Layer (DONE on this branch)
- Added root `CLAUDE.md` with project map, stack, commands, conventions.
- Added `.claude/` with `settings.json` (permissions deny secrets/stores), `context.md`, `rules.md`, `prompts.md`.
- Added `.claudeignore` to exclude build outputs, large media, runtime data.
- This gives AI a clear map immediately. No code moves yet.

## Phase 2: Directory Clarity (Proposed, non-breaking first)
Current top-level is functional but scattered for AI navigation. Proposed target (migrate gradually):

```
liquidgov-us/
├── CLAUDE.md
├── .claude/
├── .claudeignore
├── .gitignore
├── apps/
│   └── platform/          # rename/move of liquidgov-platform/
├── packages/
│   └── core/              # liquidgov-core if applicable
├── commons/               # keep or nest
├── deploy/
├── scripts/
├── tests/                # optional top-level consolidation later
├── Persistence/
├── docs/
└── .github/
```

**Migration steps (do one at a time, verify CI/deploy after each):**
1. Create `apps/` and move `liquidgov-platform/` → `apps/platform/` (update all relative paths in CMake, scripts, CI, deploy).
2. Update `.github/workflows/*.yml` paths.
3. Update deploy scripts and naming.toml roots if needed.
4. Update Persistence references and CLAUDE.md directory map.
5. Keep old paths as temporary symlinks if desired during transition (not required).

## Phase 3: Testing Organization
- Keep existing tests co-located or under `apps/platform/tests/` initially.
- Consider a top-level `tests/` runner script that invokes platform + commons tests.
- Ensure e2e scripts remain functional with updated binary paths.
- Add or refine CI to run the full suite on the new structure.

## Phase 4: Deployment Organization
- Group deploy scripts more clearly (e.g., `deploy/features/` or keep flat with naming.toml enforcement).
- Document the allowed restart units and prebuilt flow in CLAUDE.md / docs/.
- Standardize remote script naming and make the restart_allowed list the single source of truth.

## Phase 5: Skills and Further AI Alignment
- Add project-specific skills under `.claude/skills/` for common tasks (new feature, deploy review, store migration, etc.).
- Refine rules.md and prompts.md based on frequent AI corrections.
- Optionally nest CLAUDE.md in major subdirs (apps/platform/CLAUDE.md) for deeper context.

## Principles
- Every move is accompanied by path updates so builds, tests, and deploys continue to work.
- No functional or visual changes in this reorganization pass.
- CLAUDE.md and .claude/ stay the living map — update them as structure changes.
- Consult Persistence/notes.md for historical decisions before large moves.

This plan implements the ideal structures from the referenced articles (CLAUDE.md + .claude/ + ignore + clear shallow hierarchy) while respecting the existing modular C++ + assets + deploy-script architecture.

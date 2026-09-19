# Makefile practices — not a BUILD

The daemon already builds with CMake (`commons-daemon/CMakeLists.txt`).
This note is so a Makefile does not get invented next to it.

## For this repo

Do **not** add a Makefile that compiles `commons_daemon`. A second graph
would fight:

* FetchContent + SHA256 pin for the sqlite amalgamation
* `commons_build_info` (runs on every `cmake --build`, not at configure)
* CI (`cmake -S . -B build` then `cmake --build`)
* the droplet (`-j1`, no git clone, `.deployed-rev` is the revision)

If someone wants a shorter local command, a five-line wrapper that only
calls cmake is fine. It must not list `.cpp` files. The source list lives
in CMakeLists.txt once.

```make
.PHONY: daemon test
daemon:
	cmake -S commons-daemon -B commons-daemon/build -DCMAKE_BUILD_TYPE=Release
	cmake --build commons-daemon/build -j2
test: daemon
	bash commons-daemon/tests/run_all.sh
```

Use `$(MAKE)` if that wrapper ever calls make. Do not use `make` inside
the recipe (jobserver and `-j` break). Do not default `-j` to unbounded.
The droplet stays `-j1` (512 MB).

## If you ever write a real Makefile (not here)

1. Targets are files. `.PHONY` only for names that are not files (`all`,
   `clean`, `test`). A phony as a prerequisite of a real file rebuilds
   that file every time.
2. First target is the default. Set `.DEFAULT_GOAL` if the file order is
   not obvious.
3. Immediate assignment (`:=`) for `$(wildcard)` / `$(shell)`. Recursive
   `=` re-runs the shell on every expansion.
4. Directories are order-only: `foo.o: foo.c | build/` so a new `build/`
   timestamp does not rebuild everything.
5. Automatic vars: `$@` target, `$<` first prereq, `$^` all prereqs. Do
   not hard-code the output name twice.
6. One recipe line, or `.ONESHELL`, or `&&` — make runs each line in a
   new shell. `cd dir` on its own line does nothing to the next.
7. Parallel: declare real prerequisites; do not sprinkle `.NOTPARALLEL`
   to hide missing deps. Pattern rules must not write the same file from
   two recipes.
8. `make -n` must be readable. Recipes that always `touch` a stamp to
   fake success are a lie.

None of that replaces CMake for this daemon. Client is still static files.
No code change.

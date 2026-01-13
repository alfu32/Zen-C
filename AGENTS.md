# Repository Guidelines

## Project Structure & Module Organization
- `src/` holds the Zen C compiler and tooling sources (parser, lexer, codegen, LSP).
- `std/` is the Zen C standard library; `std.zc` is the default entry for std imports.
- `plugins/` contains compiler plugins (e.g., `regex.c`, `sql.c`).
- `examples/` provides sample `.zc` programs.
- `tests/` is reserved for `.zc` test files and includes `run_tests.sh`.
- `man/` contains the `zc` man page; `obj/` is build output.

## Build, Test, and Development Commands
- `make` builds the `zc` compiler (default gcc).
- `make CC=clang` or `make zig` builds with clang or `zig cc`.
- `make test` runs `./tests/run_tests.sh` (expects `.zc` tests in `tests/`).
- `make clean` removes `obj/`, `zc`, and generated `out.c`.
- `make install` / `make uninstall` install or remove `zc` from `/usr/local`.
- `./zc run path/to/file.zc` compiles and runs a Zen C program.

## Coding Style & Naming Conventions
- C sources use 4-space indentation and braces on their own line.
- Prefer `snake_case` for functions/variables and `SCREAMING_SNAKE_CASE` for macros.
- Keep new files consistent with current layout: `*.c` in `src/`, `*.zc` in `std/` or `tests/`.
- No auto-formatter is configured; match surrounding style.

## Testing Guidelines
- Tests are Zen C programs in `tests/` (suggested pattern: `test_*.zc`).
- Run the suite via `make test` or `./tests/run_tests.sh --cc clang`.
- Add a focused `.zc` test for new language or compiler behavior.

## Commit & Pull Request Guidelines
- Recent history uses short, imperative summaries (e.g., `Fix for #29`, `feat: add set type`).
- Include issue references when applicable and keep subject lines concise.
- PRs should describe behavior changes, list tests run, and attach minimal repros or snippets when useful.

## Configuration Tips
- Set `ZC_ROOT` to the repo path to enable std imports from any directory:
  `export ZC_ROOT=/path/to/Zen-C`.

# AGENTS.md (root)

Cross-cutting instructions for agents working anywhere in the
low-latency-event-processing-engine (Shot-Caller-WoW) repository. If a component
subfolder later gets its own `AGENTS.md`, that file owns stack-specific rules;
this file owns what applies everywhere.

## Project overview

- Pipeline: WoW combat log → C++20 engine (parse, identify, predict, assign) →
  newline-framed JSON over TCP 9999 → Python Discord bot → ElevenLabs TTS → voice.
- C++ lives in `include/` (headers, static game data), `src/` (parser, engine,
  line reader/writer, socket sender), and `tests/` (GoogleTest). The testable
  core is the `ShotCallerLib` target; keep it free of file, socket, and thread I/O.
- Python lives in `discord_bot/` (socket listener, TTS playback, Discord commands).
- `include/constants.h` is static game knowledge. Changing it changes runtime
  behavior and is covered by `tests/test_constants.cpp`; update tests with it.
- Component ownership: add owners/paths here as they are established.

## Implementation workflow

1. Inspect `git status`, the relevant code, and the existing tests before editing.
2. Make a short checklist for substantial work, then implement a runnable increment.
3. Preserve public interfaces — callback signatures, TCP message shape, CMake
   targets — unless the task changes them. Report conflicts rather than silently
   redesigning shared surfaces.
4. Build, run focused tests, inspect the diff, and fix relevant failures.
5. Run the full verification suite (below), then report what changed, what was
   actually verified, and what remains unverified.

The C++ engine and every C++ test must build and run without the Discord bot,
ElevenLabs credentials, or network access. Keep them offline and deterministic.
Continue independent work when credentials or a teammate's component are missing;
resolve routine implementation choices without repeatedly asking for confirmation.

## Build, test, lint, sanitizers

Linux-first (the socket code uses POSIX headers). Commands:

```bash
# Configure and build
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"

# Tests
ctest --test-dir build --output-on-failure

# ASan + UBSan, warnings as errors
cmake -B build-san -S . -DSHOTCALLER_ENABLE_SANITIZERS=ON -DSHOTCALLER_WARNINGS_AS_ERRORS=ON
cmake --build build-san -j"$(nproc)"
ctest --test-dir build-san --output-on-failure

# Formatting: fix touched files, then check
clang-format -i <changed files>
clang-format --dry-run --Werror <changed files>

# Python bot
python3 -m venv .venv && . .venv/bin/activate
pip install -r discord_bot/requirements-dev.txt
python -m pytest discord_bot/tests
```

- Use separate build directories (`build-a/`, `build-b/`) for concurrent builds.

## CI: what "green" means

Every change must pass CI, or the exact local equivalent when CI is unavailable.
CI is expected to run on Linux and cover:

1. Configure + build with warnings as errors (GCC, and Clang where practical).
2. `ctest --output-on-failure`.
3. ASan + UBSan build and tests.
4. `clang-format --dry-run --Werror` over `src/`, `include/`, and `tests/`.
5. Python: install dev requirements and run `pytest`.

Rules:

- Never report a task complete with failing tests, sanitizer errors, new warnings,
  or formatting violations. Fix them or state precisely why they cannot be fixed.
- Do not silence warnings, disable sanitizers, relax assertions, or skip tests to
  make CI pass.
- Keep diagnostics from `clang-tidy` clean where it is configured; do not introduce
  new findings.
- For changes that touch threading, locking, or the scheduler (`process_shotcalls`,
  `dispatch_due`, `mtx_`), additionally run a ThreadSanitizer build locally:
  `cmake -B build-tsan -S . -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"` then
  `cmake --build build-tsan -j"$(nproc)" && ctest --test-dir build-tsan --output-on-failure`.
  TSan and ASan are separate runs; never combine them.
- Keep the build reproducible: no new system packages or external services beyond
  what CI installs.

## Code Style

### Language & dependencies

- C++20 or later. Standard library only; no new third-party C++ dependencies.

### Initialization

- Prefer copy initialization with `=` over brace initialization `{}`.
  - Use `int x = 0;` not `int x{0};`
  - Use `Node* p = head;` not `Node* p{head};`
  - Use `auto total = compute();` not `auto total{compute()};`
- Prefer brace intialization `{}` over copy constructor initialization `()`
- If default initialization is needed, prefer `T x{};` over `T x = {};`
- Use `{}` only when explicitly necessary:
  1. Aggregate/array initialization: `int a[] = {1, 2, 3};`
  2. Disambiguating the most vexing parse: `Widget w{};` or `Widget w{arg};`
  3. Intentionally invoking an `initializer_list` constructor: `std::vector<int> v = {1, 2, 3};`
  4. Required by an `explicit` constructor in return/argument context: `return {x, y};`
  5. Value-initialization where `=` would be ambiguous: `T t{};`
- Never use `{}` purely as a stylistic default.
- Do not convert existing `=` initialization to `{}` in refactors.

### Ownership & safety

- RAII everywhere; no owning raw pointers, no manual `new`/`delete`.
- `const` correctness throughout.
- No C-style casts; use `static_cast`, `std::bit_cast`, etc.
- No `using namespace` in headers.
- Prefer `std::optional`, `std::string_view`, `std::span`, and `std::chrono`
  types at interfaces.
- Do not copy immutable global data; reference it through pointers, spans, or views.

### API design & concurrency

- Public APIs stay minimal. Handlers that assume a lock is already held must be
  private; only synchronized entry points are public.
- Keep engine callbacks (file/socket I/O) outside `mtx_`; never perform I/O or
  blocking waits while holding the engine lock.

### Formatting

- 4-space indentation; `.clang-format` (WebKit-based) is the source of truth.

### Comments

- One concise doc comment per function/type explaining what it does and any
  non-obvious constraint (for example "caller must hold `mtx_`").
- No comments that restate code, no commented-out code, no TODOs.
- Inline comments only for non-obvious rationale: bit flags, protocol formats,
  timing models, concurrency boundaries, and game-data semantics.

### Python

- `discord_bot/` targets Python 3.10+.
- Never block the event loop: run TTS and other blocking SDK work via
  `asyncio.to_thread`; serialize playback through one queue.
- Validate environment variables at startup with actionable messages, never with an
  import-time `KeyError`.
- Protocol changes are cross-component: update the C++ sender, `discord_bot/protocol.py`,
  and both test suites in the same change.
- Pin dependencies in `discord_bot/requirements*.txt`; no floating versions.
- Manage temporary audio files with `tempfile` and delete them in `finally`.

### Tests

- GoogleTest for C++, deterministic and offline. Never use real sleeps; expose or
  accept a "now" value so tests advance a fake clock.
- Prefer literal lines captured from real combat logs (inline literals or
  `sample_logs/combat_log_sample.txt`). If a synthetic line is unavoidable, the test
  name or comment must say why.
- Python tests use `pytest` and must run without Discord or ElevenLabs credentials.
- A bug fix starts with a regression test that reproduces the bug before the fix.

## Repository boundaries and handoff

- Never commit or push secrets: `ELEVENLABS_API_KEY`, `DISCORD_BOT_TOKEN`, `.env`
  files, tokens, or captured credentials. The bot reads them from the environment.
- Never commit generated artifacts: `build*/`, generated `compile_commands.json`,
  `__pycache__/`, `.venv/`, `_deps/`, `output/*.txt`, or `tts_*.mp3`.
- `sample_logs/combat_log_large.txt` is huge. Do not add large logs to git; use the
  small sample/trimmed fixtures and literal test lines.
- Preserve user and teammate changes. Do not switch a shared checkout's branch while
  another agent is working. Commit, push, and open PRs only when the task authorizes it.
- One writer per file. The primary agent owns shared surfaces — `include/*.h`,
  `CMakeLists.txt`, callback/protocol signatures, and test-target wiring — unless
  explicitly reassigned.
- Handoff must state: what changed, the exact commands and results, and which
  behavior remains unverified.

## Commit/PR hygiene

- Commits and PRs must read as ordinary human engineering work: imperative subject,
  what changed, why, and the tests run.

## Parallel agents

- Delegate independent substantial work when subagent tools are available: default
  to two helpers; up to four only with clearly separate responsibilities.
- Give each helper a goal, owned files, interface constraints, and acceptance
  checks. Pass the relevant instructions; helpers do not share this conversation.
- One writer per file. Use separate build directories for concurrent builds. The
  primary reviews helpers' changes and builds/tests the integrated result.
- Good splits: parser vs engine, C++ engine vs Python bot, implementation vs
  fixtures/tests. Do not split a single header or a single protocol change.
- If delegation is unavailable, continue sequentially and report that in the handoff.

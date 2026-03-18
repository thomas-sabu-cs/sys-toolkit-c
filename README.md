## sys-toolkit-c

Minimal, interview-ready C17 systems programming toolkit showcasing manual memory management, structured concurrency, and practical logging under strict compilation flags.

### Architecture

```
          +-------------------+
          |   sys-toolkit-c   |
          +----------+--------+
                     |
     +---------------+---------------------+
     |                                     |
+----v-----+                       +-------v-------+
|  Library |                       |    Tests      |
| (src/)   |                       |  (tests/)     |
+----+-----+                       +-------+-------+
     |                                     |
     |                                     |
     |                                     |
     |        +----------------+           |
     |        | Benchmarks     |           |
     |        | (benchmarks/)  |           |
     |        +----------------+           |
     |                                     |
     |                                     |
+----v------------------------------+      |
|  Public Headers (include/)        |      |
|  - sys_arena.h                    |      |
|  - sys_threadpool.h               |      |
|  - sys_log.h                      |      |
+-----------------------------------+------+
```

### Design Philosophy

- **C17, zero external deps**: Only the C standard library and pthreads, compiled with `-Wall -Wextra -Werror -pedantic -fstack-protector-all`.
- **Auditability over features**: Each module is intentionally small so it can be read and reasoned about in a single sitting.
- **Predictable failure modes**: All APIs return explicit error codes instead of hiding failures behind global state.
- **Interview friendly**: Modules are deliberately chosen to map to common systems design questions: arenas, thread pools, and logging.

### Modules

- **Memory (`sys_arena`)**:
  - Linear bump allocator with explicit capacity and `sys_arena_reset` for phase-based reuse.
  - No hidden growth (`realloc` is never used).
  - Alignment at `alignof(max_align_t)` where possible.

- **Concurrency (`sys_threadpool`)**:
  - Pthread-based worker pool with a bounded queue and natural back-pressure.
  - Cooperative, graceful shutdown that drains queued work.
  - Suitable as a reference implementation for "build a thread pool" interview prompts.

- **I/O (`sys_log`)**:
  - Thread-safe logging with three levels: `ERR`, `INFO`, `DEBUG`.
  - Writes to both `stdout` and a rolling file with a simple size cap.
  - Pure C, no syslog or platform-specific extensions.

### Build Instructions

#### Prerequisites

- CMake >= 3.20
- C17-capable compiler (e.g., `clang`, `gcc`)
- `pthread` (POSIX threads)

#### Configure and Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -- -j$(nproc)
```

The default compilation flags include:

```bash
-Wall -Wextra -Werror -pedantic -fstack-protector-all
```

#### Sanitizer Builds

Two additional build types are wired for sanitizers:

- **ASan**: AddressSanitizer
- **UBSan**: UndefinedBehaviorSanitizer

Example:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=ASAN
cmake --build build-asan --config ASAN -- -j$(nproc)
```

> Note: Sanitizer support depends on your compiler and platform.

#### Tests

```bash
ctest --test-dir build --output-on-failure
```

The main test entry point is `tests/test_main.c`, which exercises:

- `sys_arena`
- `sys_threadpool`
- `sys_log`

#### Benchmarks

A simple micro-benchmark for the arena allocator is provided:

```bash
./build/sys_toolkit_benchmarks
```

### Development Workflow

- **Formatting**: If `clang-format` is available on your PATH, the CMake build will run it automatically on all C sources and headers via the `format` target, which is wired as a dependency of the main targets.
- **CI**: GitHub Actions (`.github/workflows/ci.yml`) builds and runs the test suite on every push and pull request using Clang on `ubuntu-latest`.

### Extensibility Notes

- **Memory**: Additional allocators (free-lists, pooling, region hierarchies) can be layered on top of `sys_arena`.
- **Concurrency**: Timer queues, work stealing, and IO integration can build on top of `sys_threadpool`.
- **Logging**: Structured logging and per-module categories can be added while reusing the existing core.


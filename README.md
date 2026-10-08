# Resource-Aware Local Work Executor

> **Status:** Work in progress. The current implementation is a **fixed-concurrency Linux process executor**. Memory-aware admission and resource-based scheduling are planned, not implemented.

## Overview

This C++17 command-line program runs independent local processes specified in a JSON manifest. It limits the number of launched child processes that have not yet been reaped, while handling process launch, termination, and selected failure paths.

The current project is a systems-programming development baseline, **not** a production-ready job manager or a demonstrated memory-optimization solution.

## Implemented behavior

- Validates the entire JSON manifest before launching jobs. The top-level value must be an array of job objects; an empty array is valid.
- Requires an `executable` string. `arguments` is an optional array of strings. `id` is an optional, non-empty, unique string; omitted IDs are not generated automatically.
- Starts each job using `fork()` and `execv()` and tracks child processes by PID.
- Uses `--concurrency <N>` to bound the number of children launched but not yet reaped; `N` must be a positive decimal integer.
- Reaps a child when all available concurrency slots are occupied, before launching another job into the freed slot.
- Reports individual job launch and termination outcomes. A child program's non-zero exit or failed `execv()` does not, by itself, stop the remaining independent jobs or cause the executor process to exit unsuccessfully.
- On scheduler-level failures, attempts bounded cleanup of **tracked direct child processes** using `SIGTERM` and, if needed, `SIGKILL`.

## Requirements

- Linux with a C++17-capable compiler and build tool
- CMake 3.24 or later
- Network access for an initial default CMake configure, which fetches the pinned `nlohmann/json` v3.12.0 archive and checks its SHA-256 hash (a previously populated CMake dependency cache may avoid another download)
- For the process integration tests: `/bin/sh` and Linux utilities including `setsid`, `pgrep`, `ps`, and `timeout`

The documented workflow targets Linux; builds and integration tests on other operating systems have not been validated.

## Build and run

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
```

Example manifest (`jobs.json`):

```json
[
  {
    "id": "greeting",
    "executable": "/bin/echo",
    "arguments": ["hello", "world"]
  },
  {
    "executable": "/bin/echo",
    "arguments": ["another", "job"]
  }
]
```

Run with an upper bound of two unreaped child processes:

```sh
./build/work_executor jobs.json --concurrency 2
```

Or use an existing fixture without creating `jobs.json`:

```sh
./build/work_executor tests/fixtures/multiple_valid_jobs.json --concurrency 2
```

**Execution details:** The `executable` value is passed to `execv()` as a path. It is not resolved through the shell or searched via `PATH`; use a valid absolute or relative executable path. The `arguments` are passed directly, and child standard output/error are inherited rather than captured. Output from concurrent children may interleave. The optional manifest `id` is validated, but current terminal job reports identify jobs by their **0-based manifest index**, not by this ID.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

Tests cover CLI/manifest validation, fixed-concurrency scheduling and slot reuse, real child-process execution, and selected scheduler-failure cleanup paths. Some integration tests execute actual Linux processes rather than using only mocks.

**Known test gap:** Some negative CTest cases use `WILL_FAIL` without asserting the intended error message, so they can pass for the wrong failure reason. A passing suite does not mean the test coverage is exhaustive or that the project's first technical milestone is accepted.

## Design boundaries and limitations

- Jobs are independent; there are no job dependencies, retries, fail-fast job policy, cancellation workflow, or persistent resume state.
- No memory-based admission decisions, runtime resource-pressure feedback, or validated performance-improvement claim exist yet.
- The executor reports per-job outcomes to the terminal but does not yet produce a final aggregated summary or machine-readable results.
- The executor exit status distinguishes executor-level failures from job-level outcomes: an `execv()` failure or a job's non-zero/signal termination does not automatically produce an unsuccessful executor exit status.
- The scheduler assumes it owns child reaping inside the standalone executor process. It is not intended for embedding in a process with unrelated children.
- Cleanup is scoped to tracked direct child PIDs; it is **not** a general guarantee about descendant process trees or an external cancellation mechanism.
- The close-on-exec launch-error pipe has a known ambiguity if a child is terminated asynchronously after `execv()` fails but before it writes an error report.
- This project does not sandbox launched executables or provide production-grade job isolation.

## Planned work

- Improve negative-test diagnostic assertions, add a final terminal summary and machine-readable results, and complete the fixed-concurrency milestone review.
- Investigate **one** memory-resource signal and **one** memory-aware admission policy, then evaluate them against this baseline using a documented, reproducible comparison. This work has not been completed.

## License and third-party dependency

This project's own source code is licensed under the [MIT License](LICENSE).

The build fetches [nlohmann/json v3.12.0](https://github.com/nlohmann/json/tree/v3.12.0), which is independently licensed under the MIT License; its upstream [copyright and license notice](https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT) remain applicable. The dependency is not authored or relicensed by this project.

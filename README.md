# Minimal OJ

Minimal OJ is a small internal online judge for trusted participants. It is intentionally simple: no account system, no sandbox/container, no complex checker, and no interactive problems.

## Stack

- C++20
- Drogon, coroutine handlers only where practical
- SQLite3
- `g++` from MSYS2 UCRT64

## Problem Workflow

Each problem is represented by:

- `config.json`: test count, time limit, memory limit
- `statement.md`: problem statement
- `gentest.cpp`: generates one test. The judge passes the test index as `argv[1]`.
- `gen_answer_from_test.cpp`: reads a generated test from stdin and writes the expected answer.

Refreshing a problem recompiles the generators and recreates all testcases. Student submissions are compiled and run against the generated tests. Output is compared by token sequence.

## Quick Start

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\minimal_oj.exe

## Problem Workflow

Each problem is represented by:

- `config.json`: test count, time limit, memory limit
- `statement.md`: problem statement
- `gentest.cpp`: generates one test. The judge passes the test index as `argv[1]` and the total number of tests as `argv[2]`.
- `gen_answer_from_test.cpp`: reads a generated test from stdin and writes the expected answer.

### Using `argv` in `gentest.cpp`

You can use the passed arguments to generate different types of tests or implement subtasks:
```

Open <http://localhost:3335>.

If Drogon is installed through MSYS2 UCRT64, run the commands inside the UCRT64 shell or make sure the CMake package paths are visible.

## Data Layout

Runtime data is stored under `data/` by default:

```text
data/
  oj.sqlite3
  problems/
    sum-a-b/
      config.json
      statement.md
      gentest.cpp
      gen_answer_from_test.cpp
      tests/
        001.in
        001.ans
```

## API Sketch

- `GET /api/health`
- `GET /api/problems`
- `POST /api/problems`
- `GET /api/problems/{slug}`
- `PUT /api/problems/{slug}`
- `DELETE /api/problems/{slug}`
- `POST /api/problems/{slug}/refresh`
- `POST /api/problems/{slug}/submit`
- `GET /api/submissions/recent?limit=25`

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for boundaries and contracts.

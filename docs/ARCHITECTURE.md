# Minimal OJ Architecture

## Boundary Map

```text
HTTP/UI
  |
  v
Server routes
  |
  v
ProblemService ---- ProblemRepository ---- SQLite
  |
  v
Problem files on disk
  |
  v
JudgeService ---- g++ and trusted executables
```

## Contracts

Public C++ contracts live in `include/moj/*.hpp`.

- `Contracts.hpp`: DTOs shared across boundaries.
- `ProblemRepository.hpp`: SQLite metadata boundary.
- `ProblemService.hpp`: problem CRUD and file persistence boundary.
- `JudgeService.hpp`: compile, testcase refresh, submission execution, token comparison.

## Problem Contract

A problem has a stable `slug`. The slug is used in URLs and as the problem directory name.

`config.json` currently supports:

```json
{
  "test_count": 20,
  "time_limit_ms": 1000,
  "memory_limit_mb": 256
}
```

`gentest.cpp` contract:

```cpp
int main(int argc, char** argv) {
    int testIndex = argc > 1 ? std::atoi(argv[1]) : 1;
    // write test input to stdout
}
```

`gen_answer_from_test.cpp` contract:

```cpp
int main() {
    // read one generated test from stdin
    // write expected answer to stdout
}
```

## Submission Contract

`POST /api/problems/{slug}/submit` accepts JSON:

```json
{
  "participant": "team-a",
  "source_code": "#include <bits/stdc++.h>\nint main(){...}"
}
```

The judge compiles the source with `g++ -std=c++20 -O2`, runs it against all generated tests, and token-compares stdout with `.ans` files.

## Trust Model

This system assumes internal, trusted participants. Submitted code and generator code are executed directly on the server machine. This is convenient for teaching, but not appropriate for untrusted public submissions.

## Extension Points

- Add auth at the HTTP boundary without changing judge internals.
- Add custom checkers by extending `JudgeService` and the problem config.
- Add a sandbox/container by replacing the process execution implementation.
- Add richer result history by adding a `submissions` table and keeping run artifacts.

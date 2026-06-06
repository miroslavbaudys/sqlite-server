# Contributing to sqlite-server

Thanks for your interest in improving `sqlite-server`! This document explains how
to build the project, the conventions we follow, and how to get changes merged.

By participating you agree to keep interactions respectful and constructive.

## Ways to contribute

- **Report a bug** — open a [GitHub issue](../../issues) with steps to reproduce.
- **Suggest a feature** — open an issue describing the use case before sending a
  large change, so we can agree on the approach.
- **Send a fix or improvement** — open a pull request (see below).

> **Security issues:** please do **not** open a public issue. Follow
> [`SECURITY.md`](SECURITY.md) and report privately via GitHub Security Advisories.

## Reporting bugs

A good report includes:

- The version or commit hash you are on.
- Your OS, compiler, and CMake version.
- The exact request that triggers it — the `cmd` / `db` / `query` payload — and
  the response or crash you observed versus what you expected.
- A minimal reproduction if possible.

## Development setup

You need a C++17 compiler, CMake ≥ 3.11, Git, and a build tool (Ninja or Make).
Boost, nlohmann/json and fmt are fetched automatically by CMake; SQLite is bundled.

```sh
git clone https://github.com/miroslavbaudys/sqlite-server.git
cd sqlite-server
cmake -S . -B build          # first run downloads dependencies
cmake --build build          # produces build/sqlite3-server
```

## Running and testing your change

There is no automated test suite yet, so verify changes by running the server and
exercising the protocol:

```sh
mkdir -p /tmp/sqlite-data
./build/sqlite3-server --ip 127.0.0.1 --port 3333 --databases-folder /tmp/sqlite-data
```

Then send requests with the reference client in
[`examples/python/sqlite.py`](examples/python/sqlite.py) or the raw-protocol
snippet in the [README](README.md#python-client). Please confirm:

- The project **builds cleanly** (no new warnings on the files you touched).
- Existing commands (`QUERY`, `LIST`, `DELETE_DB`) still behave as documented.
- Any new behaviour is covered by a manual test you describe in the PR.

If your change adds non-trivial logic, consider adding a small reproducible test
or example so reviewers can verify it.

## Coding guidelines

- **Language:** C++17. Prefer standard-library and RAII over manual resource
  management; wrap C/SQLite handles in types that release them in their destructor
  (see `sqlite3_wrapper/`).
- **Match the surrounding style** — indentation, naming, and comment density of
  the file you are editing. Keep the existing file header comment intact.
- **Error handling:** surface failures as exceptions/error responses rather than
  crashing; never let a malformed request take down the server.
- **Untrusted input:** treat the `db` name and `query` as hostile. Preserve the
  existing path-traversal and validation guarantees, and add tests/notes when you
  touch them.
- **Keep diffs focused** — one logical change per pull request. Unrelated
  formatting churn makes review harder.
- Do not commit build artifacts; `cmake-build-debug/` and `build/` are ignored.

## Commit messages

- Use the **imperative mood** in the subject: "Add …", "Fix …", "Refactor …".
- Keep the subject concise (≈ 72 characters) and capitalized, with no trailing period.
- Add a body when the *why* isn't obvious from the subject; wrap it at ~72 columns.
- Group related changes into a single, coherent commit.

## Pull request process

1. Fork the repository and create a topic branch off `master`
   (e.g. `fix/blob-encoding`).
2. Make your change, following the guidelines above.
3. Ensure it builds and you have verified the behaviour locally.
4. Open a pull request with a clear description of **what** changed and **why**,
   and link any related issue.
5. Be responsive to review feedback; keep the branch up to date with `master`.

## License

By contributing, you agree that your contributions will be licensed under the
project's [MIT License](LICENSE).

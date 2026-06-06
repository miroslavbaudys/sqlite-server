<!--
Thanks for contributing to sqlite-server! Please fill in the sections below.
See CONTRIBUTING.md for the full guidelines.
-->

## Summary

<!-- What does this change do, and why? Keep it focused on one logical change. -->

## Related issue

<!-- e.g. "Closes #123", or "N/A" -->

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] Refactor / cleanup
- [ ] Documentation
- [ ] Build / CI

## How was this tested?

<!--
There is no automated test suite yet, so describe how you verified the change:
the commands you ran, the request payload(s) you sent, and what you observed.
-->

## Checklist

- [ ] The project builds cleanly (`cmake --build build`) with no new warnings on the files I touched.
- [ ] Existing commands (`QUERY`, `LIST`, `DELETE_DB`) still behave as documented.
- [ ] I manually verified the new behavior and described it above.
- [ ] Untrusted input handling is preserved (no regression in `db`-name / path-traversal validation).
- [ ] Commit messages use the imperative mood and a concise subject.
- [ ] Documentation (README / comments) is updated where relevant.

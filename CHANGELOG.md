# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-06-08

### Added
- **Optional password authentication** (`--auth` / `"auth"`). When set, each connection
  must send `{"auth":"<password>"}` as its first message before any command is processed;
  the server replies `{"result":"ok"}` or `{"result":"error"}`. Authentication state is
  per-connection. Disabled by default.
- **Optional IP whitelist** (`--ip-whitelist` / `"ip_whitelist"`). Accepts comma-separated
  values or a JSON array of CIDRs and bare addresses (IPv4 and IPv6); peers outside the
  list are dropped at accept time, before any per-connection state is built. An invalid
  entry is a fatal config error. Disabled by default.
- Documentation for connecting from letos / SQLiteStudio, including screenshots.
- Notes on running as a systemd service.
- GitHub community health files (issue/PR templates, contributing, security policy).

### Changed
- Both `auth` and `ip_whitelist` config keys are optional, so existing config files
  without them keep working unchanged.
- Updated the bundled SQLite amalgamation to 3.53.2.
- Serialize the response length header in explicit little-endian order.
- The reference Python client now performs the authentication handshake and accepts
  `ip` / `port` / `auth` arguments.
- Documented drop-in compatibility with the Rust port
  ([sqlite-server-rs](https://github.com/miroslavbaudys/sqlite-server-rs)).

## [1.0.0]

### Added
- Initial release: a multi-threaded TCP server exposing SQLite databases over a
  length-prefixed JSON protocol, with `QUERY`, `LIST`, and `DELETE_DB` commands.

[1.1.0]: https://github.com/miroslavbaudys/sqlite-server/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/miroslavbaudys/sqlite-server/releases/tag/v1.0.0

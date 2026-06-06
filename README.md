# sqlite-server

A small, multi-threaded TCP server that exposes [SQLite](https://sqlite.org) databases
over a length-prefixed JSON protocol. Clients open a socket, send a JSON request, and
receive a JSON response. The protocol is simple enough to talk to from any language.

- **Transport:** raw TCP, one 4-byte length header + UTF-8 JSON body per message.
- **Commands:** `QUERY`, `LIST`, `DELETE_DB`.
- **Concurrency:** a configurable pool of worker threads on a shared Boost.Asio io_context.
- **Storage:** one SQLite database file per name, kept in a configured folder.

---

## Building

The build uses CMake with `FetchContent`, so Boost, nlohmann/json and fmt are downloaded
and built automatically — you only need a C++17 compiler, CMake ≥ 3.11, Git, and a build
tool (Ninja or Make).

```sh
cmake -S . -B build           # configure (fetches Boost/json/fmt on first run)
cmake --build build           # compile -> build/sqlite3-server
```

Bundled/declared dependencies:

| Dependency | Version | Purpose |
|------------|---------|---------|
| Boost      | 1.91    | asio, system, regex, filesystem, thread, program_options |
| nlohmann/json | 3.12 | JSON parsing / serialization |
| fmt        | 12.1    | logging / formatting |
| SQLite3    | bundled (`sqlite3/`) | database engine |

SQLite is compiled with `SQLITE_THREADSAFE=1` (plus several size/feature trims — see
`CMakeLists.txt`).

---

## Running

```sh
./build/sqlite3-server --port 3333 --databases-folder ./data
```

### Command-line options

| Option | Default | Description |
|--------|---------|-------------|
| `-h, --help` | — | Show usage and exit |
| `-v, --version` | — | Show git branch + commit and exit |
| `-c, --config <path>` | — | Load settings from a JSON config file (see below) |
| `--ip <ip>` | `localhost` | Listen address |
| `-p, --port <port>` | `3333` | Listen port |
| `-d, --databases-folder <dir>` | `sqlite` | Folder holding the database files (must exist) |
| `-w, --workers <n>` | CPU cores | Number of worker threads |
| `--client-max-packet-size <bytes>` | `16777216` (16 MiB) | Max request size; larger requests cause the connection to be closed |

### Config file

When `--config` is given, all settings come from a JSON file and the other options are
ignored:

```json
{
  "client_max_packet_size": 16777216,
  "workers": 4,
  "listen_ip": "127.0.0.1",
  "listen_port": 3333,
  "databases_folder": "./data"
}
```

The databases folder **must already exist** — the server will not create it (individual
database files inside it are created on demand). Shutdown is graceful on `SIGINT`/`SIGTERM`.

### Running as a service (systemd)

The server runs in the foreground and logs to stdout, and it shuts down cleanly on
`SIGTERM` (the default signal systemd sends), so it works well as a `Type=simple` service.

**1. Install the binary and create a dedicated user + data directory:**

```sh
sudo install -m 0755 build/sqlite3-server /usr/local/bin/sqlite3-server
sudo useradd --system --no-create-home --shell /usr/sbin/nologin sqlite-server
sudo mkdir -p /var/lib/sqlite-server
sudo chown sqlite-server:sqlite-server /var/lib/sqlite-server
```

**2. Create `/etc/systemd/system/sqlite-server.service`:**

```ini
[Unit]
Description=sqlite-server (SQLite over TCP/JSON)
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/sqlite3-server --ip 127.0.0.1 --port 3333 --databases-folder /var/lib/sqlite-server
User=sqlite-server
Group=sqlite-server
Restart=on-failure
RestartSec=2

# Hardening — the protocol has no authentication or TLS, so keep it bound to
# localhost (or a trusted interface) and lock the process down.
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true
ProtectControlGroups=true
ProtectKernelModules=true
ProtectKernelTunables=true
RestrictAddressFamilies=AF_INET AF_INET6
ReadWritePaths=/var/lib/sqlite-server

[Install]
WantedBy=multi-user.target
```

**3. Enable and start it:**

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now sqlite-server
```

**4. Check status and follow logs:**

```sh
systemctl status sqlite-server
journalctl -u sqlite-server -f
```

To use a config file instead of flags, point `ExecStart` at
`... --config /etc/sqlite-server/config.json` and add that path to `ReadOnlyPaths=`.

> **Tip:** on systemd ≥ 235 you can skip the manual `useradd`/`mkdir` by using
> `DynamicUser=yes` together with `StateDirectory=sqlite-server` (which creates and owns
> `/var/lib/sqlite-server` for you) and pointing `--databases-folder` at it.

---

## Communication protocol

### Framing

Every message — in both directions — is:

```
+---------------------------+-------------------------------+
| 4 bytes  little-endian    |  N bytes  UTF-8 JSON          |
| uint32 length = N         |  payload                      |
+---------------------------+-------------------------------+
```

The header is the byte length of the JSON payload that follows. Read the 4 bytes, decode
the length, then read exactly that many bytes.

### Connection lifecycle

A connection is **persistent**: after the server replies it waits for the next request on
the same socket, so you can send many requests over one connection. Requests on a single
connection are processed sequentially (send a request, read its full response, then send
the next).

### Request format

A request is a JSON object with a `cmd` field (matched case-insensitively) plus
command-specific fields.

| Command | Required fields | Purpose |
|---------|-----------------|---------|
| `QUERY` | `db`, `query` | Run a SQL statement against database `db` |
| `LIST` | — | List available database files |
| `DELETE_DB` | `db` | Delete the database file `db` |

> Tolerance: the server will also accept lightly-malformed JSON where object keys are
> unquoted (e.g. `{cmd:"LIST"}`), repairing it before parsing.

---

### `QUERY`

```json
{ "cmd": "QUERY", "db": "mydb", "query": "SELECT id, name FROM users WHERE id = 1" }
```

Success response — `columns` is the column list in `SELECT` order, `data` is an array of
row objects:

```json
{
  "columns": ["id", "name"],
  "data": [ { "id": 1, "name": "Alice" } ]
}
```

For statements that return no result set (`INSERT`, `UPDATE`, `CREATE`, …) both arrays are
empty: `{ "columns": [], "data": [] }`.

#### Value encoding

SQLite values map to JSON as follows:

| SQLite type | JSON representation |
|-------------|---------------------|
| INTEGER | number (64-bit) |
| FLOAT | number |
| TEXT | string |
| NULL | `null` |
| BLOB | string `"X'<hex>'"` (lowercase hex), e.g. `"X'00ff'"` |

A client that needs the raw bytes of a BLOB decodes the `X'<hex>'` literal itself.

#### Column ordering

Each row is a JSON **object**, and the server serializes object keys **alphabetically**
(not in `SELECT` order). Use the top-level `columns` array when you need the original
column order; it is also present when `data` is empty.

#### Statement scope

Each `QUERY` compiles and runs a **single** SQL statement (the first one in the string).
To run several statements, send them as separate requests. Wrap multi-step work in an
explicit transaction by issuing `BEGIN` / `COMMIT` as their own requests on the same
connection when atomicity matters.

---

### `LIST`

```json
{ "cmd": "LIST" }
```

Returns the regular files in the databases folder, excluding SQLite sidecar files
(`-wal`, `-shm`, `-journal`):

```json
{ "list": ["mydb", "sales", "logs"] }
```

---

### `DELETE_DB`

```json
{ "cmd": "DELETE_DB", "db": "mydb" }
```

Response:

```json
{ "result": "ok" }
```

`"result"` is `"ok"` if the file was removed, `"error"` otherwise (e.g. it did not exist).

---

### Database name rules

`db` must be a **plain file name inside the configured folder**. The server resolves the
full path (following symlinks for the existing part) and confirms it stays directly inside
the databases folder. Anything that escapes — path separators, absolute paths, `.`/`..`
traversal, or a symlink pointing outside — is rejected. A non-existent database is created
on first `QUERY`.

---

### Error responses

**Command/validation errors** carry a numeric `generic_error` code and echo the `request`:

```json
{ "generic_error": 3, "request": { "cmd": "QUERY", "db": "../etc/passwd", "query": "..." } }
```

| Code | Name | Meaning |
|------|------|---------|
| 0 | `INVALID_FORMAT` | Request body was not valid JSON (also includes a `message`) |
| 1 | `NO_COMMAND_SPECIFIED` | Missing `cmd` |
| 2 | `UNKNOWN_COMMAND` | `cmd` is not one of the supported commands |
| 3 | `NO_DATABASE_SPECIFIED` | Missing `db`, or `db` is not a safe/valid name |
| 4 | `ERROR_READING_FROM_CLIENT` | Missing `query` on a `QUERY` request |

**SQL errors** (raised while preparing/running a `QUERY`) carry the SQLite error code,
its message, and the original request:

```json
{ "error_code": 1, "error_message": "no such table: ghosts",
  "query": { "cmd": "QUERY", "db": "mydb", "query": "SELECT * FROM ghosts" } }
```

An empty/whitespace/comment-only `query` is reported as `error_code` 21 (`SQLITE_MISUSE`)
with message `"empty query"`.

---

## Python client

### Reference library

A ready-to-use client lives at [`examples/python/sqlite.py`](examples/python/sqlite.py).
It is a single, dependency-free module (standard library only) that handles framing,
parameter binding with client-side escaping, and a typed result wrapper. Copy it into your
project and use it directly:

```python
from sqlite import Sqlite

# Connects to 127.0.0.1:3333 by default (edit _SQLITE_IP / _SQLITE_PORT to change).
with Sqlite("mydb") as db:
    db.send_query("CREATE TABLE IF NOT EXISTS users(id INTEGER, name TEXT)")
    db.send_query("INSERT INTO users VALUES(?, ?)", [1, "Alice"])   # ? params are escaped

    result = db.query("SELECT id, name FROM users WHERE id = ?", [1])
    for row in result:
        print(row.id, row.name)          # rows support both row["id"] and row.id

    n = db.query("SELECT COUNT(*) AS n FROM users").scalar()   # first column of first row -> 1
```

Highlights of the wrapper:

- **Parameter binding** — `?` and `?N` placeholders are escaped client-side
  (`SELECT … WHERE id = ?`, `[1]`); `bytes` values become `X'..'` BLOB literals.
- **`QueryResult`** — iterable/sized/truthy; `.first()`, `.scalar()`, `.column(name)`,
  `.rows`, `.columns` (true `SELECT` order). Never `None`.
- **`Row`** — a `dict` subclass with attribute access (`row.name`) and `row.blob("col")`
  to decode a BLOB column back to `bytes`.

### Minimal raw protocol

If you would rather speak the protocol directly, the framing is just a 4-byte length plus
JSON:

```python
import json, socket, struct

def call(host, port, request):
    payload = json.dumps(request).encode("utf-8")
    with socket.create_connection((host, port)) as sock:
        sock.sendall(struct.pack("<I", len(payload)) + payload)   # 4-byte LE length + body

        def recv_exactly(n):
            buf = b""
            while len(buf) < n:
                chunk = sock.recv(n - len(buf))
                if not chunk:
                    raise ConnectionError("connection closed")
                buf += chunk
            return buf

        size = struct.unpack("<I", recv_exactly(4))[0]
        return json.loads(recv_exactly(size).decode("utf-8"))

print(call("127.0.0.1", 3333, {"cmd": "LIST"}))
print(call("127.0.0.1", 3333,
           {"cmd": "QUERY", "db": "mydb", "query": "SELECT 1 AS one"}))
```

---

## Project layout

| File | Responsibility |
|------|----------------|
| `main.cpp` | CLI/config parsing, starts the network worker |
| `Config.{h,cpp}` | Singleton holding runtime configuration |
| `Network.h` | `NetworkWorker`: io_context, thread pool, signal handling |
| `ListenSocket.h` | Accepts incoming connections |
| `Socket.{h,cpp}`, `SQLiteSocket.{h,cpp}` | Per-connection read/write + framing |
| `RequestHandler.{h,cpp}` | Parses requests, dispatches commands, builds responses |
| `Response.{h,cpp}`, `IResponse.h` | Serializes a JSON response with its length header |
| `sqlite3_wrapper/` | RAII wrappers: `SQLDatabase`, `SQLStatement`, `SQLException` |
| `sqlite3/` | Bundled SQLite amalgamation |
| `Logger.h` | Timestamped console logging (debug logs only in debug builds) |
| `examples/python/` | Reference Python client (`sqlite.py`) |

---

## License

Released under the [MIT License](LICENSE). The bundled SQLite amalgamation in `sqlite3/`
is public domain.

"""
This module provides a custom SQLite client implementation `Sqlite` that communicates with an external SQLite service over a TCP socket.
"""

# coding=utf-8
import json
import math
import re
import socket
import struct
from typing import Optional, Any, List

# Defaults
_SQLITE_IP = "127.0.0.1"
_SQLITE_PORT = 3333

# The server serialises BLOB columns as a JSON string of the form X'<hex>' (see
# RequestHandler.cpp). This matches the literal we emit in _serialize_sql_value.
_BLOB_LITERAL_RE = re.compile(r"^[Xx]'([0-9A-Fa-f]*)'$")


def decode_blob_literal(value: Any) -> Optional[bytes]:
    """
    Decodes a BLOB value returned by the server into ``bytes``.

    BLOB columns arrive as a hex string literal ``X'..'`` (the server cannot put raw
    binary into JSON). This reverses that encoding.

    Decoding must be explicit because the response carries no type information: a TEXT
    column literally containing ``X'00ff'`` is indistinguishable from a BLOB, so callers
    opt in per column rather than risk corrupting text values.

    :param value: ``None``, raw ``bytes``/``bytearray``, or an ``X'..'`` string literal.
    :return: The decoded ``bytes``, or ``None`` for a NULL value.
    :raises ValueError: If ``value`` is a string that is not a valid ``X'..'`` literal.
    """
    if value is None:
        return None
    if isinstance(value, (bytes, bytearray)):
        return bytes(value)
    match = _BLOB_LITERAL_RE.match(value) if isinstance(value, str) else None
    if not match:
        raise ValueError(f"Not a BLOB literal: {value!r}")
    return bytes.fromhex(match.group(1))


class Row(dict):
    """
    A single result row.

    Behaves exactly like a ``dict`` (so ``row["col"]``, ``.get()``, ``in`` and JSON
    serialization all keep working) but additionally exposes columns as attributes::

        row["name"] == row.name

    BLOB columns arrive as ``X'..'`` hex strings; use :meth:`blob` to decode one to bytes.
    """

    def __getattr__(self, name: str) -> Any:
        try:
            return self[name]
        except KeyError:
            raise AttributeError(name)

    def __setattr__(self, name: str, value: Any) -> None:
        self[name] = value

    def __delattr__(self, name: str) -> None:
        try:
            del self[name]
        except KeyError:
            raise AttributeError(name)

    def blob(self, key: str) -> Optional[bytes]:
        """
        Returns column ``key`` decoded from its ``X'..'`` hex literal into ``bytes``.

        Returns ``None`` when the column is NULL. Raises ``ValueError`` if the column does
        not hold a BLOB literal (i.e. it was a plain TEXT/number value).
        """
        return decode_blob_literal(self[key])


class QueryResult:
    """
    A lightweight, read-only wrapper around a query response.

    The server replies with a JSON object shaped like ``{"data": [{col: val, ...}, ...]}``.
    This wrapper exposes that payload as an iterable, sized and truthy sequence of
    :class:`Row` objects, while still supporting the legacy ``result["data"]`` access so
    existing call sites keep working unchanged.

    A ``QueryResult`` is *always* well-formed: a ``None`` or malformed payload simply
    yields an empty result, so callers never have to guard against ``None``.

    Example:
        result = db.query("SELECT COUNT(*) AS count FROM users")
        if result:                       # truthy only when there are rows
            total = result.scalar()      # first column of first row
        for row in result:               # iterate rows directly
            print(row.name)              # columns reachable as attributes
    """

    __slots__ = ("_payload",)

    def __init__(self, payload: Any = None):
        # Normalise into a dict carrying a "data" list, regardless of what we received.
        if isinstance(payload, list):
            payload = {"data": payload}
        elif not isinstance(payload, dict):
            payload = {}

        # Wrap row dicts as Rows so columns are reachable as attributes (row.col).
        data = payload.get("data")
        if isinstance(data, list):
            payload = {**payload, "data": [r if isinstance(r, Row) else Row(r)
                                           for r in data if isinstance(r, dict)]}
        self._payload = payload

    @property
    def rows(self) -> List[Row]:
        """The list of :class:`Row` objects (always a list, never ``None``)."""
        data = self._payload.get("data")
        return data if isinstance(data, list) else []

    @property
    def columns(self) -> List[str]:
        """
        The query's column names, in ``SELECT`` order.

        This is the authoritative column order: the server serialises each row as a JSON
        object whose keys come back alphabetically sorted, so a row's ``keys()`` do *not*
        reflect the original order. ``columns`` is also present when the result is empty.
        """
        cols = self._payload.get("columns")
        return cols if isinstance(cols, list) else []

    # --- Sequence protocol over rows -------------------------------------
    def __iter__(self):
        return iter(self.rows)

    def __len__(self) -> int:
        return len(self.rows)

    def __bool__(self) -> bool:
        return len(self.rows) > 0

    def __getitem__(self, key):
        # Legacy/dict access: result["data"] always yields the rows list (never raises);
        # any other string key indexes the raw payload; int/slice indexes the rows.
        if key == "data":
            return self.rows
        if isinstance(key, str):
            return self._payload[key]
        return self.rows[key]

    def __contains__(self, key) -> bool:
        return key in self._payload

    def __repr__(self) -> str:
        return f"QueryResult(rows={len(self)})"

    # --- Convenience accessors -------------------------------------------
    def first(self) -> Optional[Row]:
        """The first row, or ``None`` when the result set is empty."""
        rows = self.rows
        return rows[0] if rows else None

    def scalar(self, default: Any = None) -> Any:
        """
        The first column of the first row.

        Ideal for single-value queries such as ``COUNT(*)`` or ``MAX(...)``.
        Returns ``default`` when there are no rows.

        Uses ``columns[0]`` for the true first column when available, since a row's own
        key order is alphabetical rather than ``SELECT`` order.
        """
        row = self.first()
        if not row:
            return default
        cols = self.columns
        key = cols[0] if cols else next(iter(row), None)
        return row.get(key, default) if key is not None else default

    def column(self, name: str) -> List[Any]:
        """Every value of column ``name`` across all rows."""
        return [row[name] for row in self.rows if name in row]

    def get(self, key, default=None):
        """Dict-style access to the raw payload (e.g. metadata keys)."""
        return self._payload.get(key, default)


class Sqlite:
    """
    A client for connecting to and executing queries against an external SQLite server via TCP sockets.

    Supports context management (with-statement) for safe resource cleanup.

    Example:
        with Sqlite("my_database") as db:
            result = db.query("SELECT * FROM users WHERE id = ?", [1])
    """

    def __init__(self, database: str):
        """
        Initializes the Sqlite client and establishes a TCP connection.

        :param database: The name or path of the database file on the server.
        """
        self._database = database
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.settimeout(120.0)
        self._sock.connect((_SQLITE_IP, _SQLITE_PORT))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self) -> None:
        """
        Closes the socket connection safely. This method shuts down both sending and receiving operations
        before completely closing the socket to ensure a graceful disconnect.
        """
        if self._sock:
            try:
                # Shutdown tells the other end we are done sending/receiving
                self._sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                # Connection might already be closed or reset
                pass
            finally:
                try:
                    self._sock.close()
                except OSError:
                    pass

    def query(self, query: str, params: Optional[List[Any]] = None) -> QueryResult:
        """
        Executes a SQL query on the server and returns the result.

        If parameters are provided, they are safely escaped client-side and injected into the query string
        before transmission. The server responds with JSON-encoded data.

        :param query: The SQL query string, optionally containing '?' or '?N' placeholders.
        :param params: A list of parameter values to bind to the placeholders.
        :return: A :class:`QueryResult` wrapping the response. Always returned (never None);
                 an error or empty response yields an empty result.
        """
        try:
            if params:
                query = self._client_side_prepare(query, params)

            data = self._send_query(query)
            payload = json.loads(data) if data else None

            # The server reports failures as a JSON object with no "data" key (e.g.
            # {"error_code", "error_message"} or {"generic_error"}). Surface it instead of
            # silently degrading to an empty result set.
            if isinstance(payload, dict) and (
                "error_message" in payload or "error_code" in payload or "generic_error" in payload
            ):
                print(f"Sqlite.query server error: {payload.get('error_message', payload)}")

            return QueryResult(payload)
        except Exception as e:
            print(f"Sqlite.query error: {e}")
            return QueryResult()

    def send_query(self, query: str, params: Optional[List[Any]] = None) -> None:
        """
        Executes a SQL query without expecting a return value.
        Useful for INSERT, UPDATE, or DELETE operations where the result set is not needed.

        :param query: The SQL query string.
        :param params: A list of parameter values to bind.
        """
        self.query(query, params)

    def _client_side_prepare(self, query: str, params: List[Any]) -> str:
        """
        Replaces '?' and '?N' placeholders with sanitized parameter values.

        Both placeholder styles are resolved in a single left-to-right pass so that a
        substituted value is never re-scanned: this keeps the escaping intact even when
        a parameter value itself contains a '?' character.

        - '?N' (positional) binds to ``params[N - 1]``.
        - '?'  (standard) binds to the next not-yet-consumed standard parameter.

        :param query: The raw SQL query.
        :param params: The parameter values.
        :return: The formatted query string with parameters safely injected.
        """
        # Standard '?' placeholders are consumed in order, independently of positional ones.
        param_iter = iter(params)

        def replacement(match):
            digits = match.group(1)
            if digits:
                # Positional parameter (?N): SQL ?1 maps to index 0.
                index = int(digits) - 1
                if 0 <= index < len(params):
                    return self._serialize_sql_value(params[index])
                return match.group(0)
            # Standard parameter (?): take the next available value.
            try:
                return self._serialize_sql_value(next(param_iter))
            except StopIteration:
                return "?"  # No more params left

        # A single regex matches both forms; '?N' is tried before bare '?' via alternation.
        return re.sub(r'\?(\d+)|\?', replacement, query)

    @staticmethod
    def _serialize_sql_value(value: Any) -> str:
        """
        Sanitizes Python types into SQL-safe string literals to prevent injection.

        :param value: The Python value to serialize.
        :return: The SQL string literal.
        """
        if value is None:
            return "NULL"
        if isinstance(value, bool):
            return "1" if value else "0"
        if isinstance(value, float) and not math.isfinite(value):
            # SQLite has no NaN/Infinity literals: NaN -> NULL, +/-Inf -> +/-9e999.
            if math.isnan(value):
                return "NULL"
            return "9e999" if value > 0 else "-9e999"
        if isinstance(value, (int, float)):
            return str(value)
        if isinstance(value, (bytes, bytearray)):
            # Emit a BLOB literal X'..' so binary data round-trips without corruption.
            return f"X'{value.hex()}'"

        # Escape single quotes by doubling them for security
        escaped = str(value).replace("'", "''")
        return f"'{escaped}'"

    def _send_query(self, query: str) -> Optional[str]:
        """
        Constructs the JSON payload and sends it over the socket.

        :param query: The final, prepared SQL query string.
        :return: The raw string response from the server.
        """
        payload = {
            "db": self._database,
            "cmd": "QUERY",
            "query": query
        }
        self._send_data(json.dumps(payload))
        return self._recv_data()

    def _send_data(self, data: str) -> None:
        """
        Encodes the string data and prepends a 4-byte little-endian length header before sending.

        :param data: The JSON payload string.
        """
        encoded_data = data.encode("utf-8")
        # Header is a 4-byte unsigned little-endian length, matching the server's uint32_t.
        header = struct.pack("<I", len(encoded_data))
        self._sock.sendall(header + encoded_data)

    def _recv_data(self) -> Optional[str]:
        """
        Reads the 4-byte header to determine payload size, then reads the full payload.

        :return: The decoded string response, or None if the connection fails.
        """
        # 1. Read 4-byte header
        header = self._read_n_bytes(4)
        if not header:
            return None

        size, = struct.unpack("<I", header)

        # 2. Read the full payload based on header size
        payload = self._read_n_bytes(size)
        return payload.decode("utf-8") if payload else None

    def _read_n_bytes(self, n: int) -> Optional[bytes]:
        """
        Helper to ensure we receive exactly N bytes from the socket.

        :param n: The number of bytes to read.
        :return: The byte string read, or None if the connection closed early.
        """
        data = b''
        while len(data) < n:
            chunk = self._sock.recv(n - len(data))
            if not chunk:  # Connection closed
                return None
            data += chunk
        return data

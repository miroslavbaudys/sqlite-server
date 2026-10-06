"""
End-to-end test of the built server: concurrent writers over separate connections must not lose writes,
and a failing statement must come back as an error, not as an empty result.

    python3 tests/test_concurrent_writes.py [path/to/sqlite3-server]    # default: build/sqlite3-server

Run by ctest (see CMakeLists.txt). Before the busy_timeout / step error fix, ~40% of the inserts were
lost while another connection was deleting in batches - the server answered them with an empty "success".
"""
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

SERVER = os.path.abspath(sys.argv.pop(1) if len(sys.argv) > 1 else
                         os.path.join(os.path.dirname(__file__), "..", "build", "sqlite3-server"))


class Connection:
    """ raw protocol client - returns the server's JSON as it is (the example client hides errors) """

    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=30)

    def query(self, db, query):
        data = json.dumps({"db": db, "cmd": "QUERY", "query": query}).encode()
        self.sock.sendall(struct.pack("<I", len(data)) + data)
        size, = struct.unpack("<I", self._read(4))
        return json.loads(self._read(size))

    def _read(self, n):
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError("server closed the connection")
            data += chunk
        return data

    def close(self):
        self.sock.close()


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class ConcurrentWritesTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.port = free_port()
        self.server = subprocess.Popen([SERVER, "--ip", "127.0.0.1", "--port", str(self.port),
                                        "--databases-folder", self.folder.name, "--workers", "4"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(self.server.wait)
        self.addCleanup(self.server.terminate)
        for _ in range(100):
            try:
                self.db = Connection(self.port)
                break
            except OSError:
                time.sleep(0.05)
        else:
            self.fail("server did not start")
        self.addCleanup(self.db.close)
        self.db.query("test", "PRAGMA journal_mode=WAL")
        self.db.query("test", "CREATE TABLE t (id INTEGER PRIMARY KEY, writer INTEGER, value TEXT)")

    def test_no_write_is_lost_while_another_connection_deletes_in_batches(self):
        # like retention.py next to the API: one connection deletes big batches, others insert
        self.db.query("test", "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 200000) "
                              "INSERT INTO t(id, writer, value) SELECT -i, 0, hex(randomblob(32)) FROM n")
        stop = threading.Event()
        errors = []
        writes = [0] * 4

        def deleter():
            conn = Connection(self.port)
            while not stop.is_set():
                result = conn.query("test", "DELETE FROM t WHERE id IN (SELECT id FROM t WHERE writer = 0 LIMIT 5000)")
                if "error_code" in result:
                    errors.append(result)
            conn.close()

        def writer(n):
            conn = Connection(self.port)
            for i in range(300):
                result = conn.query("test", "INSERT INTO t(writer, value) VALUES ({}, 'x')".format(n + 1))
                if "error_code" in result:
                    errors.append(result)
                else:
                    writes[n] += 1
            conn.close()

        threads = [threading.Thread(target=writer, args=(n,)) for n in range(4)]
        delete_thread = threading.Thread(target=deleter)
        delete_thread.start()
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        stop.set()
        delete_thread.join()

        self.assertEqual(errors, [])
        stored = self.db.query("test", "SELECT COUNT(*) AS c FROM t WHERE writer > 0")["data"][0]["c"]
        self.assertEqual(sum(writes), 1200)
        self.assertEqual(stored, 1200, "every acknowledged insert is stored")

    def test_failing_statement_returns_an_error(self):
        self.db.query("test", "INSERT INTO t(id, writer, value) VALUES (1, 1, 'a')")
        result = self.db.query("test", "INSERT INTO t(id, writer, value) VALUES (1, 1, 'b')")
        self.assertIn("error_code", result)
        self.assertEqual(result["error_code"], 1555)    # SQLITE_CONSTRAINT_PRIMARYKEY
        self.assertNotIn("data", result)

    def test_select_still_returns_columns_and_rows(self):
        self.db.query("test", "INSERT INTO t(id, writer, value) VALUES (1, 1, 'a'), (2, 1, 'b')")
        result = self.db.query("test", "SELECT id, value FROM t ORDER BY id")
        self.assertEqual(result["columns"], ["id", "value"])
        self.assertEqual(result["data"], [{"id": 1, "value": "a"}, {"id": 2, "value": "b"}])


if __name__ == "__main__":
    unittest.main()

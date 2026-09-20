"""Integration validation inside a disposable Ubuntu container, never on the host."""
import hashlib
import json
import os
from pathlib import Path
import signal
import sqlite3
import subprocess
import time


def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


folder = Path("/home/tester/.local/share/MHSoftware/MH Store")
database = folder / "mhstore.sqlite"
package = next(Path("/packages").glob("*.deb"))


def sql(statement):
    with sqlite3.connect(database) as connection:
        return connection.execute(statement).fetchall()


def launch(expected_schema=20, failure=False):
    process = subprocess.Popen(["runuser", "-u", "tester", "--", "env",
                                "QT_QPA_PLATFORM=offscreen", "QT_QUICK_BACKEND=software",
                                "MHStore"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=True)
    try:
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError(process.communicate()[1].decode())
            if database.exists():
                try:
                    if sql("SELECT MAX(version) FROM schema_migrations")[0][0] == expected_schema:
                        break
                except sqlite3.Error:
                    pass
            time.sleep(0.1)
        else:
            raise AssertionError("Application did not initialize schema")
        # Allow QML/plugin loading after database setup.
        time.sleep(2)
        assert process.poll() is None, "Application exited during QML loading"
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
        out, err = process.communicate(timeout=10)
    messages = err.decode()
    for forbidden in ("failed to load component", "is not installed", "ReferenceError", "TypeError", "SyntaxError"):
        assert forbidden not in messages, messages
    if failure:
        assert "Falha ao inicializar o banco" in messages, messages
    else:
        assert "Falha ao inicializar o banco" not in messages, messages


assert Path("/usr/share/applications/mhstore.desktop").is_file()
assert "Exec=MHStore" in Path("/usr/share/applications/mhstore.desktop").read_text()
assert Path("/usr/share/icons/hicolor/scalable/apps/mhstore.svg").is_file()
assert "not found" not in run("ldd", "/usr/bin/MHStore")
assert "/home/" not in run("ldd", "/usr/bin/MHStore")
launch()
sql("INSERT INTO customers(name) VALUES('Package preservation sentinel')")
# Construct a real schema-19 fixture, then check migration by the installed binary.
sql("DROP INDEX products_variant_group")
sql("ALTER TABLE products DROP COLUMN variant_group")
sql("DELETE FROM schema_migrations WHERE version=20")
before = hashlib.sha256(database.read_bytes()).digest()
run("dpkg", "-i", str(package))
assert hashlib.sha256(database.read_bytes()).digest() == before, "Package install changed user data"
launch()
assert sql("SELECT name FROM customers")[0][0] == "Package preservation sentinel"
assert sql("SELECT MAX(version) FROM schema_migrations")[0][0] == 20
report = json.loads(run("runuser", "-u", "tester", "--", "mhstore-diagnostico"))
assert report["esquema"] == 20
assert "Package preservation sentinel" not in json.dumps(report)
before = hashlib.sha256(database.read_bytes()).digest()
run("dpkg", "--purge", "mhstore")
assert hashlib.sha256(database.read_bytes()).digest() == before, "Purge changed user data"
assert not Path("/usr/bin/MHStore").exists()
run("dpkg", "-i", str(package))
launch()
assert sql("SELECT name FROM customers")[0][0] == "Package preservation sentinel"
# Future schemas must not be downgraded by an older binary.
sql("INSERT INTO schema_migrations(version) VALUES(999)")
before = hashlib.sha256(database.read_bytes()).digest()
launch(expected_schema=999, failure=True)
assert hashlib.sha256(database.read_bytes()).digest() == before
print("PASS: clean runtime install, QML/plugins, schema migration, data preservation, purge/reinstall, future-schema rejection and support report.")

"""Compile the actual garage callback with inert actuator stubs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


def callback(source):
    start = source.index("boolean DEV_GarageDoor::update()")
    end = source.index("void DEV_GarageDoor::loop()", start)
    return source[start:end]


HARNESS = r"""
#include <cassert>
using boolean = bool;
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
enum GarageDoorCurrentState { OPENING = 2, CLOSING = 3 };
int opens = 0, closes = 0, locks = 0;
GarageDoorCurrentState open_door() { ++opens; return OPENING; }
GarageDoorCurrentState close_door() { ++closes; return CLOSING; }
void set_lock(bool) { ++locks; }
struct Config { int security = 2; int getGDOSecurityType() { return security; } };
Config config;
Config *userConfig = &config;
struct Value {
    static constexpr int OPEN = 0, CLOSED = 1, LOCK = 1;
    bool changed = false;
    int value = 0;
    bool updated() { return changed; }
    int getNewVal() { return value; }
    void setVal(int v) { value = v; }
};
struct DEV_GarageDoor {
    Value *target, *lockTarget, *obstruction, *current;
    boolean update();
};
"""

CASES = r"""
int main() {
    Value target, lock, obstruction, current;
    DEV_GarageDoor door{&target, &lock, &obstruction, &current};
    auto check = [&](bool writeDoor, int desired, bool writeLock,
                     int security, bool hasLock, int expectedOpens,
                     int expectedCloses, int expectedLocks) {
        opens = closes = locks = 0;
        target.changed = writeDoor; target.value = desired;
        lock.changed = writeLock; lock.value = Value::LOCK;
        config.security = security;
        door.lockTarget = hasLock ? &lock : nullptr;
        assert(door.update());
        assert(opens == expectedOpens && closes == expectedCloses && locks == expectedLocks);
    };
    // Cached OPEN must never become a command when only another field changes.
    check(false, Value::OPEN, false, 2, true, 0, 0, 0);
    check(false, Value::OPEN, true, 2, true, 0, 0, 1);
    check(true, Value::OPEN, false, 2, true, 1, 0, 0);
    assert(current.value == OPENING && obstruction.value == 0);
    check(true, Value::CLOSED, false, 2, true, 0, 1, 0);
    assert(current.value == CLOSING);
    check(true, Value::CLOSED, true, 2, true, 0, 1, 1);
    check(false, Value::OPEN, true, 3, false, 0, 0, 0);
    check(false, Value::OPEN, true, 2, false, 0, 0, 0);
}
"""


class HomeKitWritesTest(unittest.TestCase):
    def test_only_explicit_writes_can_actuate(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler)
        body = callback(Path("src/homekit.cpp").read_text())
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "homekit-writes.cpp"
            binary = Path(directory) / "homekit-writes"
            source.write_text(HARNESS + body + CASES)
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            str(source), "-o", str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True, timeout=5)

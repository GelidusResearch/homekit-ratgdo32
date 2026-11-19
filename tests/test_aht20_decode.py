"""Compile the AHT20 decode from aht20.cpp and check it against the datasheet formulas."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
DECODE
static void check(uint8_t h1, uint8_t h2, uint8_t mid, uint8_t t1, uint8_t t2, float rh, float c)
{
    const uint8_t data[6] = {0x1C, h1, h2, mid, t1, t2};
    float celsius = 0, humidity = 0;
    aht20_decode(data, celsius, humidity);
    assert(std::fabs(humidity - rh) < 0.0001f && std::fabs(celsius - c) < 0.0001f);
}
int main()
{
    // RH = raw / 2^20 * 100, T = raw / 2^20 * 200 - 50; byte 3 holds both nibbles
    check(0x80, 0x00, 0x06, 0x00, 0x00, 50.0f, 25.0f);
    check(0x00, 0x00, 0x00, 0x00, 0x00, 0.0f, -50.0f);
    check(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 99.999905f, 149.999809f);
    check(0x12, 0x34, 0xA5, 0x67, 0x89, 7.111549f, 17.555428f);
}
'''


class AHT20DecodeTest(unittest.TestCase):
    def test_decode(self):
        source = (ROOT / "src/aht20.cpp").read_text()
        start = source.index("static void aht20_decode(")
        decode = source[start:source.index("\n}\n", start) + 3]
        with tempfile.TemporaryDirectory(prefix="ratgdo-aht20-") as directory:
            cpp = Path(directory) / "aht20.cpp"
            binary = Path(directory) / "aht20"
            cpp.write_text(HARNESS.replace("DECODE", decode))
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()

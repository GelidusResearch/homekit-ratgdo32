#!/usr/bin/env python3
"""Exercise the actual IPv4 refresh function from homekit.cpp."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def refresh_function(source):
    start = source.index("static void refresh_ipv4_config()")
    end = source.index("\nvoid connectionCallback", start)
    return source[start:end]


HARNESS = r"""
#include <cassert>
#include <map>
#include <string>
#include <utility>
enum IPType { IPv4, IPv6 };
struct IPAddress {
    std::string value;
    IPType kind;
    IPAddress(int a, int b, int c, int d)
        : value(std::to_string(a)+"."+std::to_string(b)+"."+std::to_string(c)+"."+std::to_string(d)), kind(IPv4) {}
    IPAddress(std::string v, IPType k=IPv4) : value(std::move(v)), kind(k) {}
    IPType type() const { return kind; }
    std::string toString() const { return value; }
    bool operator==(const IPAddress &other) const { return value==other.value && kind==other.kind; }
    bool operator!=(const IPAddress &other) const { return !(*this==other); }
};
struct Config {
    std::map<std::string,std::string> values;
    void set(const char *key, const char *value) { values[key]=value; }
};
struct WiFiMock {
    IPAddress local{"0.0.0.0"}, mask{"0.0.0.0"}, gateway{"0.0.0.0"}, dns{"0.0.0.0"};
    IPAddress localIP() const { return local; }
    IPAddress subnetMask() const { return mask; }
    IPAddress gatewayIP() const { return gateway; }
    IPAddress dnsIP() const { return dns; }
} WiFi;
constexpr char cfg_localIP[]="localIP", cfg_gatewayIP[]="gatewayIP";
constexpr char cfg_subnetMask[]="subnetMask", cfg_nameserverIP[]="nameserverIP";
Config *userConfig;
bool rebooting=false, softAPmode=false;
unsigned notifications=0;
void notify_new_ipv4_address() { ++notifications; }
"""


CASES = r"""
int main() {
    Config config; userConfig=&config;
    config.values={{"localIP","192.168.23.32"},{"gatewayIP","192.168.23.1"},
                   {"subnetMask","255.255.255.0"},{"nameserverIP","192.168.23.1"}};
    const auto saved=config.values;

    // IPv6-first connection must preserve saved IPv4 until DHCP supplies one.
    WiFi.dns=IPAddress("fe80::1",IPv6);
    refresh_ipv4_config();
    assert(config.values==saved && notifications==0);

    WiFi.local=IPAddress("192.168.23.33");
    WiFi.mask=IPAddress("255.255.255.0");
    WiFi.gateway=IPAddress("192.168.23.1");
    refresh_ipv4_config();
    assert(config.values.at("localIP")=="192.168.23.33");
    assert(config.values.at("nameserverIP")=="192.168.23.1" && notifications==1);

    // A DHCP renewal updates the lease and an available IPv4 DNS address.
    WiFi.local=IPAddress("192.168.23.34");
    WiFi.dns=IPAddress("192.168.23.2");
    refresh_ipv4_config();
    assert(config.values.at("localIP")=="192.168.23.34");
    assert(config.values.at("nameserverIP")=="192.168.23.2" && notifications==2);

    Config fresh; userConfig=&fresh;
    WiFi.local=IPAddress("0.0.0.0");
    refresh_ipv4_config();
    assert(fresh.values.empty() && notifications==2);

    userConfig=&config;
    const auto updated=config.values;
    WiFi.local=IPAddress("192.168.23.35");
    rebooting=true; refresh_ipv4_config();
    assert(config.values==updated && notifications==2);
    rebooting=false; softAPmode=true; refresh_ipv4_config();
    assert(config.values==updated && notifications==2);
}
"""


class IPv4ConfigTest(unittest.TestCase):
    def test_ipv6_first_and_ipv4_renewal(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler)
        source = (ROOT / "src/homekit.cpp").read_text()
        function = refresh_function(source)
        connection = source[source.index("void connectionCallback"):source.index("\nvoid WiFiGotIP")]
        got_ip = source[source.index("void WiFiGotIP"):source.index("\nvoid statusCallback")]
        self.assertIn("refresh_ipv4_config();", connection)
        self.assertIn("refresh_ipv4_config();", got_ip)

        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "ipv4-config.cpp"
            binary = Path(directory) / "ipv4-config"
            cpp.write_text(HARNESS + function + CASES)
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            str(cpp), "-o", str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True, timeout=5)


if __name__ == "__main__":
    unittest.main()

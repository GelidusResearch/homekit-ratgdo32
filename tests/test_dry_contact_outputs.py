#!/usr/bin/env python3
"""Run the actual output setup and dry-contact command blocks with fake GPIO."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def block(source, declaration):
    start = source.index(declaration)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        depth += (source[i] == "{") - (source[i] == "}")
        if not depth:
            return source[start:i + 1]
    raise AssertionError("unclosed block")


class DryContactOutputs(unittest.TestCase):
    def test_disconnected_and_configured_outputs(self):
        header = (ROOT / "src/ratgdo.h").read_text()
        helpers = "\n".join(block(header, name) for name in [
            "inline void write_status_pin(", "inline void setup_status_pin(",
            "inline void configure_dry_contact_output("])
        source = (ROOT / "src/comms.cpp").read_text()
        start = source.index("        // Dry contact commands\n", source.index("bool door_command(DoorAction action)\n{"))
        end = source.index("\n    }\n    return true;", start)
        command = source[start:end]
        harness = r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <utility>
#define ESP32 1
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
using gpio_num_t=int;
constexpr int GPIO_MODE_OUTPUT=1, GPIO_PULLUP_DISABLE=0, GPIO_PULLDOWN_ENABLE=1, GPIO_INTR_DISABLE=0;
struct gpio_config_t { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; };
std::vector<std::pair<int,int>> writes;
std::vector<int> modes, preloads;
std::vector<uint64_t> configs;
std::vector<void(*)()> timers;
void gpio_set_level(int pin,int value) { assert(pin>=0 && value==0); preloads.push_back(pin); }
void gpio_config(const gpio_config_t *c) { assert(c->pin_bit_mask); configs.push_back(c->pin_bit_mask); }
void pinMode(int pin,int) { assert(pin>=0); modes.push_back(pin); }
void digitalWrite(int pin,int value) { assert(pin>=0); writes.emplace_back(pin,value); }
struct Ticker { void once_ms(unsigned ms,void(*fn)()) { assert(ms==500); timers.push_back(fn); } };
enum class DoorAction { Open, Close, Toggle, Stop };
enum GarageDoorCurrentState { CURR_OPEN, CURR_CLOSED, CURR_STOPPED };
struct { GarageDoorCurrentState current_state=CURR_CLOSED; } garage_door;
int DISCRETE_OPEN_PIN=-1, DISCRETE_CLOSE_PIN=-1;
constexpr int UART_TX_PIN=22;
''' + helpers + r'''
bool command(DoorAction action) {
''' + command + r'''
    return true;
}
void clear() { writes.clear(); modes.clear(); preloads.clear(); configs.clear(); timers.clear(); }
void complete() { auto pending=timers; timers.clear(); for(auto fn:pending) fn(); }
int main() {
    configure_dry_contact_output(-1); setup_status_pin(-1); write_status_pin(-1,HIGH);
    assert(writes.empty() && modes.empty() && preloads.empty() && configs.empty());
    configure_dry_contact_output(UART_TX_PIN);
    assert(preloads==std::vector<int>{22} && configs==std::vector<uint64_t>{1ULL<<22});
    clear(); assert(command(DoorAction::Open));
    assert(writes.size()==1 && writes[0]==std::make_pair(22,HIGH) && timers.size()==1);
    complete(); assert(writes.back()==std::make_pair(22,LOW));
    clear(); garage_door.current_state=CURR_OPEN;
    assert(!command(DoorAction::Open) && writes.empty() && timers.empty());
    assert(command(DoorAction::Close) && writes.size()==1 && timers.size()==1);
    complete(); clear();
    garage_door.current_state=CURR_CLOSED;
    assert(!command(DoorAction::Close) && writes.empty() && timers.empty());
    DISCRETE_OPEN_PIN=16; DISCRETE_CLOSE_PIN=17;
    configure_dry_contact_output(16); configure_dry_contact_output(17);
    assert(configs.size()==2 && configs[0]==(1ULL<<16) && configs[1]==(1ULL<<17));
    clear(); assert(command(DoorAction::Open));
    assert(writes.size()==2 && writes[0]==std::make_pair(16,HIGH) && writes[1]==std::make_pair(22,HIGH));
    assert(timers.size()==2); complete();
    assert(writes[2]==std::make_pair(16,LOW) && writes[3]==std::make_pair(22,LOW));
    clear(); garage_door.current_state=CURR_OPEN; assert(command(DoorAction::Close));
    assert(writes[0]==std::make_pair(17,HIGH) && writes[1]==std::make_pair(22,HIGH));
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / "dry-contact.cpp"
            binary = Path(temp) / "dry-contact"
            cpp.write_text(harness)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()

#include "../lib/ratgdo/serial_console.h"
#include <cassert>
#include <functional>

struct Port
{
    bool open = false, fail_begin = false;
    bool inside_driver = false;
    unsigned begins = 0, ends = 0;
    std::function<void()> on_begin, on_end;
    Port() = default;
    Port(const Port &) = delete;
    Port &operator=(const Port &) = delete;
    explicit operator bool() const { assert(!inside_driver); return open; }
    void begin(unsigned baud)
    {
        assert(baud == 115200);
        ++begins;
        inside_driver = true;
        if (on_begin) on_begin(); // Framework logging before UART becomes valid.
        open = !fail_begin;
        inside_driver = false;
    }
    void end()
    {
        ++ends;
        inside_driver = true;
        open = false;
        if (on_end) on_end(); // Framework logging while driver teardown owns locks.
        inside_driver = false;
    }
};

int main()
{
    SerialConsoleState console;
    Port port;
    port.on_begin = [&] { assert(!console.isReady()); console.ensureStarted(port); };
    console.ensureStarted(port);
    assert(port.begins == 1 && port.open); // Nested logs cannot recurse into begin().
    port.on_end = [&] { console.ensureStarted(port); };
    console.releaseForI2C(port);
    assert(!console.isEnabled() && !port.open && port.ends == 1);
    for (int i = 0; i < 20; ++i) console.ensureStarted(port);
    assert(port.begins == 1); // Logs cannot reclaim pins while I2C owns them.
    console.resumeAfterI2C(port); // Missing sensor: Wire has released GPIO1/3.
    assert(console.isEnabled() && port.open && port.begins == 2);

    SerialConsoleState unavailable;
    Port failed;
    failed.fail_begin = true;
    failed.on_begin = [&] { unavailable.ensureStarted(failed); };
    unavailable.ensureStarted(failed);
    assert(!failed.open && failed.begins == 1); // Failure returns without spinning.
}

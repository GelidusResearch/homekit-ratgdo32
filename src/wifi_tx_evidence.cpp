#if defined(ESP32) && defined(RATGDO_RUNTIME_DIAGNOSTICS) && defined(RATGDO_WIFI_TX_EVIDENCE)
#include <esp_attr.h>
#define WIFI_TX_IRAM IRAM_ATTR
#include "wifi_tx_evidence.h"

#include <esp_memory_utils.h>
#include <esp_private/wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
using wifi_tx_evidence::Record;
using wifi_tx_evidence::Counters;
namespace {
constexpr unsigned CAPACITY = 64;
DRAM_ATTR portMUX_TYPE ringMutex = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR Record ring[CAPACITY];
DRAM_ATTR unsigned head = 0, count = 0;
DRAM_ATTR Counters totals{};

// Explicit volatile fields avoid a compiler-generated memcpy in the hook.
IRAM_ATTR void copy(volatile Record &d, const volatile Record &s) {
    d.call=s.call; d.at=s.at; d.tcpSequence=s.tcpSequence; d.payloadLength=s.payloadLength;
    d.result=s.result; d.sourcePort=s.sourcePort; d.destinationPort=s.destinationPort;
    d.ipVersion=s.ipVersion; d.flags=s.flags; d.api=s.api; d.interface=s.interface; d.completed=s.completed;
}
IRAM_ATTR void push(const Record &r) {
    portENTER_CRITICAL_SAFE(&ringMutex);
    if (count == CAPACITY) { head = (head + 1) % CAPACITY; --count; ++totals.overwritten; }
    copy(ring[(head + count) % CAPACITY], r);
    ++count;
    portEXIT_CRITICAL_SAFE(&ringMutex);
}
IRAM_ATTR bool begin(unsigned api, wifi_interface_t interface, void *buffer, size_t length, Record &r) {
    uint32_t call;
    portENTER_CRITICAL_SAFE(&ringMutex);
    call = ++totals.calls;
    portEXIT_CRITICAL_SAFE(&ringMutex);
    // Avoid touching external RAM with flash/cache disabled. Unknown buffers are
    // forwarded unchanged but explicitly counted rather than parsed unsafely.
    const uintptr_t first = reinterpret_cast<uintptr_t>(buffer);
    if (!buffer || !length || first + length - 1 < first ||
        !esp_ptr_in_dram(buffer) || !esp_ptr_in_dram(reinterpret_cast<void *>(first + length - 1))) {
        portENTER_CRITICAL_SAFE(&ringMutex); ++totals.unreadable; portEXIT_CRITICAL_SAFE(&ringMutex);
        return false;
    }
    wifi_tx_evidence::Header header;
    const auto parsed = wifi_tx_evidence::parse(buffer, length, header);
    if (parsed != wifi_tx_evidence::Parse::Captured) {
        portENTER_CRITICAL_SAFE(&ringMutex);
        if (parsed == wifi_tx_evidence::Parse::Malformed) ++totals.malformed;
        if (parsed == wifi_tx_evidence::Parse::Unsupported) ++totals.unsupported;
        portEXIT_CRITICAL_SAFE(&ringMutex);
        return false;
    }
    r.call=call; r.at=static_cast<uint32_t>(xTaskGetTickCountFromISR()) * portTICK_PERIOD_MS;
    r.tcpSequence=header.tcpSequence; r.payloadLength=header.payloadLength;
    r.result=0; r.sourcePort=header.sourcePort; r.destinationPort=header.destinationPort;
    r.ipVersion=header.ipVersion; r.flags=header.flags; r.api=api; r.interface=interface; r.completed=0;
    portENTER_CRITICAL_SAFE(&ringMutex); ++totals.captured; portEXIT_CRITICAL_SAFE(&ringMutex);
    push(r);
    return true;
}
IRAM_ATTR void end(Record &r, int result) {
    // Read only our copied metadata after forwarding; the caller/driver may have
    // already recycled the input buffer, especially on the by-reference path.
    r.at=static_cast<uint32_t>(xTaskGetTickCountFromISR()) * portTICK_PERIOD_MS; r.result=result; r.completed=1;
    push(r);
}
}
extern "C" int __real_esp_wifi_internal_tx(wifi_interface_t, void *, uint16_t);
extern "C" esp_err_t __real_esp_wifi_internal_tx_by_ref(wifi_interface_t, void *, size_t, void *);
extern "C" IRAM_ATTR int __wrap_esp_wifi_internal_tx(wifi_interface_t interface, void *buffer, uint16_t length) {
    Record record;
    const bool captured=begin(1, interface, buffer, length, record);
    const int result=__real_esp_wifi_internal_tx(interface, buffer, length);
    if (captured) end(record, result);
    return result;
}
extern "C" IRAM_ATTR esp_err_t __wrap_esp_wifi_internal_tx_by_ref(wifi_interface_t interface, void *buffer, size_t length, void *netstackBuffer) {
    Record record;
    const bool captured=begin(2, interface, buffer, length, record);
    const esp_err_t result=__real_esp_wifi_internal_tx_by_ref(interface, buffer, length, netstackBuffer);
    if (captured) end(record, result);
    return result;
}
bool wifi_tx_evidence_pop(Record &record) {
    portENTER_CRITICAL_SAFE(&ringMutex);
    if (!count) { portEXIT_CRITICAL_SAFE(&ringMutex); return false; }
    copy(record, ring[head]); head=(head+1)%CAPACITY; --count;
    portEXIT_CRITICAL_SAFE(&ringMutex);
    return true;
}
void wifi_tx_evidence_counters(Counters &c) {
    portENTER_CRITICAL_SAFE(&ringMutex);
    c.calls=totals.calls; c.captured=totals.captured; c.malformed=totals.malformed;
    c.unsupported=totals.unsupported; c.unreadable=totals.unreadable; c.overwritten=totals.overwritten;
    portEXIT_CRITICAL_SAFE(&ringMutex);
}
#endif

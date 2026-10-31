#pragma once
#include <cstdint>
#include <cstddef>
#ifndef WIFI_TX_IRAM
#define WIFI_TX_IRAM
#endif

namespace wifi_tx_evidence {
enum class Parse : uint8_t { Ignored, Captured, Malformed, Unsupported };
struct Header {
    uint32_t tcpSequence, payloadLength;
    uint16_t sourcePort, destinationPort;
    uint8_t ipVersion, flags;
};
inline __attribute__((always_inline)) WIFI_TX_IRAM uint16_t read16(const volatile uint8_t *p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}
inline __attribute__((always_inline)) WIFI_TX_IRAM uint32_t read32(const volatile uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
// The caller guarantees a contiguous readable Ethernet frame for `length` bytes.
// Read only headers. Never copy payload, hold a buffer, or alter ownership.
inline __attribute__((always_inline)) WIFI_TX_IRAM Parse parse(const void *buffer, size_t length, Header &out) {
    if (!buffer || length < 14) return Parse::Malformed;
    const volatile uint8_t *b = static_cast<const volatile uint8_t *>(buffer);
    uint16_t type = read16(b + 12);
    size_t offset = 14;
    if (type == 0x8100 || type == 0x88a8) return Parse::Unsupported;
    size_t tcpOffset, tcpLength;
    uint8_t version;
    if (type == 0x0800) {
        if (length - offset < 20 || (b[offset] >> 4) != 4) return Parse::Malformed;
        const size_t header = (b[offset] & 15) * 4;
        const size_t total = read16(b + offset + 2);
        if (header < 20 || header > length - offset || total < header || total > length - offset)
            return Parse::Malformed;
        if (b[offset + 9] != 6) return Parse::Ignored; // excludes our UDP diagnostics
        if (read16(b + offset + 6) & 0x3fff) return Parse::Unsupported; // all IPv4 fragments
        tcpOffset = offset + header;
        tcpLength = total - header;
        version = 4;
    } else if (type == 0x86dd) {
        if (length - offset < 40 || (b[offset] >> 4) != 6) return Parse::Malformed;
        const size_t payload = read16(b + offset + 4);
        if (payload > length - offset - 40) return Parse::Malformed;
        const uint8_t next = b[offset + 6];
        if (next == 17 || next == 58) return Parse::Ignored;
        if (next != 6 || !payload) return Parse::Unsupported; // extension headers/jumbograms
        tcpOffset = offset + 40;
        tcpLength = payload;
        version = 6;
    } else return Parse::Ignored;
    if (tcpLength < 20) return Parse::Malformed;
    const uint16_t source = read16(b + tcpOffset);
    if (source != 80 && source != 5556) return Parse::Ignored;
    const size_t header = (b[tcpOffset + 12] >> 4) * 4;
    if (header < 20 || header > tcpLength) return Parse::Malformed;
    out.sourcePort = source;
    out.destinationPort = read16(b + tcpOffset + 2);
    out.tcpSequence = read32(b + tcpOffset + 4);
    out.payloadLength = tcpLength - header;
    out.ipVersion = version;
    out.flags = b[tcpOffset + 13];
    if (!out.payloadLength && !(out.flags & 0x07)) return Parse::Ignored;
    return Parse::Captured;
}
struct Record {
    uint32_t call, at, tcpSequence, payloadLength;
    int32_t result;
    uint16_t sourcePort, destinationPort;
    uint8_t ipVersion, flags, api, interface, completed;
};
struct Counters { uint32_t calls, captured, malformed, unsupported, unreadable, overwritten; };
}

#if defined(ESP32) && defined(RATGDO_RUNTIME_DIAGNOSTICS) && defined(RATGDO_WIFI_TX_EVIDENCE)
bool wifi_tx_evidence_pop(wifi_tx_evidence::Record &record);
void wifi_tx_evidence_counters(wifi_tx_evidence::Counters &counters);
#endif

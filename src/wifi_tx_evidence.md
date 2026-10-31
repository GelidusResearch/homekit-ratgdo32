# Diagnostic Wi-Fi driver handoff trace

Base: diagnostic branch a2bef70. Enabled only in the Rev1 diagnostics environment; ordinary builds exclude this trace. Physical device validation remains pending.

Enable both RATGDO_RUNTIME_DIAGNOSTICS and RATGDO_WIFI_TX_EVIDENCE, preserving every existing board flag, and add both linker options:

```
-Wl,--wrap=esp_wifi_internal_tx
-Wl,--wrap=esp_wifi_internal_tx_by_ref
```

Exact contracts were checked in the installed IDF5.5.5 esp_private/wifi.h. The copy API takes uint16_t length and returns int. The by-reference API takes size_t length and an opaque netstack buffer, increments its reference count, and lets the driver free it after processing. The wrappers forward every argument exactly once, return the original result, and never retain, modify, free, or reread the buffer after forwarding. A negative or ESP_ERR_NO_MEM result is evidence of rejection at this boundary. ESP_OK means accepted by the Wi-Fi driver, not delivered over the air or acknowledged by the peer.

libesp_wifi.a:wifi_netif.c.obj has an unresolved esp_wifi_internal_tx reference; both real APIs are defined in libnet80211.a:ieee80211_output.o. GNU linker wrapping can intercept that external stack-to-driver call. Calls resolved internally within the defining object may bypass --wrap, including a possible nested call. Do not claim every internal driver call is observed.

The parser reads only a contiguous internal-DRAM Ethernet frame, before forwarding. External/unknown memory is counted unreadable rather than dereferenced. IPv4 and plain IPv6 TCP responses from source port80 or5556 are captured. UDP diagnostics and payload-free ACK-only frames are excluded. SYN, FIN and RST frames are retained. This avoids trace recursion and reduces ring pressure. VLANs, IPv4 fragments, and IPv6 extension chains/jumbograms are counted unsupported. Checksums are not verified; no payload bytes or IP addresses are captured.

Two events are stored per matched API call: done=0 before forwarding and done=1 after the real return. call identifies the pair; api=1 is copy, api=2 is by-reference. Only done=1 has a meaningful result. Both events include pre-forward copied source/destination ports, TCP sequence, payload length and flags, IP version, Wi-Fi interface and event uptime from the FreeRTOS ISR-safe tick counter. A start without completion can indicate a blocked call, overwritten ring data, lost UDP, or capture ending; do not infer one cause without counters and surrounding data.

The dedicated DRAM ring has64 records (about2KiB), overwrites oldest entries with an explicit counter, and holds a dedicated SMP-safe mux only during fixed-size push/pop/counter operations. No heap, log, socket, Wi-Fi, or real transmit call runs while holding that mux. The emitter drains at most32 events per five-second sample to existing UDP5515; this bounds added traffic but may lose events during bursts. Individual emitted packet worst-case formats are204 bytes. seq and overwritten counters reveal some gaps; UDP cannot guarantee capture.

Instrumentation entry points and parser helpers are marked IRAM; input and ring data are internal DRAM. The final ESP32 ELF still needs an instruction/callee audit, because source attributes alone do not prove generated code cache safety. The original real driver's own cache requirements are unchanged.

Run python3 tests/test_wifi_tx_evidence.py for native ASan/UBSan tests compiling the actual parser and wrappers with fake SDK functions. Tests cover truncated/malformed packets, IPv4/IPv6, unsupported headers, UDP exclusion, exact forwarding and returns, nested wrapper calls, lifetime destruction in the real by-ref stub, ring overwrite bounds, and untruncated size_t forwarding. A full ESP32 build/link and ELF IRAM audit remain required before use.

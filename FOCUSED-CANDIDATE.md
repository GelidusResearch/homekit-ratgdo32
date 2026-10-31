# Focused issue 19 candidate

This branch is unreleased and has not been flashed or validated on hardware. Its embedded version is `1.4.5-focused`, distinct from published images. It does not establish the complete freeze cause.

## Source and scope

Started from `16e7a470e82830a2d249a80881ff1c9129206983` (regular 1.4.5), then merged ratgdo main `ac7f751bcfcf272daded9558a3588ace062fc918`. Three communications setup conflicts retain existing GRGDO1 status/dry-contact GPIO guards; the rest of the merge is accepted.

The baseline retains focused OTA pause, allocation-free SSE, logger/core lock ordering, bounded log-download chunks, verified HTTP/SSE connection retirement, and UART/pin ownership fixes. Added OTA recovery `e2ccd02`, browser log-page recovery `06b2bdb` with HTTP503 regression `ebd7aae`, direct ESP8266 log-ring reads `1b86a79`, and project clock types `000ffe4`. The OTA checksum is copied directly into fixed request storage while the argument temporary remains alive, without an extra retained String.

Later 1.4.6 audit changes and the resource refactor are deferred: command ownership/encoder/timer rewrites, JSON/config/auth hardening, SDK upload overrides, asynchronous scans and incremental HTTP body pumping. The original integration and refactor branches are preserved separately.

SSE timer changes `1ddd56a` and six-second unconnected-registration expiry `0aa27e6` are omitted. The baseline timer callback only signals an atomic flag; this choice limits scope and does not prove all timer lifetimes safe. Initial subscription loss remains unexplained. Browser recovery retries a fresh registration after failure.

## Dependencies and resources

- Platform stays at the 1.4.5 baseline, pioarduino `55.03.311`. This differs from 1.4.6's `55.03.312` and requires fresh hardware validation.
- HomeSpan is official 2.1.8, exact `107ffc07f4455754ea89068d8cf2e992de3583e6`. Experimental input/output transport patches are deferred. Native reproductions established mechanisms, but no Apple-client capture ties them to issue 19. Removing custom input reassembly changes retained-buffer behavior; this is not presented as the cause or cure of memory pressure.
- Optional diagnostic build environments are removed. Existing disabled diagnostic source remains available. Ordinary builds add no diagnostic tasks, queues or network collector.

## Remaining gates

Local checks, both target builds and matched artifact hashes are recorded by exact source commit under `Downloads/GRGDO1-Focused-Candidate`. No CI result is claimed here. This candidate must not be published as a release or installer update.

Before promotion, establish live HomeKit/pairing/OTA behavior and sustained recovery on both revisions. Rev2 mDNS allocation errors still need internal byte-addressable free heap, largest block and failure evidence. Broad heap totals, successful builds and log-page recovery cannot establish that the original freeze is fixed.

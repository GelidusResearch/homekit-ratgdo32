#!/usr/bin/env python3
"""Exercise production snapshot code without touching a device or network."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "src/runtime_diagnostics.cpp"


def block(source, declaration):
    start = source.index(declaration)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        depth += (source[i] == "{") - (source[i] == "}")
        if depth == 0:
            return source[start:i + 1]
    raise AssertionError("Unclosed block")


class RuntimeDiagnostics(unittest.TestCase):
    def test_actual_callback_bounds_and_request_ownership(self):
        source = SOURCE.read_text()
        declarations = "\n".join(block(source, name) + ";" for name in
                                 ["struct Connection {", "struct TcpSnapshot {"])
        functions = "\n".join(block(source, name) for name in
                              ["uint32_t elapsedSince(", "uint32_t segmentBytes(", "void snapshotTcp(", "void requestTcpSnapshot("])
        harness = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
constexpr unsigned MAX_PCBS=16, MAX_SEGMENTS=64;
struct tcp_seg { uint32_t len; tcp_seg *next; };
struct tcp_pcb {
    uint16_t local_port, remote_port, snd_queuelen, rto;
    uint8_t state, nrtx; int16_t rtime;
    uint32_t snd_buf, lastack, snd_nxt, snd_lbb;
    tcp_seg *unsent, *unacked; tcp_pcb *next;
};
tcp_pcb *tcp_active_pcbs=nullptr, *tcp_tw_pcbs=nullptr;
std::atomic<uint32_t> tcpPending{0}, tcpRequestedAt{0}, tcpRejected{0}, firstBadHeap{0};
uint32_t now=100;
uint32_t milliseconds() { return now; }
uint32_t uxTaskGetStackHighWaterMark(void *) { return 4096; }
constexpr int ERR_OK=0;
int result=ERR_OK, requests=0;
void (*posted)(void *)=nullptr;
int tcpip_try_callback(void (*callback)(void *), void *) {
    ++requests;
    if (result==ERR_OK) posted=callback;
    return result;
}
''' + declarations + r'''
int queueHandle;
void *tcpQueue=&queueHandle;
TcpSnapshot captured{};
void xQueueOverwrite(void *, const TcpSnapshot *value) { captured=*value; }
''' + functions + r'''
int main() {
    now=101; assert(elapsedSince(100)==1);
    now=0; assert(elapsedSince(UINT32_MAX)==1);
    now=100;
    requestTcpSnapshot();
    assert(requests==1 && tcpPending==1 && tcpRequestedAt==100);
    requestTcpSnapshot(); assert(requests==1);
    now=125; posted(nullptr);
    assert(tcpPending==0 && captured.delay==25 && captured.count==0);

    // Bounded traversal must also terminate on cyclic/corrupt lists.
    tcp_seg segment{100, nullptr}; segment.next=&segment;
    tcp_pcb pcb{};
    pcb.local_port=80; pcb.remote_port=54321; pcb.snd_buf=123;
    pcb.snd_queuelen=4; pcb.nrtx=3; pcb.lastack=10; pcb.snd_nxt=20; pcb.snd_lbb=30;
    pcb.unsent=pcb.unacked=&segment; pcb.next=&pcb;
    tcp_active_pcbs=tcp_tw_pcbs=&pcb;
    requestTcpSnapshot(); posted(nullptr);
    assert(captured.active==MAX_PCBS && captured.count==MAX_PCBS);
    assert(captured.timeWait==MAX_PCBS && captured.truncated==1);
    const auto &c=captured.connections[0];
    assert(c.local==80 && c.remote==54321 && c.sendBuffer==123);
    assert(c.queued==4 && c.retransmits==3 && c.acknowledged==10);
    assert(c.next==20 && c.buffered==30);
    assert(c.unsent==MAX_SEGMENTS*100 && c.unacked==MAX_SEGMENTS*100);

    result=-1;
    requestTcpSnapshot(); assert(tcpPending==0 && tcpRejected==1);
    const int before=requests;
    firstBadHeap=1; requestTcpSnapshot(); assert(requests==before);
    firstBadHeap=0; tcpQueue=nullptr; requestTcpSnapshot(); assert(requests==before);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / "diagnostics.cpp"
            binary = Path(temp) / "diagnostics"
            cpp.write_text(harness)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_every_telemetry_frame_fits_small_packet(self):
        formats = re.findall(r'"(RDIAG [^"\n]+)"', SOURCE.read_text())
        self.assertGreaterEqual(len(formats), 7)
        for template in formats:
            worst = re.sub(r"%l[ux]|%u|%d", lambda match:
                           "ffffffff" if match.group() == "%lx" else
                           "-2147483648" if match.group() == "%d" else "4294967295", template)
            self.assertLess(len(worst), 240, template)


if __name__ == "__main__":
    unittest.main()

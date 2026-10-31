#!/usr/bin/env python3
"""Compile the actual parser and both wrappers with fake SDK forwarding."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]

class TxEvidence(unittest.TestCase):
    def test_parser_forwarding_lifetime_and_ring(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            headers={
                'esp_attr.h':'#define IRAM_ATTR\n#define DRAM_ATTR\n',
                'esp_memory_utils.h':'#pragma once\nextern bool readable; inline bool esp_ptr_in_dram(const void*) { return readable; }\n',
                'freertos/task.h':'#pragma once\n#include <cstdint>\n#define portTICK_PERIOD_MS 1\ninline uint32_t xTaskGetTickCountFromISR() { return 1234; }\n',
                'esp_private/wifi.h':'#pragma once\ntypedef int wifi_interface_t; typedef int esp_err_t;\n',
                'freertos/FreeRTOS.h':'#pragma once\n',
                'freertos/portmacro.h':r'''
#pragma once
#include <cassert>
extern unsigned lockDepth;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void enter(int*) { assert(lockDepth==0); ++lockDepth; }
inline void leave(int*) { assert(lockDepth==1); --lockDepth; }
#define portENTER_CRITICAL_SAFE(m) enter(m)
#define portEXIT_CRITICAL_SAFE(m) leave(m)
'''}
            for name,data in headers.items():
                q=p/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(data)
            harness=p/'harness.cpp'
            harness.write_text(r'''
#include "wifi_tx_evidence.h"
#include <cassert>
#include <vector>
#include <cstdint>
using namespace wifi_tx_evidence;
unsigned lockDepth=0; bool readable=true;
void *expectBuffer, *expectNetstack;
size_t expectLength; int expectInterface, returned, directCalls=0, refCalls=0;
bool nested=false, destroy=false;
std::vector<uint8_t> *owned=nullptr;
extern "C" int __wrap_esp_wifi_internal_tx(int,void*,uint16_t);
extern "C" int __wrap_esp_wifi_internal_tx_by_ref(int,void*,size_t,void*);
extern "C" int __real_esp_wifi_internal_tx(int i,void *b,uint16_t n) {
 assert(lockDepth==0 && i==expectInterface && b==expectBuffer && n==expectLength); ++directCalls;
 if(nested) assert(__wrap_esp_wifi_internal_tx_by_ref(i,b,n,expectNetstack)==returned);
 if(destroy) { owned->clear(); owned->shrink_to_fit(); }
 return returned;
}
extern "C" int __real_esp_wifi_internal_tx_by_ref(int i,void *b,size_t n,void *opaque) {
 assert(lockDepth==0 && i==expectInterface && b==expectBuffer && n==expectLength && opaque==expectNetstack);
 ++refCalls;
 if(destroy) { owned->clear(); owned->shrink_to_fit(); }
 return returned;
}
std::vector<uint8_t> ipv4(unsigned payload=7) {
 std::vector<uint8_t>b(54+payload,0); b[12]=8;b[14]=0x45;b[16]=(40+payload)>>8;b[17]=(40+payload)&255;b[23]=6;
 b[34]=0;b[35]=80;b[36]=0x12;b[37]=0x34;b[38]=0x12;b[39]=0x34;b[40]=0x56;b[41]=0x78;b[46]=0x50;b[47]=0x19; return b;
}
void drain() {Record r;while(wifi_tx_evidence_pop(r)){} }
int main() {
 Header h;
 auto control=ipv4(0); control[47]=0x10; assert(parse(control.data(),control.size(),h)==Parse::Ignored);
 for(unsigned flag: {1U,2U,4U}) { control[47]=flag; assert(parse(control.data(),control.size(),h)==Parse::Captured); }
 auto b=ipv4(); assert(parse(b.data(),b.size(),h)==Parse::Captured);
 assert(h.tcpSequence==0x12345678 && h.payloadLength==7 && h.flags==0x19 && h.sourcePort==80 && h.destinationPort==0x1234);
 for(size_t n=0;n<b.size();++n) {
  auto shortBuffer=std::vector<uint8_t>(b.begin(),b.begin()+n);
  assert(parse(shortBuffer.data(),shortBuffer.size(),h)!=Parse::Captured);
 }
 b[14]=0x44;assert(parse(b.data(),b.size(),h)==Parse::Malformed);b=ipv4();
 b[20]=0x20;assert(parse(b.data(),b.size(),h)==Parse::Unsupported);b=ipv4();
 b[23]=17;assert(parse(b.data(),b.size(),h)==Parse::Ignored);b=ipv4();
 b[46]=0x40;assert(parse(b.data(),b.size(),h)==Parse::Malformed);b=ipv4();
 b[12]=0x81;b[13]=0;assert(parse(b.data(),b.size(),h)==Parse::Unsupported);b=ipv4();
 b[35]=81;assert(parse(b.data(),b.size(),h)==Parse::Ignored);
 std::vector<uint8_t> v6(77,0);v6[12]=0x86;v6[13]=0xdd;v6[14]=0x60;v6[19]=23;v6[20]=6;
 v6[54]=0x15;v6[55]=0xb4;v6[56]=0xab;v6[57]=0xcd;v6[66]=0x50;
 assert(parse(v6.data(),v6.size(),h)==Parse::Captured && h.ipVersion==6 && h.payloadLength==3 && h.sourcePort==5556);
 for(size_t n=0;n<v6.size();++n) {
  auto shortBuffer=std::vector<uint8_t>(v6.begin(),v6.begin()+n);
  assert(parse(shortBuffer.data(),shortBuffer.size(),h)!=Parse::Captured);
 }
 uint32_t random=123;
 for(unsigned n=0;n<3000;++n) {
  std::vector<uint8_t> fuzz(n%180);
  for(auto &byte:fuzz){random^=random<<13;random^=random>>17;random^=random<<5;byte=random;}
  if(fuzz.size()>=14){fuzz[12]=(n&1)?8:0x86;fuzz[13]=(n&1)?0:0xdd;}
  parse(fuzz.data(),fuzz.size(),h);
 }
 v6[20]=0;assert(parse(v6.data(),v6.size(),h)==Parse::Unsupported);
 v6[20]=17;assert(parse(v6.data(),v6.size(),h)==Parse::Ignored);
 v6[20]=6;v6[19]=255;assert(parse(v6.data(),v6.size(),h)==Parse::Malformed);
 b=ipv4();expectBuffer=b.data();expectLength=b.size();expectInterface=1;expectNetstack=&h;returned=-77;
 assert(__wrap_esp_wifi_internal_tx(expectInterface,b.data(),b.size())==-77);
 Record first,last;assert(wifi_tx_evidence_pop(first)&&wifi_tx_evidence_pop(last));assert(!first.completed&&last.completed&&first.call==last.call&&last.result==-77&&last.api==1);
 assert(!wifi_tx_evidence_pop(last));
 returned=0;assert(__wrap_esp_wifi_internal_tx_by_ref(1,b.data(),b.size(),expectNetstack)==0);
 assert(wifi_tx_evidence_pop(first)&&wifi_tx_evidence_pop(last)&&last.api==2&&last.result==0);drain();
 nested=true;__wrap_esp_wifi_internal_tx(1,b.data(),b.size());nested=false;
 std::vector<Record> records;while(wifi_tx_evidence_pop(first))records.push_back(first);
 assert(records.size()==4&&records[0].api==1&&!records[0].completed&&records[1].api==2&&!records[1].completed&&records[2].completed&&records[3].completed);
 readable=false;__wrap_esp_wifi_internal_tx(1,b.data(),b.size());readable=true;assert(!wifi_tx_evidence_pop(first));
 owned=&b;destroy=true;__wrap_esp_wifi_internal_tx_by_ref(1,b.data(),b.size(),expectNetstack);destroy=false;drain();
 b=ipv4();expectBuffer=b.data();expectLength=b.size();
 for(int n=0;n<100;++n)__wrap_esp_wifi_internal_tx(1,b.data(),b.size());
 unsigned count=0;while(wifi_tx_evidence_pop(first))++count;assert(count==64);
 Counters c;wifi_tx_evidence_counters(c);assert(c.overwritten==136&&c.unreadable==1&&c.captured==105);
 // The by-ref size_t parameter is forwarded without uint16 truncation.
 expectLength=65536;__wrap_esp_wifi_internal_tx_by_ref(1,b.data(),65536,expectNetstack);drain();
 assert(directCalls==103&&refCalls==4&&lockDepth==0);
}
''')
            exe=p/'tx-test'
            subprocess.run(['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                            '-DESP32','-DRATGDO_RUNTIME_DIAGNOSTICS','-DRATGDO_WIFI_TX_EVIDENCE',
                            '-I'+str(p),'-I'+str(ROOT/'src'),str(ROOT/'src/wifi_tx_evidence.cpp'),str(harness),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()

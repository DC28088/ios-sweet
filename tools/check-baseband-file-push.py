#!/usr/bin/env python3
"""Check the patched BSP file acknowledgement using synthetic data only."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="sweet-bsp-check-") as tmp:
    work = Path(tmp)
    subprocess.run(["git", "init", "-q", str(work)], check=True)
    for number in (184, 186, 187, 188, 189, 190):
        patch = next((ROOT / "xnu/patches").glob(f"{number:04d}-*.patch"))
        subprocess.run(["git", "-C", str(work), "apply",
                        "--include=iokit/Kernel/arm/SweetBaseband.inc",
                        "--include=iokit/Kernel/arm/coachz_bb_*.h", str(patch)], check=True)
    inc = work / "iokit/Kernel/arm"
    source = (inc / "SweetBaseband.inc").read_text()
    begin = source.index("#define BB_BSP_SEND_FILE")
    end = source.index("// A private request", begin)
    (work / "handler.inc").write_text(source[begin:end])
    (work / "check.cpp").write_text(r'''
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <vector>
#include "coachz_bb_qmux.h"
#include "coachz_bb_data.h"
static uint8_t bb_out[65536];
struct Frame { uint32_t channel; std::vector<uint8_t> bytes; };
static std::vector<Frame> frames;
static void bb_push(uint32_t channel, const uint8_t *p, uint32_t n)
{ frames.push_back({channel, std::vector<uint8_t>(p, p + n)}); }
#include "handler.inc"

static void check(unsigned channel, unsigned cid, unsigned segment,
                  unsigned total, unsigned info_len, bool valid)
{
  frames.clear();
  uint8_t sdu[32] = {};
  sdu[0] = CBQ_QMI_REQUEST;
  cbd_wr16(sdu + 1, 0x1234); // synthetic transaction, not a device identifier
  cbd_wr16(sdu + 3, BB_BSP_SEND_FILE);
  cbd_wr16(sdu + 5, 3 + info_len);
  sdu[7] = 0x01;
  cbd_wr16(sdu + 8, info_len);
  sdu[10] = 9;
  cbd_wr16(sdu + 12, segment);
  cbd_wr16(sdu + 14, total);
  cbq_result r = {};
  r.service = CBD_SVC_BSP; r.cid = cid; r.len = 10 + info_len;
  assert(bb_send_file(channel, r, sdu) == 1);
  const bool last = valid && segment + 1 == total;
  assert(frames.size() == (last ? 2u : 1u));
  assert(frames[0].channel == channel);
  const auto &f = frames[0].bytes;
  assert(f[4] == CBD_SVC_BSP && f[5] == cid);
  assert(cbq_rd16(f.data() + 1) + 1u == f.size());
  const uint8_t *reply = f.data() + CBQ_HDR;
  assert(reply[0] == CBQ_QMI_RESPONSE);
  assert(cbd_rd16(reply + 1) == 0x1234 && cbd_rd16(reply + 3) == BB_BSP_SEND_FILE);
  assert(cbd_sdu_ok(reply, f.size() - CBQ_HDR));
  assert(cbd_error(reply, f.size() - CBQ_HDR) == (valid ? 0u : CBQ_ERR_MALFORMED_MESSAGE));
  uint32_t len = 0;
  const auto *next = cbd_find(reply, f.size() - CBQ_HDR, 0x40, &len);
  if (valid) assert(next && len == 2 && cbd_rd16(next) == segment + 1);
  else assert(!next);
  if (last) {
    assert(frames[1].channel == channel);
    const auto &done = frames[1].bytes;
    assert(done[4] == CBD_SVC_BSP && done[5] == cid);
    const auto *ind = done.data() + CBQ_HDR;
    assert(ind[0] == CBQ_QMI_INDICATION && cbd_rd16(ind + 1) == 0);
    assert(cbd_rd16(ind + 3) == BB_BSP_FILE_TRANSFER_STATUS);
    const auto *status = cbd_find(ind, done.size() - CBQ_HDR, 0x01, &len);
    assert(status && len == 2 && status[0] == 9 && status[1] == 0);
  }
}
int main()
{
  // A complete multi-segment transfer, duplicate ACK, both channels/clients, single segment and u16 boundary.
  for (unsigned ch : {1u, 2u}) {
    for (unsigned segment = 0; segment < 3; ++segment) check(ch, 7, segment, 3, 7, true);
    check(ch, 11, 0, 3, 7, true);
    check(ch, 11, 0, 1, 7, true);
    check(ch, 11, 65534, 65535, 7, true);
    check(ch, 11, 0, 0, 7, false);
    check(ch, 11, 3, 3, 7, false);
    for (unsigned len = 0; len < 6; ++len) check(ch, 11, 0, 1, len, false);
  }
  puts("BSP file acknowledgement checks passed (synthetic traffic).");
}
''')
    exe = work / "check"
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-type-limits", "-I", str(inc), str(work / "check.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

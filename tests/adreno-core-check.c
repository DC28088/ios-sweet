/* Host checks for failure containment at the hardware boundary. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sweet_adreno.h"

#define CHECK(condition) do { if (!(condition)) { \
 fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
struct model {
 uint32_t regs[0x72000 / 4];
 unsigned writes;
 unsigned reads;
 unsigned aon_accesses;
 unsigned cx_snoc_accesses;
 uint32_t bad_read;
 uint32_t stuck;
 int stuck_update;
 int gpucc;
};
static struct model gpucc, gcc, gmu;

static uint32_t
read_reg(void *cookie, uint32_t offset)
{
 struct model *m = cookie;
 CHECK(!(offset & 3) && offset < sizeof(m->regs));
 ++m->reads;
 if (m->gpucc && offset == 0x1004) ++m->aon_accesses;
 if (m->gpucc && offset == 0x108c) ++m->cx_snoc_accesses;
 if (offset == m->bad_read) return 0xffffffffu;
 return m->regs[offset / 4];
}
static void
write_reg(void *cookie, uint32_t offset, uint32_t value)
{
 struct model *m = cookie;
 CHECK(!(offset & 3) && offset < sizeof(m->regs));
 ++m->writes;
 if (m->gpucc && offset == 0x1004) ++m->aon_accesses;
 if (m->gpucc && offset == 0x108c) ++m->cx_snoc_accesses;
 m->regs[offset / 4] = value;
 if (offset == m->stuck) return;
 if (m->gpucc && offset == 0x1120) {
  if (value & 2) m->regs[offset / 4] &= ~(1u << 31);
  if (!m->stuck_update) m->regs[offset / 4] &= ~1u;
 } else if ((offset >= 0x1004 && offset <= 0x109c) ||
     offset == 0x4452c || offset == 0x7100c || offset == 0x71018) {
  if (value & 1) m->regs[offset / 4] &= ~(1u << 31);
 }
}
static void delay_us(void *cookie, unsigned delay) { (void)cookie; CHECK(delay <= 10); }
static struct a618_io io(struct model *m) {
 struct a618_io ops = { m, read_reg, write_reg, delay_us }; return ops;
}
static void reset_model(struct model *m) {
 memset(m, 0, sizeof(*m)); m->bad_read = m->stuck = 0xfffffffc;
}
static void setup(void) {
 reset_model(&gpucc); reset_model(&gcc); reset_model(&gmu);
 gpucc.gpucc = 1;
 gpucc.regs[0x106c / 4] = 0xf8228000;
 gpucc.regs[0x1540 / 4] = 0x80001fe0;
 gpucc.regs[0x1120 / 4] = 0x80000000;
 gcc.regs[0] = 0xc0100001;
 gcc.regs[4 / 4] = 31; gcc.regs[0x38 / 4] = 0x4000;
 gcc.regs[0x52000 / 4] = 0x80;
 gcc.regs[0x52004 / 4] = 0x15;
 static const unsigned branches[] = { 0x1004, 0x109c, 0x1078, 0x108c, 0x1098 };
 for (unsigned i = 0; i < sizeof(branches) / sizeof(*branches); ++i)
  gpucc.regs[branches[i] / 4] = 0x80000000;
 gcc.regs[0x4452c / 4] = gcc.regs[0x7100c / 4] = gcc.regs[0x71018 / 4] = 0x80000000;
 gmu.regs[A618_GMU_SYSRESET / 4] = 1;
}
static void check_clocks(void) {
 struct a618_io g = io(&gpucc), c = io(&gcc);
 struct a618_clock_result r;
 setup();
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == 0);
 CHECK(r.completed == 6);
 CHECK(!gpucc.cx_snoc_accesses);
 CHECK(!gpucc.aon_accesses); /* Not a GMU DT dependency; no AON access. */
 CHECK(gpucc.regs[0x1124 / 4] == 0x602);
 CHECK((gpucc.regs[0x1120 / 4] & 3) == 2);
 CHECK((gpucc.regs[0x1098 / 4] & 0xff1) == 0xff1);
 CHECK(gcc.regs[0x52004 / 4] == 0x10015);
 CHECK(gcc.regs[0x52000 / 4] == 0x81);
 CHECK(gcc.regs[4 / 4] == 31 && gcc.regs[0x38 / 4] == 0x4000);

 setup(); gpucc.regs[0x1004 / 4] = gpucc.regs[0x108c / 4] = 0; /* Hardware unused-gate state. */
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == 0);
 CHECK(!gpucc.aon_accesses && gpucc.regs[0x1004 / 4] == 0);
 CHECK(!gpucc.cx_snoc_accesses && gpucc.regs[0x108c / 4] == 0);
 setup(); gpucc.stuck = 0x109c; /* Required CXO must still be observed on. */
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == ETIMEDOUT);
 CHECK(r.offset == 0x109c && !gpucc.regs[0x1124 / 4]);
 setup(); gcc.regs[0] = 0x40100001; /* ACTIVE suffices in vote-FSM mode. */
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == 0);
 setup(); gcc.regs[0] = 0x80000001; gcc.regs[0x2c / 4] = 1;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == 0);
 CHECK(gcc.regs[0x52000 / 4] == 0x80); /* No irrelevant vote in direct mode. */
 setup(); gcc.regs[0] = 0x80000001; gcc.regs[0x2c / 4] = 0;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == EBUSY);
 CHECK(!gcc.writes && !gpucc.writes);
 setup(); gpucc.regs[0x1540 / 4] = 0;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == EAGAIN);
 CHECK(!gcc.writes && !gpucc.writes);
 setup(); gcc.regs[4 / 4] = 42;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == ERANGE);
 CHECK(!gcc.writes && !gpucc.writes);
 setup(); gcc.bad_read = 0x38;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == EIO);
 CHECK(!gcc.writes && !gpucc.writes);
 setup(); gcc.stuck = 0x7100c;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == ETIMEDOUT);
 CHECK(r.block == 1 && r.offset == 0x7100c && !gpucc.writes);
 setup(); gpucc.stuck = 0x1120;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == ETIMEDOUT);
 CHECK(r.offset == 0x1120 && !gpucc.regs[0x1124 / 4]);
 setup(); gpucc.stuck_update = 1;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == ETIMEDOUT);
 CHECK(r.offset == 0x1120 && !(gpucc.regs[0x1098 / 4] & 1));
 setup(); gpucc.bad_read = 0x1098;
 CHECK(a618_enable_gmu_clocks(&g, &c, &r) == EIO);
 CHECK(r.offset == 0x1098);
 puts("clock path: configuration, shared-bit preservation and failure containment passed");
}
static void check_firmware(void) {
 unsigned char firmware[32768] = { 0x80, 0x3f, 0x04, 0, 9 };
 const uint32_t crc = 0xa3ba296a; /* Python zlib.crc32 of this fixed fixture. */
 struct a618_io gm = io(&gmu);
 unsigned verified;
 setup();
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc, &verified) == 0);
 CHECK(verified == sizeof(firmware) && gmu.writes == sizeof(firmware) / 4);
 CHECK(gmu.regs[A618_GMU_ITCM / 4] == 0x43f80);
 CHECK(gmu.regs[A618_GMU_SYSRESET / 4] == 1);
 setup();
 CHECK(a618_gmu_stage_legacy(&gm, firmware, 32764, crc, &verified) == EINVAL);
 CHECK(!gmu.writes && !gmu.reads && !verified);
 setup();
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc ^ 1, &verified) == EIO);
 CHECK(!gmu.writes && !gmu.reads);
 setup(); gmu.regs[A618_GMU_SYSRESET / 4] = 0;
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc, &verified) == EBUSY);
 CHECK(!gmu.writes);
 setup(); gmu.bad_read = A618_GMU_SYSRESET;
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc, &verified) == EIO);
 CHECK(!gmu.writes);
 setup(); gmu.bad_read = A618_GMU_ITCM + 0x800;
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc, &verified) == EIO);
 CHECK(verified == 0x800 && gmu.writes == 513);
 setup(); firmware[4] = 8;
 CHECK(a618_gmu_stage_legacy(&gm, firmware, sizeof(firmware), crc, &verified) == EINVAL);
 CHECK(!gmu.writes && !gmu.reads);
 puts("firmware staging: bounds, CRC, reset ownership and readback failure checks passed");
}
int main(void) { check_clocks(); check_firmware(); return 0; }

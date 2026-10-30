/* SPDX-License-Identifier: MIT */
/*
 * Host test for the PM4 builder. No device, no Metal, no Mesa: compile with
 * any C++17 compiler and run.
 *
 *     g++ -std=c++17 -I userspace/native/src \
 *         userspace/native/test/pm4_test.cpp -o /tmp/sweet-pm4-test
 *     /tmp/sweet-pm4-test
 */
#include <stdio.h>
#include <stdint.h>

#include "sweet_pm4.h"

static int failures;

static void expect(bool condition, const char *what)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

static void expect_word(const sweet::Pm4 &pm4, size_t index, uint32_t want, const char *what)
{
    if (index >= pm4.size()) {
        fprintf(stderr, "FAIL: %s (missing word %zu)\n", what, index);
        failures++;
        return;
    }
    const uint32_t got = pm4.words()[index];
    if (got != want) {
        fprintf(stderr, "FAIL: %s: word %zu = 0x%08x, want 0x%08x\n", what, index, got, want);
        failures++;
    }
}

int main()
{
    sweet::Pm4 pm4;
    expect(pm4.type4(0xa000, {1, 2, 3}), "type4 accepted");
    expect(pm4.size() == 4, "type4 length");
    expect_word(pm4, 0, 0x40022800u, "type4 header");
    expect_word(pm4, 1, 1, "type4 payload 0");
    expect_word(pm4, 3, 3, "type4 payload 2");

    pm4.clear();
    expect(pm4.type7(sweet::CP_MEM_WRITE, {0x23456789u, 1u, 0xdeadbeefu}),
           "mem_write accepted");
    expect(pm4.size() == 4, "mem_write length");
    expect_word(pm4, 0, 0x7002003du, "mem_write header");

    pm4.clear();
    expect(pm4.mem_write(0x123456789ull, {0xdeadbeefu}), "mem_write helper accepted");
    expect(pm4.size() == 4, "mem_write helper length");
    expect_word(pm4, 0, 0x7002003du, "mem_write helper header");
    expect_word(pm4, 1, 0x23456789u, "mem_write address lo");
    expect_word(pm4, 2, 0x1u, "mem_write address hi");
    expect_word(pm4, 3, 0xdeadbeefu, "mem_write value");

    pm4.clear();
    expect(pm4.event_write(sweet::CACHE_FLUSH_TS | (1u << 31), 0x1000, 7),
           "event_write accepted");
    expect(pm4.size() == 5, "event_write length");
    expect_word(pm4, 0, 0x70030046u, "event_write header");
    expect_word(pm4, 1, 0x80000004u, "event_write event");
    expect_word(pm4, 2, 0x1000u, "event_write address lo");
    expect_word(pm4, 4, 7u, "event_write value");

    pm4.clear();
    expect(pm4.nop(0), "nop accepted");
    expect(pm4.size() == 1, "nop length");
    expect_word(pm4, 0, 0x70000010u, "nop header");

    pm4.clear();
    expect(pm4.wait_for_idle(), "wait_for_idle accepted");
    expect(pm4.size() == 1, "wait_for_idle length");
    expect_word(pm4, 0, 0x70000026u, "wait_for_idle header");

    /* Refusals must not emit anything. */
    pm4.clear();
    expect(!pm4.type4(0x40000, {1}), "type4 refuses register 0x40000");
    expect(!pm4.type4(0xa002, {1}), "type4 refuses a misaligned register");
    expect(!pm4.type4(0xa000, {}), "type4 refuses an empty payload");
    expect(!pm4.type7(0x80, {1}), "type7 refuses opcode 0x80");
    expect(pm4.size() == 0, "refusals emit nothing");

    pm4.clear();
    expect(pm4.type4(0x3ffff & ~3u, {0}), "type4 accepts the top register");
    expect(pm4.size() == 2, "top register length");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("pm4: all checks passed\n");
    return 0;
}

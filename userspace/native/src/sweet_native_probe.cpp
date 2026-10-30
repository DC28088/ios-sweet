/* SPDX-License-Identifier: MIT */
/*
 * The N0 hardware probe for the native driver.
 *
 * Runs inside backboardd when asked (SWEET_NATIVE_PROBE=1 in
 * /var/local/mtl2-env, or a /var/local/mtl2-native-probe file whose first
 * byte is '1'), before the Metal device opens Turnip. It proves the parts a
 * native driver needs and nothing else:
 *
 *   open the NativeAdreno user client (the only process the kernel admits is
 *   backboardd, which is where this code runs)
 *   allocate a buffer, map it at a client IOVA, map it on the CPU
 *   submit CP_MEM_WRITE into that buffer plus a NOP
 *   wait on the real CP fence
 *   read the two words back
 *
 * It does not touch the GPU's render state, and it closes the connection
 * before returning, so the normal SweetMetal2/Turnip path that follows opens
 * the single connection slot as usual.
 *
 * A failed probe logs a sentence and returns; it never aborts backboardd.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sweet_gpu.h"
#include "sweet_pm4.h"

namespace {

constexpr const char *kLogPath = "/var/local/mtl2-native-probe.log";
constexpr const char *kEnvPath = "/var/local/mtl2-env";
constexpr const char *kFlagPath = "/var/local/mtl2-native-probe";

FILE *log_file;

void logf(const char *fmt, ...)
{
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    fprintf(stderr, "SWEET NATIVE PROBE: %s\n", text);
    if (!log_file)
        log_file = fopen(kLogPath, "a");
    if (log_file) {
        fprintf(log_file, "%s\n", text);
        fflush(log_file);
    }
}

bool requested()
{
    if (const char *env = getenv("SWEET_NATIVE_PROBE"))
        if (!strcmp(env, "1"))
            return true;
    if (FILE *f = fopen(kFlagPath, "r")) {
        const int c = fgetc(f);
        fclose(f);
        if (c == '1')
            return true;
    }
    if (FILE *f = fopen(kEnvPath, "r")) {
        char line[256];
        bool on = false;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "SWEET_NATIVE_PROBE=", 19))
                on = !strcmp(line + 19, "1");
        }
        fclose(f);
        if (on)
            return true;
    }
    return false;
}

uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void run()
{
    char why[256] = {0};
    logf("probe: start (pid %d)", (int)getpid());

    SweetGpu *gpu = sweet_gpu_open(why, sizeof(why));
    if (!gpu) {
        logf("probe: FAIL open: %s", why);
        return;
    }

    SweetGpuParams params{};
    SweetGpuInfo info{};
    if (sweet_gpu_params(gpu, &params, why, sizeof(why)) != SweetGpuSuccess ||
        sweet_gpu_info(gpu, &info, why, sizeof(why)) != SweetGpuSuccess) {
        logf("probe: FAIL query: %s", why);
        sweet_gpu_close(gpu);
        return;
    }
    logf("probe: abi=%u input_bits=%u output_bits=%u flags=0x%x max_bo=%llu",
         params.abi_version, params.input_bits, params.output_bits, params.flags,
         (unsigned long long)params.max_bo_bytes);
    logf("probe: chip=0x%08x gmem=%u@0x%llx va=0x%llx+0x%llx bank=%u ubwc=%u/%u macro=%u",
         info.chip_id, info.gmem_bytes, (unsigned long long)info.gmem_base,
         (unsigned long long)info.va_start, (unsigned long long)info.va_size,
         info.highest_bank_bit, info.ubwc_version, info.ubwc_swizzle,
         info.macrotile_mode);

    const uint64_t iova = align_up(info.va_start, 4096);
    const uint64_t bo = sweet_gpu_bo_alloc(gpu, 4096, 12, why, sizeof(why));
    if (!bo) {
        logf("probe: FAIL %s", why);
        sweet_gpu_close(gpu);
        return;
    }
    if (sweet_gpu_bo_map(gpu, bo, iova, 3, why, sizeof(why)) != SweetGpuSuccess) {
        logf("probe: FAIL %s", why);
        sweet_gpu_bo_free(gpu, bo, nullptr, 0);
        sweet_gpu_close(gpu);
        return;
    }
    uint64_t mapped_bytes = 0;
    volatile uint32_t *cpu = (volatile uint32_t *)sweet_gpu_bo_cpu_map(
        gpu, bo, &mapped_bytes, why, sizeof(why));
    if (!cpu) {
        logf("probe: FAIL %s", why);
        sweet_gpu_bo_free(gpu, bo, nullptr, 0);
        sweet_gpu_close(gpu);
        return;
    }
    cpu[0] = 0;
    cpu[1] = 0;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    const uint32_t want0 = 0x600df00du;
    const uint32_t want1 = 0x12345678u;
    sweet::Pm4 pm4;
    const bool built = pm4.mem_write(iova, {want0, want1}) && pm4.nop(2);
    if (!built) {
        logf("probe: FAIL pm4 build");
        sweet_gpu_bo_cpu_unmap(gpu, bo, (void *)cpu, nullptr, 0);
        sweet_gpu_bo_free(gpu, bo, nullptr, 0);
        sweet_gpu_close(gpu);
        return;
    }

    const uint64_t before = sweet_gpu_timestamp(gpu);
    SweetGpuSubmitBuffer buffer{bo, iova, (uint64_t)pm4.size() * 4};
    uint64_t fence = 0;
    const int submitted = sweet_gpu_submit(gpu, &buffer, 1, 0, 0, &fence, why, sizeof(why));
    if (submitted != SweetGpuSuccess) {
        logf("probe: FAIL submit: %s (%d)", why, submitted);
        sweet_gpu_bo_cpu_unmap(gpu, bo, (void *)cpu, nullptr, 0);
        sweet_gpu_bo_free(gpu, bo, nullptr, 0);
        sweet_gpu_close(gpu);
        return;
    }
    logf("probe: submitted %zu words, fence=0x%llx", pm4.size(), (unsigned long long)fence);

    const int waited = sweet_gpu_wait(gpu, fence, 2000000000ull, why, sizeof(why));
    if (waited != SweetGpuSuccess) {
        logf("probe: FAIL wait: %s (%d)", why, waited);
    } else {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        const uint64_t after = sweet_gpu_timestamp(gpu);
        const uint32_t got0 = cpu[0];
        const uint32_t got1 = cpu[1];
        logf("probe: waited, timestamp %llu -> %llu, got 0x%08x 0x%08x",
             (unsigned long long)before, (unsigned long long)after, got0, got1);
        if (got0 == want0 && got1 == want1)
            logf("probe: PASS gpu wrote and fence completed");
        else
            logf("probe: FAIL values (wanted 0x%08x 0x%08x)", want0, want1);
    }

    if (sweet_gpu_bo_cpu_unmap(gpu, bo, (void *)cpu, why, sizeof(why)) != SweetGpuSuccess)
        logf("probe: cpu unmap: %s", why);
    if (sweet_gpu_bo_free(gpu, bo, why, sizeof(why)) != SweetGpuSuccess)
        logf("probe: free: %s", why);
    sweet_gpu_close(gpu);
    logf("probe: end");
}

}  // namespace

extern "C" __attribute__((constructor)) static void sweet_native_probe_entry(void)
{
    try {
        if (requested())
            run();
    } catch (...) {
        logf("probe: FAIL exception");
    }
}

/* Streaming memory bandwidth of the phone's CPUs: read, write (memset) and copy over buffers far larger than
 * the caches, with 1, 2, 4 and 6 threads. Tells what the DRAM can deliver (the GPU shares it). */
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES (96u << 20)
static mach_timebase_info_data_t tb;
static double now(void) { return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9; }

struct job { int mode; uint8_t *a, *b; size_t n; uint64_t sink; int reps; };

static void *run(void *arg)
{
	struct job *j = arg;
	for (int r = 0; r < j->reps; r++) {
		if (j->mode == 0) {
			uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
			const uint64_t *p = (const uint64_t *)j->a;
			for (size_t i = 0; i < j->n / 8; i += 4) { s0 += p[i]; s1 += p[i + 1]; s2 += p[i + 2]; s3 += p[i + 3]; }
			j->sink += s0 + s1 + s2 + s3;
		} else if (j->mode == 1) {
			memset(j->a, r + 1, j->n);
		} else {
			memcpy(j->b, j->a, j->n);
		}
	}
	return NULL;
}

int main(void)
{
	static const char *const names[] = { "read", "write", "copy" };
	mach_timebase_info(&tb);
	for (int threads = 1; threads <= 6; threads = threads == 1 ? 2 : threads == 2 ? 4 : threads == 4 ? 6 : 7) {
		for (int mode = 0; mode < 3; mode++) {
			struct job jobs[6];
			pthread_t t[6];
			size_t each = BYTES / threads & ~(size_t)63;
			for (int i = 0; i < threads; i++) {
				jobs[i] = (struct job){ mode, malloc(each), malloc(each), each, 0, 4 };
				memset(jobs[i].a, 1, each);
				memset(jobs[i].b, 2, each);
			}
			double t0 = now();
			for (int i = 0; i < threads; i++) pthread_create(&t[i], NULL, run, &jobs[i]);
			for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
			double dt = now() - t0;
			double bytes = (double)each * threads * 4 * (mode == 2 ? 2 : 1);
			printf("%d thread%s %-5s %6.2f GB/s (counting %s)\n", threads, threads > 1 ? "s" : " ", names[mode],
			    bytes / dt / 1e9, mode == 2 ? "read+write" : "the traffic");
			for (int i = 0; i < threads; i++) { free(jobs[i].a); free(jobs[i].b); }
		}
	}
	return 0;
}

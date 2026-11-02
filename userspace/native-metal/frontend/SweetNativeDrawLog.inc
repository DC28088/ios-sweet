/* SPDX-License-Identifier: MIT */
/* Draw log (2026-11-02, Control Centre vector glyph diagnosis).
 *
 * Touching /var/local/sweet-native-drawlog (optional content: a draw count,
 * default 1500, at most 20000) logs that many draws: pipeline label and stage
 * functions, primitive and counts, colour attachments, the depth/stencil
 * attachment and state, and the bound fragment textures (Metal pixel format
 * numbers, sizes). The file's mtime is checked once a second; a file present
 * when the process starts does not arm it. Included by redwoodmetal.m. */
#include <sys/stat.h>
#include <time.h>

struct native_drawlog_name {
	const void *render;
	char label[96], vfn[48], ffn[48];
};
static struct native_drawlog_name native_drawlog_names[4096];
static pthread_mutex_t native_drawlog_lock = PTHREAD_MUTEX_INITIALIZER;

static struct native_drawlog_name *
native_drawlog_slot(const void *render, bool insert)
{
	unsigned h = (unsigned)(((uintptr_t)render >> 4) % 4096);
	for (unsigned i = 0; i < 4096; i++) {
		struct native_drawlog_name *e = &native_drawlog_names[(h + i) % 4096];
		if (e->render == render)
			return e;
		if (!e->render)
			return insert ? e : NULL;
	}
	return NULL;
}

static void
native_drawlog_remember(const void *render, const char *label, const char *vfn, const char *ffn)
{
	pthread_mutex_lock(&native_drawlog_lock);
	struct native_drawlog_name *e = native_drawlog_slot(render, true);
	if (e) {
		e->render = render;
		snprintf(e->label, sizeof(e->label), "%s", label ? label : "-");
		snprintf(e->vfn, sizeof(e->vfn), "%s", vfn ? vfn : "-");
		snprintf(e->ffn, sizeof(e->ffn), "%s", ffn ? ffn : "-");
	}
	pthread_mutex_unlock(&native_drawlog_lock);
}

/* Nonzero: the sequence number of a draw to log. */
static unsigned
native_drawlog_next(void)
{
	static uint64_t last_check_ms, seq;
	static long last_sec = -1, last_nsec = -1;
	static unsigned remaining;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	const uint64_t ms = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
	if (ms - last_check_ms >= 1000) {
		last_check_ms = ms;
		struct stat st;
		if (stat("/var/local/sweet-native-drawlog", &st) == 0) {
			if (st.st_mtimespec.tv_sec != last_sec || st.st_mtimespec.tv_nsec != last_nsec) {
				const bool first = last_sec < 0;
				last_sec = st.st_mtimespec.tv_sec;
				last_nsec = st.st_mtimespec.tv_nsec;
				unsigned n = 1500;
				FILE *f = fopen("/var/local/sweet-native-drawlog", "r");
				if (f) {
					unsigned v = 0;
					if (fscanf(f, "%u", &v) == 1 && v)
						n = v > 20000 ? 20000 : v;
					fclose(f);
				}
				if (!first) {
					remaining = n;
					fprintf(stderr, "SweetNativeMetal: DRAWLOG armed for %u draws\n", n);
				}
			}
		} else if (last_sec < 0) {
			last_sec = 0;   /* absent at start: the first touch arms */
		}
	}
	if (!remaining)
		return 0;
	remaining--;
	return (unsigned)++seq;
}

static void
native_drawlog_draw(unsigned seq, const void *render, unsigned prim, unsigned start, unsigned count,
                    bool indexed, unsigned index_type, size_t index_offset, unsigned instances,
                    id const *colors, unsigned ncolors, id depth, bool dss, unsigned ref,
                    id const *textures, unsigned ntextures)
{
	struct native_drawlog_name nm = { render, "?", "?", "?" };
	pthread_mutex_lock(&native_drawlog_lock);
	struct native_drawlog_name *e = native_drawlog_slot(render, false);
	if (e)
		nm = *e;
	pthread_mutex_unlock(&native_drawlog_lock);
	char rt[160] = "", ds[80] = "-", tx[320] = "";
	for (unsigned i = 0; i < ncolors && i < 4; i++) {
		id t = colors[i];
		size_t at = strlen(rt);
		snprintf(rt + at, sizeof(rt) - at, "%s%u:%ux%u", i ? "," : "",
		         t ? (unsigned)[t pixelFormat] : 0, t ? (unsigned)[t width] : 0,
		         t ? (unsigned)[t height] : 0);
	}
	if (depth != nil)
		snprintf(ds, sizeof(ds), "%u:%ux%u%s ref%u", (unsigned)[depth pixelFormat],
		         (unsigned)[depth width], (unsigned)[depth height], dss ? " dss" : "", ref);
	for (unsigned i = 0; i < ntextures && i < 8; i++) {
		id t = textures[i];
		if (t == nil)
			continue;
		size_t at = strlen(tx);
		snprintf(tx + at, sizeof(tx) - at, "%s%u:%u:%ux%u", at ? "," : "", i,
		         (unsigned)[t pixelFormat], (unsigned)[t width], (unsigned)[t height]);
	}
	fprintf(stderr, "SweetNativeMetal: DRAW %u %s %s|%s prim %u start %u n %u idx %s@%zu inst %u rt %s ds %s tex %s\n",
	        seq, nm.label, nm.vfn, nm.ffn, prim, start, count,
	        indexed ? (index_type ? "u32" : "u16") : "-", index_offset, instances, rt, ds,
	        tx[0] ? tx : "-");
}

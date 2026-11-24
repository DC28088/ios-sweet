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
	/* vertex descriptor: up to four attributes and the first four layouts */
	unsigned nattr, attr_fmt[4], attr_off[4], attr_buf[4], stride[4];
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
native_drawlog_remember(const void *render, const char *label, const char *vfn, const char *ffn,
                        id descriptor)
{
	unsigned nattr = 0, fmt[4] = {0}, off[4] = {0}, buf[4] = {0}, stride[4] = {0};
	id vd = descriptor != nil && [descriptor respondsToSelector:@selector(vertexDescriptor)]
	        ? [descriptor vertexDescriptor] : nil;
	if (vd != nil) {
		for (unsigned i = 0; i < 31 && nattr < 4; i++) {
			id a = [[vd attributes] objectAtIndexedSubscript:i];
			if (a == nil || [a format] == 0)
				continue;
			fmt[nattr] = (unsigned)[a format];
			off[nattr] = (unsigned)[a offset];
			buf[nattr] = (unsigned)[a bufferIndex];
			nattr++;
		}
		for (unsigned b = 0; b < 4; b++)
			stride[b] = (unsigned)[[[vd layouts] objectAtIndexedSubscript:b] stride];
	}
	pthread_mutex_lock(&native_drawlog_lock);
	struct native_drawlog_name *e = native_drawlog_slot(render, true);
	if (e) {
		e->render = render;
		e->nattr = nattr;
		memcpy(e->attr_fmt, fmt, sizeof(fmt));
		memcpy(e->attr_off, off, sizeof(off));
		memcpy(e->attr_buf, buf, sizeof(buf));
		memcpy(e->stride, stride, sizeof(stride));
		snprintf(e->label, sizeof(e->label), "%s", label ? label : "-");
		snprintf(e->vfn, sizeof(e->vfn), "%s", vfn ? vfn : "-");
		snprintf(e->ffn, sizeof(e->ffn), "%s", ffn ? ffn : "-");
	}
	pthread_mutex_unlock(&native_drawlog_lock);
}

/* Nonzero: the sequence number of a draw to log. */
/* Optional second token in the file, "WxH": decode vertices and constants of logged
 * draws whose first colour attachment has that size (0x0: every draw of <= 64 vertices). */
static unsigned native_drawlog_detail_w, native_drawlog_detail_h;
static bool native_drawlog_detail_on;
/* 2026-11-15 (iOS 26 slow frames): /var/local/sweet-native-drawtime present when the log is armed ->
 * each logged draw is flushed and waited for and its time logged as "DRAWTIME seq us" (serialises the GPU). */
static bool native_drawlog_time_on;

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
				native_drawlog_detail_on = false;
				FILE *f = fopen("/var/local/sweet-native-drawlog", "r");
				if (f) {
					unsigned v = 0, w = 0, h = 0;
					if (fscanf(f, "%u", &v) == 1 && v)
						n = v > 20000 ? 20000 : v;
					if (fscanf(f, " %ux%u", &w, &h) == 2) {
						native_drawlog_detail_on = true;
						native_drawlog_detail_w = w;
						native_drawlog_detail_h = h;
					}
					fclose(f);
				}
				if (!first) {
					struct stat ts;
					remaining = n;
					native_drawlog_time_on = stat("/var/local/sweet-native-drawtime", &ts) == 0;
					fprintf(stderr, "SweetNativeMetal: DRAWLOG armed for %u draws%s\n", n,
					        native_drawlog_time_on ? " (timed: each draw flushed and waited for)" : "");
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

/* One attribute of one vertex, decoded by MTLVertexFormat. */
static void
native_drawlog_attr(char *out, size_t size, unsigned fmt, const uint8_t *p)
{
	switch (fmt) {
	case 28: case 29: case 30: case 31: {          /* Float .. Float4 */
		const float *f = (const float *)p;
		const unsigned n = fmt - 27;
		snprintf(out, size, n == 1 ? "%.2f" : n == 2 ? "%.2f,%.2f" : n == 3 ? "%.2f,%.2f,%.2f" : "%.2f,%.2f,%.2f,%.2f",
		         f[0], n > 1 ? f[1] : 0, n > 2 ? f[2] : 0, n > 3 ? f[3] : 0);
		break;
	}
	case 25: case 26: case 27: {                   /* Half2 .. Half4 */
		const __fp16 *h = (const __fp16 *)p;
		const unsigned n = fmt - 23;
		snprintf(out, size, n == 2 ? "h%.3f,%.3f" : n == 3 ? "h%.3f,%.3f,%.3f" : "h%.3f,%.3f,%.3f,%.3f",
		         (float)h[0], (float)h[1], n > 2 ? (float)h[2] : 0.f, n > 3 ? (float)h[3] : 0.f);
		break;
	}
	case 9: case 42:                               /* UChar4Normalized (_BGRA) */
		snprintf(out, size, "u8:%u,%u,%u,%u%s", p[0], p[1], p[2], p[3], fmt == 42 ? "bgra" : "");
		break;
	default:
		snprintf(out, size, "f%u:%02x%02x%02x%02x%02x%02x%02x%02x", fmt, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	}
}

/* Vertices (up to six, through the index buffer) and the first eight floats of every
 * vertex/fragment buffer slot, for a draw the detail filter selects. */
static void
native_drawlog_detail(unsigned seq, const void *render, id color0, unsigned start, unsigned count,
                      id index_buffer, unsigned index_type, size_t index_offset, int base_vertex,
                      id const *vbufs, const size_t *voffs, unsigned nvb,
                      id const *fbufs, const size_t *foffs, unsigned nfb)
{
	if (!native_drawlog_detail_on)
		return;
	const unsigned w = color0 ? (unsigned)[color0 width] : 0, h = color0 ? (unsigned)[color0 height] : 0;
	if (native_drawlog_detail_w || native_drawlog_detail_h) {
		if (w != native_drawlog_detail_w || h != native_drawlog_detail_h)
			return;
	} else if (count > 64) {
		return;
	}
	struct native_drawlog_name nm = { render };
	pthread_mutex_lock(&native_drawlog_lock);
	struct native_drawlog_name *e = native_drawlog_slot(render, false);
	if (e)
		nm = *e;
	pthread_mutex_unlock(&native_drawlog_lock);
	char line[640];
	for (unsigned k = 0; k < count && k < 6; k++) {
		unsigned vtx = start + k;
		if (index_buffer != nil) {
			const uint8_t *ib = (const uint8_t *)[index_buffer contents];
			if (!ib)
				break;
			vtx = index_type ? ((const uint32_t *)(ib + index_offset))[start + k]
			                 : ((const uint16_t *)(ib + index_offset))[start + k];
			vtx += base_vertex;
		}
		size_t used = (size_t)snprintf(line, sizeof(line), "SweetNativeMetal: DRAWV %u v%u:", seq, vtx);
		for (unsigned a = 0; a < nm.nattr && used < sizeof(line); a++) {
			const unsigned b = nm.attr_buf[a];
			if (b >= nvb || b >= 4 || vbufs[b] == nil || !nm.stride[b])
				continue;
			const uint8_t *base = (const uint8_t *)[vbufs[b] contents];
			const size_t len = (size_t)[vbufs[b] length];
			const size_t at = voffs[b] + (size_t)vtx * nm.stride[b] + nm.attr_off[a];
			if (!base || at + 16 > len)
				continue;
			char v[96];
			native_drawlog_attr(v, sizeof(v), nm.attr_fmt[a], base + at);
			used += (size_t)snprintf(line + used, sizeof(line) - used, " a%u(b%u)=%s", a, b, v);
		}
		fprintf(stderr, "%s\n", line);
	}
	for (unsigned stage = 0; stage < 2; stage++) {
		id const *bufs = stage ? fbufs : vbufs;
		const size_t *offs = stage ? foffs : voffs;
		const unsigned n = stage ? nfb : nvb;
		/* 26c: glass draws get every fragment slot up to 7, the whole uniform block (as floats and as halves)
		 * and tiny buffers (setFragmentBytes of a half, e.g. edr_scale) as raw bytes */
		const bool glass = strstr(nm.ffn, "glass") != NULL;
		for (unsigned i = 0; i < n && i < (glass ? 8u : 6u); i++) {
			if (bufs[i] == nil)
				continue;
			const uint8_t *base = (const uint8_t *)[bufs[i] contents];
			const size_t len = (size_t)[bufs[i] length];
			if (!base || offs[i] >= len)
				continue;
			if (offs[i] + 32 > len) {
				char raw[80] = "";
				for (size_t k = 0; k < len - offs[i] && k < 16; k++)
					snprintf(raw + strlen(raw), sizeof(raw) - strlen(raw), "%02x", base[offs[i] + k]);
				fprintf(stderr, "SweetNativeMetal: DRAWB %u %s%u@%zu raw %s (%zu bytes)\n", seq, stage ? "f" : "v", i,
				        offs[i], raw, len - offs[i]);
				continue;
			}
			const float *f = (const float *)(base + offs[i]);
			fprintf(stderr, "SweetNativeMetal: DRAWB %u %s%u@%zu %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
			        seq, stage ? "f" : "v", i, offs[i], f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
			if (glass && stage && offs[i] + 272 <= len) {
				for (unsigned row = 1; row < 9; row++)
					fprintf(stderr, "SweetNativeMetal: DRAWB %u f%u+%u %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n", seq, i,
					        row * 32, f[row * 8], f[row * 8 + 1], f[row * 8 + 2], f[row * 8 + 3], f[row * 8 + 4],
					        f[row * 8 + 5], f[row * 8 + 6], f[row * 8 + 7]);
				const __fp16 *hh = (const __fp16 *)(base + offs[i] + 128);
				char hs[400] = "";
				for (unsigned k = 0; k < 72; k++)
					snprintf(hs + strlen(hs), sizeof(hs) - strlen(hs), " %.2f", (float)hh[k]);
				fprintf(stderr, "SweetNativeMetal: DRAWB %u f%u+128 halves%s\n", seq, i, hs);
			}
		}
	}
}

/* 26c A/B test (USER: glass grey): /var/local/sweet-glass-noinf present -> before a draw whose fragment function is
 * a glass shader, +-inf floats in fragment buffer 1's uniform block (GlassBackgroundUniforms, 272 B) become
 * +-65504 (half max). Hypothesis: inf * 0 = NaN in the translated half math darkens the glass; 65504 * 0 = 0 and
 * large values still saturate to 1. Checked once a second; counts are logged. */
static bool
native_glass_noinf_on(void)
{
	static time_t last;
	static bool on;
	const time_t now = time(NULL);
	if (now != last) {
		struct stat st;
		last = now;
		on = stat("/var/local/sweet-glass-noinf", &st) == 0;
	}
	return on;
}
static void
native_glass_noinf(const void *render, id const *fbufs, const size_t *foffs, unsigned nfb)
{
	if (!native_glass_noinf_on() || nfb < 2 || fbufs[1] == nil)
		return;
	pthread_mutex_lock(&native_drawlog_lock);
	struct native_drawlog_name *e = native_drawlog_slot(render, false);
	const bool glass = e && strstr(e->ffn, "glass") != NULL;
	pthread_mutex_unlock(&native_drawlog_lock);
	if (!glass)
		return;
	uint8_t *base = (uint8_t *)[fbufs[1] contents];
	const size_t len = (size_t)[fbufs[1] length];
	if (!base || foffs[1] + 272 > len)
		return;
	float *f = (float *)(base + foffs[1]);
	unsigned fixed = 0;
	for (unsigned k = 0; k < 68; k++)
		if (isinf(f[k])) {
			f[k] = f[k] > 0 ? 65504.0f : -65504.0f;
			fixed++;
		}
	static unsigned draws, total;
	total += fixed;
	if (++draws <= 4 || (draws & (draws - 1)) == 0)
		fprintf(stderr, "SweetNativeMetal: glass-noinf %u draws, %u inf floats clamped (%s)\n", draws, total, e->ffn);
}

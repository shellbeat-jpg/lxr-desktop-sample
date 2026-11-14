/*
 * wavwrite.h
 *
 * MOD: a small WAV writer for the host tests (header only, C99, stdio).
 * Writes the chunks in the order they are called, little endian whatever the
 * host is, and patches the RIFF and data sizes on close:
 *
 *   WavWriter w;
 *   if(wavw_open(&w, "%s/%s.wav", dir, name) == 0)
 *   {
 *       wavw_fmt(&w, WAV_FORMAT_PCM, 1, 44100, 16);
 *       wavw_chunk(&w, "LIST", data, len);	// optional, before or after fmt
 *       wavw_data(&w);
 *       wavw_s16(&w, sample); ...
 *       wavw_close(&w);
 *   }
 *
 * Every error (path too long, open, write, seek, close, calls in the wrong
 * order) is reported on stderr and counted in wavw_errors: a test checks it
 * at the end. After an error the writer ignores further calls.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef WAVWRITE_H_
#define WAVWRITE_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define WAV_FORMAT_PCM		1
#define WAV_FORMAT_FLOAT	3
#define WAV_PATH_MAX		1024	/* longer paths are an error, not cut off */
#define WAV_TAG_SIZE		4		/* chunk ids ("RIFF", "fmt ", ...) */
#define WAV_CHUNK_HEAD		8		/* id + size */
#define WAV_FMT_SIZE		16		/* fmt chunk of PCM and float */
#define WAV_RIFF_SIZE_POS	4		/* file offset of the RIFF size */

typedef struct
{
	FILE* f;
	long dataSizePos;		/* file offset of the data chunk's size, 0 = no data chunk yet */
	uint16_t blockAlign;	/* bytes per frame, 0 = no fmt chunk yet */
	int failed;
	char path[WAV_PATH_MAX];
} WavWriter;

static int wavw_errors = 0;

static inline void wavw_fail(WavWriter* w, const char* what)
{
	fprintf(stderr, "WAV %s: %s\n", w->path[0] ? w->path : "(no path)", what);
	wavw_errors++;
	w->failed = 1;
}

static inline void wavw_put(WavWriter* w, const void* b, const size_t n)
{
	if(w->failed) return;
	if(fwrite(b, 1, n, w->f) != n) wavw_fail(w, "write failed");
}

static inline void wavw_le16(WavWriter* w, const uint16_t v)
{
	const uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
	wavw_put(w, b, sizeof b);
}

static inline void wavw_le32(WavWriter* w, const uint32_t v)
{
	const uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
	wavw_put(w, b, sizeof b);
}

static inline void wavw_tag(WavWriter* w, const char* tag)
{
	if(strlen(tag) != WAV_TAG_SIZE) { wavw_fail(w, "chunk id is not 4 characters"); return; }
	wavw_put(w, tag, WAV_TAG_SIZE);
}

/* creates the file (path as printf format) and writes "RIFF" <size> "WAVE"; 0 = ok */
static inline int wavw_open(WavWriter* w, const char* pathFmt, ...)
{
	memset(w, 0, sizeof *w);
	va_list ap;
	va_start(ap, pathFmt);
	const int n = vsnprintf(w->path, sizeof w->path, pathFmt, ap);
	va_end(ap);
	if(n < 0 || (size_t)n >= sizeof w->path) { w->path[0] = 0; wavw_fail(w, "path too long"); return -1; }
	w->f = fopen(w->path, "wb");
	if(!w->f) { wavw_fail(w, "cannot create"); return -1; }
	wavw_tag(w, "RIFF");
	wavw_le32(w, 0);	/* patched on close */
	wavw_tag(w, "WAVE");
	return w->failed ? -1 : 0;
}

/* the fmt chunk: format WAV_FORMAT_PCM (8..32 bit) or WAV_FORMAT_FLOAT (32 bit) */
static inline void wavw_fmt(WavWriter* w, const uint16_t format, const uint16_t channels, const uint32_t rate, const uint16_t bits)
{
	if(w->failed) return;
	if(w->blockAlign) { wavw_fail(w, "second fmt chunk"); return; }
	if(!channels || !bits || bits % 8) { wavw_fail(w, "bad channels or bits"); return; }
	w->blockAlign = (uint16_t)(channels * (bits / 8));
	wavw_tag(w, "fmt ");
	wavw_le32(w, WAV_FMT_SIZE);
	wavw_le16(w, format);
	wavw_le16(w, channels);
	wavw_le32(w, rate);
	wavw_le32(w, rate * w->blockAlign);	/* bytes per second */
	wavw_le16(w, w->blockAlign);
	wavw_le16(w, bits);
}

/* any other chunk before the data chunk; an odd length gets the pad byte of RIFF */
static inline void wavw_chunk(WavWriter* w, const char* tag, const void* data, const uint32_t len)
{
	if(w->failed) return;
	if(w->dataSizePos) { wavw_fail(w, "chunk after the data chunk"); return; }
	wavw_tag(w, tag);
	wavw_le32(w, len);
	wavw_put(w, data, len);
	if(len & 1) wavw_put(w, "", 1);
}

/* starts the data chunk: the samples follow */
static inline void wavw_data(WavWriter* w)
{
	if(w->failed) return;
	if(!w->blockAlign) { wavw_fail(w, "data chunk before the fmt chunk"); return; }
	wavw_tag(w, "data");
	w->dataSizePos = ftell(w->f);
	if(w->dataSizePos <= 0) { wavw_fail(w, "ftell failed"); return; }
	wavw_le32(w, 0);	/* patched on close */
}

static inline void wavw_s16(WavWriter* w, const int16_t v) { wavw_le16(w, (uint16_t)v); }

/* 24 bit: the low 3 bytes of v */
static inline void wavw_s24(WavWriter* w, const int32_t v)
{
	const uint32_t u = (uint32_t)v;
	const uint8_t b[3] = { (uint8_t)u, (uint8_t)(u >> 8), (uint8_t)(u >> 16) };
	wavw_put(w, b, sizeof b);
}

static inline void wavw_f32(WavWriter* w, const float v)
{
	uint32_t u;
	memcpy(&u, &v, sizeof u);
	wavw_le32(w, u);
}

/* patches the sizes and closes; 0 = ok (also counted in wavw_errors) */
static inline int wavw_close(WavWriter* w)
{
	if(!w->f) return -1;
	if(!w->failed && !w->dataSizePos) wavw_fail(w, "no data chunk");
	if(!w->failed)
	{
		const long end = ftell(w->f);
		if(end < w->dataSizePos + 4) wavw_fail(w, "ftell failed");
		else
		{
			const uint32_t data = (uint32_t)(end - w->dataSizePos - 4);
			if(data & 1) wavw_put(w, "", 1);	/* pad byte of an odd data chunk */
			const uint32_t riff = (uint32_t)(end + (data & 1) - WAV_CHUNK_HEAD);
			if(fseek(w->f, WAV_RIFF_SIZE_POS, SEEK_SET) != 0) wavw_fail(w, "seek failed");
			wavw_le32(w, riff);
			if(!w->failed && fseek(w->f, w->dataSizePos, SEEK_SET) != 0) wavw_fail(w, "seek failed");
			wavw_le32(w, data);
		}
	}
	if(fclose(w->f) != 0 && !w->failed) wavw_fail(w, "close failed");
	w->f = NULL;
	return w->failed ? -1 : 0;
}

#endif /* WAVWRITE_H_ */

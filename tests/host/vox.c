/* MOD: host test of the vox engine's sound (VoxCore.h): stability, level, demo renders.
 * Usage: gcc -O2 -ICore/Inc tests/host/vox.c -lm && ./a.out [dir for wavs] */
#include "Soundengine/DSPAudio/VoxCore.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define FS 44140.f
/* demo WAVs: 16 bit PCM mono at FS, little endian. A NULL dir (no demo output wanted, only the
 * checks run) gives a NULL file, which put() and wavClose() accept. Write errors count as test
 * failures (wavErrors). */
#define WAV_HEADER_SIZE	44	/* RIFF + fmt + data chunk of a plain PCM WAV */
#define WAV_FMT_SIZE	16	/* fmt chunk of PCM */
#define WAV_PCM			1
#define WAV_CHANNELS	1
#define WAV_BITS		16
static int wavErrors = 0;
static FILE* wavOpen(const char* dir, const char* name)
{
	if(!dir) return NULL;
	char p[512];
	const int n = snprintf(p, sizeof p, "%s/%s.wav", dir, name);
	if(n < 0 || (size_t)n >= sizeof p) { fprintf(stderr, "WAV path too long: %s/%s\n", dir, name); wavErrors++; return NULL; }
	FILE* f = fopen(p, "wb");
	if(!f) { perror(p); wavErrors++; return NULL; }
	if(fseek(f, WAV_HEADER_SIZE, SEEK_SET) != 0) { perror(p); fclose(f); wavErrors++; return NULL; }
	return f;
}
static uint8_t* le16(uint8_t* p, const uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint8_t* le32(uint8_t* p, const uint32_t v) { for(int i=0;i<4;i++) p[i] = (uint8_t)(v >> (8*i)); return p + 4; }
static uint8_t* tag(uint8_t* p, const char t[4]) { memcpy(p, t, 4); return p + 4; }
/* writes the header for the data written so far and closes the file */
static void wavClose(FILE* f)
{
	if(!f) return;
	const long end = ftell(f);
	if(end < WAV_HEADER_SIZE) { fclose(f); wavErrors++; return; }
	const uint32_t data = (uint32_t)(end - WAV_HEADER_SIZE);
	const uint16_t align = WAV_CHANNELS * WAV_BITS / 8;
	const uint32_t rate = (uint32_t)FS;
	uint8_t h[WAV_HEADER_SIZE], *p = h;
	p = tag(p, "RIFF"); p = le32(p, WAV_HEADER_SIZE - 8 + data); p = tag(p, "WAVE");
	p = tag(p, "fmt "); p = le32(p, WAV_FMT_SIZE); p = le16(p, WAV_PCM); p = le16(p, WAV_CHANNELS);
	p = le32(p, rate); p = le32(p, rate * align); p = le16(p, align); p = le16(p, WAV_BITS);
	p = tag(p, "data"); p = le32(p, data);
	const int ok = (p - h == WAV_HEADER_SIZE) && fseek(f, 0, SEEK_SET) == 0 && fwrite(h, sizeof h, 1, f) == 1;
	if(fclose(f) != 0 || !ok) { fprintf(stderr, "WAV header not written\n"); wavErrors++; }
}
/* one sample, x * g clipped to -1..1 */
static void put(FILE* w, const float x, const float g)
{
	if(!w) return;
	float v = x * g;
	if(v > 1.f) v = 1.f;
	if(v < -1.f) v = -1.f;
	const int16_t s = (int16_t)(v * 32767.f);
	uint8_t b[2];
	le16(b, (uint16_t)s);
	if(fwrite(b, sizeof b, 1, w) != 1) wavErrors++;
}
static float renderVowel(VoxParams p, float f0, float sec, FILE* w)
{
	VoxCore v; memset(&v, 0, sizeof v);
	voxCore_setParams(&v, &p); voxCore_setPitch(&v, f0, FS); voxCore_trigger(&v);
	int n = (int)(sec * FS); float* buf = malloc(n * sizeof(float)); float pk = 0;
	for(int b=0;b+32<=n;b+=32) voxCore_render(&v, 0, buf + b, 32, FS);
	for(int i=0;i<n;i++) { if(!(buf[i]==buf[i])) { free(buf); return -1; } if(fabsf(buf[i])>pk) pk=fabsf(buf[i]); }
	for(int i=0;i<n;i++) put(w, buf[i], 0.4f / (pk + 1e-9f));
	free(buf); return pk;
}
int main(int argc, char** argv)
{
	const char* dir = argc > 1 ? argv[1] : 0;
	static const char* names[VOX_VOWELS] = { "u", "o", "O", "a", "E", "e", "i", "y" };
	int fail = 0;
	/* 1. the vowels at a male pitch (formants: checked offline, see VoxCore.h) */
	FILE* w = wavOpen(dir, "vox_vowels");
	for(int i=0;i<VOX_VOWELS;i++)
	{
		VoxParams p = { .vow = (uint8_t)(i * 127 / (VOX_VOWELS-1)), .len = 64, .tns = 64, .veg = 63 };
		float pk = renderVowel(p, 110.f, 0.6f, w);
		printf("%s  peak %.3f\n", names[i], pk);
		if(!(pk > 0.01f && pk < 50.f)) { printf("   FEHLER: Pegel\n"); fail++; }
	}
	wavClose(w);
	/* 2. stability: extremes of every parameter, both sources, low and high pitch */
	float maxpk = 0, minpk = 1e9;
	for(int src=0;src<2;src++) for(int vow=0;vow<=127;vow+=127) for(int nas=0;nas<=127;nas+=127)
	for(int len=0;len<=127;len+=127) for(int chd=0;chd<VOX_CHORDS;chd+=VOX_CHORDS-1) for(int tns=0;tns<=127;tns+=127)
	for(int fi=0;fi<3;fi++)
	{
		const float f0 = src ? (float[]){300,1500,4000}[fi] : (float[]){30,200,1200}[fi];
		VoxParams p = { .vow = (uint8_t)vow, .nas = (uint8_t)nas, .len = (uint8_t)len, .tns = (uint8_t)tns, .src = (uint8_t)src, .veg = 63, .chd = (uint8_t)chd };
		float pk = renderVowel(p, f0, 0.4f, 0);
		if(pk < 0.005f || pk > 20.f) { printf("FEHLER: src %d vow %d nas %d len %d chd %d tns %d f0 %.0f: peak %.3f\n", src, vow, nas, len, chd, tns, f0, pk); fail++; }
		if(pk > maxpk) maxpk = pk;
		if(pk >= 0 && pk < minpk) minpk = pk;
	}
	printf("Extremwerte: Pegel %.4f .. %.3f\n", minpk, maxpk);
	/* 2b. the syrinx sounds at both ends of the tension, at a low and a high pitch */
	for(int tns=0;tns<=127;tns+=127) for(int hi=0;hi<2;hi++)
	{
		VoxCore v; memset(&v, 0, sizeof v); float x[32]; double s = 0;
		VoxParams p = { .vow = 54, .len = 64, .tns = (uint8_t)tns, .src = 1, .veg = 63 };
		voxCore_setParams(&v, &p); voxCore_setPitch(&v, hi ? 800.f : 55.f, FS); voxCore_trigger(&v);
		for(int i=0;i<1000;i++) { voxCore_render(&v, 0, x, 32, FS); if(i >= 500) for(int j=0;j<32;j++) s += x[j]*x[j]; }
		const double rms = sqrt(s / (500 * 32));
		if(rms < 0.1) { printf("FEHLER: Syrinx leise bei tns %d f0 %s (rms %.4f)\n", tns, hi ? "800" : "55", rms); fail++; }
	}
	/* 3. demos */
	if(dir)
	{
		VoxCore v; float buf[32];
		struct { const char* name; int src; float f0; int nas, len, chd, tns; int sweep; } d[] = {
			{ "vox_sweep",  0, 110, 0, 64, 0, 64, 1 },
			{ "vox_nasal",  0, 110, 100, 64, 0, 64, 1 },
			{ "vox_child",  0, 280, 0, 25, 1, 50, 1 },
			{ "vox_giant",  0, 55, 0, 127, 0, 90, 1 },
			{ "vox_chord",  0, 110, 0, 64, 4, 40, 1 },
			{ "syr_tone",   1, 2500, 0, 30, 0, 30, 1 },
			{ "syr_pulse",  1, 900, 0, 40, 0, 110, 1 },
		};
		for(unsigned k=0;k<sizeof d/sizeof d[0];k++)
		{
			FILE* f = wavOpen(dir, d[k].name);
			memset(&v, 0, sizeof v);
			VoxParams p = { .nas = (uint8_t)d[k].nas, .len = (uint8_t)d[k].len, .tns = (uint8_t)d[k].tns, .src = (uint8_t)d[k].src, .veg = 63, .chd = (uint8_t)d[k].chd };
			voxCore_setParams(&v, &p); voxCore_setPitch(&v, d[k].f0, FS); voxCore_trigger(&v);
			int nb = (int)(3.f * FS / 32); float pk = 1e-6f;
			float* all = malloc(nb * 32 * sizeof(float));
			for(int b=0;b<nb;b++)
			{
				const float t = b / (float)nb;
				p.vow = (uint8_t)(127 * (0.5f - 0.5f * cosf(2 * 3.14159f * t)));
				if(d[k].src) p.vow = (uint8_t)(t * 127);
				voxCore_setParams(&v, &p);
				if(d[k].src) voxCore_setPitch(&v, d[k].f0 * (1.f + 0.3f * sinf(2*3.14159f*3*t)), FS);
				voxCore_render(&v, 0, all + b*32, 32, FS);
			}
			for(int i=0;i<nb*32;i++) if(fabsf(all[i])>pk) pk=fabsf(all[i]);
			for(int i=0;i<nb*32;i++) { float env = i < 400 ? i/400.f : (i > nb*32-2000 ? (nb*32-i)/2000.f : 1.f); put(f, all[i]*env, 0.5f/pk); }
			free(all); wavClose(f);
		}
		/* drum-like: short triggers with the AEG done here as a decay */
		FILE* f = wavOpen(dir, "vox_hits");
		memset(&v, 0, sizeof v);
		for(int hit=0;hit<16;hit++)
		{
			VoxParams p = { .vow = (uint8_t)((hit*37)%128), .nas = (uint8_t)(hit%4==3?90:0), .len = (uint8_t)(hit%2?40:70), .tns = 80, .src = (uint8_t)(hit%8==7), .veg = (uint8_t)(hit%3==0 ? 10 : (hit%3==1 ? 120 : 63)), .chd = (uint8_t)(hit%5==4 ? 4 : 0) };
			voxCore_setParams(&v, &p); voxCore_setPitch(&v, hit%8==7 ? 1800.f : (hit%2 ? 180.f : 90.f), FS); voxCore_trigger(&v);
			for(int b=0;b<(int)(0.25f*FS/32);b++)
			{
				const float e = expf(-(b*32)/FS*14.f);
				voxCore_setEnv(&v, e * e);	/* vowel EG as in VoxVoice */
				voxCore_render(&v, 0, buf, 32, FS);
				for(int i=0;i<32;i++) { float t = (b*32+i)/FS; put(f, buf[i] * expf(-t*14.f), 0.4f); }
			}
		}
		wavClose(f);
	}
	/* 4. time per sample on this host (only a hint for the STM32) */
	{
		VoxCore v; memset(&v, 0, sizeof v); float buf[32]; volatile float sink = 0;
		VoxParams p = { .vow = 40, .nas = 60, .len = 64, .tns = 64, .veg = 63, .chd = 7 };
		voxCore_setParams(&v, &p); voxCore_setPitch(&v, 110, FS); voxCore_trigger(&v);
		for(int s=0;s<2;s++)
		{
			p.src = (uint8_t)s; voxCore_setParams(&v, &p); voxCore_setPitch(&v, s ? 2000.f : 110.f, FS);
			clock_t c0 = clock(); const int nb = 200000;
			for(int b=0;b<nb;b++) { voxCore_render(&v, 0, buf, 32, FS); sink += buf[0]; }
			double ns = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e9 / (nb * 32.0);
			printf("%s: %.1f ns pro Sample (Host)\n", s ? "Syrinx" : "Stimme+Nase", ns);
		}
	}
	if(wavErrors) { printf("FEHLER: %d beim Schreiben der WAVs\n", wavErrors); fail++; }
	printf(fail ? "VOX: %d FEHLER\n" : "VOX: ok\n", fail);
	return fail != 0;
}

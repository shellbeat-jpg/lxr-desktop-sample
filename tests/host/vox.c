/* MOD: host test of the vox engine's sound (VoxCore.h): stability, level, demo renders.
 * Usage: gcc -O2 -ICore/Inc tests/host/vox.c -lm && ./a.out [dir for wavs] */
#include "Soundengine/DSPAudio/VoxCore.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define FS 44140.f
static FILE* wavOpen(const char* dir, const char* name)
{
	if(!dir) return 0;
	char p[512]; snprintf(p, sizeof p, "%s/%s.wav", dir, name);
	FILE* f = fopen(p, "wb"); if(!f) return 0;
	fseek(f, 44, SEEK_SET); return f;
}
static void wavClose(FILE* f)
{
	if(!f) return;
	long n = ftell(f) - 44; uint32_t sr = 44140, br = sr * 2, sz = (uint32_t)n, rs = sz + 36;
	fseek(f, 0, SEEK_SET);
	fwrite("RIFF", 1, 4, f); fwrite(&rs, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	uint32_t fl = 16; uint16_t fmt = 1, ch = 1, ba = 2, bps = 16;
	fwrite(&fl, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
	fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&sz, 4, 1, f); fclose(f);
}
static void put(FILE* w, float x, float g)
{
	if(!w) return;
	float v = x * g; if(v > 1) v = 1; if(v < -1) v = -1; int16_t s = (int16_t)(v * 32767); fwrite(&s, 2, 1, w);
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
		VoxParams p = { (uint8_t)(i * 127 / (VOX_VOWELS-1)), 0, 64, 0, 64, 0 };
		float pk = renderVowel(p, 110.f, 0.6f, w);
		printf("%s  peak %.3f\n", names[i], pk);
		if(!(pk > 0.01f && pk < 50.f)) { printf("   FEHLER: Pegel\n"); fail++; }
	}
	wavClose(w);
	/* 2. stability: extremes of every parameter, both sources, low and high pitch */
	float maxpk = 0, minpk = 1e9;
	for(int src=0;src<2;src++) for(int vow=0;vow<=127;vow+=127) for(int nas=0;nas<=127;nas+=127)
	for(int len=0;len<=127;len+=127) for(int bre=0;bre<=127;bre+=127) for(int tns=0;tns<=127;tns+=127)
	for(int fi=0;fi<3;fi++)
	{
		const float f0 = src ? (float[]){300,1500,4000}[fi] : (float[]){30,200,1200}[fi];
		VoxParams p = { (uint8_t)vow, (uint8_t)nas, (uint8_t)len, (uint8_t)bre, (uint8_t)tns, (uint8_t)src };
		float pk = renderVowel(p, f0, 0.4f, 0);
		if(pk < 0.005f || pk > 20.f) { printf("FEHLER: src %d vow %d nas %d len %d bre %d tns %d f0 %.0f: peak %.3f\n", src, vow, nas, len, bre, tns, f0, pk); fail++; }
		if(pk > maxpk) maxpk = pk;
		if(pk >= 0 && pk < minpk) minpk = pk;
	}
	printf("Extremwerte: Pegel %.4f .. %.3f\n", minpk, maxpk);
	/* 3. demos */
	if(dir)
	{
		VoxCore v; float buf[32];
		struct { const char* name; int src; float f0; int nas, len, bre, tns; int sweep; } d[] = {
			{ "vox_sweep",  0, 110, 0, 64, 10, 64, 1 },
			{ "vox_nasal",  0, 110, 100, 64, 10, 64, 1 },
			{ "vox_child",  0, 280, 0, 25, 20, 50, 1 },
			{ "vox_giant",  0, 55, 0, 127, 10, 90, 1 },
			{ "vox_breath", 0, 140, 0, 64, 127, 0, 1 },
			{ "syr_tone",   1, 2500, 0, 30, 20, 30, 1 },
			{ "syr_pulse",  1, 900, 0, 40, 120, 110, 1 },
		};
		for(unsigned k=0;k<sizeof d/sizeof d[0];k++)
		{
			FILE* f = wavOpen(dir, d[k].name);
			memset(&v, 0, sizeof v);
			VoxParams p = { 0, (uint8_t)d[k].nas, (uint8_t)d[k].len, (uint8_t)d[k].bre, (uint8_t)d[k].tns, (uint8_t)d[k].src };
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
			VoxParams p = { (uint8_t)((hit*37)%128), (uint8_t)(hit%4==3?90:0), (uint8_t)(hit%2?40:70), 30, 80, (uint8_t)(hit%8==7) };
			voxCore_setParams(&v, &p); voxCore_setPitch(&v, hit%8==7 ? 1800.f : (hit%2 ? 180.f : 90.f), FS); voxCore_trigger(&v);
			for(int b=0;b<(int)(0.25f*FS/32);b++)
			{
				voxCore_render(&v, 0, buf, 32, FS);
				for(int i=0;i<32;i++) { float t = (b*32+i)/FS; put(f, buf[i] * expf(-t*14.f), 0.05f); }
			}
		}
		wavClose(f);
	}
	/* 4. time per sample on this host (only a hint for the STM32) */
	{
		VoxCore v; memset(&v, 0, sizeof v); float buf[32]; volatile float sink = 0;
		VoxParams p = { 40, 60, 64, 20, 64, 0 };
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
	printf(fail ? "VOX: %d FEHLER\n" : "VOX: ok\n", fail);
	return fail != 0;
}

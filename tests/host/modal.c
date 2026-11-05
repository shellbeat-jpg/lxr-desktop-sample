/* MOD: host test of the modal engine's sound (ModalCore.h): stability, level,
 * decay. Usage: gcc -O1 -ICore/Inc tests/host/modal.c -lm && ./a.out [wav] */
#include "Soundengine/DSPAudio/ModalCore.h"
#include <stdio.h>
#include <string.h>
#define FS 44140.f
static float run(ModalParams p, float f0, float sec, float* t60out, FILE* wav)
{
	ModalCore m; memset(&m, 0, sizeof(m));
	modalCore_setParams(&m, &p, FS); modalCore_setPitch(&m, f0, FS); modalCore_strike(&m, &p);
	int16_t ex[32] = {0}; float out[32]; float peak = 0; int n = (int)(sec * FS / 32); int lastLoud = 0;
	for(int b=0;b<n;b++)
	{
		modalCore_render(&m, ex, out, 32);
		for(int i=0;i<32;i++)
		{
			float a = fabsf(out[i]); if(a > peak) peak = a;
			if(a > 0.001f * peak) lastLoud = b*32+i;
			if(wav) { float v = out[i] * 0.5f; if(v>1)v=1; if(v<-1)v=-1; int16_t s = (int16_t)(v*32767); fwrite(&s,2,1,wav); }
			if(!(out[i] == out[i])) { printf("NaN\n"); return -1; }
		}
	}
	*t60out = lastLoud / FS;
	return peak;
}
int main(int argc, char** argv)
{
	static const char* names[] = { "mem", "bar", "str", "bel", "plt", "gls" };
	FILE* w = argc > 1 ? fopen(argv[1], "wb") : 0;
	int fail = 0;
	for(int model=0;model<MODAL_MODELS;model++)
	for(int hard=0;hard<=127;hard+=127)
	for(int noise=0;noise<2;noise++)
	{
		ModalParams p = { (uint8_t)model, 70, 30, 0, (uint8_t)hard, 60, (uint8_t)noise };
		float t; float pk = run(p, 220.f, 3.f, &t, w);
		printf("%s hard %3d %s  peak %.3f  -60dB at %.2f s\n", names[model], hard, noise ? "nse" : "mal", pk, t);
		if(!(pk > 0.5f && pk < 4.f) || t < 0.2f || t > 1.5f) { printf("   FEHLER: Pegel oder Abklingzeit\n"); fail++; }
	}
	float t, pk;
	ModalParams lo = { 2, 127, 64, 127, 127, 0, 0 }, hi = { 1, 0, 0, 0, 127, 127, 1 }, top = { 5, 64, 64, 127, 127, 0, 1 };
	pk = run(lo, 40.f, 8.f, &t, 0);	printf("str long 40 Hz: peak %.3f, -60dB at %.2f s\n", pk, t);
	if(!(pk > 0.5f && pk < 4.f) || t < 5.f) { printf("   FEHLER\n"); fail++; }
	pk = run(hi, 4000.f, 1.f, &t, 0);	printf("bar short 4 kHz: peak %.3f, -60dB at %.3f s\n", pk, t);
	if(!(pk > 0.05f && pk < 4.f) || t > 0.1f) { printf("   FEHLER\n"); fail++; }
	pk = run(top, 12000.f, 1.f, &t, 0);	printf("gls 12 kHz (modes above 20 kHz): peak %.3f\n", pk);
	if(!(pk >= 0.f && pk < 4.f)) { printf("   FEHLER\n"); fail++; }
	if(w) fclose(w);
	printf(fail ? "MODAL: %d FEHLER\n" : "MODAL: ok\n", fail);
	return fail != 0;
}

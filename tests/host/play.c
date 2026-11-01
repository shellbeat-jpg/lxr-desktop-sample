#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/MIDI/MidiNoteNumbers.h"
#include "SampleRom/sampleFlash.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
int harness_main(void);
const int16_t sine_table[TABLESIZE+1]; const int16_t sawTable[11][1024]; const int16_t triTable[11][1024]; const int16_t recTable[11][1024];
uint32_t GetRngValue(void){return 0;}
#define SEMITONE_UP 1.059463094f
float midiParser_calcDetune(uint8_t value){return 1+(value/127.f-0.5f)*(SEMITONE_UP-1);}
void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, const uint8_t size, const float gain, const float pwm);
void calcNextOscSampleFmBlock(OscInfo* osc, int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain, const float pwm);
void osc_setFreq(OscInfo* osc); void osc_setBaseNote(OscInfo* osc, uint8_t baseNote);
extern char lcdbuf[]; 
int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("   FAIL: %s\n",msg);fails++;} else printf("   ok: %s\n",msg);}while(0)
static void wav16(const char* name,int rate,int frames,int16_t (*f)(int)){
  char b[256];snprintf(b,256,"sd/SAMPLES/%s",name);FILE* fp=fopen(b,"wb");int data=frames*2,riff=36+data,fl=16,br=rate*2;short fmt=1,c=1,ba=2,bb=16;
  fwrite("RIFF",1,4,fp);fwrite(&riff,4,1,fp);fwrite("WAVEfmt ",1,8,fp);fwrite(&fl,4,1,fp);fwrite(&fmt,2,1,fp);fwrite(&c,2,1,fp);fwrite(&rate,4,1,fp);fwrite(&br,4,1,fp);fwrite(&ba,2,1,fp);fwrite(&bb,2,1,fp);
  fwrite("data",1,4,fp);fwrite(&data,4,1,fp);for(int i=0;i<frames;i++){int16_t s=f(i);fwrite(&s,2,1,fp);}fclose(fp);}
static int16_t ramp(int i){return (int16_t)(i*7-12000);}
static int16_t sine(int i){return (int16_t)(20000*sin(2*M_PI*i/100.0));}
static OscInfo mk(uint8_t slot){OscInfo o;memset(&o,0,sizeof o);o.waveform=NUM_OSC_WAVEFORMS+slot;o.pitchMod=1;o.modNodeValue=1;o.fmMod=0;o.midiFreq=(60<<8)|64;return o;}
static int render(OscInfo* o,int16_t* out,int n,int fm,int16_t modval){int k=0;int16_t buf[32],mod[32];for(int i=0;i<32;i++)mod[i]=modval;
  while(k<n){osc_setFreq(o); if(fm) calcNextOscSampleFmBlock(o,mod,buf,32,1.f,0); else calcNextOscSampleBlock(o,buf,32,1.f,0); for(int i=0;i<32&&k<n;i++)out[k++]=buf[i];} return k;}
int main(){
  void* m=mmap((void*)0x08080000,0x80000,PROT_READ|PROT_WRITE,MAP_FIXED_NOREPLACE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0); assert(m==(void*)0x08080000); memset(m,0xFF,0x80000);
  system("rm -rf sd; mkdir -p sd/SAMPLES");
  wav16("01RAMP.WAV",(int)REAL_FS,3000,ramp); wav16("02SINE.WAV",48000,4800,sine);
  SampleFlashResult r=sampleFlash_importAtBoot(); CHECK(r==SF_OK_IMPORTED,"Import 2 Samples");
  CHECK(sampleFlash_entry(0)&&sampleFlash_entry(1)&&!sampleFlash_entry(2),"Slots 1-2 belegt, Slot 3 leer");
  static int16_t out[20000];
  /* 1: Grundton, Rate = REAL_FS -> bitgenau */
  OscInfo o=mk(0); osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60]; osc_setFreq(&o);
  CHECK(o.phaseInc==(1u<<17),"Grundton C4 -> genau 1 Frame pro Ausgabesample");
  o=mk(0); osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60];
  { int k=0; int16_t buf[32]; while(k<3200){ osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,0); for(int i=0;i<32;i++) out[k++]=buf[i]; } }
  int exact=1; for(int i=0;i<3000;i++) if(out[i]!=ramp(i)) {exact=0; printf("   diff at %d: %d vs %d\n",i,out[i],ramp(i)); break;}
  CHECK(exact,"Wiedergabe in Originalhoehe ist bitgenau");
  int silent=1; for(int i=3000;i<3200;i++) if(out[i]) silent=0; CHECK(silent,"One-Shot: nach dem Ende Stille");
  CHECK(o.phase==3000,"Position bleibt am Ende stehen");
  /* 2: Retrigger */
  osc_setBaseNote(&o,60); { int16_t buf[32]; calcNextOscSampleBlock(&o,buf,32,1.f,0); CHECK(buf[0]==ramp(0)&&buf[31]==ramp(31),"Trigger setzt auf Anfang zurueck"); }
  /* 3: Oktave hoeher via Coarse 72 */
  o=mk(0); o.midiFreq=(72<<8)|64; osc_setBaseNote(&o,60); osc_setFreq(&o);
  double ratio=o.phaseInc/(double)(1u<<17); CHECK(fabs(ratio-2.0)<0.002,"Coarse +12 -> doppelte Geschwindigkeit");
  /* 4: Rate-Korrektur 48k */
  o=mk(1); o.freq=MidiNoteFrequencies[60]; osc_setFreq(&o); ratio=o.phaseInc/(double)(1u<<17);
  CHECK(fabs(ratio-48000.0/REAL_FS)<0.0005,"48-kHz-Sample: Tempo korrigiert (Faktor 1.0875)");
  /* sine period check: 100 samples at 48k = 480 Hz; at output FS=44140 -> period 100*44140/48000=91.96 samples */
  o=mk(1); o.freq=MidiNoteFrequencies[60]; osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60];
  { int k=0; int16_t buf[32]; while(k<4000){ osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,0); for(int i=0;i<32;i++) out[k++]=buf[i]; } }
  int zc=0,first=-1,last=-1; for(int i=1;i<4000;i++) if(out[i-1]<0&&out[i]>=0){ if(first<0) first=i; last=i; zc++; } double period=(double)(last-first)/(zc-1);
  printf("      gemessene Periode %.2f Samples (soll %.2f)\n",period,100*REAL_FS/48000.0);
  CHECK(fabs(period-100*REAL_FS/48000.0)<0.2,"Tonhoehe stimmt nach Ratenkorrektur");
  /* 5: Interpolation bei halber Geschwindigkeit */
  o=mk(0); o.midiFreq=(48<<8)|64; osc_setBaseNote(&o,60); 
  { int k=0; int16_t buf[32]; while(k<64){ osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,0); for(int i=0;i<32;i++) out[k++]=buf[i]; } }
  int interp=1; for(int i=0;i<60;i+=2){ int a=ramp(i/2), b=ramp(i/2+1); if(abs(out[i]-a)>2 || abs(out[i+1]-(a+b)/2)>3) {interp=0; printf("   %d: %d %d exp %d %d\n",i,out[i],out[i+1],a,(a+b)/2); break;} }
  CHECK(interp,"Coarse -12: halbe Geschwindigkeit, Zwischenwerte interpoliert");
  /* 6: leerer Slot */
  o=mk(5); osc_setBaseNote(&o,60); int16_t b2[32]; memset(b2,0x55,sizeof b2); calcNextOscSampleBlock(&o,b2,32,1.f,0);
  int z=1; for(int i=0;i<32;i++) if(b2[i]) z=0; CHECK(z,"Leerer Slot (z.B. Kit mit s6 bei nur 2 Samples) -> Stille, kein Absturz");
  o=mk(200); calcNextOscSampleBlock(&o,b2,32,1.f,0); CHECK(b2[0]==0,"Ungueltiger Wellenformwert -> Stille");
  /* 7: FM ohne Modulation = normal; FM mit Modulation verschiebt Leseposition */
  o=mk(0); o.freq=MidiNoteFrequencies[60]; osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60]; o.fmMod=1.f;
  { int k=0; int16_t buf[32],mod[32]={0}; while(k<256){ osc_calcUserSampleFreq(&o); calcNextOscSampleFmBlock(&o,mod,buf,32,1.f,0); for(int i=0;i<32;i++) out[k++]=buf[i]; } }
  int same=1; for(int i=0;i<256;i++) if(out[i]!=ramp(i)) same=0; CHECK(same,"FM-Pfad ohne Modulation = identisch");
  o=mk(0); osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60]; o.fmMod=1.f;
  { int16_t buf[32],mod[32]; for(int i=0;i<32;i++) mod[i]=800; osc_calcUserSampleFreq(&o); calcNextOscSampleFmBlock(&o,mod,buf,32,1.f,0); CHECK(buf[0]==ramp(100),"FM verschiebt Leseposition (+800 -> +100 Frames)"); 
    for(int i=0;i<32;i++) mod[i]=-32768; o.phase=0; calcNextOscSampleFmBlock(&o,mod,buf,32,1.f,0); CHECK(buf[0]==ramp(0),"FM negativ wird am Anfang begrenzt"); }
  /* 8: Extreme Tonhoehe begrenzt */
  o=mk(0); o.freq=20000; o.pitchMod=8; osc_setFreq(&o); CHECK(o.phaseInc<=64u*(1u<<17),"Extreme Pitch-Modulation begrenzt (max 64x)");
  o.freq=NAN; osc_setFreq(&o); CHECK(o.phaseInc==0,"NaN-Frequenz -> kein Unsinn");
  /* 9: Start (PWM) und Laenge (Schritt 5) */
  #define PLAY(pwm,len,N) do{ o=mk(0); o.freq=MidiNoteFrequencies[60]; o.smpLen=(len); osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60]; \
     int k=0; int16_t buf[32]; while(k<(N)){ osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,(pwm)); for(int i=0;i<32;i++) out[k++]=buf[i]; } }while(0)
  PLAY(0.5f,0,3200);  /* start = 0.5^2 * 3000 = 750 */
  CHECK(abs(out[0]) <= abs(ramp(750))/32+1,"Start 50%: Einblenden beginnt leise");
  CHECK(out[40]==ramp(790),"Start 50% (quadratisch) -> ab Frame 750");
  CHECK(out[2249]==ramp(2999) && out[2250]==0,"ohne Laenge bis zum Ende, ohne Ausblenden");
  PLAY(0.f,64,1000);  /* len = (64/127)^2 * 3000 = 761 */
  CHECK(out[690]==ramp(690),"Laenge: vor dem Ausblenden unveraendert");
  CHECK(out[740]==(int16_t)((ramp(740)*21)/64),"Laenge: Ausblenden ueber 64 Frames vor dem Ende");
  { int s=1; for(int i=761;i<1000;i++) if(out[i]) s=0; CHECK(s,"Laenge 64 -> Ende bei Frame 761, danach Stille"); }
  PLAY(0.9f,127,3200); /* start 2430, len = ganzes Sample -> Ende = Sampleende */
  { int st=(int)(0.9f*0.9f*3000.f); CHECK(out[100]==ramp(st+100) && out[3000-st-1]==ramp(2999) && out[3000-st]==0,"Start+Laenge ueber das Ende -> Ende des Samples"); }
  PLAY(1.f,0,64); CHECK(out[1]==0,"Start 100% -> letzter Frame, kein Lesen hinter dem Sample");
  PLAY(NAN,0,64); CHECK(out[40]==ramp(40),"PWM NaN -> Start 0");
  PLAY(2.5f,0,64); CHECK(out[1]==0,"PWM > 1 (Modulation) begrenzt");
  o=mk(0); o.smpLen=1; o.freq=MidiNoteFrequencies[60]; osc_setBaseNote(&o,60); { int16_t buf[32]; int k=0; while(k<128){ osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,0); for(int i=0;i<32;i++) out[k++]=buf[i];} }
  CHECK(out[63]!=0 || out[62]!=0,"Mindestlaenge 64 Frames"); CHECK(out[64]==0,"... und dann Ende");
  /* FM-Pfad: gleicher Start */
  o=mk(0); o.freq=MidiNoteFrequencies[60]; osc_setBaseNote(&o,60); o.fmMod=1.f;
  { int16_t buf[32],mod[32]={0}; for(int b=0;b<2;b++){ osc_calcUserSampleFreq(&o); calcNextOscSampleFmBlock(&o,mod,buf,32,1.f,0.5f);} CHECK(buf[31]==ramp(750+63),"FM-Pfad: Start 50% ebenso"); }
  /* Wellenform-Wechsel waehrend Wiedergabe: Ende eines anderen Samples */
  o=mk(1); o.smpLen=0; osc_setBaseNote(&o,60); o.freq=MidiNoteFrequencies[60]; { int16_t buf[32]; osc_calcUserSampleFreq(&o); calcNextOscSampleBlock(&o,buf,32,1.f,0.f); o.waveform=NUM_OSC_WAVEFORMS; o.smpEnd=999999; calcNextOscSampleBlock(&o,buf,32,1.f,0.f); CHECK(1,"veraltetes Ende wird begrenzt (kein Absturz)"); }
  /* 10: Loop (Schritt 7) - Slot 0 als Loop markieren (Header im Test-Flash patchen) */
  { SampleFlashEntry* le=(SampleFlashEntry*)&sampleFlash_getHeader()->entry[0]; le->flags|=SF_FLAG_LOOP; }
  PLAY(0.f,0,7000);   /* ganzes Sample (3000) als Loop, Start 0 -> Einbruch statt Crossfade */
  CHECK(out[3000]!=0 || out[3001]!=0 || out[3020]!=0,"Loop: nach dem Ende geht es weiter");
  CHECK(out[3040]==ramp(40) && out[6040]==ramp(40),"Loop: springt an den Start zurueck (jede Runde)");
  CHECK(abs(out[2999]) < abs(ramp(2999))/16+1,"Loop mit Start 0: Ausblenden vor dem Loop-Punkt");
  PLAY(0.5f,64,4000); /* Start 750, Laenge 761 -> Loop 750..1511, Crossfade min(190,256)=190 */
  { int st=750, ln=761, xf=190, ok=1;
    for(int k=0;k<3*ln;k++){ int pos=st+(k%ln); int toEnd=st+ln-pos; int exp=ramp(pos);
      if(toEnd<=xf) exp=(ramp(pos)*toEnd + ramp(pos-ln)*(xf-toEnd))/xf;
      if(k<32) exp=(exp*(k+1))/32;   /* Einblenden nur beim ersten Start */
      if(out[k]!=(int16_t)exp){ printf("   k=%d out=%d exp=%d\n",k,out[k],exp); ok=0; break; } }
    CHECK(ok,"Loop zwischen Sta und Len, Crossfade mit dem Material vor dem Start, Einblenden nur einmal"); }
  CHECK(out[3*761+100]!=0,"Loop laeuft weiter (Ende nur durch die Huellkurve)");
  { SampleFlashEntry* le=(SampleFlashEntry*)&sampleFlash_getHeader()->entry[0]; le->flags&=~SF_FLAG_LOOP; }
  PLAY(0.f,0,3200); CHECK(out[3100]==0,"ohne Loop-Kennung wieder One-Shot");
  /* 11: modulierte Sample-Auswahl */
  { const uint8_t W=NUM_OSC_WAVEFORMS;
    CHECK(sampleFlash_modulatedWave(W, (W)/127.f, W)==W,"Sample-Basis ohne Modulation bleibt");
    CHECK(sampleFlash_modulatedWave(W, (W+1)/127.f, W)==W+1,"+1/127 -> naechstes Sample");
    CHECK(sampleFlash_modulatedWave(W, (W+9)/127.f, W)==W+1,"ueber dem letzten Sample begrenzt (nur 2 da)");
    CHECK(sampleFlash_modulatedWave(W+1, 0.f, W)==W,"unter s1 begrenzt, nie eine normale Wellenform");
    CHECK(sampleFlash_modulatedWave(2, 0.5f, W)==3,"normale Wellenform: Verhalten wie bisher");
    CHECK(sampleFlash_modulatedWave(W, NAN, W)==W,"NaN -> s1");
    /* Bereich: Basis s1, Amount 1/127 -> s1..s2, gleichmaessig quantisiert */
    CHECK(sampleFlash_modulatedWave(W, sampleFlash_rangeValue(W, 1/127.f, 0.f), W)==W,"Bereich: Wert 0 -> erstes Sample");
    CHECK(sampleFlash_modulatedWave(W, sampleFlash_rangeValue(W, 1/127.f, 0.49f), W)==W,"Bereich: untere Haelfte -> erstes Sample");
    CHECK(sampleFlash_modulatedWave(W, sampleFlash_rangeValue(W, 1/127.f, 0.51f), W)==W+1,"Bereich: obere Haelfte -> zweites Sample");
    CHECK(sampleFlash_modulatedWave(W, sampleFlash_rangeValue(W, 1/127.f, 1.f), W)==W+1,"Bereich: Wert 1 -> letztes Sample des Bereichs");
    CHECK(fabsf(sampleFlash_rangeValue(W+4, 4/127.f, 0.6f)*127.f-(W+7))<0.01f,"Bereich s5..s9: 0.6 -> s8 (5 gleiche Stufen)");
    CHECK(fabsf(sampleFlash_rangeValue(W+4, 0.f, 0.9f)*127.f-(W+4))<0.01f,"Amount 0 -> immer das gewaehlte Sample"); }
  printf("\n%s (%d Fehler)\n", fails?"FEHLER":"ALLE TESTS BESTANDEN", fails); return fails; }

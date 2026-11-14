#define _GNU_SOURCE
#include "main.h"
#include "ff.h"
#include "lcd.h"
#include "SampleRom/sampleFlash.h"
#include "SampleRom/wtBuild.h"
#include <sys/mman.h>
void* dw_open(const char*); const char* dw_next(void*); void dw_close(void*);
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "wavwrite.h"
static char root[512];
int eraseCalls=0, badErase=0, badProg=0, progCount=0, failAfter=-1, locked=1;
int bigOk=1, bigErase=0, failUpper=0, maxSector=0;
uint8_t host_bigAvailable(void){ return (uint8_t)bigOk; }
void* rec_scratch(uint32_t n){ static char b[16384] __attribute__((aligned(8))); return n<=sizeof b ? b : 0; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t* err){
  eraseCalls++; if(locked) badErase++;
  if(e->Sector>=6 && e->Sector<=11) memset((void*)(uintptr_t)(0x08080000+(e->Sector-6)*0x40000),0xFF,0x40000);
  else { badErase++; }
  if(e->Sector>7) bigErase++;
  if((int)e->Sector>maxSector) maxSector=(int)e->Sector;
  *err=0xFFFFFFFF; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t t, uint32_t a, uint64_t d){
  if(locked) badProg++;
  if(a<0x08080000||a>=0x08200000||(a&3)) { badProg++; return HAL_ERROR; }
  if(failUpper && a>=0x08100000) return HAL_ERROR;
  if(failAfter>=0 && progCount>=failAfter) return HAL_ERROR;
  uint32_t* p=(uint32_t*)(uintptr_t)a; if(*p!=0xFFFFFFFF) { badProg++; }
  *p &= (uint32_t)d; progCount++; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Unlock(void){locked=0;return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void){locked=1;return HAL_OK;}
void SCB_CleanInvalidateDCache(void){}
void HAL_Delay(uint32_t x){(void)x;}
static char lcdbuf[64]; static int quiet=1, sawSkip=0;
void lcd_clear(void){lcdbuf[0]=0;} void lcd_home(void){}
void lcd_string(const char* s){if(strstr(s,"skip"))sawSkip=1; strcat(lcdbuf,s); strcat(lcdbuf,"|"); if(!quiet) printf("   LCD: %s\n",lcdbuf);}
void lcd_setcursor(uint8_t x,uint8_t y){(void)x;(void)y;}
static void mkpath(char* out,const char* p){snprintf(out,600,"%s%s",root,p);}
FRESULT f_open(FIL* f,const char* p,BYTE m){char b[600];mkpath(b,p);f->f=fopen(b,"rb");return f->f?FR_OK:FR_NO_FILE;}
FRESULT f_close(FIL* f){if(f->f)fclose(f->f);f->f=0;return FR_OK;}
FRESULT f_read(FIL* f,void* b,UINT n,UINT* br){*br=fread(b,1,n,f->f);return FR_OK;}
FRESULT f_lseek(FIL* f,FSIZE_t o){return fseek(f->f,o,SEEK_SET)?FR_DISK_ERR:FR_OK;}
FSIZE_t f_tell_impl(FIL* f){return ftell(f->f);}
FRESULT f_opendir(DIR* d,const char* p){mkpath(d->path,p);d->d=dw_open(d->path);return d->d?FR_OK:FR_NO_PATH;}
FRESULT f_readdir(DIR* d,FILINFO* fi){const char* en=dw_next(d->d); memset(fi,0,sizeof(*fi)); if(!en)return FR_OK;
  if(!strcmp(en,".")||!strcmp(en,"..")) return f_readdir(d,fi);
  char b[900];snprintf(b,900,"%s/%s",d->path,en);struct stat st;stat(b,&st);
  strncpy(fi->fname,en,12);fi->fsize=st.st_size;fi->fattrib=S_ISDIR(st.st_mode)?AM_DIR:0;return FR_OK;}
FRESULT f_closedir(DIR* d){dw_close(d->d);return FR_OK;}
/* WAV writer (tests/host/wavwrite.h): a sample file in /SAMPLES, 16 or 24 bit PCM, a test
 * pattern from the name; extraChunk: an odd "LIST" chunk (with its pad byte) before fmt */
static void wav(const char* name,int ch,int bits,int rate,int frames,int extraChunk){
  WavWriter w; if(wavw_open(&w,"%s/SAMPLES/%s",root,name)) return;
  if(extraChunk) wavw_chunk(&w,"LIST","abcde",5);
  wavw_fmt(&w,WAV_FORMAT_PCM,(uint16_t)ch,(uint32_t)rate,(uint16_t)bits);
  wavw_data(&w);
  for(int i=0;i<frames;i++)for(int k=0;k<ch;k++){ int v=(i*37+k*1000+ (int)(name[0])*11)%30000-15000; if(bits==16) wavw_s16(&w,(int16_t)v); else wavw_s24(&w,v*256);}
  wavw_close(&w);}
/* MOD: a wavetable file in /SAMPLES/TABLES: cycles of L points (cycle c: a sine
 * with harmonic 1..c+1, phase shifted by c*0.3 rad), 16 bit or 32 bit float,
 * optionally with a Serum "clm " chunk */
static void table(const char* name,int L,int cycles,int isFloat,int clm,int ch){
  WavWriter w; if(wavw_open(&w,"%s/SAMPLES/TABLES/%s",root,name)) return;
  wavw_fmt(&w,isFloat?WAV_FORMAT_FLOAT:WAV_FORMAT_PCM,(uint16_t)ch,44100,isFloat?32:16);
  if(clm){ char cl[16]; const int n=snprintf(cl,sizeof cl,"<!>%d 1000",L); if(n>0&&(size_t)n<sizeof cl) wavw_chunk(&w,"clm ",cl,(uint32_t)n); else wavw_fail(&w,"clm chunk too long"); }
  wavw_data(&w);
  for(int cy=0;cy<cycles;cy++)for(int i=0;i<L;i++){ double x=0; for(int h=1;h<=cy+1&&h<6;h++) x+=sin(2*M_PI*h*i/L+cy*0.3*h)/h; x*=0.5;
    for(int k=0;k<ch;k++){ if(isFloat) wavw_f32(&w,(float)x); else wavw_s16(&w,(short)(x*32000)); } }
  wavw_close(&w);}
/* frame f, level l of a set: the sample nearest to a sine of the fundamental at phase 0? */
static double tbl_fundPhase(const int16_t* fr,int l){ int n=WT_LEVEL_LEN(l); const int16_t* p=fr+WT_LEVEL_OFS(l); double re=0,im=0;
  for(int i=0;i<n;i++){ re+=p[i]*cos(2*M_PI*i/n); im+=p[i]*sin(2*M_PI*i/n);} return atan2(re,im); }
static double tbl_harm(const int16_t* fr,int l,int h){ int n=WT_LEVEL_LEN(l); const int16_t* p=fr+WT_LEVEL_OFS(l); double re=0,im=0;
  for(int i=0;i<n;i++){ re+=p[i]*cos(2*M_PI*h*i/n); im+=p[i]*sin(2*M_PI*h*i/n);} return sqrt(re*re+im*im)*2/n; }
static int expect(const char* name,int ch,int frames,const SampleFlashEntry* e){
  /* recompute expected mono data and compare with flash */
  const int16_t* p=(const int16_t*)(uintptr_t)e->offset;
  if(e->frames!=(uint32_t)frames) return 0;
  for(int i=0;i<frames;i++){int v0=(i*37+(int)name[0]*11)%30000-15000; int v1=(i*37+1000+(int)name[0]*11)%30000-15000;
    int16_t m= ch==1? (int16_t)v0 : (int16_t)(((int32_t)(int16_t)v0+(int16_t)v1)/2); if(p[i]!=m) return 0;}
  return 1;}
static const char* rn[]={"UNCHANGED","IMPORTED","NO_FOLDER","ERR_FORMAT","ERR_TOO_BIG","ERR_READ","ERR_FLASH","ERR_VERIFY"};
#define RUN(label) do{ eraseCalls=0;progCount=0; SampleFlashResult r=sampleFlash_importAtBoot(); printf("%-46s -> %-11s erase=%d LCD=[%s]\n",label,rn[r],eraseCalls,lcdbuf); last=r;}while(0)
FRESULT f_stat(const char* p, FILINFO* fi){char b[600];mkpath(b,p);struct stat st; if(stat(b,&st)) return FR_NO_FILE; fi->fsize=st.st_size; return FR_OK;}
int main(){
  void* m=mmap((void*)0x08080000,0x180000,PROT_READ|PROT_WRITE,MAP_FIXED_NOREPLACE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  assert(m==(void*)0x08080000); memset(m,0xFF,0x180000);
  snprintf(root,sizeof root,"%s","./sd"); system("rm -rf sd; mkdir -p sd"); SampleFlashResult last; int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("   FAIL: %s\n",msg);fails++;} else printf("   ok: %s\n",msg);}while(0)
  RUN("1 kein /SAMPLES-Ordner"); CHECK(last==SF_NO_FOLDER && eraseCalls==0,"Ordner fehlt -> Flash unberuehrt");
  system("mkdir -p sd/SAMPLES"); 
  wav("1.WAV",1,16,44100,22050,0); wav("2.WAV",2,16,48000,10001,1); wav("3.WAV",1,16,22050,7,0);
  system("touch sd/SAMPLES/README.TXT; cp sd/SAMPLES/1.WAV sd/SAMPLES/._1.WAV");
  RUN("2 drei WAVs (mono, stereo+LIST, 7 Frames)"); CHECK(last==SF_OK_IMPORTED,"Import ok");
  const SampleFlashHeader* h=sampleFlash_getHeader(); CHECK(h && h->count==3,"3 Eintraege, ._ und .TXT ignoriert");
  CHECK(h && !strcmp(h->entry[0].name,"1.WAV") && !strcmp(h->entry[2].name,"3.WAV"),"Sortierung nach Name");
  CHECK(h && expect("1.WAV",1,22050,&h->entry[0]),"1.WAV Daten identisch");
  CHECK(h && expect("2.WAV",2,10001,&h->entry[1]) && h->entry[1].rate==48000,"2.WAV Stereo->Mono korrekt, Rate 48k gemerkt");
  CHECK(h && expect("3.WAV",1,7,&h->entry[2]),"3.WAV ungerade Framezahl");
  CHECK(eraseCalls==1,"nur Sektor 6 geloescht (Daten < 256KB)");
  RUN("3 Neustart ohne Aenderung"); CHECK(last==SF_OK_UNCHANGED && eraseCalls==0 && progCount==0,"kein Loeschen/Schreiben");
  wav("2.WAV",1,16,44100,30000,0);
  RUN("4 Datei geaendert"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getHeader()->entry[1].frames==30000,"Reimport");
  wav("4.WAV",1,24,44100,100,0);
  sawSkip=0; RUN("5 24-Bit-WAV dazu"); CHECK(last==SF_OK_IMPORTED && sawSkip,"24-Bit-Datei uebersprungen und gemeldet");
  CHECK(sampleFlash_getCount()==3 && sampleFlash_getHeader()->reserved[0]==1,"3 importiert, 1 als uebersprungen vermerkt");
  CHECK(expect("2.WAV",1,30000,&sampleFlash_getHeader()->entry[1]),"gueltige Dateien unveraendert korrekt");
  RUN("5b Neustart mit uebersprungener Datei"); CHECK(last==SF_OK_UNCHANGED && eraseCalls==0,"kein erneuter Import");
  wav("0.WAV",1,24,44100,100,0); wav("25.WAV",1,16,44100,500,0);
  RUN("5c ungueltige Datei vorne + neue gueltige"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==4,"luckenlos: 1,2,3,25");
  CHECK(!strcmp(sampleFlash_getHeader()->entry[0].name,"1.WAV") && !strcmp(sampleFlash_getHeader()->entry[2].name,"3.WAV") && !strcmp(sampleFlash_getHeader()->entry[3].name,"25.WAV") && sampleFlash_getHeader()->reserved[0]==2,"Zahlen-Sortierung (3 vor 25), 2 uebersprungen");
  system("rm sd/SAMPLES/0.WAV sd/SAMPLES/25.WAV; mkdir -p sd2/SAMPLES; mv sd/SAMPLES/*.WAV sd2/; cp sd/SAMPLES/../SAMPLES/../../sd2/*.WAV sd2/ 2>/dev/null; true");
  wav("4B.WAV",1,24,44100,100,0); system("rm -f sd/SAMPLES/4.WAV");
  RUN("5d nur ungueltige Datei im Ordner"); CHECK(last==SF_ERR_FORMAT && eraseCalls==0 && sampleFlash_getCount()==4,"nichts importierbar -> Flash unberuehrt");
  system("rm sd/SAMPLES/4B.WAV; mv sd2/*.WAV sd/SAMPLES/; rm -rf sd2"); wav("4.WAV",1,24,44100,100,0);
  system("rm sd/SAMPLES/4.WAV"); wav("5.WAV",1,16,44100,300000,0);
  sawSkip=0; RUN("6 Datei passt nicht mehr (600 KB)"); CHECK(last==SF_OK_IMPORTED && sawSkip && sampleFlash_getCount()==3 && sampleFlash_getHeader()->reserved[1]==1,"zu grosse Datei uebersprungen, Rest importiert");
  wav("6.WAV",1,16,44100,1000,0);
  RUN("6b kleine Datei nach der zu grossen"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==4 && !strcmp(sampleFlash_getHeader()->entry[3].name,"6.WAV"),"passt noch -> uebernommen");
  system("mkdir -p sd2; mv sd/SAMPLES/1.WAV sd/SAMPLES/2.WAV sd/SAMPLES/3.WAV sd/SAMPLES/6.WAV sd2/");
  RUN("6c nur eine zu grosse Datei"); CHECK(last==SF_ERR_TOO_BIG && eraseCalls==0 && sampleFlash_getCount()==4,"nichts passt -> Flash unberuehrt, alter Bestand bleibt");
  CHECK(!strcmp(lcdbuf,"Too big: 6.8s|max 5.9s @44.1k|"),"Meldung in Sekunden");
  system("mv sd2/1.WAV sd2/2.WAV sd2/3.WAV sd/SAMPLES/; rm -rf sd2");
  system("rm sd/SAMPLES/5.WAV"); wav("5.WAV",1,16,44100,200000,0);
  RUN("7 groesser als 1 Sektor (400 KB)"); CHECK(last==SF_OK_IMPORTED && eraseCalls==2,"Sektor 6+7");
  CHECK(expect("5.WAV",1,200000,&sampleFlash_getHeader()->entry[3]),"5.WAV ueber Sektorgrenze korrekt");
  wav("6.WAV",1,16,44100,1000,0); failAfter=5000;
  RUN("8 Stromausfall/Schreibfehler simuliert"); CHECK(last==SF_ERR_FLASH && sampleFlash_getCount()==0,"Header ungueltig -> Samples aus");
  failAfter=-1;
  RUN("9 naechster Start nach Ausfall"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==5,"automatischer Reimport");
  /* Loops aus /SAMPLES/LOOPS, gemeinsam nach Namen sortiert */
  system("rm -f sd/SAMPLES/*.WAV; mkdir -p sd/SAMPLES/LOOPS");
  wav("10HAT.WAV",1,16,44100,500,0); wav("2KICK.WAV",1,16,44100,500,0);
  { char b[600]; snprintf(b,600,"%s/SAMPLES/LOOPS/5PAD.WAV",root); FILE* f=fopen(b,"wb"); fclose(f); }
  system("cp sd/SAMPLES/2KICK.WAV sd/SAMPLES/LOOPS/5PAD.WAV; cp sd/SAMPLES/2KICK.WAV sd/SAMPLES/LOOPS/2KICK.WAV");
  RUN("9b Loops-Ordner"); { const SampleFlashHeader* hh=sampleFlash_getHeader();
    CHECK(last==SF_OK_IMPORTED && hh && hh->count==4,"Samples + Loops importiert");
    CHECK(hh && !strcmp(hh->entry[0].name,"2KICK.WAV") && !(hh->entry[0].flags&SF_FLAG_LOOP) && !strcmp(hh->entry[1].name,"2KICK.WAV") && (hh->entry[1].flags&SF_FLAG_LOOP)
          && !strcmp(hh->entry[2].name,"5PAD.WAV") && (hh->entry[2].flags&SF_FLAG_LOOP) && !strcmp(hh->entry[3].name,"10HAT.WAV"),"Reihenfolge 2KICK, 2KICK(L), 5PAD(L), 10HAT; Loop-Kennung"); }
  system("mv sd/SAMPLES/LOOPS/5PAD.WAV sd/SAMPLES/5PAD.WAV");
  RUN("9c Datei von LOOPS nach SAMPLES verschoben"); CHECK(last==SF_OK_IMPORTED && !(sampleFlash_getHeader()->entry[2].flags&SF_FLAG_LOOP),"Ordnerwechsel erkannt, kein Loop mehr");
  system("rm -rf sd/SAMPLES/LOOPS");
  system("rm -f sd/SAMPLES/*.WAV"); char nm[16]; for(int i=0;i<40;i++){snprintf(nm,16,"%02d.WAV",i);wav(nm,1,16,44100,100,0);}
  RUN("10 vierzig Dateien"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==31,"max 31 uebernommen");
  RUN("11 erneuter Start"); CHECK(last==SF_OK_UNCHANGED,"Signatur stabil");
  sampleFlash_showStatus(); CHECK(!strcmp(lcdbuf,"Samples: 31|Used 0.0/5.9s|"),"Statuszeile beim Start: Anzahl, Belegung in Sekunden");
  CHECK(bigErase==0,"ohne BIGFLASH nie Sektor 8-11 geloescht");
  /* experimenteller grosser Flash */
  system("rm -f sd/SAMPLES/*.WAV"); wav("1BIG.WAV",1,16,44100,200000,0); wav("2BIG.WAV",1,16,44100,200000,0);
  RUN("12 800 KB ohne BIGFLASH"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==1 && bigErase==0,"nur 1 passt in 512 KB");
  system("touch sd/SAMPLES/BIGFLASH"); maxSector=0;
  RUN("13 800 KB mit BIGFLASH"); { const SampleFlashHeader* hh=sampleFlash_getHeader();
    CHECK(last==SF_OK_IMPORTED && hh && hh->count==2 && hh->reserved[2]==3,"beide importiert, grosser Flash vermerkt");
    CHECK(hh && hh->entry[1].offset+hh->entry[1].frames*2 > 0x08100000 && expect("2BIG.WAV",1,200000,&hh->entry[1]),"Daten oberhalb 1 MB korrekt");
    CHECK(maxSector==9,"nur die noetigen Sektoren 6-9 geloescht"); }
  sampleFlash_showStatus(); CHECK(!strcmp(lcdbuf,"Samples: 2|Used 9.0/17.8s|"),"Statuszeile zeigt Belegung im grossen Flash");
  RUN("13b Neustart"); CHECK(last==SF_OK_UNCHANGED,"kein erneuter Import");
  wav("3BIG.WAV",1,16,44100,1000,0); failUpper=1;
  RUN("14 oberer Bereich defekt (simuliert)"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==2 && sampleFlash_getHeader()->reserved[2]==0,"Rueckfall auf 512 KB, was passt importiert");
  failUpper=0; system("rm sd/SAMPLES/3BIG.WAV"); bigOk=0;
  RUN("15 BIGFLASH, Bereich nicht lesbar"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==1 && sampleFlash_getHeader()->reserved[2]==0,"nicht nutzbar -> 512 KB");
  bigOk=1; system("rm sd/SAMPLES/BIGFLASH");
  RUN("16 BIGFLASH entfernt"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==1,"Signatur aendert sich, Import in 512 KB");
  /* MOD: user waves from /SAMPLES/WAVES */
  system("mkdir -p sd/SAMPLES/WAVES");
  wav("WAVES/2SAW.WAV",1,16,44100,512,0); wav("WAVES/10SIN.WAV",2,16,48000,4096,1); wav("WAVES/1BAD.WAV",1,24,44100,100,0);
  { const int before=sampleFlash_getCount();
    RUN("17 Waves dazu (2 gueltig, 1 ungueltig)"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getWaveCount()==2,"2 Waves importiert");
    CHECK(sampleFlash_getCount()==before,"Samples unveraendert gezaehlt");
    CHECK(!strcmp(sampleFlash_waveName(0),"2SAW.WAV") && !strcmp(sampleFlash_waveName(1),"10SIN.WAV"),"eigene Nummerierung, Zahlen-Sortierung (2 vor 10)");
    int okw=1; const char* nm="WAVES/2SAW.WAV"; const int16_t* w=sampleFlash_wave(0);
    for(int k=0;k<1024&&w;k++){ unsigned long long pp=(unsigned long long)k*512*65536/1024; int i=pp>>16, f=pp&0xFFFF;
      int ia=i, ib=(i+1)>=512?0:i+1; int a=(ia*37+(int)nm[0]*11)%30000-15000, bb=(ib*37+(int)nm[0]*11)%30000-15000;
      int16_t e=(int16_t)(a+(((bb-a)*f)>>16)); if(w[k]!=e){okw=0;break;} }
    CHECK(w && okw,"2SAW: 512 Frames auf 1024 Punkte interpoliert (Ende -> Anfang)");
    okw=1; nm="WAVES/10SIN.WAV"; w=sampleFlash_wave(1);
    for(int k=0;k<1024&&w;k++){ unsigned long long pp=(unsigned long long)k*4096*65536/1024; int i=pp>>16, f=pp&0xFFFF;
      int ib=(i+1)>=4096?0:i+1; int a=(i*37+(int)nm[0]*11)%30000-15000, bb=(ib*37+(int)nm[0]*11)%30000-15000;
      int16_t e=(int16_t)(a+(((bb-a)*f)>>16)); if(w[k]!=e){okw=0;break;} }
    CHECK(w && okw,"10SIN stereo: nur der linke Kanal (keine Summe)");
    CHECK(sampleFlash_wave(2)==NULL && !strcmp(sampleFlash_waveName(2),""),"nicht vorhandene Wave -> NULL");
    CHECK(sampleFlash_getHeader()->reserved[0]>=1,"ungueltige Wave als uebersprungen vermerkt");
    RUN("18 Neustart mit Waves"); CHECK(last==SF_OK_UNCHANGED && eraseCalls==0,"kein erneuter Import");
    CHECK(sampleFlash_modulatedWave(6+31,1.f/127.f*(6+31+1),6)==6+31+1,"Modulation einer Wave: naechste Wave");
    CHECK(sampleFlash_modulatedWave(6+31,1.f,6)==6+31+1,"ueber der letzten Wave begrenzt");
    CHECK(sampleFlash_modulatedWave(6+31,0.f,6)==6+31,"unter W1 begrenzt, nie ein Sample");
  }
  system("mkdir -p sd3; mv sd/SAMPLES/*.WAV sd3/ 2>/dev/null; true");
  RUN("19 nur Waves, keine Samples"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getCount()==0 && sampleFlash_getWaveCount()==2 && sampleFlash_getHeader()!=NULL,"gueltig ohne Samples");
  system("mv sd3/*.WAV sd/SAMPLES/; rm -rf sd3 sd/SAMPLES/WAVES");
  RUN("20 WAVES-Ordner entfernt"); CHECK(last==SF_OK_IMPORTED && sampleFlash_getWaveCount()==0 && sampleFlash_getCount()>0,"keine Waves mehr, Samples wieder da");
  /* MOD: wave sets from /SAMPLES/TABLES and from the user waves */
  system("mkdir -p sd/SAMPLES/TABLES sd/SAMPLES/WAVES sd4; mv sd/SAMPLES/*.WAV sd4/ 2>/dev/null; true");
  wav("WAVES/1A.WAV",1,16,44100,600,0); wav("WAVES/2B.WAV",1,16,44100,333,0);
  table("1SERUM.WAV",2048,4,1,1,1);   /* 32 bit float, clm 2048, 4 cycles */
  table("2WEDIT.WAV",256,64,0,0,2);    /* 16 bit stereo, 64 x 256, no chunk */
  table("3BIG.WAV",2048,100,0,1,1);    /* 100 cycles: 64 picked */
  { char b[600]; snprintf(b,600,"%s/SAMPLES/TABLES/4BAD.WAV",root); FILE* f=fopen(b,"wb"); fputs("nonsense",f); fclose(f); }
  RUN("21 Wave-Sets (Serum float, WaveEdit, gross, kaputt)");
  { uint8_t n0=0,n1=0,n2=0,n3=0,n4=0; const int16_t* t0=sampleFlash_table(0,&n0); const int16_t* t1=sampleFlash_table(1,&n1);
    const int16_t* t2=sampleFlash_table(2,&n2); const int16_t* t3=sampleFlash_table(3,&n3); sampleFlash_table(4,&n4);
    CHECK(last==SF_OK_IMPORTED && t0 && n0==2+3,"Set aus 2 User-Waves: 2 + 3 Zwischenwellen");
    CHECK(t1 && n1==4+3*7 && !strcmp(sampleFlash_tableName(1),"1SERUM.WAV"),"Serum (clm 2048, float): 4 Zyklen + je 7 Zwischenwellen = 25");
    CHECK(t2 && n2==64,"WaveEdit 64 x 256 ohne clm erkannt: 64 Frames");
    CHECK(t3 && n3==64,"100 Zyklen: 64 gleichmaessig ausgewaehlt");
    CHECK(n4==0,"kaputte Datei uebersprungen");
    int aligned=1; for(int f=0;f<n1&&t1;f++) for(int l=0;l<6;l++) if(fabs(tbl_fundPhase(t1+f*WT_FRAME_SIZE,l))>0.05) aligned=0;
    CHECK(aligned,"alle Frames und Stufen: Grundton bei Phase 0 (ausgerichtet)");
    double pk=0; for(int i=0;t1&&i<256;i++) if(abs(t1[i])>pk) pk=abs(t1[i]);
    CHECK(pk>29000 && pk<=30001,"Frame normalisiert (Spitze 30000)");
    CHECK(t1 && tbl_harm(t1,0,2)<50 && tbl_harm(t1+24*WT_FRAME_SIZE,0,2)>3000,"Frame 0 reiner Sinus, letzter Frame mit 2. Oberton");
    double h2a=t1?tbl_harm(t1+7*WT_FRAME_SIZE,0,2):0, h2b=t1?tbl_harm(t1+8*WT_FRAME_SIZE,0,2):0;   /* in-between before cycle 1 / cycle 1 */
    CHECK(h2a>0.5*h2b && h2a<h2b,"Zwischenwelle: Oberton waechst stetig zum naechsten Zyklus");
    CHECK(t1 && tbl_harm(t1+24*WT_FRAME_SIZE,5,4)<50 && tbl_harm(t1+24*WT_FRAME_SIZE,5,3)>1000 && tbl_harm(t1+24*WT_FRAME_SIZE,0,4)>1000,"Stufe mit 3 Obertoenen: Oberton 4 weg, 3 bleibt");
    sampleFlash_showStatus(); CHECK(strstr(lcdbuf,"T:3")!=NULL,"Statuszeile zeigt 3 Tabellen");
  }
  RUN("22 Neustart mit Wave-Sets"); CHECK(last==SF_OK_UNCHANGED,"kein erneuter Import");
  system("rm -rf sd/SAMPLES/TABLES sd/SAMPLES/WAVES; mv sd4/*.WAV sd/SAMPLES/; rm -rf sd4");
  RUN("23 TABLES entfernt"); { uint8_t n=0; CHECK(last==SF_OK_IMPORTED && sampleFlash_table(1,&n)==NULL && sampleFlash_table(0,&n)==NULL,"keine Sets mehr"); }
  CHECK(badErase==0,"NIE ein anderer Sektor als 6-11 geloescht, nie ohne Unlock");
  CHECK(badProg==0,"NIE ausserhalb 0x08080000-0x080FFFFF oder auf nicht geloeschte Woerter geschrieben");
  if(wavw_errors){ printf("   FAIL: %d Fehler beim Schreiben der Test-WAVs\n",wavw_errors); fails++; }
  printf("\n%s (%d Fehler)\n", fails?"FEHLER":"ALLE TESTS BESTANDEN", fails); return fails;}

#define _GNU_SOURCE
#include "main.h"
#include "ff.h"
#include "lcd.h"
#include "SampleRom/sampleFlash.h"
#include <sys/mman.h>
void* dw_open(const char*); const char* dw_next(void*); void dw_close(void*);
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static char root[512];
int eraseCalls=0, badErase=0, badProg=0, progCount=0, failAfter=-1, locked=1;
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t* err){
  eraseCalls++; if(locked) badErase++;
  if(e->Sector==6) memset((void*)0x08080000,0xFF,0x40000);
  else if(e->Sector==7) memset((void*)0x080C0000,0xFF,0x40000);
  else { badErase++; }
  *err=0xFFFFFFFF; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t t, uint32_t a, uint64_t d){
  if(locked) badProg++;
  if(a<0x08080000||a>=0x08100000||(a&3)) { badProg++; return HAL_ERROR; }
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
/* WAV writer */
static void wav(const char* name,int ch,int bits,int rate,int frames,int extraChunk){
  char b[600];snprintf(b,600,"%s/SAMPLES/%s",root,name);FILE* f=fopen(b,"wb");
  int bps=bits/8, data=frames*ch*bps; int fmtlen=16;
  int riff=4+8+fmtlen+(extraChunk?8+5+1:0)+8+data;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVE",1,4,f);
  if(extraChunk){fwrite("LIST",1,4,f);int l=5;fwrite(&l,4,1,f);fwrite("abcde\0",1,6,f);}
  fwrite("fmt ",1,4,f);fwrite(&fmtlen,4,1,f);short fmt=1,c=ch,bb=bits;int br=rate*ch*bps;short ba=ch*bps;
  fwrite(&fmt,2,1,f);fwrite(&c,2,1,f);fwrite(&rate,4,1,f);fwrite(&br,4,1,f);fwrite(&ba,2,1,f);fwrite(&bb,2,1,f);
  fwrite("data",1,4,f);fwrite(&data,4,1,f);
  for(int i=0;i<frames;i++)for(int k=0;k<ch;k++){ int v=(i*37+k*1000+ (int)(name[0])*11)%30000-15000; if(bits==16){short s=v;fwrite(&s,2,1,f);} else {int s=v<<8;fwrite(&s,3,1,f);} }
  fclose(f);}
static int expect(const char* name,int ch,int frames,const SampleFlashEntry* e){
  /* recompute expected mono data and compare with flash */
  const int16_t* p=(const int16_t*)(uintptr_t)e->offset;
  if(e->frames!=(uint32_t)frames) return 0;
  for(int i=0;i<frames;i++){int v0=(i*37+(int)name[0]*11)%30000-15000; int v1=(i*37+1000+(int)name[0]*11)%30000-15000;
    int16_t m= ch==1? (int16_t)v0 : (int16_t)(((int32_t)(int16_t)v0+(int16_t)v1)/2); if(p[i]!=m) return 0;}
  return 1;}
static const char* rn[]={"UNCHANGED","IMPORTED","NO_FOLDER","ERR_FORMAT","ERR_TOO_BIG","ERR_READ","ERR_FLASH","ERR_VERIFY"};
#define RUN(label) do{ eraseCalls=0;progCount=0; SampleFlashResult r=sampleFlash_importAtBoot(); printf("%-46s -> %-11s erase=%d LCD=[%s]\n",label,rn[r],eraseCalls,lcdbuf); last=r;}while(0)
int main(){
  void* m=mmap((void*)0x08080000,0x80000,PROT_READ|PROT_WRITE,MAP_FIXED_NOREPLACE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  assert(m==(void*)0x08080000); memset(m,0xFF,0x80000);
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
  sampleFlash_showStatus(); CHECK(!strcmp(lcdbuf,"Samples: 31|Flash: 2048 KB|"),"Statuszeile beim Start mit Chip-Flash-Groesse");
  CHECK(badErase==0,"NIE ein anderer Sektor als 6/7 geloescht, nie ohne Unlock");
  CHECK(badProg==0,"NIE ausserhalb 0x08080000-0x080FFFFF oder auf nicht geloeschte Woerter geschrieben");
  printf("\n%s (%d Fehler)\n", fails?"FEHLER":"ALLE TESTS BESTANDEN", fails); return fails;}

#pragma once
#include <stdio.h>
#include <stdint.h>
typedef unsigned int UINT; typedef uint8_t BYTE; typedef uint32_t FSIZE_t;
typedef enum { FR_OK=0, FR_DISK_ERR, FR_NO_FILE=4, FR_NO_PATH=5 } FRESULT;
typedef struct { FILE* f; } FIL;
typedef struct { void* d; char path[256]; } DIR;
typedef struct { FSIZE_t fsize; BYTE fattrib; char fname[13]; } FILINFO;
#define FA_READ 1
#define FA_OPEN_EXISTING 0
#define AM_HID 2
#define AM_SYS 4
#define AM_DIR 0x10
FRESULT f_open(FIL*, const char*, BYTE); FRESULT f_close(FIL*);
FRESULT f_read(FIL*, void*, UINT, UINT*); FRESULT f_lseek(FIL*, FSIZE_t);
FSIZE_t f_tell_impl(FIL*);
#define f_tell(fp) f_tell_impl(fp)
FRESULT f_opendir(DIR*, const char*); FRESULT f_readdir(DIR*, FILINFO*); FRESULT f_closedir(DIR*);

#pragma once
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef struct { uint32_t TypeErase, Banks, Sector, NbSectors, VoltageRange; } FLASH_EraseInitTypeDef;
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_VOLTAGE_RANGE_3 2
#define FLASH_TYPEPROGRAM_WORD 2
#define FLASH_SECTOR_0 0
#define FLASH_SECTOR_6 6
#define FLASH_SECTOR_7 7
#define FLASH_SECTOR_11 11u
#define FLASH_FLAG_ALL_ERRORS 0
#define __HAL_FLASH_CLEAR_FLAG(x) ((void)0)
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*, uint32_t*);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t, uint32_t, uint64_t);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
void SCB_CleanInvalidateDCache(void);
void HAL_Delay(uint32_t);

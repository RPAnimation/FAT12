#ifndef __FAT_STRUCTS_H__
#define __FAT_STRUCTS_H__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

struct bpb_t {
	uint8_t			BS_jmpBoot[3];
	unsigned char 	BS_OEMName[8];
	uint16_t 		BPB_BytsPerSec;
	uint8_t 		BPB_SecPerClus;
	uint16_t 		BPB_RsvdSecCnt;
	uint8_t 		BPB_NumFATs;
	uint16_t 		BPB_RootEntCnt;
	uint16_t 		BPB_TotSec16;
	uint8_t 		BPB_Media;
	uint16_t 		BPB_FATSz16;
	uint16_t 		BPB_SecPerTrk;
	uint16_t 		BPB_NumHeads;
	uint32_t 		BPB_HiddSec;
	uint32_t 		BPB_TotSec32;
	uint8_t 		BS_DrvNum;
	uint8_t 		BS_Reserved1;
	uint8_t 		BS_BootSig;
	uint32_t 		BS_VolID;
	unsigned char 	BS_VolLab[11];
	unsigned char 	BS_FilSysType[8];
	uint8_t         BS_BootCode[448];
	uint16_t        BS_Signature;
} __attribute__((__packed__));

struct date_format_t {
	union {
		uint16_t date;
		uint8_t  day   : 5;
		uint8_t  month : 4;
		uint8_t  year  : 7;
	};
} __attribute__((__packed__));

struct time_format_t {
	union {
		uint16_t time;
		uint8_t  second : 5;
		uint8_t  minute : 6;
		uint8_t  hour   : 5;
	};
} __attribute__((__packed__));

struct actual_dir_entry_t {
	unsigned char DIR_Name[11];
	struct {
		uint8_t ATTR_READ_ONLY : 1;
		uint8_t ATTR_HIDDEN    : 1;
		uint8_t ATTR_SYSTEM    : 1;
		uint8_t ATTR_VOLUME_ID : 1;
		uint8_t ATTR_DIRECTORY : 1;
		uint8_t ATTR_ARCHIVE   : 1;
		uint8_t ATTR_LONG_NAME : 1;
	} DIR_Attr;

	uint8_t DIR_NTRes;

	// CREATION MS/TIME/DATE
	uint8_t DIR_CrtTimeTenth;
	struct time_format_t DIR_CrtTime;
	struct date_format_t DIR_CrtDate;
	// LAST ACCESS DATE
	struct date_format_t DIR_LstAccDate;

	uint16_t DIR_FstClusHI;

	// LAST WRITE TIME/DATE
	struct time_format_t DIR_WrtTime;
	struct date_format_t DIR_WrtDate;

	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
};

#endif
#include "file_reader.h"
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>

uint32_t fat1_addr(struct bpb_t bpb) {
	return bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec;
}
uint32_t fat2_addr(struct bpb_t bpb) {
	return fat1_addr(bpb) + bpb.BPB_BytsPerSec * bpb.BPB_FATSz16;
}
uint32_t root_addr(struct bpb_t bpb) {
	if (bpb.BPB_NumFATs > 1) {
		return fat2_addr(bpb) + bpb.BPB_BytsPerSec * bpb.BPB_FATSz16;
	}
	return fat1_addr(bpb) + bpb.BPB_BytsPerSec * bpb.BPB_FATSz16;
}
uint32_t data_addr(struct bpb_t bpb) {
	return root_addr(bpb) + bpb.BPB_RootEntCnt * FAT_RECORD_SIZE;
}

struct disk_t* disk_open_from_file(const char* volume_file_name) {
	if (volume_file_name == NULL) {
		errno = EFAULT;
		return NULL;
	}
	struct disk_t *pdisk = malloc(sizeof(struct disk_t));
	if (pdisk == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	pdisk->filename = volume_file_name;
	pdisk->handle = fopen(volume_file_name, "rb");
	if (pdisk->handle == NULL) {
		free(pdisk);
		errno = ENOENT;
		return NULL;
	}
	return pdisk;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read) {
	if (pdisk == NULL || pdisk->filename == NULL || pdisk->handle == NULL) {
		errno = EFAULT;
		return -1;
	}
	fseek(pdisk->handle, first_sector*BLOCK_SIZE, SEEK_SET);
	if (fread(buffer, BLOCK_SIZE, sectors_to_read, pdisk->handle) != (size_t)sectors_to_read) {
		errno = ERANGE;
		return -1;
	}
	return sectors_to_read;
}
int disk_close(struct disk_t* pdisk) {
	if (pdisk == NULL || pdisk->filename == NULL || pdisk->handle == NULL) {
		errno = EFAULT;
		return -1;
	}
	fclose(pdisk->handle);
	free(pdisk);
	return 0;
}
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
	if (pdisk == NULL || pdisk->filename == NULL || pdisk->handle == NULL) {
		errno = EFAULT;
		return NULL;
	}
	struct bpb_t bpb;
	if (disk_read(pdisk, first_sector, &bpb, 1) != 1) {
		errno = ERANGE;
		return NULL;
	}
	if (   bpb.BS_Signature != SIG
		|| bpb.BPB_BytsPerSec != BLOCK_SIZE
		|| (bpb.BPB_SecPerClus & (bpb.BPB_SecPerClus - 1)) != 0
		|| bpb.BPB_RsvdSecCnt < 1
		|| bpb.BPB_NumFATs < 1
		|| bpb.BPB_NumFATs > 2
		|| bpb.BPB_RootEntCnt == 0 
		|| (bpb.BPB_RootEntCnt * FAT_RECORD_SIZE) % bpb.BPB_BytsPerSec != 0
		|| bpb.BPB_TotSec16 == 0
		|| bpb.BPB_FATSz16 < 1
		|| bpb.BPB_FATSz16 > 12) {
		errno = EINVAL;
		return NULL;
	}

	// CALC
	uint32_t RootDirSectors = ((bpb.BPB_RootEntCnt * FAT_RECORD_SIZE) + (bpb.BPB_BytsPerSec - 1)) / bpb.BPB_BytsPerSec;
	uint32_t FirstDataSector = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz16) + RootDirSectors;
	uint32_t TotalSec = (bpb.BPB_TotSec16 != 0) ? bpb.BPB_TotSec16 : bpb.BPB_TotSec32;
	uint32_t DataSec = TotalSec - FirstDataSector;
	uint32_t CountofClusters = DataSec / bpb.BPB_SecPerClus;

	// CHECK FOR FAT12
	if (CountofClusters >= 4085) {
		errno = EINVAL;
		return NULL;
	}

	struct volume_t* pvolume = malloc(sizeof(struct volume_t));
	if (pvolume == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	// FILL STRUCT
	pvolume->pdisk = pdisk;
	pvolume->bpb = bpb;

	// COPY FAT 1
	pvolume->FAT1 = malloc(bpb.BPB_BytsPerSec * bpb.BPB_FATSz16);
	if (pvolume->FAT1 == NULL) {
		errno = ENOMEM;
		free(pvolume);
		return NULL;
	}
	disk_read(pdisk, fat1_addr(bpb) / BLOCK_SIZE, pvolume->FAT1, bpb.BPB_FATSz16);

	// COPY FAT 2
	if (bpb.BPB_NumFATs > 1) {
		pvolume->FAT2 = malloc(bpb.BPB_BytsPerSec * bpb.BPB_FATSz16);
		if (pvolume->FAT2 == NULL) {
			errno = ENOMEM;
			free(pvolume->FAT1);
			free(pvolume);
			return NULL;
		}
		disk_read(pdisk, fat2_addr(bpb) / BLOCK_SIZE, pvolume->FAT2, bpb.BPB_FATSz16);
		if (memcmp(pvolume->FAT1, pvolume->FAT2, bpb.BPB_FATSz16) != 0) {
			free(pvolume->FAT1);
			free(pvolume->FAT2);
			free(pvolume);
			errno = EINVAL;
			return NULL;
		}
	}
	// RETURN
	return pvolume;
}
int fat_close(struct volume_t* pvolume) {
	if (pvolume == NULL || pvolume->pdisk == NULL || pvolume->FAT1 == NULL || (pvolume->bpb.BPB_NumFATs > 1 && pvolume->FAT2 == NULL)) {
		errno = EFAULT;
		return -1;
	}
	if (pvolume->bpb.BPB_NumFATs > 1) {
		free(pvolume->FAT2);
	}
	free(pvolume->FAT1);
	free(pvolume);
	return 0;
}
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster) {
	if (buffer == NULL || size < 3 || first_cluster < 1) {
		return NULL;
	}
	uint16_t pos = first_cluster;
	int len = 0;
	while(1) {
		// CALC VALUES
		uint16_t bytes = pos % 2 == 0 ? (pos/2) * 3 : ((pos - 1)/2) * 3;
		uint8_t l = *((uint8_t *)buffer + bytes);
		uint8_t m = *((uint8_t *)buffer + bytes + 1);
		uint8_t h = *((uint8_t *)buffer + bytes + 2);
		uint16_t param = pos % 2 != 0 ? (h << 4) | (m >> 4) : ((m & 0xF) << 8) | l;
		len++;

		// BAD CLUSTER
		if (param == 0x0FF7) {
			return NULL;
		}
		// LAST CLUSTER IN CHAIN OR UNUSED OR RESERVED CLUSTER
		if ((param >= 0x0FF8 && param <= 0x0FFF) || param == 0x0000 || (param >= 0x0FF0 && param <= 0x0FF6)) {
			break;
		}
		pos = param;
	}
	if (len == 0) {
		return NULL;
	}
	// ALLOC STRUCT
	struct clusters_chain_t *ret = malloc(sizeof(struct clusters_chain_t));
	if (ret == NULL) {
		return NULL;
	}
	// ALLOC ARRAY
	ret->clusters = malloc(sizeof(uint16_t)*len);
	if (ret->clusters == NULL) {
		free(ret);
		return NULL;
	}
	pos = first_cluster;
	for (int i = 0; i < len; ++i) {
		// CALC VALUES
		uint16_t bytes = pos % 2 == 0 ? (pos/2) * 3 : ((pos - 1)/2) * 3;
		uint8_t l = *((uint8_t *)buffer + bytes);
		uint8_t m = *((uint8_t *)buffer + bytes + 1);
		uint8_t h = *((uint8_t *)buffer + bytes + 2);
		uint16_t param = pos % 2 != 0 ? (h << 4) | (m >> 4) : ((m & 0xF) << 8) | l;

		// SAVE POS TO ARRAY
		*(ret->clusters + i) = pos;

		// LAST CLUSTER IN CHAIN OR UNUSED OR RESERVED CLUSTER
		if ((param >= 0x0FF8 && param <= 0x0FFF) || param == 0x0000 || (param >= 0x0FF0 && param <= 0x0FF6)) {
			break;
		}
		pos = param;
	}
	ret->size = len;
	return ret;
}
struct file_t* file_open(struct volume_t* pvolume, const char* file_name) {
	if (file_name == NULL || pvolume == NULL || pvolume->pdisk == NULL || pvolume->bpb.BPB_NumFATs < 1 || pvolume->FAT1 == NULL) {
		errno = EFAULT;
		return NULL;
	}
	// SPLIT INTO DIR AND FILE
	char *full_path = malloc(strlen(file_name) + 1);
	if (full_path == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	*(full_path + strlen(file_name)) = '\0';
	for (int i = 0; *(file_name + i) != '\0'; ++i) {
		*(full_path + i) = toupper(*(file_name + i));
	}

	char *tok = strtok(full_path, "\\");
	if (tok == NULL) {
		free(full_path);
		return NULL;
	}
	int tmp = 0;
	char* file_path = NULL;
	while(tok != NULL) {
		tmp++;
		file_path = tok;
		tok = strtok(NULL, "\\");
	}
	for (int i = 0; *(file_name + i) != '\0'; ++i) {
		*(full_path + i) = toupper(*(file_name + i));
	}
	// OPEN DIR
	struct dir_t *dir = NULL;
	if (tmp < 2) {

		dir = dir_open(pvolume, "\\");
	} else {
		*(file_path - 1) = '\0';
		dir = dir_open(pvolume, full_path);
	}
	// printf("DIR: %s\n", full_path);
	// printf("FILE: %s\n", file_path);
	if (dir == NULL) {
		errno = ENOENT;
		free(full_path);
		return NULL;
	}
	// FIND RECORD
	struct dir_entry_t ent;
	while(1) {
		int ret = dir_read(dir, &ent);
		// IF NOT FOUND
		if (ret == -1) {
			errno = EFAULT;
			dir_close(dir);
			free(full_path);
			return NULL;
		}
		if (ret == 1) {
			errno = ENOENT;
			dir_close(dir);
			free(full_path);
			return NULL;
		}
		// IF FOUND
		if (strcmp(ent.name, file_path) == 0) {
			break;
		}
	}
	// CLOSE DIR
	if (dir_close(dir) != 0) {
		errno = EFAULT;
		free(full_path);
		return NULL;
	}

	// IF IS A DIR
	if (ent.is_directory) {
		errno = EISDIR;
		free(full_path);
		return NULL;
	}
	// IF FOUND (AGAIN)
	if (strcmp(ent.name, file_path) == 0) {
		uint32_t BytesPerCluster = pvolume->bpb.BPB_BytsPerSec * pvolume->bpb.BPB_SecPerClus;
		// GET CHAINS
		struct clusters_chain_t *chain = get_chain_fat12(pvolume->FAT1, pvolume->bpb.BPB_BytsPerSec * pvolume->bpb.BPB_FATSz16, ent.cluster_number);
		if (chain == NULL) {
			free(full_path);
			return NULL;
		}
		// ALLOC MEM
		struct file_t *pFile = malloc(sizeof(struct file_t));
		if (pFile == NULL) {
			free(chain->clusters);
			free(chain);
			free(full_path);
			errno = ENOMEM;
			return NULL;
		}
		// ALLOC CLUSTERS
		pFile->file = malloc(chain->size*BytesPerCluster);
		if (pFile->file == NULL) {
			free(chain->clusters);
			free(chain);
			free(pFile);
			free(full_path);
			errno = ENOMEM;
			return NULL;
		}
		pFile->size = ent.size;

		
		// MAP CLUSTERS WITH CHAINS ONTO ALLOCATED MEM
		for (uint32_t i = 0; i < chain->size; ++i) {
			if (disk_read(pvolume->pdisk, (data_addr(pvolume->bpb) + (*(chain->clusters + i) - 2) * BytesPerCluster) / BLOCK_SIZE, pFile->file + i * BytesPerCluster, BytesPerCluster / BLOCK_SIZE) == -1) {
				free(chain->clusters);
				free(chain);
				free(pFile->file);
				free(pFile);
				free(full_path);
				return NULL;
			}
		}
		free(chain->clusters);
		free(chain);
		free(full_path);

		// SET POS
		pFile->pos = pFile->file;
		// RETURN
		return pFile;
	}
	free(full_path);
	// IF NOT FOUND
	errno = ENOENT;
	return NULL;
}
int file_close(struct file_t* stream) {
	if (stream == NULL || stream->file == NULL) {
		errno = EFAULT;
		return -1;
	}
	free(stream->file);
	free(stream);
	return 0;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence) {
	if (stream == NULL || stream->file == NULL || stream->pos == NULL) {
		errno = EFAULT;
		return -1;
	}
	int32_t s_pos = 0;
	switch(whence) {
		case SEEK_SET:
			s_pos = s_pos + offset;
			break;
		case SEEK_CUR:
			s_pos = stream->pos - stream->file + offset;
			break;
		case SEEK_END:
			s_pos = stream->size + offset;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	if ((uint32_t)s_pos > stream->size || s_pos < 0) {
		errno = EINVAL;
		return -1;
	}
	stream->pos = stream->file + s_pos;
	return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {
	if (stream == NULL || stream->file == NULL || stream->pos == NULL || ptr == NULL || size == 0 || nmemb == 0) {
		errno = EFAULT;
		return -1;
	}
	size_t got = 0;

	for (; got < nmemb; ++got) {
		// CHECK IF CHUNK OUT OF RANGE
		if (file_seek(stream, size, SEEK_CUR) != 0) {
			// COPY REST AVAILABLE
			memcpy((uint8_t *)ptr + got*size, stream->pos, stream->size - (size_t)(stream->pos - stream->file));
			return got;
		}
		// GO BACK
		file_seek(stream, -size, SEEK_CUR);
		// COPY CHUNK
		memcpy((uint8_t *)ptr + got*size, stream->pos, size);
		// SET TO NEXT CHUNK
		file_seek(stream, size, SEEK_CUR);
	}
	return got;
}
void convert_entry_name(struct actual_dir_entry_t ent, char *dest) {
	if (dest == NULL) {
		return;
	}
	int n_len = 0;
	int e_len = 0;
	for (int i = 0; i < 11; ++i) {
		if (ent.DIR_Name[i] != ' ') {
			if (i < 8) {
				n_len++;
			}
			if (i >= 8) {
				e_len++;
			}
		}
	}
	for (int i = 0; i < n_len; ++i) {
		*(dest + i) = ent.DIR_Name[i];
	}
	if (e_len) {
		*(dest + n_len) = '.';
	}
	for (int i = 0; i < e_len; ++i) {
		*(dest + n_len + 1 + i) = ent.DIR_Name[i + 8];
	}
	*(dest + n_len + (e_len ? e_len + 1 : 0)) = '\0';
}
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path) {
	if (pvolume == NULL || dir_path == NULL) {
		errno = EFAULT;
		return NULL;
	}
	struct dir_t *r_dir = malloc(sizeof(struct dir_t));
	if (r_dir == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	char *curr_path = malloc(strlen(dir_path) + 1);
	if (curr_path == NULL) {
		free(r_dir);
		errno = ENOMEM;
		return NULL;
	}
	*(curr_path + strlen(dir_path)) = '\0';
	for (int i = 0; *(dir_path + i) != '\0'; ++i) {
		*(curr_path + i) = toupper(*(dir_path + i));
	}

	char *tok = strtok(curr_path, "\\");

	r_dir->pvolume = pvolume;
	r_dir->cluster_number = 0;
	r_dir->curr_i = 0;

	// MAIN DIRECTORY ONLY
	if (tok == NULL) {
		free(curr_path);
		return r_dir;
	}
	struct dir_entry_t ent;
	ent.cluster_number = 0;

	while(tok != NULL) {
		r_dir->curr_i = 0;
		r_dir->cluster_number = ent.cluster_number;
		// ROOT DIR SEARCH
		do {
			int ret = dir_read(r_dir, &ent);
			if (ret == -1) {
				free(curr_path);
				free(r_dir);
				errno = ENXIO;
				return NULL;
			} else if (ret == 1) {
				free(curr_path);
				free(r_dir);
				errno = ENOENT;
				return NULL;
			}
		} while(strcmp(ent.name, tok) != 0);
		// CHECK IF IT IS A FILE
		if (!ent.is_directory) {
			free(curr_path);
			dir_close(r_dir);
			errno = ENOTDIR;
			return NULL;
		}
		// GET NEXT PATH
		tok = strtok(NULL, "\\");
	}
	r_dir->curr_i = 0;
	r_dir->cluster_number = ent.cluster_number;
	free(curr_path);
	return r_dir;
}
int dir_close(struct dir_t* pdir) {
	if (pdir == NULL) {
		errno = EFAULT;
		return -1;
	}
	free(pdir);
	return 0;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry) {
	if (pdir == NULL || pentry == NULL) {
		errno = EFAULT;
		return -1;
	}
	uint8_t *buffer = NULL;
	// SEARCH ROOT DIR
	if (pdir->cluster_number == 0) {
		buffer = malloc(pdir->pvolume->bpb.BPB_RootEntCnt*FAT_RECORD_SIZE);
		if (buffer == NULL) {
			errno = ENOMEM;
			return -1;
		}
		// READ TO BUFFER
		if (disk_read(pdir->pvolume->pdisk, root_addr(pdir->pvolume->bpb) / BLOCK_SIZE, buffer, pdir->pvolume->bpb.BPB_RootEntCnt*FAT_RECORD_SIZE / BLOCK_SIZE) == -1) {
			errno = ENXIO;
			free(buffer);
			return -1;
		}
	}
	else {
		struct clusters_chain_t *chain = get_chain_fat12(pdir->pvolume->FAT1, pdir->pvolume->bpb.BPB_BytsPerSec * pdir->pvolume->bpb.BPB_FATSz16, pdir->cluster_number);
		if (chain == NULL) {
			errno = ENOMEM;
			return -1;
		}
		uint32_t ClusterSize = pdir->pvolume->bpb.BPB_SecPerClus * pdir->pvolume->bpb.BPB_BytsPerSec;
		// CLUSTER AWARE BUFFER
		buffer = malloc(chain->size*ClusterSize);
		if (buffer == NULL) {
			free(chain->clusters);
			free(chain);
			errno = ENOMEM;
			return -1;
		}
		// READ ALL DIR CLUSTERS
		for (size_t i = 0; i < chain->size; ++i) {
			uint32_t address = data_addr(pdir->pvolume->bpb) + (*(chain->clusters + i) - 2) * ClusterSize;
			// READ TO BUFFER
			if (disk_read(pdir->pvolume->pdisk, address / BLOCK_SIZE, buffer + i*ClusterSize, ClusterSize / BLOCK_SIZE) == -1) {
				errno = ENXIO;
				free(chain->clusters);
				free(chain);
				free(buffer);
				return -1;
			}
		}
		free(chain->clusters);
		free(chain);
	}
	// GO OVER WHOLE DIR
	struct actual_dir_entry_t *ent;
	while(1) {
		ent = (struct actual_dir_entry_t *)(buffer + pdir->curr_i * FAT_RECORD_SIZE);
		// ENTRY CORRUPTION
		if (ent == NULL) {
			errno = EFAULT;
			free(buffer);
			return 1;
		}
		pdir->curr_i++;
		// END OF DIR
		if (ent->DIR_Name[0] == 0x00) {
			errno = EIO;
			free(buffer);
			return 1;
		} else
		// DIR IS FREE
		if (ent->DIR_Name[0] == 0xE5) {
			continue;
		} else
		// DIR[0] IS A KANJI
		if (ent->DIR_Name[0] == 0x05) {
			ent->DIR_Name[0] = 0xE5;
			break;
		} else {
			break;
		}
	}	
	convert_entry_name(*ent, pentry->name);
	pentry->size = ent->DIR_FileSize;
	pentry->is_archived = ent->DIR_Attr.ATTR_ARCHIVE;
	pentry->is_readonly = ent->DIR_Attr.ATTR_READ_ONLY;
	pentry->is_system = ent->DIR_Attr.ATTR_SYSTEM;
	pentry->is_hidden = ent->DIR_Attr.ATTR_HIDDEN;
	pentry->is_directory = ent->DIR_Attr.ATTR_DIRECTORY;
	pentry->cluster_number = ent->DIR_FstClusLO;
	free(buffer);
	return 0;
}
void print_fat_info(struct bpb_t bpb) {
	uint32_t RootDirSectors = ((bpb.BPB_RootEntCnt * FAT_RECORD_SIZE) + (bpb.BPB_BytsPerSec - 1)) / bpb.BPB_BytsPerSec;
	uint32_t TotalSec = (bpb.BPB_TotSec16 != 0) ? bpb.BPB_TotSec16 : bpb.BPB_TotSec32;
	uint32_t DataSec = TotalSec - (bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz16) + RootDirSectors);
	uint32_t CountofClusters = DataSec / bpb.BPB_SecPerClus;
	uint32_t FatSize = bpb.BPB_BytsPerSec * bpb.BPB_FATSz16;

	printf("FAT Filesystem information\n\n");

	printf("Filesystem type: FAT12\n");
	printf("OEM name: ");
	for (int i = 0; i < 8; ++i) {
		putchar(bpb.BS_OEMName[i]);
	}
	printf("\n");

	printf("Total sectors: %u\n", TotalSec);
	printf("Total data clusters: %u\n", CountofClusters + 2);
	printf("Data size: %u\n", (CountofClusters + 2) * (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus));
	printf("Disk size: %u\n", TotalSec * bpb.BPB_BytsPerSec);
	printf("Bytes per sector: %hu\n", bpb.BPB_BytsPerSec);
	printf("Sectors per cluster: %hu\n", bpb.BPB_SecPerClus);
	printf("Bytes per cluster: %d\n", bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus);
	printf("Reserved sectors: %hu\n", bpb.BPB_RsvdSecCnt);
	printf("Root entries: %hu\n", bpb.BPB_RootEntCnt);
	printf("Root clusters: %u\n", (bpb.BPB_RootEntCnt * FAT_RECORD_SIZE)/(bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus));
	printf("Sectors per FAT: %hu\n", bpb.BPB_FATSz16);
	printf("Fat size: %u\n", FatSize);

	printf("FAT1 start address: %016x\n", fat1_addr(bpb));
	printf("FAT2 start address: %016x\n", fat2_addr(bpb));
	printf("Root start address: %016x\n", root_addr(bpb));
	printf("Data start address: %016x\n", data_addr(bpb));
	printf("Root directory cluster: %u\n", root_addr(bpb) / bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus);
	printf("Disk label: ");
	for (int i = 0; i < 11; ++i) {
		putchar(bpb.BS_VolLab[i]);
	}
	printf("\n");
}
void print_entry_info(struct dir_entry_t dir) {
	printf("Name: %s\n", dir.name);
	printf("Attributes: \n");
	printf("\tRead Only    : %s\n", dir.is_readonly ? "Yes" : "No");
	printf("\tHidden       : %s\n", dir.is_hidden ? "Yes" : "No");
	printf("\tSystem file  : %s\n", dir.is_system ? "Yes" : "No");
	printf("\tDirectory    : %s\n", dir.is_directory ? "Yes" : "No");
	printf("\tArchive      : %s\n", dir.is_archived ? "Yes" : "No");
}
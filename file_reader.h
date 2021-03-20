#ifndef __FILE_READER_H__
#define __FILE_READER_H__

#include "fat_structs.h"

#define BLOCK_SIZE      512
#define SIG             0xAA55
#define FAT_RECORD_SIZE 32

struct disk_t {
	const char*			filename;
	FILE*				handle;
};
struct volume_t {
	struct disk_t*		pdisk;
	struct bpb_t		bpb;

	void*				FAT1;
	void*				FAT2;
};
struct dir_t {
	struct volume_t*	pvolume;
	uint16_t			curr_i;
	uint16_t			cluster_number;
};
struct dir_entry_t {
	char 				name[13];
	uint32_t 			size;

	int 				is_archived;
	int 				is_readonly;
	int 				is_system;
	int 				is_hidden;
	int 				is_directory;

	uint16_t			cluster_number;
};
struct file_t {
	uint8_t*			file;
	uint8_t*			pos;
	uint32_t 			size;
};
struct clusters_chain_t {
	uint16_t			*clusters;
	uint32_t			size;
};


// FAT ADDRESSES
uint32_t fat1_addr(struct bpb_t bpb);
uint32_t fat2_addr(struct bpb_t bpb);
uint32_t root_addr(struct bpb_t bpb);
uint32_t data_addr(struct bpb_t bpb);

// FILE HANDLING
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

// FAT INIT
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

// FAT HELPER FUNCTIONS
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster);

// POSIX FUNCTIONS
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);

// DIR
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_close(struct dir_t* pdir);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);

// PRINTING
void print_fat_info(struct bpb_t bpb);
void print_entry_info(struct dir_entry_t dir);

#endif
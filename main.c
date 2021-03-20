#include "file_reader.h"
#include <assert.h>
#include <stdlib.h>

#define EXPECTED_FILESIZE 521
#define PATH "\\PATH\\TO\\FILE.png"

int main(void) {
	struct disk_t* disk = disk_open_from_file("sample_fat12.img");
	assert(disk != NULL);

	struct volume_t* volume = fat_open(disk, 0);
	assert(volume != NULL);

	struct dir_t* pdir = dir_open(volume, "\\");
	assert(pdir != NULL);

	struct dir_entry_t entry;
	while(!dir_read(pdir, &entry)) {
		printf("%13s | %6d bytes |\n", entry.name, entry.size);
	}
	
	dir_close(pdir);
	fat_close(volume);
	disk_close(disk);
	return 0;
}
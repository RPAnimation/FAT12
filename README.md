# FAT12 Reader
Reader of FAT12 file system additionally, it implements many of POSIX file handling functions.
## Building
Program can be built with most compiliers such as GCC or Clang. It doesn't need any external dependencies.
## Description
It is based on Microsoft's document on FAT file system and it takes most of the structures from there. It can read any FAT12 disk image or binary file and display all of it's content along with parameters and hidden files.

Implemented functions allow to display all of image's BPB data ommiting the boot code. They also provide with a directory/sub-directory navigation and ability to open, close and seek through files. Program written in `main.c` goes through the whole root directory of a sample FAT12 image `sample_fat.img`.

## Sample program
Before we can start using POSIX-like functions, we have to open our image and initialize the volume. Those procedures are meant to be simillar to what we can find in an operating system.
```cpp
#include "file_reader.h"
#include <assert.h>
#include <stdlib.h>

#define EXPECTED_FILESIZE 521
#define PATH "\\PATH\\TO\\FILE.png"

int main(void) {
    // OPENING A DISK IMAGE
    struct disk_t* disk = disk_open_from_file("fat_1.img");
    assert(disk != NULL);
    // OPENING AN ACTUAL VOLUME
    struct volume_t* volume = fat_open(disk, 0);
    assert(volume != NULL);
    // OPENING A FILE WITH A GIVEN PATH
    struct file_t* file = file_open(volume, PATH);
    assert(file != NULL);
    // ALLOC MEMORY FOR EXPECTED FILE
    char *filecontent = (char *)calloc(EXPECTED_FILESIZE, 1);
    assert(filecontent != NULL);
    // READ THE FILE
    size_t size = file_read(filecontent, 1, EXPECTED_FILESIZE, file);
    assert(size == EXPECTED_FILESIZE);
    // CLEAN UP
    free(filecontent);
    file_close(file);
    fat_close(volume);
    disk_close(disk);
    return 0;
}
```

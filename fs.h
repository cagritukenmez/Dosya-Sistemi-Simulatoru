#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include <stdbool.h>

// Disk parameters
#define DISK_NAME "disk.sim"
#define DISK_SIZE (1024*1024)  // 1 MB
#define META_SIZE 4096         // 4 KB for metadata
#define DATA_SIZE (DISK_SIZE - META_SIZE)

// File system limits
#define MAX_FILENAME_LEN 32
#define MAX_FILES 64

// Data structures
struct FileEntry {
    char name[MAX_FILENAME_LEN];
    int size;
    int start;
    char created[20];  // creation date-time string "YYYY-MM-DD HH:MM:SS"
};

struct FileSystem {
    int file_count;
    struct FileEntry files[MAX_FILES];
};

// Extern global FileSystem instance (defined in fs.c)
extern struct FileSystem fs;

// Function prototypes
int fs_format();
int fs_create(const char *filename);
int fs_delete(const char *filename);
int fs_write(const char *filename, const char *data, int size);
int fs_append(const char *filename, const char *data, int size);
int fs_read(const char *filename, int offset, int size, char *buffer);
int fs_ls();
int fs_rename(const char *oldname, const char *newname);
bool fs_exists(const char *filename);
int fs_size(const char *filename);
int fs_truncate(const char *filename, int new_size);
int fs_copy(const char *src_filename, const char *dest_filename);
int fs_mv(const char *src_filename, const char *dest_filename);
int fs_defragment();
int fs_check_integrity();
int fs_backup(const char *backup_filename);
int fs_restore(const char *backup_filename);
int fs_cat(const char *filename);
int fs_diff(const char *file1, const char *file2);
int fs_log();  // show log of operations

// Utility functions
int fs_init();   // initialize filesystem (open disk, load metadata)
int fs_close();  // close files and cleanup

#endif // SIMPLEFS_H

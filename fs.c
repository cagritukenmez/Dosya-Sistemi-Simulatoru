#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include "fs.h"

struct FileSystem fs;
static int disk_fd = -1;
static int log_fd = -1;
static const char *log_filename = "fs.log";

// Helper to log operations with timestamp
static void log_operation(const char *operation, const char *detail, int result) {
    if (log_fd < 0) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timebuf[20];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
    char msg[256];
    if (result == 0) {
        if (detail)
            snprintf(msg, sizeof(msg), "%s - %s: %s SUCCESS\n", timebuf, operation, detail);
        else
            snprintf(msg, sizeof(msg), "%s - %s SUCCESS\n", timebuf, operation);
    } else {
        if (detail)
            snprintf(msg, sizeof(msg), "%s - %s: %s FAIL\n", timebuf, operation, detail);
        else
            snprintf(msg, sizeof(msg), "%s - %s FAIL\n", timebuf, operation);
    }
    write(log_fd, msg, strlen(msg));
}

// Find index of a file in metadata by name
static int find_file_index(const char *filename) {
    for (int i = 0; i < fs.file_count; ++i) {
        if (strcmp(fs.files[i].name, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// Save metadata (fs struct) to disk
static int save_metadata() {
    if (disk_fd < 0) return -1;
    if (lseek(disk_fd, 0, SEEK_SET) < 0) return -1;
    unsigned char meta_buf[META_SIZE];
    memset(meta_buf, 0, META_SIZE);
    memcpy(meta_buf, &fs, sizeof(fs));
    ssize_t bytes = write(disk_fd, meta_buf, META_SIZE);
    if (bytes != META_SIZE) return -1;
    fsync(disk_fd);
    return 0;
}

// Load metadata from disk into fs struct
static int load_metadata() {
    if (disk_fd < 0) return -1;
    if (lseek(disk_fd, 0, SEEK_SET) < 0) return -1;
    unsigned char meta_buf[META_SIZE];
    ssize_t bytes = read(disk_fd, meta_buf, META_SIZE);
    if (bytes < 0) return -1;
    if (bytes > 0) memcpy(&fs, meta_buf, sizeof(fs));
    return 0;
}

// Initialize the file system (open or create disk file, load or format)
int fs_init() {
    disk_fd = open(DISK_NAME, O_RDWR);
    if (disk_fd < 0) {
        // Disk doesn't exist, create new file
        disk_fd = open(DISK_NAME, O_RDWR | O_CREAT, 0666);
        if (disk_fd < 0) {
            perror("Disk dosyası oluşturulamadı");
            return -1;
        }
        if (ftruncate(disk_fd, DISK_SIZE) < 0) {
            perror("Disk boyutu ayarlanamadı");
            return -1;
        }
        fs.file_count = 0;
        memset(fs.files, 0, sizeof(fs.files));
        if (save_metadata() < 0) {
            perror("Metadata yazılamadı");
            return -1;
        }
    } else {
        struct stat st;
        if (fstat(disk_fd, &st) == 0) {
            if (st.st_size != DISK_SIZE) {
                // If disk size is wrong, reset it
                ftruncate(disk_fd, DISK_SIZE);
                fs.file_count = 0;
                memset(fs.files, 0, sizeof(fs.files));
                save_metadata();
            } else {
                if (load_metadata() < 0) {
                    fprintf(stderr, "Metadata yüklenemedi, disk bozuk olabilir.\n");
                    fs.file_count = 0;
                    memset(fs.files, 0, sizeof(fs.files));
                    save_metadata();
                }
            }
        }
    }
    // Open log file for appending
    log_fd = open(log_filename, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (log_fd < 0) {
        perror("Log dosyası açılamadı");
    }
    return 0;
}

// Close disk and log file descriptors
int fs_close() {
    if (disk_fd >= 0) close(disk_fd);
    if (log_fd >= 0) close(log_fd);
    return 0;
}

// Format the disk (reset filesystem)
int fs_format() {
    fs.file_count = 0;
    memset(fs.files, 0, sizeof(fs.files));
    if (save_metadata() < 0) {
        printf("Format başarısız (metadata yazılamadı)\n");
        log_operation("fs_format", NULL, -1);
        return -1;
    }
    // Wipe data area to zeros
    char zeros[512];
    memset(zeros, 0, sizeof(zeros));
    if (lseek(disk_fd, META_SIZE, SEEK_SET) < 0) {
        printf("Format başarısız (konum hatası)\n");
        log_operation("fs_format", NULL, -1);
        return -1;
    }
    size_t remaining = DATA_SIZE;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(zeros) ? remaining : sizeof(zeros);
        write(disk_fd, zeros, chunk);
        remaining -= chunk;
    }
    fsync(disk_fd);
    printf("Disk formatlandı (tüm veriler silindi)\n");
    log_operation("fs_format", NULL, 0);
    return 0;
}

// Create a new file (empty)
int fs_create(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Hatalı dosya adı.\n");
        log_operation("fs_create", filename, -1);
        return -1;
    }
    if (find_file_index(filename) != -1) {
        printf("Hata: '%s' dosyası zaten mevcut.\n", filename);
        log_operation("fs_create", filename, -1);
        return -1;
    }
    if (fs.file_count >= MAX_FILES) {
        printf("Hata: Maksimum dosya sayısına ulaşıldı.\n");
        log_operation("fs_create", filename, -1);
        return -1;
    }
    // Prepare new file entry
    struct FileEntry new_file;
    memset(&new_file, 0, sizeof(new_file));
    strncpy(new_file.name, filename, MAX_FILENAME_LEN - 1);
    new_file.size = 0;
    new_file.start = -1;  // no data allocated yet
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(new_file.created, sizeof(new_file.created), "%Y-%m-%d %H:%M:%S", tm_info);
    // Add to metadata
    fs.files[fs.file_count] = new_file;
    fs.file_count++;
    if (save_metadata() < 0) {
        printf("Dosya oluşturulamadı (yazma hatası)\n");
        fs.file_count--;
        log_operation("fs_create", filename, -1);
        return -1;
    }
    printf("Dosya '%s' oluşturuldu.\n", filename);
    log_operation("fs_create", filename, 0);
    return 0;
}

// Delete a file
int fs_delete(const char *filename) {
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_delete", filename, -1);
        return -1;
    }
    // Remove the file entry by shifting others
    for (int i = idx; i < fs.file_count - 1; ++i) {
        fs.files[i] = fs.files[i+1];
    }
    fs.file_count--;
    if (save_metadata() < 0) {
        printf("Dosya silinirken hata oluştu.\n");
        log_operation("fs_delete", filename, -1);
        return -1;
    }
    printf("Dosya '%s' silindi.\n", filename);
    log_operation("fs_delete", filename, 0);
    return 0;
}

// Write data to a file (overwrite from beginning)
int fs_write(const char *filename, const char *data, int size) {
    if (!filename || !data || size < 0) {
        printf("Hatalı parametre.\n");
        log_operation("fs_write", filename, -1);
        return -1;
    }
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_write", filename, -1);
        return -1;
    }
    struct FileEntry *file = &fs.files[idx];
    if (size == 0) {
        // Truncate file to 0
        file->size = 0;
        file->start = -1;
        save_metadata();
        printf("Dosya '%s' içeriği sıfırlandı.\n", filename);
        log_operation("fs_write", filename, 0);
        return 0;
    }
    // If new data fits in current allocated space
    if (file->size >= size && file->start != -1) {
        if (lseek(disk_fd, file->start, SEEK_SET) < 0) {
            printf("Yazma hatası (konumlanamadı)\n");
            log_operation("fs_write", filename, -1);
            return -1;
        }
        if (write(disk_fd, data, size) != size) {
            printf("Yazma hatası (disk)\n");
            log_operation("fs_write", filename, -1);
            return -1;
        }
        file->size = size;
        save_metadata();
        printf("Dosya '%s' üzerine %d bayt veri yazıldı (üstüne yazma).\n", filename, size);
        log_operation("fs_write", filename, 0);
        return 0;
    }
    // Need to allocate new space (file empty or expanding beyond current space)
    int needed = size;
    // Build list of used segments (start and end of each file)
    struct { int start; int end; } segs[MAX_FILES];
    int seg_count = 0;
    for (int i = 0; i < fs.file_count; ++i) {
        if (fs.files[i].size > 0) {
            segs[seg_count].start = fs.files[i].start;
            segs[seg_count].end = fs.files[i].start + fs.files[i].size;
            seg_count++;
        }
    }
    // Sort segments by start address
    for (int i = 0; i < seg_count - 1; ++i) {
        for (int j = 0; j < seg_count - 1 - i; ++j) {
            if (segs[j].start > segs[j+1].start) {
                // swap
                int ts = segs[j].start;
                int te = segs[j].end;
                segs[j].start = segs[j+1].start;
                segs[j].end = segs[j+1].end;
                segs[j+1].start = ts;
                segs[j+1].end = te;
            }
        }
    }
    // Find a free gap of sufficient size
    int found_start = -1;
    if (seg_count == 0) {
        // no files, entire data area is free
        if (needed <= DATA_SIZE) {
            found_start = META_SIZE;
        }
    } else {
        // check gap before first file
        if (segs[0].start - META_SIZE >= needed) {
            found_start = META_SIZE;
        } else {
            // check between files
            for (int k = 0; k < seg_count - 1; ++k) {
                int gap_start = segs[k].end;
                int gap_end = segs[k+1].start;
                if (gap_end - gap_start >= needed) {
                    found_start = gap_start;
                    break;
                }
            }
            // check after last file
            if (found_start == -1) {
                if (DISK_SIZE - segs[seg_count-1].end >= needed) {
                    found_start = segs[seg_count-1].end;
                }
            }
        }
    }
    if (found_start == -1) {
        printf("Hata: Disk üzerinde yeterli sürekli boş alan yok.\n");
        printf("Lütfen 'fs_defragment' işlemini yapıp tekrar deneyin.\n");
        log_operation("fs_write", filename, -1);
        return -1;
    }
    // Write new data at found_start
    if (lseek(disk_fd, found_start, SEEK_SET) < 0) {
        printf("Disk konum hatası\n");
        log_operation("fs_write", filename, -1);
        return -1;
    }
    if (write(disk_fd, data, size) != size) {
        printf("Disk yazma hatası\n");
        log_operation("fs_write", filename, -1);
        return -1;
    }
    // Update metadata for file
    file->start = found_start;
    file->size = size;
    if (save_metadata() < 0) {
        printf("Metadata güncellenemedi\n");
        log_operation("fs_write", filename, -1);
        return -1;
    }
    printf("Dosyaya '%s' %d bayt veri yazıldı (yeni boyut=%d).\n", filename, size, size);
    log_operation("fs_write", filename, 0);
    return 0;
}

// Append data to end of file
int fs_append(const char *filename, const char *data, int size) {
    if (!filename || !data || size <= 0) {
        printf("Hatalı parametre.\n");
        log_operation("fs_append", filename, -1);
        return -1;
    }
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_append", filename, -1);
        return -1;
    }
    struct FileEntry *file = &fs.files[idx];
    if (file->size == 0) {
        // if file is empty, same as write
        return fs_write(filename, data, size);
    }
    int new_size = file->size + size;
    int file_end = file->start + file->size;
    // Find the next file's start or disk end
    int next_start = DISK_SIZE;
    for (int i = 0; i < fs.file_count; ++i) {
        if (fs.files[i].size > 0 && fs.files[i].start > file->start) {
            if (fs.files[i].start < next_start) {
                next_start = fs.files[i].start;
            }
        }
    }
    int contig_space = next_start - file_end;
    if (contig_space >= size) {
        // enough space right after current file
        if (lseek(disk_fd, file_end, SEEK_SET) < 0) {
            printf("Disk konumlanma hatası\n");
            log_operation("fs_append", filename, -1);
            return -1;
        }
        if (write(disk_fd, data, size) != size) {
            printf("Disk yazma hatası\n");
            log_operation("fs_append", filename, -1);
            return -1;
        }
        file->size = new_size;
        save_metadata();
        printf("Dosyaya '%s' %d bayt veri eklendi (yeni boyut=%d).\n", filename, size, new_size);
        log_operation("fs_append", filename, 0);
        return 0;
    } else {
        printf("Hata: Dosyanın sonuna ekleme için yer yok.\n");
        printf("Lütfen 'fs_defragment' işlemini yapıp tekrar deneyin.\n");
        log_operation("fs_append", filename, -1);
        return -1;
    }
}

// Read data from a file
int fs_read(const char *filename, int offset, int size, char *buffer) {
    if (!filename || !buffer || size < 0 || offset < 0) {
        printf("Hatalı parametre.\n");
        log_operation("fs_read", filename, -1);
        return -1;
    }
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_read", filename, -1);
        return -1;
    }
    struct FileEntry *file = &fs.files[idx];
    if (offset >= file->size) {
        printf("Hata: Ofset dosya boyutunun dışında.\n");
        log_operation("fs_read", filename, -1);
        return -1;
    }
    if (offset + size > file->size) {
        size = file->size - offset;
    }
    if (size <= 0) {
        printf("Okunacak veri yok.\n");
        log_operation("fs_read", filename, 0);
        return 0;
    }
    if (lseek(disk_fd, file->start + offset, SEEK_SET) < 0) {
        printf("Okuma hatası (konumlanamadı)\n");
        log_operation("fs_read", filename, -1);
        return -1;
    }
    ssize_t bytes = read(disk_fd, buffer, size);
    if (bytes < 0) {
        printf("Disk okuma hatası\n");
        log_operation("fs_read", filename, -1);
        return -1;
    }
    buffer[bytes] = '\0';
    printf("Dosyadan okunan veri (%d bayt): \"%.*s\"\n", (int)bytes, (int)bytes, buffer);
    log_operation("fs_read", filename, 0);
    return bytes;
}

// List files in the filesystem
int fs_ls() {
    if (fs.file_count == 0) {
        printf("Dosya sistemi boş.\n");
    } else {
        printf("Dosya Listesi (%d dosya):\n", fs.file_count);
        printf("%-20s %10s %20s\n", "Dosya Adı", "Boyut", "Oluşturulma Tarihi");
        printf("------------------------------------------------------------\n");
        for (int i = 0; i < fs.file_count; ++i) {
            struct FileEntry *f = &fs.files[i];
            printf("%-20s %10d %20s\n", f->name, f->size, f->created);
        }
    }
    log_operation("fs_ls", NULL, 0);
    return fs.file_count;
}

// Rename a file
int fs_rename(const char *oldname, const char *newname) {
    if (!oldname || !newname || strlen(newname) == 0) {
        printf("Hatalı dosya adı.\n");
        log_operation("fs_rename", oldname, -1);
        return -1;
    }
    int idx = find_file_index(oldname);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", oldname);
        log_operation("fs_rename", oldname, -1);
        return -1;
    }
    if (find_file_index(newname) != -1) {
        printf("Hata: '%s' ismi zaten kullanılıyor.\n", newname);
        log_operation("fs_rename", oldname, -1);
        return -1;
    }
    strncpy(fs.files[idx].name, newname, MAX_FILENAME_LEN - 1);
    fs.files[idx].name[MAX_FILENAME_LEN - 1] = '\0';
    if (save_metadata() < 0) {
        printf("Yeniden adlandırma hatası (metadata)\n");
        log_operation("fs_rename", oldname, -1);
        return -1;
    }
    printf("Dosya '%s' yeni adı '%s' olarak değiştirildi.\n", oldname, newname);
    log_operation("fs_rename", oldname, 0);
    return 0;
}

// Check if file exists
bool fs_exists(const char *filename) {
    bool exists = (find_file_index(filename) != -1);
    printf("Dosya '%s' %s\n", filename, exists ? "mevcut." : "mevcut değil.");
    log_operation("fs_exists", filename, exists ? 0 : -1);
    return exists;
}

// Get size of a file
int fs_size(const char *filename) {
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_size", filename, -1);
        return -1;
    }
    printf("Dosya '%s' boyutu: %d bayt\n", filename, fs.files[idx].size);
    log_operation("fs_size", filename, 0);
    return fs.files[idx].size;
}

// Truncate a file to a smaller size
int fs_truncate(const char *filename, int new_size) {
    if (new_size < 0) {
        printf("Hatalı boyut.\n");
        log_operation("fs_truncate", filename, -1);
        return -1;
    }
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_truncate", filename, -1);
        return -1;
    }
    struct FileEntry *file = &fs.files[idx];
    if (new_size > file->size) {
        printf("Hata: Yeni boyut mevcut boyuttan büyük (fs_append kullanın).\n");
        log_operation("fs_truncate", filename, -1);
        return -1;
    }
    if (new_size == file->size) {
        printf("Dosya zaten %d bayt.\n", new_size);
        log_operation("fs_truncate", filename, 0);
        return 0;
    }
    if (new_size == 0) {
        file->size = 0;
        file->start = -1;
        save_metadata();
        printf("Dosya '%s' boyutu sıfırlandı.\n", filename);
        log_operation("fs_truncate", filename, 0);
        return 0;
    }
    file->size = new_size;
    if (save_metadata() < 0) {
        printf("Boyut kısaltma hatası (metadata)\n");
        log_operation("fs_truncate", filename, -1);
        return -1;
    }
    printf("Dosya '%s' boyutu %d bayta kısaltıldı.\n", filename, new_size);
    log_operation("fs_truncate", filename, 0);
    return 0;
}

// Copy a file to a new file
int fs_copy(const char *src_filename, const char *dest_filename) {
    if (!src_filename || !dest_filename) {
        printf("Hatalı parametre.\n");
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    int src_idx = find_file_index(src_filename);
    if (src_idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", src_filename);
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    if (find_file_index(dest_filename) != -1) {
        printf("Hata: '%s' adı zaten kullanılıyor.\n", dest_filename);
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    // Create destination file
    if (fs_create(dest_filename) != 0) {
        return -1;
    }
    int dest_idx = find_file_index(dest_filename);
    if (dest_idx == -1) {
        printf("Kopyalama hatası: hedef oluşturulamadı.\n");
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    struct FileEntry *src = &fs.files[src_idx];
    struct FileEntry *dest = &fs.files[dest_idx];
    if (src->size == 0) {
        // Source is empty, nothing to copy
        dest->size = 0;
        dest->start = -1;
        save_metadata();
        printf("Boş dosya kopyalandı: '%s' oluşturuldu (0 bayt)\n", dest_filename);
        log_operation("fs_copy", src_filename, 0);
        return 0;
    }
    // Allocate buffer and copy data
    char *buf = (char*) malloc(src->size);
    if (!buf) {
        printf("Bellek yetersiz.\n");
        fs_delete(dest_filename);
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    if (lseek(disk_fd, src->start, SEEK_SET) < 0) {
        printf("Kopyalama hatası (kaynak okunamadı)\n");
        free(buf);
        fs_delete(dest_filename);
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    if (read(disk_fd, buf, src->size) != src->size) {
        printf("Kopyalama hatası (kaynak okuma)\n");
        free(buf);
        fs_delete(dest_filename);
        log_operation("fs_copy", src_filename, -1);
        return -1;
    }
    if (fs_write(dest_filename, buf, src->size) < 0) {
        printf("Kopyalama hatası (hedef yazma)\n");
        free(buf);
        fs_delete(dest_filename);
        // fs_write logs failure
        return -1;
    }
    free(buf);
    printf("Dosya '%s' kopyası oluşturuldu -> '%s'\n", src_filename, dest_filename);
    log_operation("fs_copy", src_filename, 0);
    return 0;
}

// Move (rename) a file
int fs_mv(const char *src_filename, const char *dest_filename) {
    if (strchr(dest_filename, '/') != NULL) {
        printf("Hata: Dizin desteği yok.\n");
        log_operation("fs_mv", src_filename, -1);
        return -1;
    }
    int result = fs_rename(src_filename, dest_filename);
    // fs_rename already logs and prints result
    return result;
}

// Defragment the disk (consolidate free space)
int fs_defragment() {
    if (fs.file_count == 0) {
        printf("Diskte dosya yok.\n");
        log_operation("fs_defragment", NULL, 0);
        return 0;
    }
    // Sort files by current start (move 0-size entries to end)
    for (int i = 0; i < fs.file_count - 1; ++i) {
        for (int j = 0; j < fs.file_count - 1 - i; ++j) {
            if (fs.files[j].size > 0 && fs.files[j+1].size > 0) {
                if (fs.files[j].start > fs.files[j+1].start) {
                    struct FileEntry temp = fs.files[j];
                    fs.files[j] = fs.files[j+1];
                    fs.files[j+1] = temp;
                }
            } else if (fs.files[j].size == 0 && fs.files[j+1].size > 0) {
                struct FileEntry temp = fs.files[j];
                fs.files[j] = fs.files[j+1];
                fs.files[j+1] = temp;
            }
        }
    }
    // Copy all file data to a new arrangement
    char *old_data = (char*) malloc(DATA_SIZE);
    if (!old_data) {
        printf("Bellek yetersiz, birleştirme yapılamadı.\n");
        log_operation("fs_defragment", NULL, -1);
        return -1;
    }
    lseek(disk_fd, META_SIZE, SEEK_SET);
    read(disk_fd, old_data, DATA_SIZE);
    char *new_data = (char*) malloc(DATA_SIZE);
    if (!new_data) {
        printf("Bellek yetersiz, birleştirme yapılamadı.\n");
        free(old_data);
        log_operation("fs_defragment", NULL, -1);
        return -1;
    }
    memset(new_data, 0, DATA_SIZE);
    int current_offset = 0;
    for (int i = 0; i < fs.file_count; ++i) {
        if (fs.files[i].size <= 0) continue;
        int old_off = fs.files[i].start - META_SIZE;
        memmove(new_data + current_offset, old_data + old_off, fs.files[i].size);
        fs.files[i].start = META_SIZE + current_offset;
        current_offset += fs.files[i].size;
    }
    // Write rearranged data back to disk
    lseek(disk_fd, META_SIZE, SEEK_SET);
    write(disk_fd, new_data, DATA_SIZE);
    fsync(disk_fd);
    free(old_data);
    free(new_data);
    save_metadata();
    printf("Disk birleştirme tamamlandı.\n");
    log_operation("fs_defragment", NULL, 0);
    return 0;
}

// Check file system integrity
int fs_check_integrity() {
    int issues = 0;
    // Check file count
    if (fs.file_count < 0 || fs.file_count > MAX_FILES) {
        printf("Hata: Dosya sayısı uyumsuz: %d\n", fs.file_count);
        issues++;
    }
    // Check duplicate names
    for (int i = 0; i < fs.file_count; ++i) {
        for (int j = i+1; j < fs.file_count; ++j) {
            if (strcmp(fs.files[i].name, fs.files[j].name) == 0) {
                printf("Hata: Birden fazla dosya '%s' ismine sahip.\n", fs.files[i].name);
                issues++;
            }
        }
    }
    // Sort by start for overlap check
    for (int i = 0; i < fs.file_count - 1; ++i) {
        for (int j = 0; j < fs.file_count - 1 - i; ++j) {
            if (fs.files[j].start > fs.files[j+1].start) {
                struct FileEntry temp = fs.files[j];
                fs.files[j] = fs.files[j+1];
                fs.files[j+1] = temp;
            }
        }
    }
    // Check each file
    for (int i = 0; i < fs.file_count; ++i) {
        struct FileEntry *f = &fs.files[i];
        if (f->size < 0) {
            printf("Hata: '%s' dosyası için negatif boyut.\n", f->name);
            issues++;
        }
        if (f->size > 0) {
            if (f->start < META_SIZE || f->start + f->size > DISK_SIZE) {
                printf("Hata: '%s' dosyasının veri aralığı geçersiz.\n", f->name);
                issues++;
            }
        }
        if (i < fs.file_count - 1 && f->size > 0 && fs.files[i+1].size > 0) {
            if (f->start + f->size > fs.files[i+1].start) {
                printf("Hata: '%s' ve '%s' dosyalarının verileri çakışıyor.\n", f->name, fs.files[i+1].name);
                issues++;
            }
        }
    }
    if (issues == 0) {
        printf("Dosya sistemi tutarlı.\n");
    } else {
        printf("Bütünlük kontrolü tamamlandı, sorun sayısı: %d\n", issues);
    }
    log_operation("fs_check_integrity", NULL, issues == 0 ? 0 : -1);
    return issues == 0 ? 0 : -1;
}

// Backup the entire disk to a file
int fs_backup(const char *backup_filename) {
    if (!backup_filename || strlen(backup_filename) == 0) {
        printf("Yedek dosya adı belirtilmedi.\n");
        log_operation("fs_backup", backup_filename, -1);
        return -1;
    }
    int backup_fd = open(backup_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (backup_fd < 0) {
        printf("Yedek dosyası oluşturulamadı.\n");
        log_operation("fs_backup", backup_filename, -1);
        return -1;
    }
    if (lseek(disk_fd, 0, SEEK_SET) < 0) {
        printf("Disk okunamadı.\n");
        close(backup_fd);
        log_operation("fs_backup", backup_filename, -1);
        return -1;
    }
    char buffer[4096];
    ssize_t bytes;
    size_t total = 0;
    while ((bytes = read(disk_fd, buffer, sizeof(buffer))) > 0) {
        write(backup_fd, buffer, bytes);
        total += bytes;
    }
    close(backup_fd);
    lseek(disk_fd, 0, SEEK_SET);
    if (total != DISK_SIZE) {
        printf("Yedekleme hatası: eksik veri kopyalandı (%zu/%d bayt)\n", total, DISK_SIZE);
        log_operation("fs_backup", backup_filename, -1);
        return -1;
    }
    printf("Disk yedeği '%s' dosyasına alındı (%zu bayt)\n", backup_filename, total);
    log_operation("fs_backup", backup_filename, 0);
    return 0;
}

// Restore disk from a backup file
int fs_restore(const char *backup_filename) {
    if (!backup_filename || strlen(backup_filename) == 0) {
        printf("Yedek dosya adı belirtilmedi.\n");
        log_operation("fs_restore", backup_filename, -1);
        return -1;
    }
    int backup_fd = open(backup_filename, O_RDONLY);
    if (backup_fd < 0) {
        printf("Yedek dosyası açılamadı.\n");
        log_operation("fs_restore", backup_filename, -1);
        return -1;
    }
    if (lseek(disk_fd, 0, SEEK_SET) < 0) {
        printf("Disk yazma hatası (konum)\n");
        close(backup_fd);
        log_operation("fs_restore", backup_filename, -1);
        return -1;
    }
    char buffer[4096];
    ssize_t bytes;
    size_t total = 0;
    while ((bytes = read(backup_fd, buffer, sizeof(buffer))) > 0) {
        write(disk_fd, buffer, bytes);
        total += bytes;
    }
    close(backup_fd);
    ftruncate(disk_fd, DISK_SIZE);
    fsync(disk_fd);
    load_metadata();
    printf("Disk '%s' yedeğinden geri yüklendi (%zu bayt)\n", backup_filename, total);
    log_operation("fs_restore", backup_filename, 0);
    return 0;
}

// Print file content to console
int fs_cat(const char *filename) {
    int idx = find_file_index(filename);
    if (idx == -1) {
        printf("Hata: '%s' dosyası bulunamadı.\n", filename);
        log_operation("fs_cat", filename, -1);
        return -1;
    }
    struct FileEntry *file = &fs.files[idx];
    if (file->size == 0) {
        printf("(boş dosya)\n");
        log_operation("fs_cat", filename, 0);
        return 0;
    }
    if (lseek(disk_fd, file->start, SEEK_SET) < 0) {
        printf("Disk okuma hatası (konum)\n");
        log_operation("fs_cat", filename, -1);
        return -1;
    }
    char buffer[256];
    int bytes_left = file->size;
    char last_char = '\0';
    while (bytes_left > 0) {
        int to_read = bytes_left < (int)sizeof(buffer) ? bytes_left : (int)sizeof(buffer);
        int bytes = read(disk_fd, buffer, to_read);
        if (bytes < 0) {
            printf("Disk okuma hatası\n");
            log_operation("fs_cat", filename, -1);
            return -1;
        }
        if (bytes > 0) last_char = buffer[bytes-1];
        write(1, buffer, bytes);
        bytes_left -= bytes;
    }
    if (last_char != '\n') {
        printf("\n");
    }
    log_operation("fs_cat", filename, 0);
    return file->size;
}

// Compare two files
int fs_diff(const char *file1, const char *file2) {
    int idx1 = find_file_index(file1);
    int idx2 = find_file_index(file2);
    if (idx1 == -1 || idx2 == -1) {
        if (idx1 == -1) printf("Hata: '%s' dosyası bulunamadı.\n", file1);
        if (idx2 == -1) printf("Hata: '%s' dosyası bulunamadı.\n", file2);
        log_operation("fs_diff", file1, -1);
        return -1;
    }
    struct FileEntry *f1 = &fs.files[idx1];
    struct FileEntry *f2 = &fs.files[idx2];
    if (f1->size != f2->size) {
        printf("Dosyalar farklı: boyutları farklı (%d vs %d bayt)\n", f1->size, f2->size);
        log_operation("fs_diff", file1, -1);
        return 1;
    }
    if (f1->size == 0 && f2->size == 0) {
        printf("Dosyalar her ikisi de boş, aynıdır.\n");
        log_operation("fs_diff", file1, 0);
        return 0;
    }
    char *buf1 = (char*) malloc(f1->size);
    char *buf2 = (char*) malloc(f2->size);
    if (!buf1 || !buf2) {
        printf("Bellek yetersiz.\n");
        free(buf1);
        free(buf2);
        log_operation("fs_diff", file1, -1);
        return -1;
    }
    lseek(disk_fd, f1->start, SEEK_SET);
    read(disk_fd, buf1, f1->size);
    lseek(disk_fd, f2->start, SEEK_SET);
    read(disk_fd, buf2, f2->size);
    int diff_found = 0;
    for (int i = 0; i < f1->size; ++i) {
        if (buf1[i] != buf2[i]) {
            printf("Dosyalar farklı: ilk fark %d. baytta (0x%02X vs 0x%02X)\n",
                   i, (unsigned char)buf1[i], (unsigned char)buf2[i]);
            diff_found = 1;
            break;
        }
    }
    if (!diff_found) {
        printf("Dosyalar aynıdır (içerikleri aynı).\n");
    }
    free(buf1);
    free(buf2);
    log_operation("fs_diff", file1, diff_found ? -1 : 0);
    return diff_found ? 1 : 0;
}

// Show operation log history
int fs_log() {
    fflush(stdout);
    printf("\nİşlem Günlüğü:\n");
    fflush(stdout);
    int fd = open(log_filename, O_RDONLY);
    if (fd < 0) {
        printf("Log okunamadı.\n");
        return -1;
    }
    char buffer[256];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        write(1, buffer, bytes);
    }
    close(fd);
    return 0;
}

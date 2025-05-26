#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

int main() {
    if (fs_init() != 0) {
        return 1;
    }
    printf("Basit Dosya Sistemi Simülatörü\n");
    printf("----- Menü -----\n");
    int choice;
    char input[256];
    char filename[128], filename2[128];
    while (1) {
        printf("\n");
        printf("1. Dosya oluştur\n");
        printf("2. Dosya sil\n");
        printf("3. Dosyaya yaz\n");
        printf("4. Dosyadan oku\n");
        printf("5. Dosyaları listele\n");
        printf("6. Diski formatla\n");
        printf("7. Dosyayı yeniden adlandır\n");
        printf("8. Dosya var mı (ara)\n");
        printf("9. Dosya boyutu\n");
        printf("10. Dosyaya ekle\n");
        printf("11. Dosyayı kısalt\n");
        printf("12. Dosya kopyala\n");
        printf("13. Dosya taşı\n");
        printf("14. Birleştir (Defragment)\n");
        printf("15. Bütünlük kontrolü\n");
        printf("16. Disk yedeğini al\n");
        printf("17. Disk yedeğinden dön\n");
        printf("18. Dosya içeriğini görüntüle (cat)\n");
        printf("19. İki dosyayı karşılaştır (diff)\n");
        printf("20. İşlem günlüğünü göster\n");
        printf("21. Çıkış\n");
        printf("Seçiminiz: ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        choice = atoi(input);
        switch (choice) {
            case 1: {
                printf("Oluşturulacak dosyanın adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) > 0) {
                    fs_create(filename);
                }
                break;
            }
            case 2: {
                printf("Silinecek dosyanın adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) > 0) {
                    fs_delete(filename);
                }
                break;
            }
            case 3: {
                printf("Yazılacak dosyanın adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Dosyaya yazılacak veri: ");
                char *data = NULL;
                size_t bufsize = 0;
                ssize_t linelen = getline(&data, &bufsize, stdin);
                if (linelen <= 0) {
                    free(data);
                    break;
                }
                if (data[linelen-1] == '\n') {
                    data[linelen-1] = '\0';
                    linelen--;
                }
                fs_write(filename, data, (int)linelen);
                free(data);
                break;
            }
            case 4: {
                printf("Okunacak dosyanın adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Okuma başlangıç ofseti: ");
                if (!fgets(input, sizeof(input), stdin)) break;
                int offset = atoi(input);
                printf("Okunacak byte sayısı: ");
                if (!fgets(input, sizeof(input), stdin)) break;
                int read_len = atoi(input);
                char *read_buf = malloc(read_len + 1);
                if (!read_buf) {
                    printf("Bellek yetersiz.\n");
                    break;
                }
                fs_read(filename, offset, read_len, read_buf);
                free(read_buf);
                break;
            }
            case 5:
                fs_ls();
                break;
            case 6: {
                printf("Disk formatlanacak, emin misiniz? (E/H): ");
                if (!fgets(input, sizeof(input), stdin)) break;
                if (input[0] == 'E' || input[0] == 'e') {
                    fs_format();
                } else {
                    printf("Formatlama iptal edildi.\n");
                }
                break;
            }
            case 7: {
                printf("Mevcut dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Yeni dosya adı: ");
                if (!fgets(filename2, sizeof(filename2), stdin)) break;
                filename2[strcspn(filename2, "\n")] = '\0';
                if (strlen(filename2) == 0) break;
                fs_rename(filename, filename2);
                break;
            }
            case 8: {
                printf("Aranacak dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                fs_exists(filename);
                break;
            }
            case 9: {
                printf("Boyutu öğrenilecek dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                fs_size(filename);
                break;
            }
            case 10: {
                printf("Veri eklenecek dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Eklenecek veri: ");
                char *data = NULL;
                size_t bufsize = 0;
                ssize_t linelen = getline(&data, &bufsize, stdin);
                if (linelen <= 0) {
                    free(data);
                    break;
                }
                if (data[linelen-1] == '\n') {
                    data[linelen-1] = '\0';
                    linelen--;
                }
                fs_append(filename, data, (int)linelen);
                free(data);
                break;
            }
            case 11: {
                printf("Kısaltılacak dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Yeni boyut (byte): ");
                if (!fgets(input, sizeof(input), stdin)) break;
                int newsize = atoi(input);
                fs_truncate(filename, newsize);
                break;
            }
            case 12: {
                printf("Kaynak dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Kopya dosyanın adı: ");
                if (!fgets(filename2, sizeof(filename2), stdin)) break;
                filename2[strcspn(filename2, "\n")] = '\0';
                if (strlen(filename2) == 0) break;
                fs_copy(filename, filename2);
                break;
            }
            case 13: {
                printf("Taşınacak/kaynağın dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("Yeni konum/isim: ");
                if (!fgets(filename2, sizeof(filename2), stdin)) break;
                filename2[strcspn(filename2, "\n")] = '\0';
                if (strlen(filename2) == 0) break;
                fs_mv(filename, filename2);
                break;
            }
            case 14:
                fs_defragment();
                break;
            case 15:
                fs_check_integrity();
                break;
            case 16: {
                printf("Yedek dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                fs_backup(filename);
                break;
            }
            case 17: {
                printf("Yedekten yüklenecek dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                fs_restore(filename);
                break;
            }
            case 18: {
                printf("Görüntülenecek dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                fs_cat(filename);
                break;
            }
            case 19: {
                printf("Birinci dosya adı: ");
                if (!fgets(filename, sizeof(filename), stdin)) break;
                filename[strcspn(filename, "\n")] = '\0';
                if (strlen(filename) == 0) break;
                printf("İkinci dosya adı: ");
                if (!fgets(filename2, sizeof(filename2), stdin)) break;
                filename2[strcspn(filename2, "\n")] = '\0';
                if (strlen(filename2) == 0) break;
                fs_diff(filename, filename2);
                break;
            }
            case 20:
                fs_log();
                break;
            case 21:
                printf("Çıkış yapılıyor...\n");
                fs_close();
                return 0;
            default:
                printf("Geçersiz seçim. Lütfen menüden bir seçenek giriniz.\n");
        }
    }
    fs_close();
    return 0;
}

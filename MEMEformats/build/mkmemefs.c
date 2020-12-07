/*
    mkmemefs.c

    Filesystem image creation tool for the Multimedia Embedded Memory
    Encapsulation Filesystem.

    Copyright (C) 2020 Lawrence Sebald

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include <arpa/inet.h>

typedef struct memefs_superblock {
    char signature[16];
    uint8_t cleanly_unmounted;
    uint8_t reseerved1[3];
    uint32_t fs_version;
    uint8_t fs_ctime[8];
    uint16_t main_fat;
    uint16_t main_fat_size;
    uint16_t backup_fat;
    uint16_t backup_fat_size;
    uint16_t directory_start;
    uint16_t directory_size;
    uint16_t num_user_blocks;
    uint16_t first_user_block;
    char volume_label[16];
    uint8_t unused[448];
} __attribute__((packed)) memefs_superblock_t;

static uint8_t block_buf[512];

static inline int write_block(int fd) {
    return write(fd, block_buf, 512) != 512;
}

static inline void clear_block_buf(void) {
    memset(block_buf, 0, 512);
}

static inline uint8_t pbcd(uint8_t num) {
    uint8_t a = num % 10, b = num / 10;
    return b <= 9 ? ((b << 4) | a) : 0xFF;
}

static void fill_superblock(const char *volname) {
    memefs_superblock_t *sb = (memefs_superblock_t *)block_buf;
    time_t now = time(NULL);
    struct tm ts;

    gmtime_r(&now, &ts);

    clear_block_buf();
    memcpy(sb->signature, "?MEMEFS++CMSC421", 16);
    sb->fs_version = htonl(1);
    sb->fs_ctime[0] = pbcd((ts.tm_year + 1900) / 100);
    sb->fs_ctime[1] = pbcd(ts.tm_year % 100);
    sb->fs_ctime[2] = pbcd(ts.tm_mon + 1);
    sb->fs_ctime[3] = pbcd(ts.tm_mday);
    sb->fs_ctime[4] = pbcd(ts.tm_hour);
    sb->fs_ctime[5] = pbcd(ts.tm_min);
    sb->fs_ctime[6] = pbcd(ts.tm_sec);
    sb->main_fat = htons(254);
    sb->main_fat_size = htons(1);
    sb->backup_fat = htons(239);
    sb->backup_fat_size = htons(1);
    sb->directory_start = htons(253);
    sb->directory_size = htons(14);
    sb->num_user_blocks = htons(220);
    sb->first_user_block = htons(1);

    if(volname)
        strncpy(sb->volume_label, volname, 16);
}

static void fill_blank_fat(void) {
    int i;
    uint16_t *fat = (uint16_t *)block_buf;

    clear_block_buf();
    fat[0] = 0xFFFF;
    fat[239] = 0xFFFF;
    fat[240] = 0xFFFF;
    fat[254] = 0xFFFF;
    fat[255] = 0xFFFF;

    for(i = 241; i < 254; ++i) {
        fat[i] = htons(i - 1);
    }
}

static int write_fat(int fd) {
    fill_blank_fat();

    if(lseek(fd, 254 * 512, SEEK_SET) < 0) {
        perror("fseek");
        return -1;
    }

    if(write_block(fd)) {
        perror("write_block");
        return -1;
    }

    if(lseek(fd, 239 * 512, SEEK_SET) < 0) {
        perror("fseek");
        return -1;
    }

    if(write_block(fd)) {
        perror("write_block");
        return -1;
    }

    return 0;
}

static int write_superblock(int fd, const char *volname) {
    fill_superblock(volname);

    if(lseek(fd, 255 * 512, SEEK_SET) < 0) {
        perror("fseek");
        return -1;
    }

    if(write_block(fd)) {
        perror("write_block");
        return -1;
    }

    if(lseek(fd, 0 * 512, SEEK_SET) < 0) {
        perror("fseek");
        return -1;
    }

    if(write_block(fd)) {
        perror("write_block");
        return -1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst) {
    FILE *sfp, *dfp;

    if(!(sfp = fopen(src, "rb"))) {
        perror("copy_file");
        return -1;
    }

    if(!(dfp = fopen(dst, "wb"))) {
        perror("copy_file");
        fclose(sfp);
        return -1;
    }

    while(fread(block_buf, 1, 512, sfp) == 512) {
        if(fwrite(block_buf, 1, 512, dfp) != 512) {
            perror("copy_file");
            fclose(dfp);
            fclose(sfp);
            unlink(dst);
            return -1;
        }
    }

    fclose(dfp);
    fclose(sfp);
    return 0;
}

int main(int argc, char *argv[]) {
    int fd;
    char tmpfn[64];

    if(argc < 2 || argc > 3) {
        if(argc > 0)
            printf("Usage: %s image_filename [vol_name]\n", argv[0]);
        else
            printf("Usage: mkmemefs image_filename [vol_name]\n");
        return 1;
    }

    strcpy(tmpfn, "/tmp/mkmemefsXXXXXX");

    if((fd = mkstemp(tmpfn)) < 0) {
        perror("mkstemp");
        return 1;
    }

    if(ftruncate(fd, 256 * 512)) {
        perror("ftruncate");
        close(fd);
        unlink(tmpfn);
        return 1;
    }

    if(write_superblock(fd, argc == 3 ? argv[2] : NULL)) {
        close(fd);
        unlink(tmpfn);
        return 1;
    }

    if(write_fat(fd)) {
        close(fd);
        unlink(tmpfn);
    }

    close(fd);

    if(rename(tmpfn, argv[1])) {
        if(errno == EXDEV) {
            if(!copy_file(tmpfn, argv[1])) {
                unlink(tmpfn);
                return 0;
            }
        }

        perror("rename");
        unlink(tmpfn);
        return 1;
    }

    return 0;
}
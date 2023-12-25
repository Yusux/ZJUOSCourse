#include <fat32.h>
#include <printk.h>
#include <virtio.h>
#include <string.h>
#include <mbr.h>
#include <mm.h>

struct fat32_bpb fat32_header;

struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    // refer to https://gaoyichao.com/Xiaotu/?book=XiaoTuOS&title=FAT%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F
    /*
     * !! BE CAREFUL !!
     * NEED TO ADD `lba` WHICH MEANS THE STARTING SECTOR OF THE PARTITION
     */
    fat32_volume.first_fat_sec = lba + fat32_header.rsvd_sec_cnt;
    fat32_volume.sec_per_cluster = fat32_header.sec_per_clus;
    // Since there is no specific "root directory" partition in FAT32,
    // just add the size of reserved sectors and fat sectors
    fat32_volume.first_data_sec = fat32_volume.first_fat_sec + fat32_header.fat_sz32 * fat32_header.num_fats;
    fat32_volume.fat_sz = fat32_header.fat_sz32 * fat32_header.num_fats;
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

int str_length(const char* str) {
    int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    return i;
}

void to_upper_case(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    // get the file name with upper case
    char file_name[9];
    // skip the prefix "/fat32/"
    int last_slash;
    while ((last_slash = next_slash(path)) != -1) {
        path += last_slash + 1;
    }
    // check the file name
    int len = str_length(path);
    if (len > 8) {
        printk("file name too long\n");
        while (1);
    }
    if (len == 0) {
        printk("file name empty\n");
        while (1);
    }
    /*
     * !! BE CAREFUL !!
     * USE SPACE TO FILL THE FILE NAME
     */
    memset(file_name, 0x20, 8);
    // copy the file name
    memcpy(file_name, path, len);
    // to upper case
    to_upper_case(file_name);

    // find the file in the root directory
    /*
     * file_found = 0: file not found
     * file_found = 1: file found
     * file_found = -1: file not found and no more files
     */
    int file_found = 0;
    uint32_t cluster = fat32_header.root_clus;
    while (!file_found) {
        // read the cluster
        uint64_t sector = cluster_to_sector(cluster);
        // traverse the cluster to find the file
        for (int i = 0; i < fat32_volume.sec_per_cluster; i++) {
            // load the sector to fat32_buf
            virtio_blk_read_sector(sector + i, fat32_buf);
            struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
            for (int j = 0; j < FAT32_ENTRY_PER_SECTOR; j++) {
                if (dir_entry[j].name[0] == 0x00) {
                    // no more files
                    file_found = -1;
                    break;
                }

                // check if the file name matches
                char dir_entry_name[9];
                memcpy(dir_entry_name, dir_entry[j].name, 8);
                dir_entry_name[8] = '\0';
                to_upper_case(dir_entry_name);

                // compare the file name
                if (memcmp(dir_entry_name, file_name, 8) == 0) {
                    file_found = 1;
                    // fill the fat32_file struct
                    file.cluster = (dir_entry[j].starthi << 16) | dir_entry[j].startlow;
                    file.dir.cluster = cluster;
                    file.dir.index = j;
                    break;
                }
            }
            if (file_found) {
                break;
            }
        }

        // check the next cluster
        uint32_t next_cluster_number = next_cluster(cluster);
        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }

    // check if the file is found
    if (file_found != 1) {
        printk("file not found\n");
        while (1);
    }

    // check the dir_entry
    return file;
}

int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    if (whence == SEEK_SET) {
        file->cfo = offset;
    } else if (whence == SEEK_CUR) {
        file->cfo = file->cfo + offset;
    } else if (whence == SEEK_END) {
        uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;
        virtio_blk_read_sector(sector, fat32_table_buf);
        uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
        uint32_t file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
        file->cfo = file_len + offset;
    } else {
        printk("fat32_lseek: whence not implemented\n");
        while (1);
    }
    return file->cfo;
}

uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_extend_filesz(struct file* file, uint64_t new_size) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t original_file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    ((struct fat32_dir_entry *)fat32_table_buf)[index].size = new_size;

    virtio_blk_write_sector(sector, fat32_table_buf);

    uint32_t clusters_required = new_size / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t clusters_original = original_file_len / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t new_clusters = clusters_required - clusters_original;

    uint32_t cluster = file->fat32_file.cluster;
    while (1) {
        uint32_t next_cluster_number = next_cluster(cluster);
        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }

    for (int i = 0; i < new_clusters; i++) {
        uint32_t cluster_to_append;
        for (int j = 2; j < fat32_volume.fat_sz * VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t); j++) {
            if (next_cluster(j) == 0) {
                cluster_to_append = j;
                break;
            }
        }
        uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
        virtio_blk_read_sector(fat_sector, fat32_table_buf);
        uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
        *(uint32_t*)(fat32_table_buf + index_in_sector) = cluster_to_append;
        virtio_blk_write_sector(fat_sector, fat32_table_buf);
        cluster = cluster_to_append;
    }

    uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
    *(uint32_t*)(fat32_table_buf + index_in_sector) = 0x0fffffff;
    virtio_blk_write_sector(fat_sector, fat32_table_buf);

    return 0;
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    // read the dir entry
    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    if (file->cfo + len > file_len) {
        // It is not an error if this number is smaller than the number of bytes requested...
        // from https://man7.org/linux/man-pages/man2/read.2.html
        len = file_len - file->cfo;
    }

    // calculate the cluster number and offset, sector number and offset
    uint32_t cluster = file->fat32_file.cluster;
    uint32_t cluster_number = file->cfo / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint64_t cluster_offset = file->cfo % (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);  // in bytes
    uint64_t sector_number = cluster_offset / VIRTIO_BLK_SECTOR_SIZE;
    uint64_t sector_offset = cluster_offset % VIRTIO_BLK_SECTOR_SIZE;   // in bytes
    // get to the desired cluster
    for (uint32_t i = 0; i < cluster_number; i++) {
        cluster = next_cluster(cluster);
    }
    // calculate the sector to read
    uint64_t sector_to_read = cluster_to_sector(cluster) + sector_number;
    uint64_t bytes_read = 0;
    while (bytes_read < len) {
        // calculate the bytes to read
        uint32_t bytes_to_read = VIRTIO_BLK_SECTOR_SIZE - sector_offset;
        if (bytes_to_read > len - bytes_read) {
            bytes_to_read = len - bytes_read;
        }

        /*
         * # CAUTION #
         * There is no need to judge whether bytes_to_read == 0
         * if so, the loop should have infinited at while (bytes_read < len)
         * which won't happen
         */
        // copy the data to buf
        // load the sector to fat32_buf
        virtio_blk_read_sector(sector_to_read, fat32_buf);
        memcpy(buf + bytes_read, fat32_buf + sector_offset, bytes_to_read);

        // update the variables
        bytes_read += bytes_to_read;
        sector_to_read++;
        sector_offset = 0;  // since read is sequential, sector_offset should be 0 for the next sector

        // check if the sector to read is in the next cluster
        if (sector_to_read % fat32_volume.sec_per_cluster == 0) {
            // if so, get to the next cluster
            cluster = next_cluster(cluster);
            sector_to_read = cluster_to_sector(cluster);
        }
    }
    // add the bytes read to cfo
    file->cfo += bytes_read;
    return bytes_read;
}

int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    // read the dir entry
    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    if (file->cfo + len > file_len) {
        fat32_extend_filesz(file, file->cfo + len);
    }

    // calculate the cluster number and offset, sector number and offset
    uint32_t cluster = file->fat32_file.cluster;
    uint32_t cluster_number = file->cfo / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint64_t cluster_offset = file->cfo % (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);  // in bytes
    uint64_t sector_number = cluster_offset / VIRTIO_BLK_SECTOR_SIZE;
    uint64_t sector_offset = cluster_offset % VIRTIO_BLK_SECTOR_SIZE;   // in bytes
    // get to the desired cluster
    for (uint32_t i = 0; i < cluster_number; i++) {
        cluster = next_cluster(cluster);
    }
    // calculate the sector to write
    uint64_t sector_to_write = cluster_to_sector(cluster) + sector_number;
    uint64_t bytes_written = 0;
    while (bytes_written < len) {
        // calculate the bytes to write
        uint32_t bytes_to_write = VIRTIO_BLK_SECTOR_SIZE - sector_offset;
        if (bytes_to_write > len - bytes_written) {
            bytes_to_write = len - bytes_written;
        }

        /*
         * # CAUTION #
         * There is no need to judge whether bytes_to_write == 0
         * if so, the loop should have infinited at while (bytes_written < len)
         * which won't happen
         */
        // copy the data from buf to fat32_buf
        // load the sector to fat32_buf
        virtio_blk_read_sector(sector_to_write, fat32_buf);
        memcpy(fat32_buf + sector_offset, buf + bytes_written, bytes_to_write);
        // write the sector to disk
        virtio_blk_write_sector(sector_to_write, fat32_buf);

        // update the variables
        bytes_written += bytes_to_write;
        sector_to_write++;
        sector_offset = 0;  // since write is sequential, sector_offset should be 0 for the next sector

        // check if the sector to write is in the next cluster
        if (sector_to_write % fat32_volume.sec_per_cluster == 0) {
            // if so, get to the next cluster
            cluster = next_cluster(cluster);
            sector_to_write = cluster_to_sector(cluster);
        }
    }

    // add the bytes write to cfo
    file->cfo += bytes_written;

    return bytes_written;
}
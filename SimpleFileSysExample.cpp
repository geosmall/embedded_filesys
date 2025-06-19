#include "FileSys.h"
#include <stdio.h>
#include <string.h>

// Simple LittleFS configuration (you'll need to implement the actual driver functions)
static uint8_t lfs_read_buffer[256];
static uint8_t lfs_prog_buffer[256];
static uint8_t lfs_lookahead_buffer[16];

static lfs_config_t lfs_cfg = {
    .read  = your_flash_read,    // Replace with your W25QXX read function
    .prog  = your_flash_prog,    // Replace with your W25QXX program function
    .erase = your_flash_erase,   // Replace with your W25QXX erase function
    .sync  = your_flash_sync,    // Replace with your W25QXX sync function
    .read_size = 256,
    .prog_size = 256,
    .block_size = 4096,
    .block_count = 4096,
    .cache_size = 256,
    .lookahead_size = 16,
    .read_buffer = lfs_read_buffer,
    .prog_buffer = lfs_prog_buffer,
    .lookahead_buffer = lfs_lookahead_buffer,
};

void test_littlefs() {
    printf("=== LittleFS Test ===\n");
    
    // Create LittleFS file system
    EmbeddedFS::FileSys fs(&lfs_cfg);
    
    // Mount
    if (fs.mount() != EmbeddedFS::FSResult::OK) {
        printf("LittleFS mount failed\n");
        return;
    }
    
    // Open file for writing (create if doesn't exist)
    EmbeddedFS::FileHandle file;
    if (fs.open(file, "/test.txt", 
                EmbeddedFS::OpenMode::WRITE | EmbeddedFS::OpenMode::CREATE) 
        != EmbeddedFS::FSResult::OK) {
        printf("LittleFS open failed\n");
        return;
    }
    
    // Write data
    const char* data = "Hello LittleFS!";
    size_t bytes_written;
    if (fs.write(file, data, strlen(data), bytes_written) != EmbeddedFS::FSResult::OK) {
        printf("LittleFS write failed\n");
        fs.close(file);
        return;
    }
    
    // Close file
    fs.close(file);
    
    printf("LittleFS: Successfully wrote %zu bytes\n", bytes_written);
    fs.unmount();
}

void test_fatfs() {
    printf("=== FatFS Test ===\n");
    
    // Create FatFS file system (SD card on drive 0)
    EmbeddedFS::FileSys fs("0:");
    
    // Mount
    if (fs.mount() != EmbeddedFS::FSResult::OK) {
        printf("FatFS mount failed\n");
        return;
    }
    
    // Open file for writing (create if doesn't exist)
    EmbeddedFS::FileHandle file;
    if (fs.open(file, "/test.txt", 
                EmbeddedFS::OpenMode::WRITE | EmbeddedFS::OpenMode::CREATE) 
        != EmbeddedFS::FSResult::OK) {
        printf("FatFS open failed\n");
        return;
    }
    
    // Write data
    const char* data = "Hello FatFS!";
    size_t bytes_written;
    if (fs.write(file, data, strlen(data), bytes_written) != EmbeddedFS::FSResult::OK) {
        printf("FatFS write failed\n");
        fs.close(file);
        return;
    }
    
    // Close file
    fs.close(file);
    
    printf("FatFS: Successfully wrote %zu bytes\n", bytes_written);
    fs.unmount();
}

int main() {
    printf("Simple File System Test\n");
    printf("=======================\n");
    
    // Test LittleFS
    test_littlefs();
    
    // Test FatFS  
    test_fatfs();
    
    return 0;
}

// Note: You need to implement these functions for your W25QXX flash driver
extern "C" {
    int your_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
        // TODO: Implement your W25QXX SPI read
        return LFS_ERR_OK;
    }
    
    int your_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
        // TODO: Implement your W25QXX SPI program
        return LFS_ERR_OK;
    }
    
    int your_flash_erase(const struct lfs_config *c, lfs_block_t block) {
        // TODO: Implement your W25QXX SPI erase
        return LFS_ERR_OK;
    }
    
    int your_flash_sync(const struct lfs_config *c) {
        // TODO: Implement sync if needed
        return LFS_ERR_OK;
    }
}
#include "FileSys.h"
#include <cstring>
#include <cctype>

namespace EmbeddedFS {

// Constructor
FatFSImpl::FatFSImpl(const char* drive_path) : mounted_(false) {
    // Copy and validate drive path
    if (drive_path && strlen(drive_path) < sizeof(drive_path_)) {
        strcpy(drive_path_, drive_path);
    } else {
        strcpy(drive_path_, "0:");
    }
    
    // Initialize FATFS structure
    memset(&fatfs_, 0, sizeof(FATFS));
}

// Destructor
FatFSImpl::~FatFSImpl() {
    if (mounted_) {
        unmount();
    }
}

// Mount the file system
FSResult FatFSImpl::mount() {
    if (mounted_) {
        return FSResult::OK;
    }
    
    FRESULT res = f_mount(&fatfs_, drive_path_, 1); // Force mount
    if (res == FR_OK) {
        mounted_ = true;
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Unmount the file system
FSResult FatFSImpl::unmount() {
    if (!mounted_) {
        return FSResult::OK;
    }
    
    FRESULT res = f_mount(nullptr, drive_path_, 0);
    mounted_ = false;
    
    return convert_fatfs_error(res);
}

// Open a file
FSResult FatFSImpl::open(FileHandle& handle, const char* path, OpenMode mode) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    if (handle.is_open) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    BYTE fat_mode = convert_open_mode(mode);
    FRESULT res = f_open(&handle.fat_file, path, fat_mode);
    
    if (res == FR_OK) {
        handle.is_open = true;
        handle.fs_impl = this;
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Close a file
FSResult FatFSImpl::close(FileHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FRESULT res = f_close(&handle.fat_file);
    handle.is_open = false;
    handle.fs_impl = nullptr;
    
    return convert_fatfs_error(res);
}

// Read from a file
FSResult FatFSImpl::read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    UINT br;
    FRESULT res = f_read(&handle.fat_file, buffer, static_cast<UINT>(size), &br);
    bytes_read = static_cast<size_t>(br);
    
    return convert_fatfs_error(res);
}

// Write to a file
FSResult FatFSImpl::write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    UINT bw;
    FRESULT res = f_write(&handle.fat_file, buffer, static_cast<UINT>(size), &bw);
    bytes_written = static_cast<size_t>(bw);
    
    return convert_fatfs_error(res);
}

// Seek in a file
FSResult FatFSImpl::seek(FileHandle& handle, int32_t offset, SeekOrigin origin) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FSIZE_t new_pos;
    FSIZE_t current_pos = f_tell(&handle.fat_file);
    FSIZE_t file_size = f_size(&handle.fat_file);
    
    switch (origin) {
        case SeekOrigin::SET:
            new_pos = static_cast<FSIZE_t>(offset >= 0 ? offset : 0);
            break;
            
        case SeekOrigin::CUR:
            if (offset >= 0) {
                new_pos = current_pos + static_cast<FSIZE_t>(offset);
            } else {
                FSIZE_t abs_offset = static_cast<FSIZE_t>(-offset);
                new_pos = (current_pos >= abs_offset) ? (current_pos - abs_offset) : 0;
            }
            break;
            
        case SeekOrigin::END:
            if (offset >= 0) {
                new_pos = file_size + static_cast<FSIZE_t>(offset);
            } else {
                FSIZE_t abs_offset = static_cast<FSIZE_t>(-offset);
                new_pos = (file_size >= abs_offset) ? (file_size - abs_offset) : 0;
            }
            break;
            
        default:
            return FSResult::ERROR_INVALID;
    }
    
    FRESULT res = f_lseek(&handle.fat_file, new_pos);
    return convert_fatfs_error(res);
}

// Get current file position
FSResult FatFSImpl::tell(FileHandle& handle, uint32_t& position) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    position = static_cast<uint32_t>(f_tell(&handle.fat_file));
    return FSResult::OK;
}

// Sync file to storage
FSResult FatFSImpl::sync(FileHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FRESULT res = f_sync(&handle.fat_file);
    return convert_fatfs_error(res);
}

// Truncate file
FSResult FatFSImpl::truncate(FileHandle& handle, uint32_t size) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    // Save current position
    FSIZE_t current_pos = f_tell(&handle.fat_file);
    
    // Seek to truncation point
    FRESULT res = f_lseek(&handle.fat_file, static_cast<FSIZE_t>(size));
    if (res != FR_OK) {
        return convert_fatfs_error(res);
    }
    
    // Truncate
    res = f_truncate(&handle.fat_file);
    if (res != FR_OK) {
        return convert_fatfs_error(res);
    }
    
    // Restore position if it's still valid
    if (current_pos <= size) {
        f_lseek(&handle.fat_file, current_pos);
    }
    
    return FSResult::OK;
}

// Remove a file or directory
FSResult FatFSImpl::remove(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FRESULT res = f_unlink(path);
    return convert_fatfs_error(res);
}

// Rename a file or directory
FSResult FatFSImpl::rename(const char* old_path, const char* new_path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FRESULT res = f_rename(old_path, new_path);
    return convert_fatfs_error(res);
}

// Get file/directory information
FSResult FatFSImpl::stat(const char* path, FileInfo& info) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    
    if (res == FR_OK) {
        // Copy filename (extract from path if needed)
        const char* filename = strrchr(path, '/');
        if (filename) {
            filename++; // Skip the '/'
        } else {
            filename = path;
        }
        
        strncpy(info.name, filename, MAX_FILENAME_LENGTH - 1);
        info.name[MAX_FILENAME_LENGTH - 1] = '\0';
        
        info.size = static_cast<uint32_t>(fno.fsize);
        info.is_directory = (fno.fattrib & AM_DIR) != 0;
        
        // Convert FatFS date/time to Unix timestamp (simplified)
        // FatFS stores date/time in a packed format
        info.modified_time = static_cast<uint32_t>(fno.fdate) << 16 | fno.ftime;
        
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Create a directory
FSResult FatFSImpl::mkdir(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FRESULT res = f_mkdir(path);
    return convert_fatfs_error(res);
}

// Remove a directory
FSResult FatFSImpl::rmdir(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FRESULT res = f_unlink(path); // f_unlink works for directories too
    return convert_fatfs_error(res);
}

// Open a directory
FSResult FatFSImpl::opendir(DirHandle& handle, const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    if (handle.is_open) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FRESULT res = f_opendir(&handle.fat_dir, path);
    
    if (res == FR_OK) {
        handle.is_open = true;
        handle.fs_impl = this;
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Close a directory
FSResult FatFSImpl::closedir(DirHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FRESULT res = f_closedir(&handle.fat_dir);
    handle.is_open = false;
    handle.fs_impl = nullptr;
    
    return convert_fatfs_error(res);
}

// Read directory entry
FSResult FatFSImpl::readdir(DirHandle& handle, FileInfo& info) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FILINFO fno;
    FRESULT res = f_readdir(&handle.fat_dir, &fno);
    
    if (res == FR_OK) {
        if (fno.fname[0] == '\0') {
            // End of directory
            info.name[0] = '\0';
            return FSResult::OK;
        }
        
        strncpy(info.name, fno.fname, MAX_FILENAME_LENGTH - 1);
        info.name[MAX_FILENAME_LENGTH - 1] = '\0';
        
        info.size = static_cast<uint32_t>(fno.fsize);
        info.is_directory = (fno.fattrib & AM_DIR) != 0;
        info.modified_time = static_cast<uint32_t>(fno.fdate) << 16 | fno.ftime;
        
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Rewind directory to beginning
FSResult FatFSImpl::rewinddir(DirHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    FRESULT res = f_rewinddir(&handle.fat_dir);
    return convert_fatfs_error(res);
}

// Get free space
FSResult FatFSImpl::get_free_space(uint64_t& free_bytes) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FATFS* fs;
    DWORD free_clusters;
    
    FRESULT res = f_getfree(drive_path_, &free_clusters, &fs);
    if (res == FR_OK) {
        free_bytes = static_cast<uint64_t>(free_clusters) * fs->csize * 512; // Assume 512 bytes per sector
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Get total space
FSResult FatFSImpl::get_total_space(uint64_t& total_bytes) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    FATFS* fs;
    DWORD free_clusters;
    
    FRESULT res = f_getfree(drive_path_, &free_clusters, &fs);
    if (res == FR_OK) {
        DWORD total_clusters = fs->n_fatent - 2; // Total clusters minus reserved
        total_bytes = static_cast<uint64_t>(total_clusters) * fs->csize * 512; // Assume 512 bytes per sector
        return FSResult::OK;
    }
    
    return convert_fatfs_error(res);
}

// Convert FatFS error codes to FSResult
FSResult FatFSImpl::convert_fatfs_error(FRESULT fresult) {
    switch (fresult) {
        case FR_OK:                 return FSResult::OK;
        case FR_DISK_ERR:          return FSResult::ERROR_IO;
        case FR_INT_ERR:           return FSResult::ERROR_CORRUPT;
        case FR_NOT_READY:         return FSResult::ERROR_IO;
        case FR_NO_FILE:           return FSResult::ERROR_NO_ENT;
        case FR_NO_PATH:           return FSResult::ERROR_NO_ENT;
        case FR_INVALID_NAME:      return FSResult::ERROR_INVALID;
        case FR_DENIED:            return FSResult::ERROR_INVALID;
        case FR_EXIST:             return FSResult::ERROR_EXIST;
        case FR_INVALID_OBJECT:    return FSResult::ERROR_BAD_FILE;
        case FR_WRITE_PROTECTED:   return FSResult::ERROR_INVALID;
        case FR_INVALID_DRIVE:     return FSResult::ERROR_NOT_MOUNTED;
        case FR_NOT_ENABLED:       return FSResult::ERROR_NOT_MOUNTED;
        case FR_NO_FILESYSTEM:     return FSResult::ERROR_CORRUPT;
        case FR_MKFS_ABORTED:      return FSResult::ERROR_IO;
        case FR_TIMEOUT:           return FSResult::ERROR_IO;
        case FR_LOCKED:            return FSResult::ERROR_INVALID;
        case FR_NOT_ENOUGH_CORE:   return FSResult::ERROR_NO_MEM;
        case FR_TOO_MANY_OPEN_FILES: return FSResult::ERROR_NO_MEM;
        case FR_INVALID_PARAMETER: return FSResult::ERROR_INVALID;
        default:                   return FSResult::ERROR_IO;
    }
}

// Convert OpenMode to FatFS mode
BYTE FatFSImpl::convert_open_mode(OpenMode mode) {
    BYTE fat_mode = 0;
    
    uint8_t mode_bits = static_cast<uint8_t>(mode);
    
    // Handle read/write flags
    bool read_flag = (mode_bits & static_cast<uint8_t>(OpenMode::READ)) != 0;
    bool write_flag = (mode_bits & static_cast<uint8_t>(OpenMode::write)) != 0;
    bool create_flag = (mode_bits & static_cast<uint8_t>(OpenMode::CREATE)) != 0;
    bool trunc_flag = (mode_bits & static_cast<uint8_t>(OpenMode::TRUNC)) != 0;
    bool append_flag = (mode_bits & static_cast<uint8_t>(OpenMode::APPEND)) != 0;
    bool excl_flag = (mode_bits & static_cast<uint8_t>(OpenMode::EXCL)) != 0;
    
    if (read_flag && !write_flag) {
        fat_mode = FA_READ;
    } else if (write_flag && !read_flag) {
        fat_mode = FA_WRITE;
    } else if (read_flag && write_flag) {
        fat_mode = FA_READ | FA_WRITE;
    }
    
    if (create_flag) {
        if (excl_flag) {
            fat_mode |= FA_CREATE_NEW;
        } else {
            fat_mode |= FA_CREATE_ALWAYS;
        }
    } else if (write_flag) {
        fat_mode |= FA_OPEN_EXISTING;
    }
    
    if (append_flag) {
        fat_mode |= FA_OPEN_APPEND;
    }
    
    // Note: FatFS doesn't have direct TRUNC support in open mode
    // This would need to be handled after opening if needed
    
    return fat_mode;
}

} // namespace EmbeddedFS
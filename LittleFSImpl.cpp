#include "FileSys.h"
#include <cstring>
#include <cctype>

namespace EmbeddedFS {

// Constructor
LittleFSImpl::LittleFSImpl(lfs_config_t* config) : config_(config), mounted_(false) {
    if (!config_) {
        // Handle null config error - could set a flag or use default
        return;
    }
    
    // Initialize LittleFS structure
    memset(&lfs_, 0, sizeof(lfs_t));
}

// Destructor
LittleFSImpl::~LittleFSImpl() {
    if (mounted_) {
        unmount();
    }
}

// Mount the file system
FSResult LittleFSImpl::mount() {
    if (mounted_) {
        return FSResult::OK;
    }
    
    if (!config_) {
        return FSResult::ERROR_INVALID;
    }
    
    int res = lfs_mount(&lfs_, config_);
    if (res == LFS_ERR_OK) {
        mounted_ = true;
        return FSResult::OK;
    }
    
    // If mount fails, try to format and mount
    if (res == LFS_ERR_CORRUPT) {
        res = lfs_format(&lfs_, config_);
        if (res == LFS_ERR_OK) {
            res = lfs_mount(&lfs_, config_);
            if (res == LFS_ERR_OK) {
                mounted_ = true;
                return FSResult::OK;
            }
        }
    }
    
    return convert_lfs_error(res);
}

// Unmount the file system
FSResult LittleFSImpl::unmount() {
    if (!mounted_) {
        return FSResult::OK;
    }
    
    int res = lfs_unmount(&lfs_);
    mounted_ = false;
    
    return convert_lfs_error(res);
}

// Open a file
FSResult LittleFSImpl::open(FileHandle& handle, const char* path, OpenMode mode) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    if (handle.is_open) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int lfs_flags = convert_open_mode(mode);
    int res = lfs_file_open(&lfs_, &handle.lfs_file, path, lfs_flags);
    
    if (res == LFS_ERR_OK) {
        handle.is_open = true;
        handle.fs_impl = this;
        return FSResult::OK;
    }
    
    return convert_lfs_error(res);
}

// Close a file
FSResult LittleFSImpl::close(FileHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_file_close(&lfs_, &handle.lfs_file);
    handle.is_open = false;
    handle.fs_impl = nullptr;
    
    return convert_lfs_error(res);
}

// Read from a file
FSResult LittleFSImpl::read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    lfs_ssize_t res = lfs_file_read(&lfs_, &handle.lfs_file, buffer, size);
    
    if (res >= 0) {
        bytes_read = static_cast<size_t>(res);
        return FSResult::OK;
    }
    
    bytes_read = 0;
    return convert_lfs_error(static_cast<int>(res));
}

// Write to a file
FSResult LittleFSImpl::write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    lfs_ssize_t res = lfs_file_write(&lfs_, &handle.lfs_file, buffer, size);
    
    if (res >= 0) {
        bytes_written = static_cast<size_t>(res);
        return FSResult::OK;
    }
    
    bytes_written = 0;
    return convert_lfs_error(static_cast<int>(res));
}

// Seek in a file
FSResult LittleFSImpl::seek(FileHandle& handle, int32_t offset, SeekOrigin origin) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int whence;
    switch (origin) {
        case SeekOrigin::SET:
            whence = LFS_SEEK_SET;
            break;
        case SeekOrigin::CUR:
            whence = LFS_SEEK_CUR;
            break;
        case SeekOrigin::END:
            whence = LFS_SEEK_END;
            break;
        default:
            return FSResult::ERROR_INVALID;
    }
    
    lfs_soff_t res = lfs_file_seek(&lfs_, &handle.lfs_file, offset, whence);
    
    if (res >= 0) {
        return FSResult::OK;
    }
    
    return convert_lfs_error(static_cast<int>(res));
}

// Get current file position
FSResult LittleFSImpl::tell(FileHandle& handle, uint32_t& position) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    lfs_soff_t res = lfs_file_tell(&lfs_, &handle.lfs_file);
    
    if (res >= 0) {
        position = static_cast<uint32_t>(res);
        return FSResult::OK;
    }
    
    return convert_lfs_error(static_cast<int>(res));
}

// Sync file to storage
FSResult LittleFSImpl::sync(FileHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_file_sync(&lfs_, &handle.lfs_file);
    return convert_lfs_error(res);
}

// Truncate file
FSResult LittleFSImpl::truncate(FileHandle& handle, uint32_t size) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_file_truncate(&lfs_, &handle.lfs_file, static_cast<lfs_off_t>(size));
    return convert_lfs_error(res);
}

// Remove a file or directory
FSResult LittleFSImpl::remove(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    int res = lfs_remove(&lfs_, path);
    return convert_lfs_error(res);
}

// Rename a file or directory
FSResult LittleFSImpl::rename(const char* old_path, const char* new_path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    int res = lfs_rename(&lfs_, old_path, new_path);
    return convert_lfs_error(res);
}

// Get file/directory information
FSResult LittleFSImpl::stat(const char* path, FileInfo& info) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    struct lfs_info lfs_info;
    int res = lfs_stat(&lfs_, path, &lfs_info);
    
    if (res == LFS_ERR_OK) {
        // Copy filename (extract from path if needed)
        const char* filename = strrchr(path, '/');
        if (filename) {
            filename++; // Skip the '/'
        } else {
            filename = path;
        }
        
        strncpy(info.name, filename, MAX_FILENAME_LENGTH - 1);
        info.name[MAX_FILENAME_LENGTH - 1] = '\0';
        
        info.size = static_cast<uint32_t>(lfs_info.size);
        info.is_directory = (lfs_info.type == LFS_TYPE_DIR);
        
        // LittleFS doesn't store modification time by default
        // Set to 0 or implement custom attribute if needed
        info.modified_time = 0;
        
        return FSResult::OK;
    }
    
    return convert_lfs_error(res);
}

// Create a directory
FSResult LittleFSImpl::mkdir(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    int res = lfs_mkdir(&lfs_, path);
    return convert_lfs_error(res);
}

// Remove a directory
FSResult LittleFSImpl::rmdir(const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    // In LittleFS, lfs_remove works for both files and directories
    int res = lfs_remove(&lfs_, path);
    return convert_lfs_error(res);
}

// Open a directory
FSResult LittleFSImpl::opendir(DirHandle& handle, const char* path) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    if (handle.is_open) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_dir_open(&lfs_, &handle.lfs_dir, path);
    
    if (res == LFS_ERR_OK) {
        handle.is_open = true;
        handle.fs_impl = this;
        return FSResult::OK;
    }
    
    return convert_lfs_error(res);
}

// Close a directory
FSResult LittleFSImpl::closedir(DirHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_dir_close(&lfs_, &handle.lfs_dir);
    handle.is_open = false;
    handle.fs_impl = nullptr;
    
    return convert_lfs_error(res);
}

// Read directory entry
FSResult LittleFSImpl::readdir(DirHandle& handle, FileInfo& info) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    struct lfs_info lfs_info;
    int res = lfs_dir_read(&lfs_, &handle.lfs_dir, &lfs_info);
    
    if (res > 0) {
        // Valid entry found
        strncpy(info.name, lfs_info.name, MAX_FILENAME_LENGTH - 1);
        info.name[MAX_FILENAME_LENGTH - 1] = '\0';
        
        info.size = static_cast<uint32_t>(lfs_info.size);
        info.is_directory = (lfs_info.type == LFS_TYPE_DIR);
        info.modified_time = 0; // LittleFS doesn't store modification time by default
        
        return FSResult::OK;
    } else if (res == 0) {
        // End of directory
        info.name[0] = '\0';
        return FSResult::OK;
    } else {
        // Error occurred
        return convert_lfs_error(res);
    }
}

// Rewind directory to beginning
FSResult LittleFSImpl::rewinddir(DirHandle& handle) {
    if (!handle.is_open || handle.fs_impl != this) {
        return FSResult::ERROR_BAD_FILE;
    }
    
    int res = lfs_dir_rewind(&lfs_, &handle.lfs_dir);
    return convert_lfs_error(res);
}

// Get free space
FSResult LittleFSImpl::get_free_space(uint64_t& free_bytes) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    lfs_ssize_t res = lfs_fs_size(&lfs_);
    if (res >= 0) {
        lfs_size_t used_blocks = static_cast<lfs_size_t>(res);
        lfs_size_t total_blocks = config_->block_count;
        lfs_size_t free_blocks = (total_blocks > used_blocks) ? (total_blocks - used_blocks) : 0;
        
        free_bytes = static_cast<uint64_t>(free_blocks) * config_->block_size;
        return FSResult::OK;
    }
    
    return convert_lfs_error(static_cast<int>(res));
}

// Get total space
FSResult LittleFSImpl::get_total_space(uint64_t& total_bytes) {
    if (!mounted_) {
        return FSResult::ERROR_NOT_MOUNTED;
    }
    
    total_bytes = static_cast<uint64_t>(config_->block_count) * config_->block_size;
    return FSResult::OK;
}

// Convert LittleFS error codes to FSResult
FSResult LittleFSImpl::convert_lfs_error(int lfs_error) {
    switch (lfs_error) {
        case LFS_ERR_OK:        return FSResult::OK;
        case LFS_ERR_IO:        return FSResult::ERROR_IO;
        case LFS_ERR_CORRUPT:   return FSResult::ERROR_CORRUPT;
        case LFS_ERR_NOENT:     return FSResult::ERROR_NO_ENT;
        case LFS_ERR_EXIST:     return FSResult::ERROR_EXIST;
        case LFS_ERR_NOTDIR:    return FSResult::ERROR_NOT_DIR;
        case LFS_ERR_ISDIR:     return FSResult::ERROR_IS_DIR;
        case LFS_ERR_NOTEMPTY:  return FSResult::ERROR_NO_EMPTY;
        case LFS_ERR_BADF:      return FSResult::ERROR_BAD_FILE;
        case LFS_ERR_FBIG:      return FSResult::ERROR_FB_BIG;
        case LFS_ERR_NOSPC:     return FSResult::ERROR_NO_SPC;
        case LFS_ERR_NOMEM:     return FSResult::ERROR_NO_MEM;
        case LFS_ERR_INVAL:     return FSResult::ERROR_INVALID;
        default:                return FSResult::ERROR_IO;
    }
}

// Convert OpenMode to LittleFS flags
uint8_t LittleFSImpl::convert_open_mode(OpenMode mode) {
    int lfs_flags = 0;
    
    uint8_t mode_bits = static_cast<uint8_t>(mode);
    
    // Handle read/write flags
    bool read_flag = (mode_bits & static_cast<uint8_t>(OpenMode::READ)) != 0;
    bool write_flag = (mode_bits & static_cast<uint8_t>(OpenMode::WRITE)) != 0;
    bool create_flag = (mode_bits & static_cast<uint8_t>(OpenMode::CREATE)) != 0;
    bool trunc_flag = (mode_bits & static_cast<uint8_t>(OpenMode::TRUNC)) != 0;
    bool append_flag = (mode_bits & static_cast<uint8_t>(OpenMode::APPEND)) != 0;
    bool excl_flag = (mode_bits & static_cast<uint8_t>(OpenMode::EXCL)) != 0;
    
    // Set basic read/write flags
    if (read_flag) {
        lfs_flags |= LFS_O_RDONLY;
    }
    if (write_flag) {
        if (read_flag) {
            lfs_flags = (lfs_flags & ~LFS_O_RDONLY) | LFS_O_RDWR;
        } else {
            lfs_flags |= LFS_O_WRONLY;
        }
    }
    
    // Handle creation flags
    if (create_flag) {
        lfs_flags |= LFS_O_CREAT;
        if (excl_flag) {
            lfs_flags |= LFS_O_EXCL;
        }
    }
    
    // Handle truncation
    if (trunc_flag) {
        lfs_flags |= LFS_O_TRUNC;
    }
    
    // Handle append mode
    if (append_flag) {
        lfs_flags |= LFS_O_APPEND;
    }
    
    return static_cast<uint8_t>(lfs_flags);
}

} // namespace EmbeddedFS
#ifndef FILESYS_H
#define FILESYS_H

#include <stdint.h>
#include <stddef.h>
#include <cstring>

// Include underlying file system headers
#include "lfs.h"    // LittleFS header
#include "ff.h"     // FatFS header

namespace EmbeddedFS {

// Configuration constants
static constexpr size_t MAX_PATH_LENGTH = 256;
static constexpr size_t MAX_FILENAME_LENGTH = 64;
static constexpr size_t MAX_OPEN_FILES = 8;
static constexpr size_t MAX_OPEN_DIRS = 4;
static constexpr size_t BUFFER_SIZE = 512;

// Error codes
enum class FSResult : int8_t {
    OK = 0,
    ERROR_IO = -1,
    ERROR_CORRUPT = -2,
    ERROR_NO_ENT = -3,
    ERROR_EXIST = -4,
    ERROR_NOT_DIR = -5,
    ERROR_IS_DIR = -6,
    ERROR_NO_EMPTY = -7,
    ERROR_BAD_FILE = -8,
    ERROR_FB_BIG = -9,
    ERROR_NO_SPC = -10,
    ERROR_NO_MEM = -11,
    ERROR_INVALID = -12,
    ERROR_NOT_MOUNTED = -13,
    ERROR_NOT_SUPPORTED = -14
};

// File open modes
enum class OpenMode : uint8_t {
    READ = 0x01,
    WRITE = 0x02,
    CREATE = 0x04,
    EXCL = 0x08,
    TRUNC = 0x10,
    APPEND = 0x20
};

// File seek origins
enum class SeekOrigin : uint8_t {
    SET = 0,
    CUR = 1,
    END = 2
};

// File information structure
struct FileInfo {
    char name[MAX_FILENAME_LENGTH];
    uint32_t size;
    bool is_directory;
    uint32_t modified_time;
    
    FileInfo() : size(0), is_directory(false), modified_time(0) {
        name[0] = '\0';
    }
};

// Forward declarations
class IFileSystemImpl;

// File handle structure
struct FileHandle {
    bool is_open;
    IFileSystemImpl* fs_impl;
    union {
        lfs_file_t lfs_file;
        FIL fat_file;
    };
    
    FileHandle() : is_open(false), fs_impl(nullptr) {}
};

// Directory handle structure
struct DirHandle {
    bool is_open;
    IFileSystemImpl* fs_impl;
    union {
        lfs_dir_t lfs_dir;
        DIR fat_dir;
    };
    
    DirHandle() : is_open(false), fs_impl(nullptr) {}
};

// Abstract file system interface
class IFileSystemImpl {
public:
    virtual ~IFileSystemImpl() = default;
    
    // Core file system operations
    virtual FSResult mount() = 0;
    virtual FSResult unmount() = 0;
    virtual bool is_mounted() const = 0;
    
    // File operations
    virtual FSResult open(FileHandle& handle, const char* path, OpenMode mode) = 0;
    virtual FSResult close(FileHandle& handle) = 0;
    virtual FSResult read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) = 0;
    virtual FSResult write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) = 0;
    virtual FSResult seek(FileHandle& handle, int32_t offset, SeekOrigin origin) = 0;
    virtual FSResult tell(FileHandle& handle, uint32_t& position) = 0;
    virtual FSResult sync(FileHandle& handle) = 0;
    virtual FSResult truncate(FileHandle& handle, uint32_t size) = 0;
    
    // File system operations
    virtual FSResult remove(const char* path) = 0;
    virtual FSResult rename(const char* old_path, const char* new_path) = 0;
    virtual FSResult stat(const char* path, FileInfo& info) = 0;
    virtual FSResult mkdir(const char* path) = 0;
    virtual FSResult rmdir(const char* path) = 0;
    
    // Directory operations
    virtual FSResult opendir(DirHandle& handle, const char* path) = 0;
    virtual FSResult closedir(DirHandle& handle) = 0;
    virtual FSResult readdir(DirHandle& handle, FileInfo& info) = 0;
    virtual FSResult rewinddir(DirHandle& handle) = 0;
    
    // File system information
    virtual FSResult get_free_space(uint64_t& free_bytes) = 0;
    virtual FSResult get_total_space(uint64_t& total_bytes) = 0;
};

// LittleFS implementation
class LittleFSImpl : public IFileSystemImpl {
public:
    explicit LittleFSImpl(lfs_config_t* config);
    ~LittleFSImpl() override;
    
    // IFileSystemImpl interface
    FSResult mount() override;
    FSResult unmount() override;
    bool is_mounted() const override { return mounted_; }
    
    FSResult open(FileHandle& handle, const char* path, OpenMode mode) override;
    FSResult close(FileHandle& handle) override;
    FSResult read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) override;
    FSResult write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) override;
    FSResult seek(FileHandle& handle, int32_t offset, SeekOrigin origin) override;
    FSResult tell(FileHandle& handle, uint32_t& position) override;
    FSResult sync(FileHandle& handle) override;
    FSResult truncate(FileHandle& handle, uint32_t size) override;
    
    FSResult remove(const char* path) override;
    FSResult rename(const char* old_path, const char* new_path) override;
    FSResult stat(const char* path, FileInfo& info) override;
    FSResult mkdir(const char* path) override;
    FSResult rmdir(const char* path) override;
    
    FSResult opendir(DirHandle& handle, const char* path) override;
    FSResult closedir(DirHandle& handle) override;
    FSResult readdir(DirHandle& handle, FileInfo& info) override;
    FSResult rewinddir(DirHandle& handle) override;
    
    FSResult get_free_space(uint64_t& free_bytes) override;
    FSResult get_total_space(uint64_t& total_bytes) override;

private:
    lfs_t lfs_;
    lfs_config_t* config_;
    bool mounted_;
    
    FSResult convert_lfs_error(int lfs_error);
    uint8_t convert_open_mode(OpenMode mode);
};

// FatFS implementation
class FatFSImpl : public IFileSystemImpl {
public:
    explicit FatFSImpl(const char* drive_path = "0:");
    ~FatFSImpl() override;
    
    // IFileSystemImpl interface
    FSResult mount() override;
    FSResult unmount() override;
    bool is_mounted() const override { return mounted_; }
    
    FSResult open(FileHandle& handle, const char* path, OpenMode mode) override;
    FSResult close(FileHandle& handle) override;
    FSResult read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) override;
    FSResult write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) override;
    FSResult seek(FileHandle& handle, int32_t offset, SeekOrigin origin) override;
    FSResult tell(FileHandle& handle, uint32_t& position) override;
    FSResult sync(FileHandle& handle) override;
    FSResult truncate(FileHandle& handle, uint32_t size) override;
    
    FSResult remove(const char* path) override;
    FSResult rename(const char* old_path, const char* new_path) override;
    FSResult stat(const char* path, FileInfo& info) override;
    FSResult mkdir(const char* path) override;
    FSResult rmdir(const char* path) override;
    
    FSResult opendir(DirHandle& handle, const char* path) override;
    FSResult closedir(DirHandle& handle) override;
    FSResult readdir(DirHandle& handle, FileInfo& info) override;
    FSResult rewinddir(DirHandle& handle) override;
    
    FSResult get_free_space(uint64_t& free_bytes) override;
    FSResult get_total_space(uint64_t& total_bytes) override;

private:
    FATFS fatfs_;
    char drive_path_[8];
    bool mounted_;
    
    FSResult convert_fatfs_error(FRESULT fresult);
    BYTE convert_open_mode(OpenMode mode);
};

// Main file system class - uses composition with polymorphism
class FileSys {
public:
    // Constructors for different file system types
    explicit FileSys(lfs_config_t* lfs_config);
    explicit FileSys(const char* fatfs_drive_path = "0:");
    
    // Destructor
    ~FileSys();
    
    // Mount/unmount operations
    FSResult mount() { return impl_->mount(); }
    FSResult unmount() { return impl_->unmount(); }
    bool is_mounted() const { return impl_->is_mounted(); }
    
    // File operations
    FSResult open(FileHandle& handle, const char* path, OpenMode mode) {
        return impl_->open(handle, path, mode);
    }
    FSResult close(FileHandle& handle) { return impl_->close(handle); }
    FSResult read(FileHandle& handle, void* buffer, size_t size, size_t& bytes_read) {
        return impl_->read(handle, buffer, size, bytes_read);
    }
    FSResult write(FileHandle& handle, const void* buffer, size_t size, size_t& bytes_written) {
        return impl_->write(handle, buffer, size, bytes_written);
    }
    FSResult seek(FileHandle& handle, int32_t offset, SeekOrigin origin) {
        return impl_->seek(handle, offset, origin);
    }
    FSResult tell(FileHandle& handle, uint32_t& position) {
        return impl_->tell(handle, position);
    }
    FSResult sync(FileHandle& handle) { return impl_->sync(handle); }
    FSResult truncate(FileHandle& handle, uint32_t size) {
        return impl_->truncate(handle, size);
    }
    
    // File system operations
    FSResult remove(const char* path) { return impl_->remove(path); }
    FSResult rename(const char* old_path, const char* new_path) {
        return impl_->rename(old_path, new_path);
    }
    FSResult stat(const char* path, FileInfo& info) { return impl_->stat(path, info); }
    FSResult mkdir(const char* path) { return impl_->mkdir(path); }
    FSResult rmdir(const char* path) { return impl_->rmdir(path); }
    
    // Directory operations
    FSResult opendir(DirHandle& handle, const char* path) {
        return impl_->opendir(handle, path);
    }
    FSResult closedir(DirHandle& handle) { return impl_->closedir(handle); }
    FSResult readdir(DirHandle& handle, FileInfo& info) {
        return impl_->readdir(handle, info);
    }
    FSResult rewinddir(DirHandle& handle) { return impl_->rewinddir(handle); }
    
    // File system information
    FSResult get_free_space(uint64_t& free_bytes) { return impl_->get_free_space(free_bytes); }
    FSResult get_total_space(uint64_t& total_bytes) { return impl_->get_total_space(total_bytes); }
    
    // Utility functions
    static bool is_valid_filename(const char* filename);
    static void sanitize_path(char* path);

private:
    IFileSystemImpl* impl_;
    
    // Static storage for implementations to avoid dynamic allocation
    union {
        LittleFSImpl littlefs_impl_;
        FatFSImpl fatfs_impl_;
    };
    
    enum class ImplType : uint8_t {
        LITTLEFS,
        FATFS
    } impl_type_;
    
    // Disable copy construction and assignment
    FileSys(const FileSys&) = delete;
    FileSys& operator=(const FileSys&) = delete;
};

// Inline operator overloads for OpenMode
inline OpenMode operator|(OpenMode a, OpenMode b) {
    return static_cast<OpenMode>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline OpenMode operator&(OpenMode a, OpenMode b) {
    return static_cast<OpenMode>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool operator!(OpenMode mode) {
    return static_cast<uint8_t>(mode) == 0;
}

} // namespace EmbeddedFS

#endif // FILESYS_H
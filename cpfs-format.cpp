#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <windows.h>

#define BLOCK_SIZE 4096
#define JOURNAL_SIZE 64
#define CPFS_MAGIC 0x53465043
#define SECTOR_SIZE 512

// Journal entry structure
struct JournalEntry {
    unsigned int sequence_num;
    unsigned int block_num;
    unsigned int data[BLOCK_SIZE / 4];
};

// Superblock structure
struct Superblock {
    unsigned int cpfs_magic;
    unsigned int block_count;
    unsigned int journal_start;
    unsigned int journal_length;
};

// Directory entry structure
struct DirectoryEntry {
    char name[128];
    unsigned int block_num;
    unsigned int size;
    bool isDirectory;
};

// Global journal buffer
std::vector<JournalEntry> journal_buffer(JOURNAL_SIZE);

// Disk I/O helper functions
HANDLE open_disk(const std::string& disk_path) {
    HANDLE disk = CreateFileA(disk_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (disk == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Cannot open disk. Code: " << GetLastError() << std::endl;
    }
    return disk;
}

bool write_to_disk(HANDLE disk, unsigned int block_num, const void *buffer, DWORD buffer_size) {
    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(block_num) * BLOCK_SIZE;
    if (!SetFilePointerEx(disk, offset, NULL, FILE_BEGIN)) {
        std::cerr << "Error: Failed to set file pointer. Code: " << GetLastError() << std::endl;
        return false;
    }
    
    DWORD written;
    if (!WriteFile(disk, buffer, buffer_size, &written, NULL) || written != buffer_size) {
        std::cerr << "Error: Failed to write to disk. Code: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

// CPFS Quick Format function
int quick_format_cpfs(const std::string& disk_path, unsigned int total_blocks) {
    HANDLE disk = open_disk(disk_path);
    if (disk == INVALID_HANDLE_VALUE) return -1;
    
    // Initialize superblock
    Superblock superblock;
    superblock.cpfs_magic = CPFS_MAGIC;
    superblock.block_count = total_blocks;
    superblock.journal_start = 1;  // Block 1 (immediately after superblock)
    superblock.journal_length = JOURNAL_SIZE;

    // Zero buffer
    std::vector<unsigned int> buffer(BLOCK_SIZE / sizeof(unsigned int), 0);
    memcpy(buffer.data(), &superblock, sizeof(Superblock));

    // Write superblock to disk
    if (!write_to_disk(disk, 0, buffer.data(), BLOCK_SIZE)) {
        CloseHandle(disk);
        return -1;
    }
    std::cout << "Superblock written successfully." << std::endl;

    // Initialize journal entries (zeroed, only the start is initialized for a quick format)
    journal_buffer.assign(JOURNAL_SIZE, JournalEntry{});
    for (unsigned int i = 0; i < JOURNAL_SIZE; ++i) {
        if (!write_to_disk(disk, superblock.journal_start + i, journal_buffer.data(), BLOCK_SIZE)) {
            CloseHandle(disk);
            return -1;
        }
    }
    std::cout << "Journal initialized successfully." << std::endl;

    // Initialize root directory entry
    DirectoryEntry root_dir{};
    strncpy(root_dir.name, "/", sizeof(root_dir.name) - 1);
    root_dir.block_num = superblock.journal_start + superblock.journal_length;
    root_dir.isDirectory = true;

    buffer.assign(BLOCK_SIZE / sizeof(unsigned int), 0);
    memcpy(buffer.data(), &root_dir, sizeof(DirectoryEntry));

    if (!write_to_disk(disk, root_dir.block_num, buffer.data(), BLOCK_SIZE)) {
        CloseHandle(disk);
        return -1;
    }
    std::cout << "Root directory initialized successfully." << std::endl;

    CloseHandle(disk);
    std::cout << "Quick format of CPFS completed successfully." << std::endl;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <disk_path> <total_blocks>" << std::endl;
        return -1;
    }
    
    std::string disk_path = argv[1];
    unsigned int total_blocks = std::stoi(argv[2]);
    return quick_format_cpfs(disk_path, total_blocks);
}

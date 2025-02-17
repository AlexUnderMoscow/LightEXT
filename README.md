# LightEXT - In-Memory File System

## Overview

**LightEXT** is a lightweight, in-memory file system inspired by EXT2, designed for high-speed file operations. Unlike traditional file systems, **LightEXT** operates entirely in RAM, making it ideal for temporary storage, caching, and rapid file access.

### Key Features:
- **In-memory operation:** Data is stored in RAM for ultra-fast access.
- **Block size:** 4 KB.
- **Direct pointers:** 844 (optimized for quick access).
- **Maximum 32768 files (1 inode bitmap)**
- **Single indirect pointer:** ✅ (Supported).
- **Double indirect pointer:** ✅ (Supported).
- **Triple indirect pointer:** ❌ (Not included).
- **Directory support:** ❌ (Not supported, flat file storage).
- **Fast file creation, reading, writing, and deletion.**

## Inode Structure

Each file is represented by an **inode**, which contains:

- **844 direct pointers** (each pointing to a 4 KB block).
- **1 single indirect pointer** (points to a block containing 1024 additional pointers).
- **1 double indirect pointer** (points to a block containing 1024 pointers, each pointing to another block of 1024 pointers).
- **Other metadata** (file size, timestamps (in process...), etc.).

### Maximum File Size Calculation

Given:
- **Direct blocks:** `844 * 4 KB = 3376 KB`
- **Single indirect blocks:** `1024 * 4 KB = 4096 KB`
- **Double indirect blocks:** `1024 * 1024 * 4 KB = 4 GB`

**Total maximum file size:** ≈ **4 GB + 7.4 MB**

## File System Structure

Since directories are not supported, **LightEXT** uses a **flat namespace**, meaning all files exist at the same level. A simple file table keeps track of file metadata and inodes.

## How It Works

1. **Initialization:** The file system is loaded into RAM and initializes its structures (inode table, block bitmap, and file table).
2. **File operations:** Users can create, read, write, and delete files in memory.
3. **Persistence (optional):** If needed, the file system state can be saved to a disk image.
4. **Shutdown:** The file system is cleared when the program exits (unless saved manually).

## Implementation Details

- **Block allocation:** Bitmap-based allocation for efficient space management.
- **Inode storage:** Stored in a fixed-size inode table.
- **File operations:** Read, write, create, delete, and list files.
- **Persistence (optional):** Can be saved to a disk image for later restoration.

## Building and Running

### Prerequisites

- GCC or Clang
- Make
- Linux or macOS

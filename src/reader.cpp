// reader.cpp
#include <iostream>

#ifndef __linux__
    #include <windows.h>
#else
    #include <fcntl.h>    // O_* константы
    #include <sys/mman.h> // shm_open, mmap
    #include <unistd.h>   // close
#endif

#include <lext.h>

const char* SHM_NAME = "/LExtFS";

int main() {
    size_t memory_size;                  // Общий размер памяти
    memory_size = MAX_BLOCKS * BLOCK_SIZE;

#ifndef __linux__
    // Открываем существующую разделяемую память
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // Права доступа для чтения/записи
        FALSE,                 // Не наследовать дескриптор
        sharedMemoryName       // Имя разделяемой памяти
        );

    if (hMapFile == NULL) {
        std::cerr << "Не удалось открыть разделяемую память. Ошибка: "
                  << GetLastError() << std::endl;
        return 1;
    }

#else
    // 1. Открываем уже существующую shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Ошибка shm_open\n";
        return 1;
    }
#endif

#ifndef __linux__
    // Проецируем разделяемую память в адресное пространство процесса
    LPVOID pBuf = MapViewOfFile(
        hMapFile,              // Дескриптор разделяемой памяти
        FILE_MAP_ALL_ACCESS,   // Права доступа для чтения/записи
        0,                     // Смещение (начало файла)
        0,                     // Смещение (конец файла)
        sharedMemorySize       // Размер для проекции
        );

    if (pBuf == NULL) {
        std::cerr << "Не удалось спроецировать разделяемую память. Ошибка: "
                  << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }
#else
    // 2. Отображаем память в адресное пространство
    void* ptr = mmap(0, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Ошибка mmap\n";
        return 1;
    }
#endif

    // 3. Читаем данные
    std::cout << "Reader прочитал"  << std::endl;
    Ext2FileSystem fs;
#ifndef __linux__
    fs.set_mem_ptr((void*)pBuf);
#else
    fs.set_mem_ptr(ptr);
#endif
    fs.print_fs_state();

    std::vector<DirEntry> dir;
    fs.dir(dir);

    for (int i=0; i<dir.size(); i++){
        std::string s(dir.at(i).filename);
        std::cout << s << " " << dir.at(i).file_size << " " << fs.time2str(dir.at(i).creation_time) <<std::endl;
    }
    std::cout << "Reader...waiting key..." << std::endl;

    std::cin.get();

    uint32_t fd = fs.open("file2.txt");
    fs.rm(fd);

    fs.dir(dir);
    for (int i=0; i<dir.size(); i++){
        std::string s(dir.at(i).filename);
        std::cout << s << " " << dir.at(i).file_size << " " << fs.time2str(dir.at(i).creation_time) <<std::endl;
    }

    std::cout << "Reader...after deletion..." << std::endl;
    std::cin.get();
    // 4. Очищаем ресурсы
#ifndef __linux__
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
#else
    munmap(ptr, memory_size);
    close(shm_fd);
#endif
}

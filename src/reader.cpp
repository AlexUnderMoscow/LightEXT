// reader.cpp
#include <iostream>
#include <fcntl.h>    // O_* константы
#include <sys/mman.h> // shm_open, mmap
#include <unistd.h>   // close
#include <lext.h>

const char* SHM_NAME = "/LExtFS";

int main() {
    size_t memory_size;                  // Общий размер памяти
    memory_size = MAX_BLOCKS * BLOCK_SIZE;
    // 1. Открываем уже существующую shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Ошибка shm_open\n";
        return 1;
    }

    // 2. Отображаем память в адресное пространство
    void* ptr = mmap(0, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Ошибка mmap\n";
        return 1;
    }

    // 3. Читаем данные
    std::cout << "Reader прочитал"  << std::endl;
    Ext2FileSystem fs;
    fs.set_mem_ptr(ptr);
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
    //munmap(ptr, memory_size);
    //close(shm_fd);
}

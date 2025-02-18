
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <bitset>
#include <cstdlib>  // Для malloc/free
#include <new>      // Для placement new

// Константы
const int BLOCK_SIZE = 4096;                            // Размер блока в байтах 1024
const int MAX_BLOCKS = 256*1024;                        // Максимальное количество блоков 265K = 1GB
const int MAX_INODES = 1024;                            // Максимальное количество файлов
const int INODE_SIZE = BLOCK_SIZE;                      // Размер inode в байтах
const int RECORDS_CNT = BLOCK_SIZE >> 2;                // количество записей в блоке указателей
const int DIRECT_POINTERS = 844;                        // количество прямых указателей на блоки

// Структура суперблока
struct SuperBlock {
    uint32_t total_blocks;     // Общее количество блоков
    uint32_t total_inodes;     // Общее количество inode
    uint32_t free_blocks;      // Количество свободных блоков
    uint32_t free_inodes;      // Количество свободных inode
    uint32_t BLOCK_SIZE;       // Размер блока
    uint32_t MAX_BLOCKS;        // Максимальное количество блоков
    uint32_t MAX_INODES;        // Максимальное количество inode
    uint32_t DIRECT_POINTERS;   // количество прямых указателей на блоки
    uint32_t reserve[RECORDS_CNT-8];
};

// Структура inode
struct Inode {
    uint64_t file_size;                // Размер файла
    uint32_t fname_size;               //рамер имени
    time_t creation_time;
    size_t name_hash;
    uint32_t reserve[41];
    char filename[512];
    uint32_t direct_block_pointer[DIRECT_POINTERS];        // Прямые указатели на блоки данных
    uint32_t indirect1_pointer;
    uint32_t indirect2_pointer;
    uint32_t indirect3_pointer;
};

struct Inode_pointers {
    uint32_t block_pointer[RECORDS_CNT];        // Прямые указатели на блоки данных
};

struct DirEntry {
    char filename[512];
    uint64_t file_size;
    time_t creation_time;
};

// Класс файловой системы EXT2
class Ext2FileSystem {
private:
    SuperBlock* superblock;               // Суперблок
    //std::vector<bool> block_bitmap;      // Битовая карта блоков
    std::bitset<MAX_BLOCKS>* block_bitmap;
    //std::vector<bool> inode_bitmap;      // Битовая карта inode
    std::bitset<MAX_INODES>* inode_bitmap;
    Inode* inodes;           // Таблица inode
    char* memory;                        // Память, выделенная в куче
    size_t memory_size;                  // Общий размер памяти
    size_t inode_index;                  // при переборе next() inode_index движется вперед

public:
    // Конструктор
    Ext2FileSystem(int total_blocks = MAX_BLOCKS, int total_inodes = MAX_INODES);
    // Деструктор
    ~Ext2FileSystem() ;
private:
    // Создание файла
    uint32_t create(const std::string& name);
    // Выделение блока
    uint32_t allocate_block();
    // Освобождение блока
    void free_block(uint32_t block_index);
    // Выделение inode
    uint32_t allocate_inode();
    // Освобождение inode
    void free_inode(uint32_t inode_index);
    // текущее время
    //time_t timeNow();


public:
    uint32_t open(const std::string& name);
    void close(uint32_t fd);
    uint32_t next();
    size_t write(uint32_t fd, const char* data, size_t size);
    // Чтение файла inode_index -> fd
    size_t read(uint32_t fd, char* data, size_t size);
    void dir(std::vector<DirEntry>& dir);
    size_t dir_size();
    uint64_t size(uint32_t fd);
    // Удаление файла inode_index -> fd
    void rm(uint32_t fd);
    // Вывод состояния файловой системы
    void print_fs_state();
    // вывод времени в удобном формате
    std::string time2str(time_t time);
    // Расчет хэша имени файла
    size_t fnv1a_hash(const char* buffer, size_t size);

};

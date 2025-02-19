#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include "lext.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

Ext2FileSystem fs;

// start MACRO TEST
int failed = 0;
#define TEST(name) void name()
#define RUN_TEST(name) printf("\n\033[1m%s\n\033[0m" , #name); name()
#define ASSERT(expr) if (!(expr)) {                   \
failed = 1;                                       \
    printf("\033[0;31mFAIL: %s\n\033[0m", #expr);     \
} else {                                               \
        printf("\033[0;32mPASS: %s\n\033[0m", #expr);       \
}
#define ASSERT_STR_EQ (str1, str2) ASSERT(strcmp(str1,str2) == 0)

#define GREEN_TEXT "\033[0;32m"
#define RED_TEXT "\033[0;31m"
#define RESET_TEXT "\033[0m"
// stop MACRO TEST

//size_t fnv1a_hash(const char* buffer, size_t size);

TEST(test_struct_size) {
    //Ext2FileSystem fs;
    ASSERT(sizeof(SuperBlock) == 4096);
    int s = sizeof(Inode);
    ASSERT(sizeof(Inode) == 4096);
    ASSERT(sizeof(Inode_pointers) == 4096);
}

TEST(test_xtra_small_files) {
    //Ext2FileSystem fs;
    // Состояние файловой системы
    fs.print_fs_state();
    // Запись
    uint32_t fd1 = fs.open("file1.txt");
    std::string s1 = "Hello, EXT2 in memory!";
    int written1 = fs.write(fd1, s1.c_str(), s1.size());

    uint32_t fd2 = fs.open("file2.txt");
    std::string s2 = "This is another file.";
    int written2 = fs.write(fd2, s2.c_str(), s2.size());

    uint32_t fd3 = fs.open("file3.txt");
    std::string s3 = "This is a third file small size of text...s";
    int written3 = fs.write(fd3, s3.c_str(), s3.size());
    ASSERT(written1 == s1.size());
    ASSERT(written2 == s2.size());
    ASSERT(written3 == s3.size());

    // Чтение файлов
    char buffer[64];
    uint64_t bytes1 = fs.read(fd1,buffer,fs.size(fd1));
    uint64_t bytes2 = fs.read(fd2,buffer,fs.size(fd2));
    uint64_t bytes3 = fs.read(fd3,buffer,fs.size(fd3));
    ASSERT(bytes1 == s1.size());
    ASSERT(bytes2 == s2.size());
    ASSERT(bytes3 == s3.size());

    // Состояние файловой системы
    fs.print_fs_state();
}

TEST(test_small_file) {
    //Ext2FileSystem fs;
    // Имя файла для чтения
    const std::string filename = "../../testfiles/4K.txt";
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "not opened: " << filename << std::endl;
        return;
    }

    // Перемещаем указатель в конец файла, чтобы узнать его размер
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg(); // Размер файла в байтах
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    // Проверяем, если файл пуст
    if (file_size == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Создаем буфер для хранения содержимого файла
    char* buf = new char[file_size+1];
    memset(buf,0x0, file_size+1);
    // Читаем содержимое файла в буфер
    if (!file.read(buf, file_size)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file.close();

    uint32_t fd1 = fs.open("small.txt");
    uint32_t written = fs.write(fd1,buf, file_size);
    ASSERT(written == file_size);
    std::hash<std::string> hash;
    //посчитаем хэш данных
    std::string s1(buf);
    size_t data_hash = hash(s1); //хэш входных данных
    memset(buf,0x0, file_size+1); //чистка буфера

    uint32_t bytes_read = fs.read(fd1, buf, file_size); // читка в буфер
    std::string s2(buf);
    size_t out_hash = hash(s2); //хэш выходных данных
    ASSERT(out_hash == data_hash);
    delete [] buf;
}

TEST(test_medium_file) {
    //Ext2FileSystem fs;
    // Имя файла для чтения
    const std::string filename = "../../testfiles/87K.jpg";
    std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open()) {
        std::cerr << "not opened: " << filename << std::endl;
        return;
    }

    // Перемещаем указатель в конец файла, чтобы узнать его размерs
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg(); // Размер файла в байтах
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    // Проверяем, если файл пуст
    if (file_size == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Создаем буфер для хранения содержимого файла
    char* buf = new char[file_size+1];
    memset(buf,0x0, file_size+1);
    // Читаем содержимое файла в буфер
    if (!file.read(buf, file_size)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file.close();
    uint32_t fd = fs.open("medium.txt");
    int part = file_size / 3;
    uint64_t written = fs.write(fd,buf,part);
    written = fs.write(fd,buf+part,part);
    int big_part = file_size - part*2;
    written = fs.write(fd,(char*)(buf+part*2),big_part);
    size_t data_hash = fs.fnv1a_hash(buf,file_size);

    char* buf2 = new char[file_size+1];
    memset(buf2,0x0,file_size+1);

    int read_size = 0;
    part = file_size >> 2;
    read_size = fs.read(fd,buf2,part);
    read_size = fs.read(fd,(char*)(buf2 + part),part);
    read_size = fs.read(fd,(char*)(buf2 + part*2),part);
    big_part = file_size - part*3;
    read_size = fs.read(fd,(char*)(buf2 + part*3),big_part);

    size_t out_hash = fs.fnv1a_hash(buf2,file_size);
    ASSERT(out_hash == data_hash);
    const std::string outfile = "../../testfiles/out.jpg";
    std::ofstream file1(outfile, std::ios::ate | std::ios::binary);
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла
    file1.write(buf2,file_size);
    file1.close();
    delete [] buf;
}

TEST(test_large_file) {
    //Ext2FileSystem fs;
    // Имя файла для чтения
    const std::string filename = "../../testfiles/279K.txt"; //xtra_large.txt
    std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open()) {
        std::cerr << "not opened: " << filename << std::endl;
        return;
    }

    // Перемещаем указатель в конец файла, чтобы узнать его размерs
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg(); // Размер файла в байтах
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    // Проверяем, если файл пуст
    if (file_size == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Создаем буфер для хранения содержимого файла
    char* buf = new char[file_size+1];
    memset(buf,0x0, file_size+1);
    // Читаем содержимое файла в буфер
    if (!file.read(buf, file_size)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file.close();
    uint32_t fd = fs.open("large.txt");
    int part = file_size / 3;
    uint64_t written = fs.write(fd,buf,part);
    written = fs.write(fd,buf+part,part);
    int big_part = file_size - part*2;
    written = fs.write(fd,(char*)(buf+part*2),big_part);
    //std::hash<std::string> hash;
    //посчитаем хэш данных
    //std::string s1(buf);
    //size_t data_hash = hash(s1); //хэш входных данных
    size_t data_hash = fs.fnv1a_hash(buf,file_size);

    char* buf2 = new char[file_size+1];
    memset(buf2,0x0,file_size+1);

    int read_size = 0;
    part = file_size >> 2;
    read_size = fs.read(fd,buf2,part);
    read_size = fs.read(fd,(char*)(buf2 + part),part);
    read_size = fs.read(fd,(char*)(buf2 + part*2),part);
    big_part = file_size - part*3;
    read_size = fs.read(fd,(char*)(buf2 + part*3),big_part);


    //std::string s2(buf2);
    //size_t out_hash = hash(s2); //хэш выходных данных
    size_t out_hash = fs.fnv1a_hash(buf2,file_size);
    ASSERT(out_hash == data_hash);
    const std::string outfile = "../../testfiles/out.zip";
    std::ofstream file1(outfile, std::ios::ate | std::ios::binary);
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла
    file1.write(buf2,file_size);
    file1.close();

    //    for (int i=0; i<file_size;i++){
    //        if (buf[i] != buf2[i]){
    //            std::cout << "DANGER! " << i << std::endl;
    //        }
    //    }

    delete [] buf;
    delete [] buf2;
}



TEST(test_all_size) {
    //Ext2FileSystem fs;
    std::cout << "Before test" << std::endl;
    fs.print_fs_state();
    std::chrono::duration<double> write_time, read_time , rm_time;
    double total_write_time, total_read_time , total_rm_time;
    total_write_time = 0;
    total_read_time = 0;
    total_rm_time = 0;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    uint64_t transmit_bytes = 0;
    uint64_t rm_counts = 0;

    // Имя файла для чтения
    const std::string filename = "../../testfiles/1920000.wav"; //xtra_large.txt
    std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open()) {
        std::cerr << "not opened: " << filename << std::endl;
        return;
    }

    // Перемещаем указатель в конец файла, чтобы узнать его размерs
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg(); // Размер файла в байтах
    file.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    // Проверяем, если файл пуст
    if (file_size == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Создаем буфер для хранения содержимого файла
    char* buf = new char[file_size+1];
    memset(buf,0x0, file_size+1);
    char* buf2 = new char[file_size+1];
    memset(buf2,0x0, file_size+1);
    // Читаем содержимое файла в буфер
    if (!file.read(buf, file_size)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file.close();
    size_t data_hash, out_hash;

    //разные размеры файла, но примерно по XXX МБ
    int XXX = 8700000;
    for (int sz = XXX; sz < XXX+1; sz+=1){

    //for (int sz = file_size; sz < file_size+1; sz+=1){
        data_hash = fs.fnv1a_hash(buf,sz);
        uint32_t fd = fs.open("file.txt");
        int write_size = 0;
        int part = 15000;

        start = std::chrono::system_clock::now();

        while (write_size < sz){
            int bytes = std::min(part, sz-write_size);
            write_size+=fs.write(fd,buf+write_size,bytes);
        }

        end = std::chrono::system_clock::now();
        write_time = end - start;
        total_write_time += write_time.count();

        memset(buf2,0x0,sz+1);

        int read_size = 0;
        part = 16000;

        start = std::chrono::system_clock::now();
        while (read_size < sz){
            int bytes = std::min(part, sz-read_size);
            transmit_bytes+=bytes;
            read_size+=fs.read(fd,buf2+read_size,bytes);
        }

        end = std::chrono::system_clock::now();
        read_time = end - start;
        total_read_time += read_time.count();

        out_hash = fs.fnv1a_hash(buf2,sz);
        //if (out_hash != data_hash){
            ASSERT(out_hash == data_hash);
        //}

        start = std::chrono::system_clock::now();
        std::cout << "Before delete" << std::endl;
        fs.print_fs_state();
        fs.rm(fd);
        end = std::chrono::system_clock::now();
        rm_time = end - start;
        total_rm_time += rm_time.count();
        rm_counts+=1;
    }
    std::cout << "End" << std::endl;
    fs.print_fs_state();
    delete [] buf;
    delete [] buf2;
    //std::printf("total bytes transmit = %d\n", transmit_bytes);
    std::cout << "total bytes transmit = " << ((float)transmit_bytes / (1024 * 1024)) << " Mbytes"  << std::endl;
    std::printf("write time = %f s\n", total_write_time);
    std::printf("read time = %f s\n", total_read_time);
    std::printf("rm counts = %d times\n", (int)rm_counts);
    std::printf("rm time = %f s\n", total_rm_time);
    std::cout << "write speed = " << ((float)(transmit_bytes / (1024 * 1024)) / total_write_time)  << " Mbps" << std::endl;
    std::cout << "read speed = " << ((float)(transmit_bytes / (1024 * 1024)) / total_read_time)  << " Mbps"<< std::endl;
    std::cout << "rm speed = " << ((float)(transmit_bytes / (1024 * 1024)) / total_rm_time)  << " Mbps" << std::endl;
    //std::time_t write_time = std::chrono::system_clock::to_time_t(total_write_time);
}


TEST(test_2x_files_all_size) {
    //Ext2FileSystem fs;
    std::cout << "Before test" << std::endl;
    fs.print_fs_state();
    std::chrono::duration<double> write_time, read_time , rm_time;
    double total_write_time, total_read_time , total_rm_time;
    total_write_time = 0;
    total_read_time = 0;
    total_rm_time = 0;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    uint64_t transmit_bytes = 0;
    uint64_t rm_counts = 0;

    // начало работы с файлами ------------------------------------------------
    // Имя файла для чтения
    const std::string filename1 = "../../testfiles/87K.jpg"; //51M.pdf
    const std::string filename2 = "../../testfiles/87K.jpg"; //51M.pdf

    std::ifstream file1(filename1, std::ifstream::ate | std::ifstream::binary);
    if (!file1.is_open()) {
        std::cerr << "not opened: " << filename1 << std::endl;
        return;
    }

    std::ifstream file2(filename2, std::ifstream::ate | std::ifstream::binary);
    if (!file1.is_open()) {
        std::cerr << "not opened: " << filename1 << std::endl;
        return;
    }

    // Перемещаем указатель в конец файла, чтобы узнать его размерs
    file1.seekg(0, std::ios::end);
    size_t file_size1 = file1.tellg(); // Размер файла в байтах
    file1.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    file2.seekg(0, std::ios::end);
    size_t file_size2 = file2.tellg(); // Размер файла в байтах
    file2.seekg(0, std::ios::beg);            // Возвращаемся в начало файла

    // Проверяем, если файл пуст
    if (file_size1 == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Проверяем, если файл пуст
    if (file_size2 == 0) {
        std::cerr << "file empty" << std::endl;
        return;
    }

    // Создаем буфер для хранения содержимого файла
    char* buf = new char[file_size1+1];
    memset(buf,0x0, file_size1+1);
    char* buf2 = new char[file_size1+1];
    memset(buf2,0x0, file_size1+1);
    // Читаем содержимое файла в буфер
    if (!file1.read(buf, file_size1)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file1.close();


    // Создаем буфер для хранения содержимого файла
    char* buf_2 = new char[file_size2+1];
    memset(buf_2,0x0, file_size2+1);
    char* buf2_2 = new char[file_size2+1];
    memset(buf2_2,0x0, file_size2+1);
    // Читаем содержимое файла в буфер
    if (!file2.read(buf, file_size2)) {
        std::cerr << "file read error" << std::endl;
        return;
    }
    // Закрываем файл
    file2.close();



    // с файлами окончена работа ------------------------------------------------
    size_t data_hash1, out_hash1 , data_hash2, out_hash2 ;

    //разные размеры файла, но примерно по XXX КБ (МБ)
    int XXX = 70;

    uint32_t fd1 = fs.open("file1.txt");
    uint32_t fd2 = fs.open("file2.txt");
    XXX = XXX * 1024;// *1024;

    for (int sz = XXX; sz < XXX+1; sz+=1){
        data_hash1 = fs.fnv1a_hash(buf,sz);
        data_hash2 = fs.fnv1a_hash(buf_2,sz);

        int write_size = 0;
        int part = 15000;

        while (write_size < sz){
            int bytes = std::min(part, sz-write_size);
            write_size+=fs.write(fd1,buf+write_size,bytes);

            fs.write(fd2,buf_2+write_size,bytes);
        }

        memset(buf2,0x0,sz+1);
        memset(buf2_2,0x0,sz+1);
        int read_size = 0;
        part = 16000;
        while (read_size < sz){
            int bytes = std::min(part, sz-read_size);
            transmit_bytes+=bytes;
            read_size+=fs.read(fd1,buf2+read_size,bytes);

            fs.read(fd2,buf2_2+read_size,bytes);
        }

        out_hash1 = fs.fnv1a_hash(buf2,sz);
        out_hash2 = fs.fnv1a_hash(buf2_2,sz);

        ASSERT(out_hash1 == data_hash1);
        ASSERT(out_hash2 == data_hash2);
        std::cout << "Before delete" << std::endl;
        std::vector<DirEntry> dir;
        fs.dir(dir);



        for (int i=0; i<dir.size(); i++){
            std::string s(dir.at(i).filename);
            std::cout << s << " " << dir.at(i).file_size << " " << fs.time2str(dir.at(i).creation_time) <<std::endl;
        }

        fs.share_mem();

        std::cout << "Memory shared...waiting key..." << std::endl;

        std::cin.get();

        fs.print_fs_state();
        fs.rm(fd1);
        fs.rm(fd2);
    }




    fs.print_fs_state();
    //std::cin.get();
    delete [] buf;
    delete [] buf2;
    delete [] buf_2;
    delete [] buf2_2;
}


void bitset_test(){
        const size_t N = MAX_INODES;  // Количество бит в bitset
    //
        // 1. Выделяем память
        size_t sz = sizeof(std::bitset<N>);
        void* buffer = std::malloc(sz);

        if (!buffer) {
            std::cerr << "Ошибка выделения памяти!" << std::endl;
            return;
        }
        // 2. Размещаем объект std::bitset<N> в выделенной памяти (placement new)
        std::bitset<N>* bitset_ptr = new (buffer) std::bitset<N>;
        // 3. Работаем с объектом
        bitset_ptr->set(0);   //
        bitset_ptr->set(1);   //
        bitset_ptr->set(2);   //
        bitset_ptr->set(3);   // Устанавливаем 3-й бит
        bitset_ptr->set(7);   // Устанавливаем 7-й бит
        bitset_ptr->set(64);
        bitset_ptr->set(65);
        bitset_ptr->reset(65);
        bool b = bitset_ptr->test(65);
        std::cout << "Bitset: " << *bitset_ptr << std::endl;
        // 4. Вызываем явный деструктор (так как память выделена вручную)
        bool a = bitset_ptr->test(64);
        bitset_ptr->reset(3);
        // 5. Освобождаем память
        std::free(buffer);
        return;
}


int main() {
    //bitset_test();
    //return 0;

    printf(GREEN_TEXT "%s\n" RESET_TEXT, "Tests:");
    //RUN_TEST(test_struct_size);

    //RUN_TEST(test_xtra_small_files);

   // RUN_TEST(test_small_file);

    //RUN_TEST(test_medium_file);

    //RUN_TEST(test_large_file);
    //std::cout << std::flush;
    //RUN_TEST(test_all_size);
    RUN_TEST(test_2x_files_all_size);



    return 0;
}

/*
test_all_size

сверхмалые файлы (10-11 КБ)
write speed = 22376.4 Mbps
read speed = 59347.2 Mbps
rm speed = 167504 Mbps

малые файлы (100 КБ)
write speed = 10858.5 Mbps
read speed = 32323 Mbps
rm speed = 319464 Mbps

малые файлы (200 КБ)
write speed = 6522.89 Mbps
read speed = 24270 Mbps
rm speed = 340706 Mbps

средние файлы (1 МБ)
write speed = 1201.29 Mbps
read speed = 19438.5 Mbps
rm speed = 435181 Mbps

средние файлы (25 МБ)
write speed = 51.0296 Mbps
read speed = 7308.98 Mbps
rm speed = 329594 Mbps

большие файлы (51 МБ)
write speed = 36.8461 Mbps
read speed = 7711.96 Mbps
rm speed = 253165 Mbps

------------------------------------- после увеличения блоков до 4кб
обычные файлы (5 МБ)
write speed = 3303.02 Mbps
read speed = 13026 Mbps
rm speed = 397852 Mbps

средние файлы (50 МБ)
write speed = 753.08 Mbps
read speed = 7382.81 Mbps
rm speed = 1.77105e+06 Mbps

большие файлы (500 МБ)
write speed = 85.5508 Mbps
read speed = 7530.88 Mbps
rm speed = 2.46947e+06 Mbps
*/

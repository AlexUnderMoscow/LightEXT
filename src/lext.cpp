#include "lext.h"

    // Конструктор
    Ext2FileSystem::Ext2FileSystem(int total_blocks, int total_inodes) {
        memory_size = total_blocks * BLOCK_SIZE;
        memory = new char[memory_size];  // Выделение памяти в куче
        std::memset(memory, 0, memory_size);

        // Инициализация суперблока
        superblock.total_blocks = total_blocks;
        superblock.total_inodes = total_inodes;
        superblock.free_blocks = total_blocks;
        superblock.free_inodes = total_inodes;

        superblock.MAX_INODES = MAX_INODES;
        superblock.BLOCK_SIZE = BLOCK_SIZE;
        superblock.MAX_BLOCKS = MAX_BLOCKS;
        superblock.DIRECT_POINTERS = DIRECT_POINTERS;

        // Инициализация битовых карт
        block_bitmap.resize(total_blocks, false);
        inode_bitmap.resize(total_inodes, false);

        // Инициализация таблицы inode
        inodes.resize(total_inodes);
        inode_index = 0;
    }

    // Деструктор
    Ext2FileSystem::~Ext2FileSystem() {
        delete [] memory;  // Освобождение памяти из кучи
    }

    // Функция для вычисления хеша FNV-1a
    size_t Ext2FileSystem::fnv1a_hash(const char* buffer, size_t size) {
        const uint64_t FNV_offset_basis = 0xcbf29ce484222325;
        const uint64_t FNV_prime = 0x100000001b3;

        uint64_t hash = FNV_offset_basis;

        for (size_t i = 0; i < size; ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= FNV_prime;
        }

        return static_cast<size_t>(hash);
    }


    // Создание файла
    uint32_t Ext2FileSystem::create(const std::string& name) {
        uint32_t inode_index = allocate_inode();
        //std::hash<std::string> hash;
        //size_t hash_filename = hash(name);
        size_t hash_filename = fnv1a_hash(name.c_str(), name.size());
        Inode& inode = inodes[inode_index];
        uint32_t name_size = (uint32_t)name.size();
        inode.file_size = 0;
        std::memcpy((void*)inode.filename,name.data(),name.size());

        inode.filename[name_size] = 0;             // конец строки
        inode.name_hash = hash_filename;
        inode.fname_size = name_size;
        inode.creation_time = 0;                    //время пока не контролируется
        inode.indirect1_pointer = -1;
        inode.indirect2_pointer = -1;
        inode.indirect3_pointer = -1;
        for (int i =0; i < DIRECT_POINTERS; i++) inode.direct_block_pointer[i] = -1;
        uint64_t * seek = (uint64_t*)inode.reserve;
        *seek = 0; // Seek(0) внутрений указатель в файле
        return inode_index;
    }

    // Выделение блока
    uint32_t Ext2FileSystem::allocate_block() {
        for (int i = 0; i < superblock.total_blocks; ++i) {
            if (!block_bitmap[i]) {
                block_bitmap[i] = true;
                superblock.free_blocks--;
                memset((void*)(memory+i*BLOCK_SIZE),0xFF,BLOCK_SIZE);
                return i;
            }
        }
        throw std::runtime_error("No free blocks");
    }

    // Освобождение блока
    void Ext2FileSystem::free_block(uint32_t block_index) {
        if (block_index >= 0 && block_index < superblock.total_blocks && block_bitmap[block_index]) {
            block_bitmap[block_index] = false;
            superblock.free_blocks++;
        }
    }

    // Выделение inode
    uint32_t Ext2FileSystem::allocate_inode() {
        for (int i = 0; i < superblock.total_inodes; ++i) {
            if (!inode_bitmap[i]) {
                inode_bitmap[i] = true;
                superblock.free_inodes--;
                return i;
            }
        }
        throw std::runtime_error("No free inodes");
    }

    // Освобождение inode
    void Ext2FileSystem::free_inode(uint32_t inode_index) {
        if (inode_index >= 0 && inode_index < superblock.total_inodes && inode_bitmap[inode_index]) {
            inode_bitmap[inode_index] = false;
            superblock.free_inodes++;
        }
    }

    uint32_t Ext2FileSystem::open(const std::string& name) {
        std::hash<std::string> hash;
        size_t hash_in = hash(name);
        for (uint32_t i = 0; i<inode_bitmap.size(); i++){
            if (inode_bitmap[i]){
                if (inodes[i].name_hash == hash_in){
                    //uint64_t * seek_write = (uint64_t*)inodes[i].reserve;
                    //*seek_write = 0; // Seek(0) внутрений указатель в файле APPEND only files
                    uint64_t * seek_read = (uint64_t*)&(inodes[i].reserve[2]);
                    *seek_read = 0;
                    return i;
                }
            }
        }
        return create(name);
    }

    void Ext2FileSystem::close(uint32_t fd){
        // Seek на чтение обнуляется
        //Seek на запись остается, файл только можно дозаписать
        *((uint64_t*)&(inodes[fd].reserve[2])) = 0;
    }

    uint32_t Ext2FileSystem::next(){
        if (superblock.free_inodes == superblock.total_inodes ) return -1; //нет inodes
        for (uint32_t i = inode_index; i<inode_bitmap.size(); i++){ // перебор с inod-ов с позиции inode_index
             if (inode_bitmap[i]) { // inode занят
                inode_index = i+1; // в следующий запрос начинаем со следующего элемента
                return i;
            }
        }
        for (uint32_t i = 0; i<inode_index; i++){
            if (inode_bitmap[i]) { // inode занят
                inode_index = i+1; // в следующий запрос начинаем со следующего элемента
                return i;
            }
        }
        return 0;
    }

    size_t Ext2FileSystem::write(uint32_t fd, const char* data, size_t size) {
        // Запись данных в блоки
        uint64_t bytes_written = 0;
        uint32_t block_index = -1;
        uint64_t offset = 0;
        int bytes_to_write = 0;
        uint32_t start_block_index = 0;
        uint32_t in_block_offset = 0;
        uint64_t * seek = (uint64_t*)inodes[fd].reserve;
        /*
            123 % 10 = 3 ( 3 - цифра единиц)
            123 // 10 = 12
        */
        start_block_index = uint32_t(*seek / BLOCK_SIZE) ;
        in_block_offset = *seek % BLOCK_SIZE;
        // пока файл не кончится и прямых ссылок на блоки меньше, чем есть на самом деле
        for (int i = start_block_index; i < DIRECT_POINTERS && bytes_written < size; ++i) {
            //if (inodes[fd].direct_block_pointer[i] != -1) continue;  //это пока не сработало
            if (in_block_offset != 0){
                offset = i * BLOCK_SIZE;
                bytes_to_write = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                std::memcpy(memory + offset + in_block_offset, data, bytes_to_write);
                bytes_written += bytes_to_write;
                *seek += bytes_to_write;
                in_block_offset = 0;
                continue;
            }
            block_index = allocate_block();
            inodes[fd].direct_block_pointer[i] = block_index;

            // Копирование данных в блок
            offset = block_index * BLOCK_SIZE;
            bytes_to_write = std::min(BLOCK_SIZE, (int)(size - bytes_written));
            std::memcpy(memory + offset, data + bytes_written, bytes_to_write);
            bytes_written += bytes_to_write;
            *seek += bytes_to_write;                                  // Seek(bytes_written)
        }
        // выход из функции, если обошлись прямыми указателями
        if (bytes_written == size){
            inodes[fd].file_size = *seek;
            return bytes_written;
        }
        Inode_pointers* pointers = nullptr;
        // теперь непрямые одинарные непрямые указатели
        // аллоцровать блок для прямых указателей или не надо решает состояние указателя
        if (inodes[fd].indirect1_pointer == -1){   // непрямой одинарный указатель неинициализирован
            block_index = allocate_block();/////////////////////////////////////////////////////////////////
            inodes[fd].indirect1_pointer = block_index;
            offset = block_index * BLOCK_SIZE;
            // указатель на блок теперь уже с прямыми указателями
            pointers = (Inode_pointers*)(memory + offset);
            start_block_index = uint32_t(*seek / BLOCK_SIZE) ;
            in_block_offset = *seek % BLOCK_SIZE;
        }else{
            offset = inodes[fd].indirect1_pointer * BLOCK_SIZE;
            pointers = (Inode_pointers*) (memory + offset);
            start_block_index = uint32_t(*seek / BLOCK_SIZE) ;
            in_block_offset = *seek % BLOCK_SIZE;
        }

        for (int i = start_block_index - DIRECT_POINTERS; i < RECORDS_CNT && bytes_written < size; ++i){
            //if (pointers->block_pointer[i] != -1) continue; //это пока не сработало
            if (in_block_offset != 0){
                offset = pointers->block_pointer[i] * BLOCK_SIZE;
                bytes_to_write = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                std::memcpy(memory + offset + in_block_offset, data, bytes_to_write);
                bytes_written += bytes_to_write;
                *seek += bytes_to_write;
                in_block_offset = 0;
                continue;
            }
            block_index = allocate_block();
            pointers->block_pointer[i] = block_index;
            // Копирование данных в блок
            offset = block_index * BLOCK_SIZE;
            bytes_to_write = std::min(BLOCK_SIZE, (int)(size - bytes_written));
            std::memcpy(memory + offset, data + bytes_written, bytes_to_write);
            bytes_written += bytes_to_write;
            *seek += bytes_to_write;                                  // Seek(bytes_written)
        }

        // выход из функции, если обошлись одинарными непрямыми указателями
        if (bytes_written == size){
            inodes[fd].file_size = *seek;
            return bytes_written;
        }


        //////////////////////////////////////////////////// теперь двойные непрямые указатели
        int j_index = 0;
        int i_index = 0;
        Inode_pointers* j_pointers = nullptr;
        Inode_pointers* i_pointers = nullptr;
        // если пусто, то аллокация двойных указателей
        uint32_t i_start_block_index = (uint32_t)((*seek) / BLOCK_SIZE) ;
        i_start_block_index -= (DIRECT_POINTERS + RECORDS_CNT);
        uint32_t j_start_block_index = i_start_block_index / RECORDS_CNT;


        if (inodes[fd].indirect2_pointer == -1){ //
            block_index = allocate_block();
            inodes[fd].indirect2_pointer = block_index;

        }

        offset = inodes[fd].indirect2_pointer * BLOCK_SIZE; // memory +
        j_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers

        in_block_offset = *seek % BLOCK_SIZE;

        for (int j = j_start_block_index; j < RECORDS_CNT && bytes_written < size; ++j){

            if (j==0){
                int brk = 0;
            }

            uint32_t i_start_block_index = (uint32_t)((*seek) / BLOCK_SIZE) ;
            i_start_block_index -= (DIRECT_POINTERS + RECORDS_CNT);
            i_start_block_index = i_start_block_index % RECORDS_CNT;

            ///
            if (j_pointers->block_pointer[j] == -1){   // непрямой одинарный указатель неинициализирован
                block_index = allocate_block();/////////////////////////////////////////////////////////////////
                j_pointers->block_pointer[j] = block_index;
            }
            offset = j_pointers->block_pointer[j] * BLOCK_SIZE; // memory +
            i_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers

            for (int i = i_start_block_index; i < RECORDS_CNT && bytes_written < size; ++i){

                if (in_block_offset != 0){ // дописать конец
                    offset = i_pointers->block_pointer[i] * BLOCK_SIZE;
                    bytes_to_write = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                    std::memcpy(memory + offset + in_block_offset, data, bytes_to_write);
                    bytes_written += bytes_to_write;
                    *seek += bytes_to_write;
                    in_block_offset = 0;
                    continue;
                }

                block_index = allocate_block();
                i_pointers->block_pointer[i] = block_index;
                // Копирование данных в блок
                offset = block_index * BLOCK_SIZE;
                bytes_to_write = std::min(BLOCK_SIZE, (int)(size - bytes_written));
                std::memcpy(memory + offset, data + bytes_written, bytes_to_write);
                bytes_written += bytes_to_write;
                *seek += bytes_to_write;                                  // Seek(bytes_written)

            }
        }

        if (bytes_written == size){
            inodes[fd].file_size = *seek;
            return bytes_written;
        }
        return bytes_written;
     }

    // Чтение файла inode_index -> fd
     size_t Ext2FileSystem::read(uint32_t fd, char* data, size_t size) {
        if (fd >= superblock.total_inodes || !inode_bitmap[fd]) {
            //std::cout << "File with inode " << fd << " not found\n";
            return 0;
        }
        int block_index = 0;
        uint64_t * seek = (uint64_t*)&(inodes[fd].reserve[2]); //внутренний указатель чтения в резервной части inode
        uint64_t bytes_to_read = 0;
        uint64_t bytes_total_read = 0;
        uint64_t offset = 0;
        uint32_t start_block_index = 0;
        uint32_t in_block_offset = 0;
        Inode& inode = inodes[fd];
        uint64_t in_data_offset = 0;

        size_t data_size = size;

        start_block_index = uint32_t(*seek / BLOCK_SIZE);
        in_block_offset = *seek % BLOCK_SIZE;

        for (int i = start_block_index; i < DIRECT_POINTERS && inode.direct_block_pointer[i] != -1 && bytes_total_read < size; ++i) {


            if (in_block_offset != 0){
                offset = i * BLOCK_SIZE;
                int bytes = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                std::memcpy(data, memory + offset + in_block_offset , bytes);
                bytes_total_read += bytes;
                *seek += bytes;
                in_block_offset = 0;
                continue;
            }


            block_index = inode.direct_block_pointer[i];
            offset = block_index * BLOCK_SIZE;
            bytes_to_read = std::min((int)(size - bytes_total_read), BLOCK_SIZE);
            memcpy(data + bytes_total_read,memory + offset,bytes_to_read);

            bytes_total_read += bytes_to_read;
            *seek += bytes_to_read; // подвижка внутреннего указателя
        }

        if (bytes_total_read==size) return bytes_total_read;
        // теперь использование одинарных непрямых указателей
        if (inodes[fd].indirect1_pointer != -1){
            // по указателю лежит блок, состоящий из 256 прямых указателей на блоки
            Inode_pointers* pointers = (Inode_pointers*)(memory+inodes[fd].indirect1_pointer*BLOCK_SIZE);
            // указатель на блок теперь уже с прямыми указателями
            start_block_index = uint32_t(*seek / BLOCK_SIZE) ;
            in_block_offset = *seek % BLOCK_SIZE;
            for (int i = (start_block_index - DIRECT_POINTERS); i < RECORDS_CNT && pointers->block_pointer[i] !=-1 && bytes_total_read < size; ++i){

                if (in_block_offset != 0){
                    offset = (pointers->block_pointer[i]) * BLOCK_SIZE;
                    bytes_to_read = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                    std::memcpy(data, memory + offset + in_block_offset , bytes_to_read);
                    bytes_total_read += bytes_to_read;
                    *seek += bytes_to_read;
                    in_block_offset = 0;
                    continue;
                }
                if (bytes_total_read == size){
                    return bytes_total_read;
                }
                offset = (pointers->block_pointer[i]) * BLOCK_SIZE;
                int bytes = std::min((int)(size - bytes_total_read), BLOCK_SIZE);
                memcpy(data + bytes_total_read,memory + offset,bytes);
                bytes_total_read += bytes;
                *seek += bytes; // подвижка внутреннего указателя
            }
        }else{
            return bytes_total_read;
        }

        if (bytes_total_read==size) return bytes_total_read;

        //if (data_size==0) return size;

        if (inodes[fd].indirect2_pointer != -1){
            Inode_pointers* j_pointers = nullptr;
            Inode_pointers* i_pointers = nullptr;
            uint32_t i_start_block_index = (uint32_t)((*seek) / BLOCK_SIZE) ;
            i_start_block_index -= (DIRECT_POINTERS + RECORDS_CNT);
            uint32_t j_start_block_index = i_start_block_index / RECORDS_CNT;
            i_start_block_index = i_start_block_index % RECORDS_CNT;
            in_block_offset = *seek % BLOCK_SIZE;

            offset = inodes[fd].indirect2_pointer * BLOCK_SIZE; // memory +
            j_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers


            for (int j = j_start_block_index; j < RECORDS_CNT && j_pointers->block_pointer[j] !=-1 && bytes_total_read < size; ++j){

                offset = j_pointers->block_pointer[j] * BLOCK_SIZE; // memory +
                i_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers

                uint32_t i_start_block_index = (uint32_t)((*seek) / BLOCK_SIZE) ;
                i_start_block_index -= (DIRECT_POINTERS + RECORDS_CNT);
                i_start_block_index = i_start_block_index % RECORDS_CNT;

                for (int i = i_start_block_index; i < RECORDS_CNT && bytes_total_read < size; ++i){

                    if (in_block_offset != 0){
                        offset = (i_pointers->block_pointer[i]) * BLOCK_SIZE;
                        bytes_to_read = std::min((int)(BLOCK_SIZE - in_block_offset), (int)size);
                        std::memcpy(data, memory + offset + in_block_offset , bytes_to_read);
                        bytes_total_read += bytes_to_read;
                        *seek += bytes_to_read;
                        in_block_offset = 0;
                        continue;
                    }
                    if (bytes_total_read == size){
                        return bytes_total_read;
                    }
                    offset = (i_pointers->block_pointer[i]) * BLOCK_SIZE;
                    int bytes = std::min((int)(size - bytes_total_read), BLOCK_SIZE);
                    memcpy(data + bytes_total_read,memory + offset,bytes);
                    bytes_total_read += bytes;
                    *seek += bytes; // подвижка внутреннего указателя
                }
            }

        }else{
            return bytes_total_read;
        }

        if (bytes_total_read == size){
            inodes[fd].file_size = *seek;
            return bytes_total_read;
        }

//        if (inodes[fd].indirect3_pointer != -1){

//        }else{
//            return bytes_total_read;
//        }
        return bytes_total_read;
    }

     void Ext2FileSystem::dir(std::vector<std::string>& dir){
        dir.clear();
        if (superblock.free_inodes == superblock.total_inodes ) return ; //нет inodes

        for (int i=0; i < this->inode_bitmap.size(); i++){
            if (inode_bitmap.at(i)) {
                std::string s(inodes[i].filename);
                dir.push_back(s);
            }
        }
    }

     size_t Ext2FileSystem::dir_size(){
        return superblock.total_inodes-superblock.free_inodes;
    }

     uint64_t Ext2FileSystem::size(uint32_t fd){
        return inodes[fd].file_size;
    }

    // Удаление файла inode_index -> fd
    void Ext2FileSystem::rm(uint32_t fd) {
        if (fd >= superblock.total_inodes || !inode_bitmap[fd]) {
            std::cout << "File in inode " << fd << " not found\n";
            return;
        }
        Inode& inode = inodes[fd];
        uint32_t blocks = inode.file_size / BLOCK_SIZE;
        if (inode.file_size % BLOCK_SIZE != 0) blocks++;

        *(uint64_t*)&inode.reserve[0] = 0;
        inode.reserve[2] = 0;
        inode.reserve[3] = 0;

        // Освобождение блоков
        for (int i = 0; i < DIRECT_POINTERS && inode.direct_block_pointer[i] != -1; ++i) {
            free_block(inode.direct_block_pointer[i]);
            blocks--;
        }
        if (blocks==0) {
            free_inode(fd);
            return;
        }

        if (inode.indirect1_pointer != -1){
            uint64_t offset = inodes[fd].indirect1_pointer * BLOCK_SIZE; // memory +
            Inode_pointers* j_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers
            for (int j=0; j < RECORDS_CNT && j_pointers->block_pointer[j]!=-1 ;j++){
                free_block(j_pointers->block_pointer[j]);
                blocks--;
                j_pointers->block_pointer[j] = -1;
            }
            free_block(inode.indirect1_pointer);
            inode.indirect1_pointer = -1;

            if (blocks==0) {
                free_inode(fd);
                return;
            }
        }

        if (inode.indirect2_pointer != -1){
            uint64_t offset = inodes[fd].indirect2_pointer * BLOCK_SIZE; // memory +
            Inode_pointers* j_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers
            for (int j=0; j < RECORDS_CNT && j_pointers->block_pointer[j]!=-1; j++){
                uint64_t offset = j_pointers->block_pointer[j]*BLOCK_SIZE; // memory +
                Inode_pointers* i_pointers = (Inode_pointers*)(memory + offset); // double indirect pointers
                for (int i=0; i < RECORDS_CNT && i_pointers->block_pointer[i]!=-1; i++){
                    free_block(i_pointers->block_pointer[i]);
                    blocks--;
                    i_pointers->block_pointer[i] = -1;
                }
                free_block(j_pointers->block_pointer[j]);
                j_pointers->block_pointer[j] = -1;
                //blocks--;
            }

            free_block(inode.indirect2_pointer);
            inode.indirect2_pointer = -1;

            if (blocks==0) {
                free_inode(fd);
                return;
            }
        }

        // Освобождение inode

        free_inode(fd);
        //std::cout << "File in inode " << fd << " deleted\n";
    }

    // Вывод состояния файловой системы
    void Ext2FileSystem::print_fs_state() {
        std::cout << "FS Status:\n";
        std::cout << "Total Blocks: " << superblock.total_blocks << ", Free Blocks: " << superblock.free_blocks << "\n";
                std::cout << "Total inodes: " << superblock.total_inodes << ", Free inodes: " << superblock.free_inodes << "\n";
    }


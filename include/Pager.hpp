#pragma once
#include"std.hpp"
#include"Row.hpp"
#include <cstdint>
#include <fstream>
#include<filesystem>
#include <string_view>
class Pager
{
public:

Pager(const std::string&  path)
{
    std::filesystem::path p = path;
    bool is_exist = std::filesystem::exists(p);
    
    std::ios::openmode mode = std::ios::in | std::ios::out | std::ios::binary;
    if(!is_exist) mode |= std::ios::trunc;
    db.open(p,mode);
    if(!db.is_open())
    {
        throw std::runtime_error("Open file ERROR: " + path);
    }

    file_length = static_cast<uint32_t>(std::filesystem::file_size(p));
    page_num = file_length / PAGE_SIZE;
    if(file_length % PAGE_SIZE != 0) page_num++;
    


}
char* get_page(uint32_t num)
{
    if(pages[num] != nullptr)//缓存命中
    {
        return pages[num];
    }
    else 
    {
        char* page =  new char[PAGE_SIZE]{};
        if(num < page_num)
        {
            //从文件读取这页的内存块
            uint32_t read_bytes = PAGE_SIZE;
            if(num == page_num - 1 && file_length % PAGE_SIZE != 0) read_bytes = file_length % PAGE_SIZE;
            db.seekg(num * PAGE_SIZE,std::ios::beg);
            
            db.read(page, read_bytes);
        }
        
        pages[num] = page;
        return pages[num];
    }

    
}

void flush(uint32_t num,uint32_t size)
{
    if(pages[num] == nullptr) return;

    db.seekp(num * PAGE_SIZE,std::ios::beg);
    db.write(pages[num],size);
}

uint32_t get_file_length()
{
    return file_length;
}
~Pager()
{
    for(int i = 0;i < TABLE_MAX_PAGE;++i)
    {
        delete[] pages[i];
    }
    db.close();
}
Pager(const Pager&) =  delete;
Pager& operator=(const Pager&) = delete;

private:
std::fstream db;
uint32_t page_num{0};
uint32_t file_length{0};
char* pages[TABLE_MAX_PAGE]{nullptr};
};
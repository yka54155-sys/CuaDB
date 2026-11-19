#pragma once
#include"std.hpp"
#include"Row.hpp"
#include <cstdint>
#include <fstream>
#include<filesystem>
#include <stdexcept>
#include <string_view>

/**
 * @brief 
 页处理器
 *@details
 *get_page（获取页地址） 
 *get_unused_page（获取未使用的页） 
 *flush（落盘） 
 *get_file_length（获取实际数据库文件长度） 
 *使用方法： 
*实例化Pager--用get_page获取页地址--写入页--flush落盘（最好是循环flush，将缓存全部落盘）  
 * 
 */
class Pager
{
public:
uint32_t num_pages{0};//数据库文件目前的页数，当页数增加时需要跟着前推，get_unused_page的返回值

/**
 * @brief Construct a new Pager object
 * 
 * @param path 要打开的数据库文件路径
 */
Pager(const std::string&  path)
{
    //打开数据库文件
    std::filesystem::path p = path;
    bool is_exist = std::filesystem::exists(p);
    std::ios::openmode mode = std::ios::in | std::ios::out | std::ios::binary;
    if(!is_exist) mode |= std::ios::trunc;
    db.open(p,mode);
    if(!db.is_open())
    {
        throw std::runtime_error("Open file ERROR: " + path);
    }

    //计算数据库文件长度
    file_length = static_cast<uint32_t>(std::filesystem::file_size(p));

    //根据文件长度和@ROW.hpp中的PAGE_SIZE计算已有的页数num_pages
    num_pages = file_length / PAGE_SIZE;
    if(file_length % PAGE_SIZE != 0)//如果文件长度不对说明文件错误
        throw std::runtime_error("DB file is not a whole number pages.Corrupt file.");
}
/**
 * @brief 从缓存中获取页地址：命中直接返回；未命中则分配一块页内存，必要时从文件读入
 * @details 按页号分三种情况：
 *          ① num < num_pages ：文件里已有这页，从文件读入（读失败会得到一页全 0）
 *          ② num == num_pages：分配新页，num_pages 前推为 num + 1
 *          ③ num > num_pages ：跳号，目前不报错，但会留下「中间页全是 0」的空洞
 * @param num 页号，必须满足 num < TABLE_MAX_PAGE，且调用方应保证 num <= num_pages
 * @return char* 页在缓存中的首地址
 * @warning num >= TABLE_MAX_PAGE(=100) 时 pages[num] 会直接越界写内存
 * @pre  num < TABLE_MAX_PAGE 且 num <= num_pages（不允许跳号）
 * @post 返回的页要么来自磁盘，要么是全新的全 0 页
 * @invariant num_pages 只增不减；flush 不会改变它
 */
char* get_page(uint32_t num)
{
    //缓存命中
    if(pages[num] != nullptr)
    {
        //直接返回页地址
        return pages[num];
    }

    //缓存未命中
    else 
    {
        //初始化一块地址，大小为@ROW.hpp中定义的PAGE_SIZE
        char* page =  new char[PAGE_SIZE]{};

        //如果要获取的这个页在实际数据库文件但是不在缓存
        //那么就从数据库文件中读取这页的内容
        if(num < num_pages)
        {
            /*从文件读取这页的内存块*/

            uint32_t read_bytes = PAGE_SIZE;//获取需要读取的字节数，从@ROW.hpp获取PAGE_SIZE
            
            //将文件指针拨到这页的开头
            db.seekg(num * PAGE_SIZE,std::ios::beg);
            
            //将read_bytes大小的字节写入page
            //注意：这里没有检查读取是否成功。读越界/文件损坏时 read 会置 failbit，
            //      之后所有 seekg/seekp/read/write 都会静默失效，直到 db.clear()
            db.read(page, read_bytes);
        }

        //如果数据库文件里没有这页（或页号跳号）
        //那么 num_pages 变为 num + 1（注意是 num + 1，不是自增 1）
        //例如 num_pages = 1、num = 10，此处过后 num_pages = 11
        //[WARNING] 跳号目前不报错，但有两个后果：
        //  ① 落盘后文件中间 1~9 页是空洞（全 0），下次读出来是一页全 0；
        //     而 node_type=0 恰好等于 INTERNAL，会被当成「合法的空内部节点」，查树时出错
        //  ② 越界读会让 fstream 置 failbit，后续所有 IO 静默失效
        //  正确做法：调用方只允许 num <= num_pages，跳号应当直接 throw
        else if(num >= num_pages)
        {
            num_pages  = num + 1;
        }
        
        //将获取到的page写入缓存
        //无论实际数据库是否有这页，都要用page来暂存这页
        //区别只在于，如果有，需要从数据写入page
        //如果没有，只需要初始化这一块内存
        pages[num] = page;
        return pages[num];//返回获取到的页地址头
    }

    
}

/**
 * @brief 获取未使用的页号
 * @details 返回 num_pages。若已有 3 页（0、1、2），返回 3 就是下一块空位。
 * @warning 这个函数只是「读」页号，不会占位。连续调用两次而不调用 get_page，
 *          拿到的是同一个页号；必须像分裂代码那样写成
 *          `get_page(pager->get_unused_page())` 一口气用掉。
 * @return uint32_t 未使用的页号，即当前的 num_pages
 */
uint32_t get_unused_page()
{
    return num_pages;
}

/**
 * @brief 把某一页的缓存内容写回数据库文件
 * @param num 要落盘的页号
 * @note 页不在缓存（pages[num] == nullptr）时直接跳过，不报错
 * @warning 不检查写入是否成功：磁盘满、只读文件、流处于 fail 状态时都会静默失败
 * @warning 落盘不会更新 num_pages 和 file_length，这两个值仍停留在打开时的状态
 */
void flush(uint32_t num)
{
    if(pages[num] == nullptr) return;//如果这页在缓存为空，则跳过

    //将数据库文件指针拨到num这页的开头，其实就是把指针拨到文件开头然后偏移页编号 * 页大小个字节
    db.seekp(num * PAGE_SIZE,std::ios::beg);

    //落盘
    db.write(pages[num],PAGE_SIZE);
}

/**
 * @brief 获取文件长度，只在初始化数据库时获取一次
 * 
 * @return uint32_t 数据库文件的字节数
 */
uint32_t get_file_length()
{
    return file_length;
}
~Pager()
{
    //TODO: 语义上应该按「缓存里实际有哪些页」遍历，现在遍历 TABLE_MAX_PAGE 只是碰巧无害
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

uint32_t file_length{0};
char* pages[TABLE_MAX_PAGE]{nullptr};//缓存中的页，初始化为空
};
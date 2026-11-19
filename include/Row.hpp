#pragma once
#include"std.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
/**
 * @brief 
        CuaDb项目的ROW头文件
 * @details 
        用于定义有关ROW的各项结构体、处理函数、各类常量的字节数、行数据在数据库中的实际结构
 */

constexpr uint32_t COLUMN_USERNAME_SIZE = 32; //row.username的size，单位：字节
constexpr uint32_t COLUMN_EMAIL_SIZE = 255;  //row.email的size，单位：字节

/**
 * @brief 行数据在数据库中的实际结构
 * @details 内存布局：id(4) + username(33) + email(256)，编译器做 4 字节对齐，
 *          所以 sizeof(Row) = 296；而写入磁盘时只序列化 293 字节（不含 padding）。
 */
struct Row
{
    uint32_t id;                             ///< 主键，4 字节无符号整数
    char username[COLUMN_USERNAME_SIZE + 1]; ///< 用户名，最多 32 字节 + 1 字节 '\0'；写入前必须保证长度不超过 32，operator>> 不做检查会写坏栈
    char email[COLUMN_EMAIL_SIZE + 1];       ///< 邮箱，最多 255 字节 + 1 字节 '\0'，理由同上
};


//row.id的实际size
constexpr uint32_t ROW_ID_SIZE = sizeof(Row{}.id);//根据Row结构体的实际成员大小定义row.id的size常量，Row{}.id的用法是创建一个临时变量来准确测量成员真正的占用大小
//row.username的实际size
constexpr uint32_t ROW_USERNAME_SIZE = sizeof(Row{}.username);//同上
//row.email的实际size
constexpr uint32_t ROW_EMAIL_SIZE = sizeof(Row{}.email);//同上
//Row 序列化后（不含内存对齐 padding）的字节数：4 + 33 + 256 = 293
//注意：sizeof(Row) 是 296，多出的 3 字节是编译器补的对齐，不写进磁盘；
//      用 sizeof(Row) 去算页内格子大小会算错，必须用这个常量
constexpr uint32_t ROW_TOTAL_SIZE = ROW_EMAIL_SIZE + ROW_USERNAME_SIZE + ROW_ID_SIZE;
constexpr uint32_t ID_OFFSET = 0;//row.id 的偏移量，也就是row.id在一块实际内存中的偏移开头位置，例如想要修改row.id的话，需要以 地址 + ID_OFFSET为起点修改ID_SIZE这么大的区域
constexpr uint32_t USERNAME_OFFSET = ROW_ID_SIZE + ID_OFFSET;//row.username的偏移开头量，使用方法同上，因为需要在实际内存中紧随id其后，所以计算方法是 ROW_ID_SIZE + ID_OFFSET，也就是 ID 这一块区域的后面就是 username 的开头
constexpr uint32_t EMAIL_OFFSET = USERNAME_OFFSET + ROW_USERNAME_SIZE;//同上
constexpr uint32_t PAGE_SIZE= 4096;//一页 4096 字节（SQLite 的默认页大小）。页越大扇出越大、树越矮，它也是页缓存与文件读写的最小单位
constexpr uint32_t TABLE_MAX_PAGE = 100;//页缓存数组长度（自定义）。每页最多 13 行，所以全库上限约 1150 行——容量瓶颈在这里，不在 B+ 树


/**
 * @brief 
        序列化Row结构体实例的函数
 * @details
        用于将Row结构体的实例序列化，也就是变成没有Padding的连续字节序列，保证只要解析方式合理那么数据就不会改变
 * @param destination 序列化后的结果，一块内存地址
 * @param source 需要序列化的内容。Row实例
 */
inline void serialize_row(void* destination, const Row& source) 
{
    //用于保存序列化结果的内存转换为char便于操作
    char* dest = static_cast<char*>(destination);

    //使用memcpy来按字节序列化Row结构体
    //第一个参数：字节拷贝的目的地的地址起始位置，此处使用dest + OFFSET
    //也就是在dest这块地址里的OFFSET处是接下来需要写入的地址开头
    //例如：dest + ID_OFFSET，就是row.id在这块内存中实际写入时的地址的开头位置
    //第二个参数：拷贝来源，也就是需要把哪一个内容拷贝进这个地址
    //第三个参数：通过第一个参数知道拷贝起始位置后，此参数定义拷贝的字节大小
    //也就是从第一个参数定义的地址为起点，从第二个参数中拷贝多少字节
    memcpy(dest + ID_OFFSET, &source.id, ROW_ID_SIZE);
    memcpy(dest + USERNAME_OFFSET, &source.username, ROW_USERNAME_SIZE);
    memcpy(dest + EMAIL_OFFSET, &source.email, ROW_EMAIL_SIZE);
}

/**
 * @brief 
        反序列化Row结构体实例的函数
 * @details 
        用于将一块已经序列化的Row结构体内存反序列化解析为Row实例
 * @param source 需要反序列化的内存地址
 * @param destination 反序列化后的结果，Row结构体指针
 */
inline void deserialize_row(void* source,Row* destination)
{

    //将已经序列化的地址转换为char，便于操作
    char* src = static_cast<char*>(source);

    //操作方法类似序列化函数
    //区别在于此函数中
    //memcpy的第一个参数为Row结构体指针中的成员
    //第二个参数为 序列化的内存 + OFFSET
    //第三个参数依旧是需要拷贝的字节大小
    memcpy(&destination->id,src + ID_OFFSET , ROW_ID_SIZE);
    memcpy(&destination->username,src + USERNAME_OFFSET , ROW_USERNAME_SIZE);
    memcpy(&destination->email,src + EMAIL_OFFSET , ROW_EMAIL_SIZE);
}

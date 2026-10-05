#pragma once
#include"std.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
constexpr uint32_t COLUMN_USERNAME_SIZE = 32;
constexpr uint32_t COLUMN_EMAIL_SIZE = 255;
struct Row
{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
};
constexpr uint32_t ROW_ID_SIZE = sizeof(Row{}.id);
constexpr uint32_t ROW_USERNAME_SIZE = sizeof(Row{}.username);
constexpr uint32_t ROW_EMAIL_SIZE = sizeof(Row{}.email);
constexpr uint32_t ROW_TOTAL_SIZE = ROW_EMAIL_SIZE + ROW_USERNAME_SIZE + ROW_ID_SIZE;
constexpr uint32_t ID_OFFSET = 0;
constexpr uint32_t USERNAME_OFFSET = ROW_ID_SIZE + ID_OFFSET;
constexpr uint32_t EMAIL_OFFSET = USERNAME_OFFSET + ROW_USERNAME_SIZE;
constexpr uint32_t PAGE_SIZE= 4096;//一页的最大字节数
constexpr uint32_t TABLE_MAX_PAGE = 100;//最大页数
constexpr uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_TOTAL_SIZE;//每页最大容纳列数
constexpr uint32_t TABLE_MAX_ROWS = TABLE_MAX_PAGE * ROWS_PER_PAGE;//表中最大容纳列数

inline void serialize_row(void* destination, const Row* source) 
{
    char* dest = static_cast<char*>(destination);
    memcpy(dest + ID_OFFSET, &source->id, ROW_ID_SIZE);
    memcpy(dest + USERNAME_OFFSET, &source->username, ROW_USERNAME_SIZE);
    memcpy(dest + EMAIL_OFFSET, &source->email, ROW_EMAIL_SIZE);
}

inline void deserialize_row(void* source,Row* destination)
{
    char* src = static_cast<char*>(source);
    memcpy(&destination->id,src + ID_OFFSET , ROW_ID_SIZE);
    memcpy(&destination->username,src + USERNAME_OFFSET , ROW_USERNAME_SIZE);
    memcpy(&destination->email,src + EMAIL_OFFSET , ROW_EMAIL_SIZE);
}

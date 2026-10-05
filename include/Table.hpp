#pragma once
#include"Row.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>
#include<filesystem>
#include"Pager.hpp"
class Table
{
    private:
    public:
    uint32_t num_rows{0};
    
    std::unique_ptr<Pager> pager;
    class iterator
    {
        public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Row;
        using difference_type = std::ptrdiff_t;
        using pointer = Row*;
        using reference = Row;
        
        iterator(Table& table,uint32_t row_num) : table(table),row_num(row_num)
        {

        }
        void write(const Row& r)
        {
            serialize_row(slot(), &r);
            if(this->row_num == table.num_rows)
            {
                table.num_rows++;
            }
        }
        Row operator*() const
        {
            Row r{};
            deserialize_row(slot(),&r);
            return r;
        }
        iterator& operator++()
        {
            row_num++;
            return *this;
        }
        iterator operator++(int)
        {
            iterator old = *this;
            row_num++;
            return old;
        }
        bool operator!=(const iterator& other) const
        {
            return this->row_num != other.row_num;
        }
        bool operator==(const iterator& other) const
        {
            return this->row_num == other.row_num;
        }

        private:
        Table& table;
        uint32_t row_num;
        char* slot() const
        {
            uint32_t page_num = row_num / ROWS_PER_PAGE;
            char* page = table.pager->get_page(page_num);
            uint32_t row_offset = row_num % ROWS_PER_PAGE;
            uint32_t offset = row_offset * ROW_TOTAL_SIZE;
            char* row_pos = page + offset;

            return row_pos;
        }


    };

    
    Table(const std::string& path)
    {
        pager = std::make_unique<Pager>(path);
        uint32_t full_pages = pager->get_file_length() / PAGE_SIZE;
        uint32_t addition_row = (pager->get_file_length() - full_pages * PAGE_SIZE) / ROW_TOTAL_SIZE;
        num_rows = full_pages * ROWS_PER_PAGE + addition_row;
    }


    ~Table()
    {
       
        uint32_t full_num = num_rows / ROWS_PER_PAGE;
        for(int i = 0 ;i < full_num;++i)
        {
            uint32_t size = PAGE_SIZE;
            pager->flush(i,size);
        }
        uint32_t addition_row = num_rows % ROWS_PER_PAGE;
        if(addition_row > 0)
        {
            uint32_t size = addition_row * ROW_TOTAL_SIZE;
            pager->flush(full_num, size);
        }
        
    }
    iterator begin()
    {
        return iterator(*this,0);
    }
    iterator end()
    {
        return iterator(*this,num_rows);
    }



};

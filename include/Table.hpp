#pragma once
#include "Node.hpp"
#include"Row.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    uint32_t root_page_num{0};//根节点类型的页的位置（？）
    std::unique_ptr<Pager> pager;//页处理器实例
    
    void create_new_root()
    {

    }

    void leaf_node_split_and_insert(uint32_t page_num,uint32_t cell_num,uint32_t key,const Row& r)
    {
        char* old_node = pager->get_page(page_num);
        char* new_node = pager->get_page(pager->get_unused_page());
        leaf_node_initialize(new_node);
        for(int32_t i = LEAF_NODE_MAX_CELLS; i >= 0;--i)
        {
            char* destination_node;
            if(i < LEAF_NODE_LEFT_SPLIT_COUNT)
            {
                destination_node = old_node;
            }
            else
            {
                destination_node = new_node;
            }
            uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
            char* destination = leaf_node_cell(destination_node, index_within_node);
            
            if(i == cell_num)
            {
                serialize_row(leaf_node_value(destination_node, cell_num),r);
            }
            else if(i > cell_num)
            {
                std::memcpy(destination,leaf_node_cell(old_node,i - 1),LEAF_NODE_CELL_SIZE);
            }
            else 
            {
                std::memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
            }
        }
        leaf_node_write_num_cells(old_node, LEAF_NODE_RIGHT_SPLIT_COUNT);
        leaf_node_write_num_cells(new_node, LEAF_NODE_LEFT_SPLIT_COUNT);

        if(is_node_root(old_node))
        {
            create_new_root();
            return;
        }   
        else 
        {
            throw std::runtime_error("Need to implement updating parent after split\n");
            exit(1);
        }
    }

    class iterator
    {
        public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Row;
        using difference_type = std::ptrdiff_t;
        using pointer = Row*;
        using reference = Row;
        
        iterator(Table& table,uint32_t page_num,uint32_t cell_num) : table(table),page_num(page_num),cell_num(cell_num)
        {

        }
        void write(const Row& r)
        {
            if(leaf_node_read_num_cells(node()) >= LEAF_NODE_MAX_CELLS)
            {
                table.leaf_node_split_and_insert(page_num,cell_num,r.id,r);
                return;
            }
            leaf_node_insert(node(),  cell_num, r.id, r);
        }
        Row operator*() const
        {
            Row r{};
            deserialize_row(slot(),&r);
            return r;
        }
        iterator& operator++()
        {
            cell_num++;
            return *this;
        }
        iterator operator++(int)
        {
            iterator old = *this;
            cell_num++;
            return old;
        }
        bool operator!=(const iterator& other) const
        {
            return this->cell_num != other.cell_num || this->page_num != other.page_num;
        }
        bool operator==(const iterator& other) const
        {
            return this->cell_num == other.cell_num && this->page_num == other.page_num;
        }
        uint32_t key() const
        {
            assert(cell_num < leaf_node_read_num_cells(node()) && "Error , you can't use key() in end().");
            return  leaf_node_read_key(node(), cell_num); 
        }

        private:
        Table& table;
        uint32_t page_num;
        uint32_t cell_num;
        char* node() const
        {
            return table.pager->get_page(page_num);
        }
        char* slot() const
        {   
            char* row_pos = leaf_node_value(node(),cell_num);
            return row_pos;
        }


    };

    
    Table(const std::string& path)
    {
        //实例化Pager打开数据库
        pager = std::make_unique<Pager>(path);
        
        
        root_page_num = 0;//将根页设为第0页

        /*
        如果当前数据库的实际页数为0
        使用Pager->get_page 获取第0页地址
        */
        if(pager->num_pages == 0)
        {
            char* root = pager->get_page(0);//获取第0页地址来初始化根节点
            leaf_node_initialize(root);//叶节点初始化函数，此处应该改为root_node_initialize[TODO]
            root[IS_ROOT_OFFSET] = 1;//将根节点的头位置的IS_ROOT属性设置为1，意思是这个节点是根节点
        }
    }

    ~Table()
    {
        //析构时自动循环调用Pager的flush将缓存落盘
        for(uint32_t i = 0 ;i < pager->num_pages;++i)
        {
            pager->flush(i);
        }   
    }
    iterator begin()
    {
        return iterator(*this,root_page_num,0);
    }
    iterator end()
    {
        return iterator(*this,root_page_num,leaf_node_read_num_cells(this->pager->get_page(root_page_num)));
    }
    iterator find(uint32_t k)
    {
        char* node = pager->get_page(root_page_num);
        if(node_type_read(node) != NodeType::LEAF)
        {
            throw std::runtime_error("Need to implement searching an internal node.");
        }

        uint32_t cell_num = leaf_node_find_idx(node, k);
        return iterator(*this,root_page_num,cell_num);
    }
};

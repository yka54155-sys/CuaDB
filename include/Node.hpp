#pragma once
#include"std.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include"Row.hpp"
/**
 * @brief 
 
 * 
 */

enum class NodeType : uint8_t
{
    INTERNAL = 0,
    LEAF  = 1 
};
//Common node header
constexpr uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
constexpr uint32_t NODE_TYPE_OFFSET = 0;
constexpr uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
constexpr uint32_t IS_ROOT_OFFSET = NODE_TYPE_OFFSET + NODE_TYPE_SIZE;
constexpr uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
constexpr uint32_t PARENT_POINTER_OFFSET = IS_ROOT_SIZE + IS_ROOT_OFFSET;
constexpr uint32_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

//Leaf node header
constexpr uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
constexpr uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
constexpr uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

//Leaf node body
constexpr uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
constexpr uint32_t LEAF_NODE_KEY_OFFSET  = 0;
constexpr uint32_t LEAF_NODE_VALUE_SIZE = ROW_TOTAL_SIZE;
constexpr uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_SIZE + LEAF_NODE_KEY_OFFSET;
constexpr uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
constexpr uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
constexpr uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

//Leaf node split
constexpr uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
constexpr uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;

//Internal node header
constexpr uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
constexpr uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
constexpr uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
constexpr uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_NUM_KEYS_OFFSET;
constexpr uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;
constexpr uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
constexpr uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
constexpr uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_KEY_SIZE + INTERNAL_NODE_CHILD_SIZE;
//Read/Write uint32 in raw memory
inline uint32_t u32_read(const char* p)
{
    uint32_t v;
    std::memcpy(&v,p,sizeof(v));
    return v;
}

inline void u32_write(char* p,uint32_t v)
{
    std::memcpy(p,&v,sizeof(v));
}

//Read/Write uint8 in raw memory
inline uint8_t u8_read(const char* p)
{
    uint8_t v;
    std::memcpy(&v,p,sizeof(v));
    return v;
}

inline void u8_write(char* p,uint8_t v)
{
    std::memcpy(p,&v,sizeof(v));
}

//Leaf node NUM_CELLS read/write
inline uint32_t leaf_node_read_num_cells(const char* node)
{
    return u32_read(node + LEAF_NODE_NUM_CELLS_OFFSET);
}
inline void leaf_node_write_num_cells(char* node,uint32_t v)
{
    u32_write(node + LEAF_NODE_NUM_CELLS_OFFSET,  v);
}

//Leaf node seek cell position 
inline char* leaf_node_cell(char* node,uint32_t cell_num)
{
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

//Leaf node key Read/Write
inline uint32_t leaf_node_read_key(char* node,uint32_t cell_num)
{
    return u32_read(leaf_node_cell(node,cell_num));
}
inline void leaf_node_write_key(char* node,uint32_t cell_num,uint32_t k)
{
    u32_write(leaf_node_cell(node,cell_num), k);
}

//Leaf node value Read/Write
inline char* leaf_node_value(char* node,uint32_t cell_num)
{
    return leaf_node_cell(node,cell_num) + LEAF_NODE_VALUE_OFFSET;
}

//Leaf node initialize
inline void leaf_node_initialize(char* node)
{
    node[NODE_TYPE_OFFSET] = static_cast<char>(NodeType::LEAF);
    node[IS_ROOT_OFFSET]  = 0;
    u32_write(node + PARENT_POINTER_OFFSET, 0);

    leaf_node_write_num_cells(node, 0);
}

//Leaf node insert
inline void leaf_node_insert(char* node, uint32_t cell_num,uint32_t key,const Row& r)
{
    uint32_t num_cell = leaf_node_read_num_cells(node);


    if(cell_num < num_cell)
    {
        for(uint32_t i = num_cell ;i > cell_num;--i)
        {
            std::memcpy(leaf_node_cell(node, i),leaf_node_cell(node, i-1),LEAF_NODE_CELL_SIZE);
        }
    }

    leaf_node_write_num_cells(node, num_cell + 1);
    leaf_node_write_key(node, cell_num, key);
    serialize_row(leaf_node_value(node, cell_num),r);
}

//Node type Read/Write
inline NodeType node_type_read(const char* node)
{
    return static_cast<NodeType>(u8_read(node + NODE_TYPE_OFFSET));
}

inline void node_type_write(char* node,NodeType type)
{
    u8_write(node + NODE_TYPE_OFFSET, static_cast<uint8_t>(type));
}

//Leaf node find key index
inline uint32_t leaf_node_find_idx(char* node,uint32_t k)
{
    uint32_t num_cells = leaf_node_read_num_cells(node);
    uint32_t min_index = 0;
    uint32_t past_last = num_cells;
    while(min_index != past_last)
    {
        uint32_t mid = (min_index + past_last) / 2;
        uint32_t mid_key = leaf_node_read_key(node, mid);
        if(k == mid_key)
        {
            return mid;
        }
        else if (k > mid_key) 
        {
            min_index = mid + 1;    
        }
        else 
        {
            past_last = mid;
        }
    }

    return min_index;
}

inline void leaf_node_print(char* node)
{
    uint32_t num_cells = leaf_node_read_num_cells(node);
    std::cout << "Node cell number: " << num_cells << "\n";
    for(uint32_t i = 0;i < num_cells;++i)
    {
        std::cout << "-- " << i << " : " << leaf_node_read_key(node, i);
        std::cout << "\n";
    }
    
}

inline bool is_node_root(const char* node)
{
    return u8_read(node + IS_ROOT_OFFSET);
}

inline void set_node_root(char* node,bool is_root)
{
    u8_write(node + IS_ROOT_OFFSET, is_root ? 1 : 0);
}


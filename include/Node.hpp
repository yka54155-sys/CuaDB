#pragma once
#include"std.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include"Row.hpp"
/**
 * @brief 此文件是CuaDB项目有关Node的函数、常量的定义头文件
 * @details Node有两种：
        1. Leaf Node
        2. Internal Node
        两种Node的头部（header）有相同的定义，也就是Common node header，也有不同的定义，Leaf Node Header,Internal Node Header
        两种Node的身体部分（body）定义不同，Leaf Node Body是key + value ， Internal Node Nody是key + child
 */
 
 /**
  * @brief 页内两种 Node 的字节分布
  * @details 一页固定 4096 字节：前面是节点头部，后面是 cell 数组。
  *
  *  ============ Leaf Node（叶子节点，type = 1）============
  *
  *   offset | size | field
  *  --------+------+--------------------------------------------
  *     0    |   1  | node type = LEAF(1)
  *     1    |   1  | is root
  *     2    |   4  | parent pointer
  *     6    |   4  | num cells
  *  --------+------+---- header: 10 bytes ----
  *    10    |   4  | cell 0: key
  *    14    | 293  | cell 0: value  (serialized Row)
  *   307    |   4  | cell 1: key
  *   311    | 293  | cell 1: value
  *   ...    |      | ...
  *
  *   每个 cell = 4(key) + 293(value) = 297 字节
  *   可用空间 = 4096 - 10 = 4086，最多 4086 / 297 = 13 个 cell
  *
  *  ============ Internal Node（内部节点，type = 0）============
  *
  *   offset | size | field
  *  --------+------+--------------------------------------------
  *     0    |   1  | node type = INTERNAL(0)
  *     1    |   1  | is root
  *     2    |   4  | parent pointer
  *     6    |   4  | num keys
  *    10    |   4  | right child  (page number)
  *  --------+------+---- header: 14 bytes ----
  *    14    |   4  | cell 0: key
  *    18    |   4  | cell 0: child  (page number)
  *    22    |   4  | cell 1: key
  *    26    |   4  | cell 1: child
  *   ...    |      | ...
  *
  *   每个 cell = 4(key) + 4(child) = 8 字节
  *   可用空间 = 4096 - 14 = 4082，最多 4082 / 8 = 510 个 cell（扇出 511）
  *
  *  ============ 两种节点的区别 ============
  *
  *   1. 头部：叶子 10 字节；内部 14 字节（多 num_keys 4 + right_child 4）
  *   2. cell：叶子 297 字节（key + 整行数据）；内部 8 字节（key + 子页号）
  *   3. 偏移 6 在叶子里是 num_cells、在内部节点里是 num_keys，语义不同，
  *      所以拿到一页必须先看 node type，不能一律按叶子解释
  *   4. 内部节点 n 个 key 对应 n+1 个孩子：第 i 个 cell 的 child 是左孩子，
  *      right_child 是最右边那个孩子（键比所有 key 都大时走它）
  *   5. 第 i 个 cell 的 key = 该 child 子树的最大键（SQLite 约定），
  *      因此插入比该键更大的键时，必须同步更新父节点的这个 key
  */

/**
 * @brief 两种节点的枚举，Internal是0，Leaf是1
 * @details uint8_t格式，只占1字节
 */
enum class NodeType : uint8_t
{
    INTERNAL = 0,
    LEAF  = 1 
};

//Common node header
/**
 * @brief 以下是所有的Node的header部分都会有的部分，common node header
 * 
 */
constexpr uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);//node type 的大小：1字节，判断节点种类
constexpr uint32_t NODE_TYPE_OFFSET = 0;//node type 的偏移量：0
constexpr uint32_t IS_ROOT_SIZE = sizeof(uint8_t);//is root 的大小：1字节，判断节点是否是根节点
constexpr uint32_t IS_ROOT_OFFSET = NODE_TYPE_OFFSET + NODE_TYPE_SIZE;//is root d的偏移量：0 + 1
constexpr uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);//parent pointer 的大小：4字节，节点的父节点是哪个
constexpr uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;//parent pointer的偏移量：1 + 1 = 2
constexpr uint32_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;//通用头部的大小：6字节，计算方式为前三个部分的大小总和

//Leaf node header
/**
 * @brief 以下是Leaf node的特有header部分，leaf node header
 * 
 */
constexpr uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);//num cell的大小：4字节，这个leaf node中有多少个cell
constexpr uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;//num cell的偏移量：6，紧随通用header后面
constexpr uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;//leaf header大小：6 + 4

/**
 * @brief 以下是leaf node的body部分
 * @details 由cell组成，cell = key + value
 * key = 主键，4字节，不可重复，用于数据库排序和查找
 * value = @Row.hpp文件中定义的的Row结构体
 */
//Leaf node body
constexpr uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);//key 大小：4字节
constexpr uint32_t LEAF_NODE_KEY_OFFSET  = 0;//key在cell中偏移量，！！！注意！！！，这里是在cell中的偏移量，而不是在整个node中的偏移量
constexpr uint32_t LEAF_NODE_VALUE_SIZE = ROW_TOTAL_SIZE;//value的大小：@Row.hpp中的ROW_TOTAL_SIZE(293)
constexpr uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_SIZE + LEAF_NODE_KEY_OFFSET;//!!!这里是value在cell中相对偏移量：4 + 0
constexpr uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;//单个cell的大小：4 + ROW_TOTAL_SIZE(293)
constexpr uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;//所有cell可以占用的大小：PAGE_SIZE(4096) - 10
constexpr uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;//最大 cell 数：(4096 - 10) / 297 = 13.75，向下取整 = 13

//Leaf node split
/**
 * @brief 以下是leaf node分裂时左右两个节点各自分配到的cell数量的常量
 * @details 用计算的方法得出，更加健壮
 */
constexpr uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;//右节点的cell数量：（13 + 1） / 2 = 7
constexpr uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;//左节点：14 - 7 = 7。用总数减右侧，保证元素数为偶数时多出的一个落在左边

//Internal node header
/**
 * @brief 以下是internal node的特有header部分，internal node header
 * 
 */
constexpr uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);//num key的大小：4字节，这个节点中的key数量，cell数量
constexpr uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;//num key的偏移量：6，紧随common header 后面
constexpr uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);//right child的大小：4字节，下面的leaf node中最大key值所在的那个节点的页号
constexpr uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_NUM_KEYS_OFFSET;//right child的偏移量：10
constexpr uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;/*
internal node header的大小：6 + 4 + 4 = 14字节
*/

/**
 * @brief 以下是internal node的body部分
 * @details body由cell组成
 cell = key + child
 key = 对应的child指向的页中的最大key值
 child = 指向一个由该节点管理的leaf node
 */
constexpr uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);//key 的大小：4字节，对应的child指向的页中的最大key值
constexpr uint32_t INTERNAL_NODE_KEY_OFFSET = 0;//key在cell中的相对偏移量：0，！注意！是相对cell的偏移量，不是在整个node中的偏移量
constexpr uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);//child的大小： 4字节，指向一个由该节点管理的leaf node
constexpr uint32_t INTERNAL_NODE_CHILD_OFFSET = INTERNAL_NODE_KEY_OFFSET + INTERNAL_NODE_KEY_SIZE;//child在cell中的相对偏移量：0 + 4 = 4（是相对 cell，不是相对 node）
constexpr uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_KEY_SIZE + INTERNAL_NODE_CHILD_SIZE;//单个cell的大小：4+4=8字节


//Read/Write uint32 in raw memory
/**
 * @brief 从原始内存读取一个 uint32_t
 * @details 用 memcpy 而不是 *reinterpret_cast<uint32_t*>(p)：页内 key 的地址不一定 4 字节对齐，强转后解引用是未定义行为
 * @param p 起始地址
 * @return uint32_t 读到的值
 */
inline uint32_t u32_read(const char* p)
{
    uint32_t v;
    std::memcpy(&v,p,sizeof(v));
    return v;
}
/**
 * @brief 把一个 uint32_t 写入原始内存
 * @details 与 u32_read 对称：memcpy 按字节写，不要求地址对齐
 * @param p 目标地址
 * @param v 要写入的值
 */
inline void u32_write(char* p,uint32_t v)
{
    std::memcpy(p,&v,sizeof(v));
}

//Read/Write uint8 in raw memory
/**
 * @brief 用于将一块4字节的内存读取为uint8_t类型的辅助函数
 * @details 之所以不用static_cast或reinterpret_cast是因为前者无法将char*转换为uint8_t，后者在转换时会默认对齐导致读取错误
 * @param p 需要读取的内存指针
 * @return uint8_t 转换结果
 */
inline uint8_t u8_read(const char* p)
{
    uint8_t v;
    std::memcpy(&v,p,sizeof(v));
    return v;
}

/**
 * @brief 用于将uint8_t数据写入一块内存
 * @details 之所以不用static_cast或reinterpret_cast是因为前者无法将char*转换为uint8_t，后者在转换时会默认对齐导致读取错误
 * @param p 写入的内存指针
 * @param v 需要写入的数据
 */
inline void u8_write(char* p,uint8_t v)
{
    std::memcpy(p,&v,sizeof(v));
}

//Leaf node NUM_CELLS read/write
/**
 * @brief leaf node的header中的num cells属性的读取函数
 * @pre u32_read函数
 * @param node 需要进行读取的leaf node指针
 * @return uint32_t 读取结果
 */
inline uint32_t leaf_node_read_num_cells(const char* node)
{
    return u32_read(node + LEAF_NODE_NUM_CELLS_OFFSET);
}

/**
 * @brief leaf node 的header中的num cells属性的写入函数
 * @pre u32_write函数
 * @param node 需要进行写入的leaf node指针
 * @param v 想要写入的数据
 */
inline void leaf_node_write_num_cells(char* node,uint32_t v)
{
    u32_write(node + LEAF_NODE_NUM_CELLS_OFFSET,  v);
}

//Leaf node seek cell position 
/**
 * @brief 获取节点内第 cell_num 个 cell 的地址
 * @details cell 从 0 开始编号。cell_num == num_cells 表示“最后一个 cell 之后的位置”，
 *          只能用于追加，不能当作已有 cell 读取
 * @param node 这个cell所在的node指针
 * @param cell_num 第几个 cell
 * @return char* cell 的起始地址
 */
inline char* leaf_node_cell(char* node,uint32_t cell_num)
{
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

//Leaf node key Read/Write
/**
 * @brief 对单个cell的key进行读取
 * 
 * @param node 这个cell所在的node
 * @param cell_num 这个cell是第几个
 * @return uint32_t 读取结果
 */
inline uint32_t leaf_node_read_key(char* node,uint32_t cell_num)
{
    //先用leaf_node_cell获取这个cell的位置
    //然后直接用u32_read读取，因为key在cell的开头，大小为uint32_t，所以直接读取即可
    return u32_read(leaf_node_cell(node,cell_num));
}

/**
 * @brief 对单个cell的key值写入
 * 
 * @param node 这个cell所在的node
 * @param cell_num 这个cell是第几个
 * @param k 需要写入的数据
 */
inline void leaf_node_write_key(char* node,uint32_t cell_num,uint32_t k)
{
    //先用leaf_node_cell获取这个cell的位置
    //然后直接用u32_write写入，因为key在cell的开头，大小为uint32_t，所以直接写入即可
    u32_write(leaf_node_cell(node,cell_num), k);
}

//Leaf node value Read/Write
/**
 * @brief 返回单个cell的value部分的地址，可供读写操作
 * 
 * @param node 这个cell所在的node
 * @param cell_num 这个cell是第几个cell
 * @return char* 返回的value指针
 */
inline char* leaf_node_value(char* node,uint32_t cell_num)
{
    //先用leaf_node_cell获取cell位置
    //然后用LEAF_NODE_VALUE_OFFSET计算出这个value的地址
    //也就是先获取这个cell的地址，然后在cell的地址基础上加上value相对cell的偏移量
    //也就是这个cell中value的实际地址
    return leaf_node_cell(node,cell_num) + LEAF_NODE_VALUE_OFFSET;
}

//Leaf node initialize
/**
 * @brief 将一个节点初始化并设置为leaf node
 * 
 * @param node 需要初始化的节点
 */
inline void leaf_node_initialize(char* node)
{
    //使用node[NODE_TYPE_OFFSET]也就是把整个地址块当成char数组直接操作NODE_TYPE_OFFSET位置
    //将NodeType::LEAF,强制转换成char写入
    node[NODE_TYPE_OFFSET] = static_cast<char>(NodeType::LEAF);
    //与NODE_TYPE相同操作方式
    node[IS_ROOT_OFFSET]  = 0;

    //用u32_write将node的PARENT_POINTER写为0
    //这里不用node[]来操作的原因是node[]只能用来操作单字节的地址
    //PARENT_POINTER为4字节大小
    //NODE_TYPE和IS_ROOT_OFFSET都是单字节，因此可以用node[]
    u32_write(node + PARENT_POINTER_OFFSET, 0);
    
    //将node的num cells初始化为0
    leaf_node_write_num_cells(node, 0);
}

//Leaf node insert
/**
 * @brief 在一个 leaf node 中插入一个 cell（key + 序列化后的 Row）
 * @details 先把 cell_num 及其之后的 cell 整体右移一格，再写入新的 key 和 value
 * @param node 目标叶子节点页
 * @param cell_num 插入位置，范围 [0, num_cells]；等于 num_cells 表示追加到末尾
 * @param key 新 cell 的键
 * @param r 要序列化进 value 的整行数据
 */
inline void leaf_node_insert(char* node, uint32_t cell_num,uint32_t key,const Row& r)
{
    //获取这个node中的cell数量
    uint32_t num_cells = leaf_node_read_num_cells(node);


    //如果需要插入的位置小于cell的数量
    //也就是这个cell需要插入已有的cell之间
    if(cell_num < num_cells)
    {
        //将i设为num cells，使用leaf_node_cell获取cell的位置
        //先指向最后一个cell的后一个位置，然后把最后一个cell搬到这里
        //然后把最后一个cell的前一个位置搬到最后一个cell的位置
        //也就是第五个搬到第六个的位置，第四个搬到第五个的位置，以此类推
        for(uint32_t i = num_cells ;i > cell_num;--i)
        {
            std::memcpy(leaf_node_cell(node, i),leaf_node_cell(node, i-1),LEAF_NODE_CELL_SIZE);
        }
    }

    //将num_cells加 1
    leaf_node_write_num_cells(node, num_cells + 1);

    //将key值写入这个cell
    leaf_node_write_key(node, cell_num, key);

    //将value值写入这个cell
    //先用leaf_node_value获取这个cell中value部分的实际地址
    //然后使用serialize序列化函数将传入的r值，没有padding的情况下写入这个value部分
    serialize_row(leaf_node_value(node, cell_num),r);
}

//Node type Read/Write
/**
 * @brief 读取一个节点的NodeType类型，返回值为NodeType枚举值
 * 
 * @param node 读取的节点
 * @return NodeType 返回NodeType::LEAF / NodeType::INTERNAL
 */
inline NodeType node_type_read(const char* node)
{
    return static_cast<NodeType>(u8_read(node + NODE_TYPE_OFFSET));
}

/**
 * @brief 写入一个节点的NodeType
 * 
 * @param node 写入的节点
 * @param type NodeType类类型
 */
inline void node_type_write(char* node,NodeType type)
{
    u8_write(node + NODE_TYPE_OFFSET, static_cast<uint8_t>(type));
}

//Leaf node find key index
/**
 * @brief 获取cell的编号，使用key值
 * @details 使用二分法查找k值进行定位，当k不存在时将返回这个k值应该在的位置
 * @param node cell所在的node
 * @param k 使用k来查找cell
 * @return uint32_t cell在所在的node中的编号
 */
inline uint32_t leaf_node_find_idx(char* node,uint32_t k)
{
    //获取cell数量
    uint32_t num_cells = leaf_node_read_num_cells(node);
    
    //最左侧的cell
    uint32_t min_index = 0;
    
    //最右侧的cell的后一个位置
    uint32_t past_last = num_cells;
    
    //使用二分法定位
    //左闭右开
    while(min_index != past_last)
    {
        //中间值位置
        uint32_t mid = (min_index + past_last) / 2;
        //中间值
        uint32_t mid_key = leaf_node_read_key(node, mid);

        //如果中间值命中，直接返回中间值位置
        if(k == mid_key)
        {
            return mid;
        }

        //如果要找的值大于中间值
        else if (k > mid_key) 
        {
            //左边界右移一位（区间缩小）
            //因为是左闭右开，mid 已经比较过了，所以从 mid + 1 开始
            min_index = mid + 1;    
        }

        //如果要找的值小于中间值
        else 
        {
            //右边界左移到 mid
            //mid 已经比较过了，但它是"第一个可能 >= k"的位置，所以要保留
            past_last = mid;
        }

        //循环直到 min_index 与 past_last 重合；
        //此时 min_index 就是 k 应该插入的位置（或 k 本身的位置）
    }

    //返回左边界，因为是左闭右开
    return min_index;
}

/**
 * @brief 打印一个leaf node的辅助函数，目前只能按顺序打印一个节点中的key值
 * 
 * @param node 打印的节点
 */
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

/**
 * @brief 判断一个节点是否为root
 * 
 * @param node 判断的节点
 * @return true 是root
 * @return false 不是root
 */
inline bool read_is_root(const char* node)
{
    return u8_read(node + IS_ROOT_OFFSET);
}

/**
 * @brief 写入一个节点的is root属性
 * 
 * @param node 写入的节点
 * @param is_root 写入的内容，true：是root ；false：不是root
 */
inline void write_is_root(char* node,bool is_root)
{
    u8_write(node + IS_ROOT_OFFSET, is_root ? 1 : 0);
}


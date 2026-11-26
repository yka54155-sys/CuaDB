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
/**
 * @brief 此文件为CuaDB项目中的表管理器
 * 
 */


 /**
  * @brief 表管理器
  * @pre 必须有Pager,Node,Row
  * @details 用此管理器将具体的数据库操作过程封装，因此使用使用时只需要操作这个类的实例即可
  */
class Table
{
    private:
    /**
     * @brief 返回指定页中的最大的key值
     * @details 逻辑：
                先进行判定，如果是内部节点，则递归调用本函数，参数为内部节点的right child，
                如果是叶节点，则返回num cells - 1位置的key值
     * @param page_num 页号
     * @return uint32_t 最大key值
     */
    [[nodiscard]]uint32_t get_max_key(uint32_t page_num)
    {
        //page num必须合法
        assert((page_num != INVALID_PAGE_NUM)&&"Table::get_max_key: page num is INVALID_PAGE_NUM");

        //获取节点指针
        char* node = pager->get_page(page_num);
        
        //获取节点类型
        NodeType type = read_node_type(node);
        
        //如果是内部节点
        if(type == NodeType::INTERNAL)
        {
            //内部节点中最大key值在right child中
            //因此直接递归调用进入right child
            uint32_t right_child = internal_node_read_right_child(node);
            return get_max_key(right_child);
        }
        //如果是叶节点
        if(type == NodeType::LEAF)
        {
            //叶节点中最大key值就是最后一个cell的key
            //先用num cells获取最后一个cell的后一个位置
            //将num cells - 1获取到最后一个cell的位置
            uint32_t num_cells = leaf_node_read_num_cells(node);
            assert((num_cells > 0) && "Table::get_max_key: leaf node is empty.");
            return leaf_node_read_key(node, num_cells - 1);
        }

        throw std::runtime_error("Table::get_max_key: Unknown node type.");
    }

    /**
     * @brief 创建新的根节点，把当前根页改造成内部节点
     * @details 步骤：
     *          ① 分配一个新页，整页拷贝根页内容，作为新根的「左孩子」（原数据左半）；
     *          ② 左孩子的 is_root 置 0、parent 指向根页；
     *          ③ 传入的 right_child（分裂产生的右半页）parent 也指向根页；
     *          ④ 根页原地重新初始化为 INTERNAL：num_keys=1、cell0=(左孩子最大键, 左孩子)、right_child=右半页。
     * @param right_child 新根的 right child 页号，即分裂产生的右半页
     * @pre 根页（root_page_num）当前的 num_cells 是分裂后的左半数据，且尚未变成内部节点
     * @post 根页类型变为 INTERNAL、is_root=1、parent=INVALID_PAGE_NUM；
     *       左孩子与右孩子的 parent 都等于 root_page_num
     * @note 根页号固定为 0，因此不需要“以哪个节点为主”的参数
     */
    void create_new_root(uint32_t right_child)
    {
        //获取未使用的页
        uint32_t new_page = pager->get_unused_page();
        char* new_node = pager->get_page(new_page);
        char* zero_node = pager->get_page(root_page_num);
        char* right_child_node = pager->get_page(right_child);
        //迁移0页的内容
       std::memcpy(new_node,zero_node,PAGE_SIZE);
        //更改新叶节点的is root、parent pointer
        write_is_root(new_node, false);
        write_parent_pointer(new_node, root_page_num);
        //更改right child的parent pointer
        write_parent_pointer(right_child_node, root_page_num);
        //将0页初始化为root node
        internal_node_initialize(zero_node,{.is_root = true,.num_keys = 1,.right_child = right_child});
        //使用get_max_key设置新root的唯一cell
        uint32_t cell_key = get_max_key(new_page);
        internal_node_write_key(zero_node, 0,cell_key);
        internal_node_write_child(zero_node,0,new_page);
    }


    public:
    uint32_t root_page_num{0};//根节点页的位置
    std::unique_ptr<Pager> pager;//页处理器实例
    


    /**
     * @brief 叶节点分裂并插入函数
     * @details 
     * @pre 调用前该页的 num_cells 必须恰好等于 @Node.hpp 中的 LEAF_NODE_MAX_CELLS（即已写满）
     * @param page_num 进行操作的页
     * @param cell_num 需要插入的位置
     * @param key 插入的key值
     * @param r 插入的Row值
     */
    void leaf_node_split_and_insert(uint32_t page_num,uint32_t cell_num,uint32_t key,const Row& r)
    {
        //分裂
        char* old_node = pager->get_page(page_num);//获取当前页地址
        uint32_t new_page_num = pager->get_unused_page();//保存新节点的页号，用于创建新root node时作为right child传入
        char* new_node = pager->get_page(new_page_num);//获取一个还没使用的页的地址

        leaf_node_initialize(new_node,{.parent_pointer = read_parent_pointer(old_node)});//新页与旧页是兄弟（同一层），父指针直接抄旧页；根分裂时这里继承 INVALID，随后由 create_new_root 改成根页号

        //迁移cell，逻辑：
        //从旧页的最后一个cell开始迁移，依次往上
        //使用LEAF_NODE_LEFT_SPLIT_COUNT作为基准
        //判断当前位置的cell放到旧页还是新页
        //如果这个cell是后半部分，即13个cell中的编号7~13
        //那么需要迁移到新节点
        //如果这个cell在前半部分，那么则位于旧页
        //计算好cell位于哪页后需要判断cell在页中的具体编号
        //使用LEAF_NODE_LEFT_SPLIT_COUNT对当前这个cell的编号取模
        //例如当前编号为13的cell，用7对其取模
        //能够得出这个cell需要放到新节点的编号为6的位置
        //12号放到新节点的编号为5的位置
        //7号放到新节点的编号为0的位置（7 不满足 i < LEFT_SPLIT_COUNT，所以进新节点）
        //需要注意的是，在此过程中需要同时检查当前这个cell的位置是不是新插入的cell需要插入的位置
        for(int32_t i = LEAF_NODE_MAX_CELLS; i >= 0;--i)
        {
            //确定当前cell的写入位置
            char* destination_node;
            //如果是前半部分
            //目标为旧节点
            if(i < LEAF_NODE_LEFT_SPLIT_COUNT)
            {
                destination_node = old_node;
            }
            //如果是后半部分
            //目标是新节点
            else
            {
                destination_node = new_node;
            }

            //提前计算迁移后cell在node中的编号
            //使用取模方法
            uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;

            //获取具体写入的目标地址，cell的地址
            char* destination = leaf_node_cell(destination_node, index_within_node);
            
            //判断当前位置是不是新插入的cell需要插入的位置
            //如果是的话则直接使用序列化在对应地址写入cell
            if(i == cell_num)
            {
                //写入key值
                leaf_node_write_key(destination_node, index_within_node,  key);
                //写入value值
                serialize_row(leaf_node_value(destination_node, index_within_node),r);
            }

            //如果当前位置在需要插入的位置的后面
            //那么需要搬移i - 1位置的cell
            //因为新cell的插入会使新cell后面的所有cell位置+1
            else if(i > cell_num)
            {
                std::memcpy(destination,leaf_node_cell(old_node,i - 1),LEAF_NODE_CELL_SIZE);
            }

            //如果当前位置在需要插入的位置的前面
            //那么直接迁移即可
            else 
            {
                std::memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
            }
        }

        //更新两个节点的num cells的值
        //注意：左半部分进的是 old_node，右半部分进的是 new_node，
        //所以 old 写 LEFT、new 写 RIGHT，写反了在 LEFT==RIGHT 时看不出来
        leaf_node_write_num_cells(old_node, LEAF_NODE_LEFT_SPLIT_COUNT);
        leaf_node_write_num_cells(new_node, LEAF_NODE_RIGHT_SPLIT_COUNT);

        //如果旧节点是根节点
        //使用create_new_root函数新建根节点并迁移已创建的节点
        if(read_is_root(old_node))
        {
            //创建新的根节点   
            create_new_root(new_page_num);
            return;
        }
        //如果旧节点不是根节点
        //更新父节点中的数据
        //[TODO]   
        else 
        {
            throw std::runtime_error("Need to implement updating parent after split\n");
            exit(1);
        }
    }

    /**
     * @brief Table的迭代器
     * @details 提供了 STL 迭代器所需的 traits，但尚未跨页；reference 是值语义（Row）
     * @warning 写入操作不支持*it，必须使用write函数写入
     */
    class iterator
    {
        public:
        /**
         * @brief 以下是标准迭代器属性定义部分，便于后续可以直接使用STL算法库
         * 
         */
        using iterator_category = std::input_iterator_tag;
        using value_type = Row;
        using difference_type = std::ptrdiff_t;
        using pointer = Row*;
        using reference = Row;
        
        /**
         * @brief 迭代器构造函数
         * 
         * @param table 迭代器指向哪个表
         * @param page_num 迭代器指向的页
         * @param cell_num 迭代器指向的具体条目
         */
        iterator(Table& table,uint32_t page_num,uint32_t cell_num) : table(table),page_num(page_num),cell_num(cell_num)
        {

        }

        /**
         * @brief 迭代器用于写入的函数
         * @details 同时写入 key（r.id）与 value（序列化后的 Row）；页满时转去分裂
         * @param r 写入的内容
         */
        void write(const Row& r)
        {
            //如果写入时发现当前节点的cell数量已经大于等于一个节点最大能够承载的数量（LEAF_NODE_MAX_CELLS）
            //需要调用leaf_node_split_and_insert函数对该节点进行分裂后插入的操作
            if(leaf_node_read_num_cells(node()) >= LEAF_NODE_MAX_CELLS)
            {
                table.leaf_node_split_and_insert(page_num,cell_num,r.id,r);
                return;
            }

            //如果当前节点数量没有大于等于最大值
            //直接进行插入操作
            leaf_node_insert(node(),  cell_num, r.id, r);
        }

        /**
         * @brief 解操作实现，*it
         * @warning 目前只能进行读操作，无法进行写入，返回的也只是个值
         * @return Row 返回的Row值
         */
        Row operator*() const
        {
            Row r{};
            //使用slot()获取cell中value的地址
            //然后用反序列化写进返回值
            deserialize_row(slot(),&r);
            return r;
        }

        /**
         * @brief 前置++，++it
         * @details 目前只在当前叶子内部前进一格
         * @warning 不跨页：走到本叶的 num_cells 之后不会自动跳到下一个叶子，
         *          而是停在一个"叶内尾后"的位置。跨页需要叶子记录 next_leaf，
         *          或者迭代器内部维护"从根到叶"的路径栈
         * @return iterator& 返回操作后的迭代器
         */
        iterator& operator++()
        {
            //单页内前进一格；跨页逻辑尚未实现（见上方 @warning）
            cell_num++;
            return *this;
        }

        /**
         * @brief 后置++，it++
         * @details 语义同前置++，只是返回前进前的副本
         * @warning 同样不跨页
         * @return iterator 返回的是++前的值，不是索引
         */
        iterator operator++(int)
        {
            //先拷贝旧值
            //迭代器前进
            //返回旧值
            iterator old = *this;
            cell_num++;
            return old;
        }

        /**
         * @brief 不等于，！=
         * 
         * @param other 等号后面的值，必须也是迭代器
         * @return true 不等于
         * @return false 等于
         */
        bool operator!=(const iterator& other) const
        {
            //只有cell位置和page页号完全相同的情况下返回false
            return this->cell_num != other.cell_num || this->page_num != other.page_num;
        }

        /**
         * @brief 等于，==
         * 
         * @param other 等号右侧迭代器
         * @return true 等于
         * @return false 不等于
         */
        bool operator==(const iterator& other) const
        {
            //只有cell位置和page页号完全相同时返回true
            return this->cell_num == other.cell_num && this->page_num == other.page_num;
        }

        /**
         * @brief 获取当前迭代器指向的cell的key值
         * @pre 迭代器必须指向一个真实存在的 cell；对 end()，或对"指向叶内
         *      num_cells（最后一个 cell 之后）"的位置调用都属于误用
         * @warning 用运行期断言 assert 检查越界；NDEBUG 构建下断言失效，
         *          越界读会读到陈旧字节而不会报错
         * @return uint32_t key值
         */
        uint32_t key() const
        {
            //使用运行期断言
            //如果迭代器指向的cell的编号超出迭代器指向的节点实际拥有的cell数量
            //直接报错（release 构建下 assert 被移除）
            assert(cell_num < leaf_node_read_num_cells(node()) && "Error , you can't use key() in end().");
            return  leaf_node_read_key(node(), cell_num); 
        }

        private:
        Table& table;//迭代器指向的表
        uint32_t page_num;//迭代器指向的页
        uint32_t cell_num;//迭代器指向的具体条目

        /**
         * @brief 获取当前迭代器指向的节点真实地址
         * 
         * @return char* 节点的地址
         */
        char* node() const
        {
            return table.pager->get_page(page_num);
        }

        /**
         * @brief 获取当前指向的cell的value地址
         * @pre 需要node()函数先获取当前节点地址
         * @return char* 迭代器指向的cell的value部分的地址
         */
        char* slot() const
        {   
            //先用node()函数获取当前节点地址
            //将节点地址和cell_num传入leaf_node_value获取具体的value部分的地址
            char* row_pos = leaf_node_value(node(),cell_num);
            return row_pos;
        }
    };

    
    /**
     * @brief Table构造函数
     * 
     * @param path 数据库文件的位置，会传给Pager处理
     */
    Table(const std::string& path)
    {
        //实例化Pager打开数据库文件
        pager = std::make_unique<Pager>(path);
        
        
        root_page_num = 0;//将根页设为第0页

        /*
        如果当前数据库的实际页数为0
        使用Pager->get_page 获取第0页地址
        */
        if(pager->num_pages == 0)
        {
            char* root = pager->get_page(0);//获取第0页地址来初始化根节点
            leaf_node_initialize(root,{.is_root = true,.parent_pointer = INVALID_PAGE_NUM,.num_cells = 0});//新建库：第0页初始化为空叶子并设为根；后两个参数就是默认值，可简写为 {.is_root = true}
        }
    }

    ~Table()
    {
        //析构时循环调用 Pager 的 flush 落盘
        //@warning 遍历范围是"文件页数"而不是"缓存里实际有哪些页"，
        //         做完 LRU 淘汰后这里需要改成按缓存帧遍历
        for(uint32_t i = 0 ;i < pager->num_pages;++i)
        {
            pager->flush(i);
        }   
    }

    /**
     * @brief 供迭代器使用的begin()，指向整张表的第一个 cell
     * @details 直接返回 (root_page_num, 0)
     * @warning 仍然假设根节点是叶子：根变成内部节点后，(root, 0) 落在内部节点的
     *          第 0 条 cell 上，不是数据行。正确做法是沿 child[0] 一路下到最左叶子，
     *          与迭代器跨页一起实现
     * @return iterator 
     */
    iterator begin()
    {
        //指向root页的0号cell
        return iterator(*this,root_page_num,0);
    }

    /**
     * @brief 供迭代器使用的end()，指向整张表最后一个 cell 之后的位置
     * @details 从根出发，内部节点反复取 right_child，直到遇到叶子；
     *          返回 (该叶子页号, num_cells)，即"最后一个 cell 的后一个位置"
     * @note 这个位置与 find() 在"k 大于全表最大值"时给出的插入位置**恰好相同**，
     *       所以 Statement 里 `pos != end()` 能正确短路掉 key()，不必加魔法值兜底。
     *       该性质依赖"没有删除 / 节点合并"：一旦实现 delete，num_cells 位置会
     *       出现在非最右叶子上，届时必须改用哨兵 INVALID_PAGE_NUM 表示 end()
     * @return iterator 
     */
    iterator end()
    {
        //从根页出发，沿 right_child 一路向下，直到遇到叶子
        //取这条链末端叶子的 num_cells 作为位置
        //因为num cells值代表数量，所以第num cells个cell的位置就是最后一个cell的后一个位置
        //举例：5个苹果，数量是5，编号为0、1、2、3、4，那么编号为‘数量’的位置就是最后一个位置的后一个

        uint32_t page = root_page_num;

        while(true)
        {
            char* node = pager->get_page(page);
            if(read_node_type(node) == NodeType::LEAF)
            {
                return iterator(*this,page,leaf_node_read_num_cells(node));
            }
            //内部节点：最大的一批 key 都在最右子树里，继续往下走
            page = internal_node_read_right_child(node);
        }
        
        
    }

    /**
     * @brief 使用key值在table中寻找对应的cell
     * @details 从根开始循环下降，对层数没有限制：
     *          - 叶子：用 leaf_node_find_idx 二分定位
     *          - 内部：用 internal_node_find_idx 定位，它返回的是"第一个 >= k"的
     *                  下标；idx == num_keys 说明 k 比该节点所有分界键都大，
     *                  走 right_child，否则走 child[idx]
     * @note 两个 find_idx 返回的都是"位置"而不是"是否命中"：返回的 cell_num
     *       可能等于叶子的 num_cells，表示"k 应插在末尾"。调用方必须自己比较
     *       key 才能判断是否命中（见 Statement::INSERT_func）
     * @todo 未命中时返回"插入位置"而不是 end()，语义上容易和迭代器终点混淆；
     *       后续可以拆成 locate() + Table::insert()，把查重和定位收进 Table
     * @param k 用于查找的key值
     * @return iterator 命中时指向该 cell，未命中时指向它应当插入的位置
     */
    iterator find(uint32_t k)
    {
        //使用迭代方法，逐层寻找cell位置
        uint32_t page_num = root_page_num;//从根开始
        while(true)
        {
            char* node = pager->get_page(page_num);
            
            //如果当前节点是leaf node，那么直接查找对应的cell位置并返回即可
            if(read_node_type(node) == NodeType::LEAF)
            {
                uint32_t cell_num = leaf_node_find_idx(node, k);
                return iterator(*this,page_num,cell_num);
            }

            //如果当前节点不是leaf
            //先用internal的find idx找到k对应的leaf node
            //如果是多层internal那么将child设为page num继续迭代逐层查找
            uint32_t cell_num = internal_node_find_idx(node, k);//找到k应该在的位置
            
            //如果找到的位置等于num keys
            //也就是大于所有的cell
            //那么应该在right child里
            //注意这里必须显式取 right_child，不能写成 read_child(node, cell_num)：
            //cell_num == num_keys 时那条 cell 越界，读到的是陈旧字节
            if(cell_num == internal_node_read_num_keys(node))
            {
                page_num = internal_node_read_right_child(node);
            }
            //如果在所有cell里定位到了k应该在的位置
            //那么下一个查询的page就是找到的这个cell对应的child页
            else
            {
                page_num = internal_node_read_child(node, cell_num);    
            }
        }
        //循环里每个分支都会 return，这里是不可达代码；
        //保留只是为了满足"非 void 函数必须有返回值"的编译检查
        throw std::runtime_error("Table::find(): Unknown Error!");

        
    }
};

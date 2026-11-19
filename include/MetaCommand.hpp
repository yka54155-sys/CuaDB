#pragma once
#include "Row.hpp"
#include "Table.hpp"
#include"std.hpp"
#include"Node.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
/**
 * @brief MetaCommand.hpp 用于定义有关CuaDB的元命令的各种函数和常量以及类
 * 
 */

 /**
  * @brief 元命令执行后的可能的结果
  * 
*/
enum class MetaResult
{
    NONE,                 //未使用：仅作默认值占位
    SUCCESS,              //命令已识别且执行成功
    UNRECOGNIZED_COMMAND, //命令表里找不到这个名字
    FAILED,               //已识别但执行失败（execute 为空）
    ERROR,                //未知错误
    EXIT                  //要求主循环退出；这是信号，不是错误
};

/**
 * @brief 
        元命令类，用于解析和执行各元命令
 * @details 
        定义元命令方法：
        1、在MetaCommand类的private里定义元命令执行函数，模板如下：  
                static MetaResult MetaXXX()
                {
                    //元命令执行内容
                    return MetaResult::SUCCESS;
                }
        2、在 MetaReflect 里登记命令名
        3、如果新增了 MetaResult 枚举值，再更新 MetaMessage
        
 */
class MetaCommand
{
    private:
    std::string Command;//元命令字符串
    
    std::optional<std::function<MetaResult(Table&)>> execute;//存放该命令对应的执行函数；命令未被识别时为空

    /**
     * @brief 元命令‘.exit’的实际执行函数
     * @warning 因为exit不好处理，因此此函数解决方法是单独定义一个MetaResult枚举值来交给调用方来执行exit操作
     * @param table 
     * @return MetaResult 应当返回EXIT 
     */
    static MetaResult MetaEXIT(Table& table)
    {
        std::cout << "Exit CuaDB." << '\n';
        // std::exit(0);
        return MetaResult::EXIT;
    }

    /**
     * @brief ‘.constant’的实际执行函数
     * @details 会打印一些自定义的常量的大小或其他内容
     * @param table 
     * @return MetaResult 返回SUCCESS
     */
    static MetaResult MetaConstant(Table& table)
    {
        std::cout << "ROW SIZE:" << ROW_TOTAL_SIZE << '\n';
        std::cout << "COMMON_NODE_HEADER_SIZE:" <<  COMMON_NODE_HEADER_SIZE << '\n';
        std::cout << "LEAF_NODE_HEADER_SIZE:" <<  LEAF_NODE_HEADER_SIZE << '\n';
        std::cout << "INTERNAL_NODE_HEADER_SIZE:" <<  INTERNAL_NODE_HEADER_SIZE << '\n';
        std::cout << "LEAF_NODE_SPACE_FOR_CELLS:" <<  LEAF_NODE_SPACE_FOR_CELLS << '\n';
        std::cout << "LEAF_NODE_CELL_SIZE:" <<  LEAF_NODE_CELL_SIZE << '\n';
        std::cout << "LEAF_NODE_MAX_CELLS:" <<  LEAF_NODE_MAX_CELLS << '\n';
        
        return MetaResult::SUCCESS;
    }

    /**
     * @brief '.btree'的实际执行函数
     * @details 打印根页的内容，内部借用了 @Node.hpp 的 leaf_node_print
     * @param table 需要打印的table
     * @return MetaResult 通常返回SUCCESS
     * @warning 只打印根页，而且固定用 leaf_node_print 解释。
     *          根变成内部节点之后（发生过分裂），打印出来的
     *          "Node cell number" 其实是 num_keys，结构是错的；
     *          多层树要改为递归打印
     */
    static MetaResult MetaBTree(Table& table)
    {
        char* node = table.pager->get_page(table.root_page_num);
        std::cout << "Tree:\n";
        leaf_node_print(node);
        return MetaResult::SUCCESS;
    }
    
    /**
    * @brief 元命令名到执行函数的映射表
    * @details 在这里（类内 private 区）声明，实际定义在文件末尾（inline）
    * @warning 新增元命令时：①private 里加 static MetaXXX ②在这张表里登记 ③若新增了 MetaResult 枚举值，还要更新 MetaMessage
    */
    static std::unordered_map<std::string, std::function<MetaResult(Table&)>> MetaReflect;
    
    public:
    
    /**
     * @brief MetaCommand类的构造函数，explicit
     * @details 只做「识别」不做「执行」：识别成功只把执行函数存进 execute，
     *          真正执行要再调用 CommandExecute。
     * @param input 需要处理的元命令字符串
     * @param result 输出参数：识别结果（SUCCESS 表示命令存在）
     * @warning 目前用整行原文查表，既不裁剪首尾空格，也不按词切分。
     *          所以 ".btree "（末尾带空格）或 " .btree" 都会被判为 UNRECOGNIZED；
     *          用文件/管道喂脚本时最容易踩到
     */
    explicit MetaCommand(std::string input,MetaResult& result) : Command(input)
    {
        result  = CommandExist() ? MetaResult::SUCCESS : MetaResult::UNRECOGNIZED_COMMAND;
    }
    
    /**
     * @brief 用于判断元命令是否存在的辅助函数，调用时同时还会初始化execute变量
     * @todo 将此函数的初始化execute功能放入构造函数内
            
     * @return true 元命令存在
     * @return false 元命令不存在
     */
    bool CommandExist()
    {
        //使用unordered_map的find方法查找该元命令是否存在定义
        auto p = MetaReflect.find(Command);
        
        //如果迭代器为end，则元命令不存在
        //此处将execute变量置空并返回false
        if(p == MetaReflect.end())
        {
            execute = std::nullopt;
            return false;
        }

        //如果元命令存在
        //将execute设为迭代器找到的函数
        //返回true
        execute = p->second;
        return true;
    }
    
    /**
     * @brief 
            元命令执行函数的调用函数
     * 
     * @param table 
     * @return MetaResult 
     */
    MetaResult CommandExecute(Table& table)
    {
        //执行前先检查execute的value是否存在
        //execute是optional类型，模板参数为元命令执行函数模板的function
        //因此相当于execute存入的是一个函数变量
        //使用has_value函数检测是否有值
        if(!execute.has_value())
        {
            return MetaResult::FAILED;
        }
        
        //使用execute.value()(table)调用之前存在execute里的函数
        //假如execute的函数为MetaEXIT
        //那么这行就相当于：
        /*
        MetaResult result;
        result = MetaEXIT(table);
        return result;
        */
        //使用这种方法可以做到定义多个相似的函数，但是调用处只需要写一遍即可
        //但是需要注意的是，使用optional和function可能会有性能问题
        return execute.value()(table);
    }

    /**
     * @brief 
            获取元命令执行结果详细内容的映射函数，当元命令更新时需要更新
     * @note 目前没有 EXIT 分支，会落到 default 返回 "Undefined behavior!"。
     *       main 里对 EXIT 不打印消息，所以暂时看不出问题
     * @param result 需要获取详细内容的MetaResult
     * @return std::string 返回的详细内容
     */
    std::string MetaMessage(MetaResult result)
    {
        std::string Message;
        std::string Return  = "Meta Command ";
        switch(result)
        {
            case MetaResult::SUCCESS:
            Message = "\' "+Command+" \'" + " Execute SUCCESS!\n";
            Return += Message;
            break;
            case MetaResult::FAILED:
            Message = "\' "+Command+" \'" + " Execute FAILED!\n";
            Return += Message;
            break;
            case MetaResult::UNRECOGNIZED_COMMAND:
            Message = "\' " + Command + " \'" + " Unrecognized Command\n";
            Return += Message;
            break;
            case MetaResult::ERROR:
             Message = "\' " + Command + " \'" + " Execute ERROR!\n";
            Return += Message;
            break;
            default:
             Message = "\' " + Command + " \'" + " Undefined behavior!\n";
            Return += Message;
            break;
        }

        return Return;
    }


};

/**
 * @brief 
        元命令字符串与实际执行函数的映射表，当元命令更新时需要更新
 * 
 */
inline std::unordered_map<std::string, std::function<MetaResult(Table&)>>MetaCommand::MetaReflect = 
{
    {".exit",MetaEXIT},
    {".constant",MetaConstant},
    {".btree",MetaBTree}

};
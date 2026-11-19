#pragma once
#include"../include/std.hpp"
#include"../include/Table.hpp"
#include <algorithm>
#include <functional>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include"Row.hpp"

//Statement执行后的可能的结果
//添加新的StatementResult需要在PrintMessage增加对应分支
enum class StatementResult
{
    NONE,//占位：目前没有任何语句会返回它，用于防止结果变量未初始化
    SUCCESS,//执行成功
    UNRECOGNIZED_COMMAND,//未定义的sql语句
    FAILED,//执行失败
    ERROR,//未知错误
    TABLE_MAX,//表满
    DUPLICATE_KEY//主键重复
};

//SQL语句类型。SQL语句更新时需要更新
enum class StatementType
{
    SELECT,
    INSERT,
    DELETE,
    UPDATE,
    NONE
};
using st = StatementType;//StatementType缩写为st

//SQL语句关键字与语句类型的映射表，新增语句时必须在这里登记
inline std::unordered_map<std::string, StatementType> StatmentToken = 
{
    {"select",st::SELECT},
    {"delete",st::DELETE},
    {"insert",st::INSERT},
    {"update",st::UPDATE}
};

/**
 * @brief 打印一行数据，格式为  id | username | email
 * 
 * @param row 要打印的那一行的指针
 */
inline void print_row(Row* row)
{
    std::cout << row->id << " | " << row->username << " | " << row->email << '\n';
}

/**
 * @brief SQL语句处理器
 * @details 
 XXX_func(各种SQL语法的实际执行函数实现) 
TokenExist(检查SQL语法是否存在) 
Execute(执行sql语句) 
PrintMessage(获取sql语句执行结果) 
推荐使用流程： 实例化Statement，将需要执行的sql语句、StatementResult索引、已经初始化的Table实例作为参数--使用Execute方法执行sql语句--检查执行结果

 */ 
 //SQL语句执行函数注册方法：
 //1.在Statement的private里写SQL语句执行函数模板如下：
 /*
 StatementResult XXX_func()
 {
    //实际执行流程
    return StatementResult::SUCCESS;
 }
 */
 //2.在StamentType和StamentToken中添加对应的SQL语句
 //3.在Execute函数里添加对应分支
class Statement
{
    private:
    std::string statement;//接收到的SQL语句
    StatementType type{st::NONE};//默认的SQL语句类型为NONE，用于防止未初始化SQL类型就执行
    Table& table;//接收已经初始化好的表
    
    //SQL语句执行函数
    /**
     * @brief SELECT 语句实际执行函数
     * @details 目前是全表扫描：用 Table 的迭代器逐行打印，不支持 WHERE 和列投影。
     * @return StatementResult 默认返回SUCCESS
     * @todo 迭代器 operator++ 还不跨页（Table::iterator::operator++ 只做 cell_num++），
     *       树分裂成多个叶子后这里会漏行或读错，等 Part12~14 补完再验证。
     */
    StatementResult SELECT_func()
    {
        //debug
        std::cout << "SELECT execute!" << '\n';

        //使用Table的自定义的迭代器实现遍历
        for(auto r : table)
        {
            print_row(&r);
        }
        
        return StatementResult::SUCCESS;
    }
    /**
     * @brief DELETE 语句的占位实现
     * @details 只打印一行 debug 信息就返回 SUCCESS，**并没有删除任何数据**。
     *          真正的删除要等 B+ 树删除（借/合并/根塌陷）做完再接进来。
     * @return StatementResult 目前恒为 SUCCESS
     * @todo 接入 Table 的删除逻辑；接入前调用方不能依赖 DELETE 真的生效
     */
    StatementResult DELETE_func()
    {
        //debug
        std::cout << "DELETE execute!" << '\n';
        return StatementResult::SUCCESS;
    }

    /**
     * @brief UPDATE 语句的占位实现
     * @details 只打印一行 debug 信息就返回 SUCCESS，**并没有修改任何数据**。
     * @return StatementResult 目前恒为 SUCCESS
     * @todo 键不变时原地改；键变了要做「删 + 插」，依赖 B+ 树删除
     */
    StatementResult UPDATE_func()
    {
        //debug
        std::cout << "UPDATE execute!" << '\n';
        return StatementResult::SUCCESS;
    }

    /**
     * @brief INSERT 语句的实际执行函数
     * @details 解析方式很原始：把整条语句按空格切成四段（insert / id / username / email）。
     *          ⚠️ 因此用户名和邮箱不能含空格，也不能带引号；
     *          ⚠️ row.username 只有 33 字节、row.email 只有 256 字节，超长输入会写坏栈（尚未做长度检查）。
     * @return StatementResult 可能返回 SUCCESS、FAILED、DUPLICATE_KEY
     * @note 枚举里的 TABLE_MAX 目前永远返回不到：页缓存越界没有检查，第 101 页会直接写坏内存。
     */
    StatementResult INSERT_func()
    {
        
        //debug
        std::cout << "INSERT execute!" << '\n';
        std::istringstream iis(statement);
        std::string token;
        Row row{};//初始化一个Row对象，{}用于控制初始化成员大小固定

        //解析sql语句，目前只是将sql语句按空格分割并赋值
        if(!(iis >> token >> row.id >> row.username >> row.email))
        {
            return StatementResult::FAILED;
        }
       try 
        {
            //使用自定义的Table迭代器来获取row
            //使用find方法在table里寻找新增的这个row的id
            Table::iterator pos = table.find(row.id);

            //如果返回的迭代器位置不为end并且迭代器的key值等于这个新增row的id
            //也就是迭代器发现这个id已经存在，那么返回DUPLICATE_KEY
            if(pos != table.end() && pos.key() == row.id)
            {
                return StatementResult::DUPLICATE_KEY;
            }

            //如果id不存在，也就是新id
            //使用迭代器的write方法写入row
            pos.write(row);
        } 
        catch (const std::runtime_error& e) 
        {
            std::cout << e.what() << '\n'; 
            return StatementResult::FAILED;
        }
        return StatementResult::SUCCESS;
    }

    /**
     * @brief 检查是否存在该Token，也就是是否存在对应的SQL语法
     * 
     * @param token 需要检查的内容
     * @return StatementResult 不存在会返回UNRECOGNIZED_COMMAND
     */
    StatementResult TokenExist(const std::string& token)
    {
        //使用unordered_map的find在已定义的StatementToken里寻找有没有这个token
        auto p = StatmentToken.find(token);

        //如果迭代器为end说明这是没有定义的token
        if(p == StatmentToken.end())
        {
            return StatementResult::UNRECOGNIZED_COMMAND;
        }
        
        return StatementResult::SUCCESS;
    }
    public:
    /**
     * @brief Statement构造函数
     * @details 构造时就会检查语句关键字：认识的存进 type，不认识的直接抛异常，
     *          所以“调用 Execute 前语句是否认得”这一关在构造阶段就过了。
     * @param s 需要执行的SQL语句
     * @param result 输出参数：关键字检查结果（SUCCESS 表示已识别，其余表示未识别）
     * @param table 初始化好的Table，从这里获取实际数据库和缓存内容
     * @throw std::runtime_error 语句关键字无法识别时抛出（消息来自 PrintMessage）
     */
    Statement(std::string s,StatementResult& result , Table& table) : statement(s) , table(table)
    {
        //处理输入的sql语句
        std::istringstream iss(s);
        std::string token;
        iss >> token;

        //辅助 lambda：把字符串整体转成小写
        //这样 StatmentToken 里只登记小写关键字就够了，不用为大写再写一遍
        auto toLower = [](std::string& s)
        {
            std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c)
            {
                return static_cast<char>(tolower(c));
            });
        };
        toLower(token);//将token小写，便于StatementToken的定义和使用，这样就不用为大写单独定义一遍了

        //检查sql语法是否存在，不存在则抛出异常
        //存在则将sql语法类型存进type变量
        result = TokenExist(token);
        if(result == StatementResult::SUCCESS)
        {
            type = StatmentToken[token];
        }
        else
        {
            throw std::runtime_error(PrintMessage(result));
        }
    }
    
    /**
     * @brief 对应的sql语法调用处，使用switch做分支判断，SQL语法更新时需要更新这里
     * 
     * @return StatementResult 返回的执行结果 
     */
    [[nodiscard]] StatementResult Execute()
    {
        switch (type) 
        {
            case st::SELECT:
                return SELECT_func();
            case st::INSERT:
                return INSERT_func();
            case st::DELETE:
                return DELETE_func();
            case st::UPDATE:
                return UPDATE_func();
            default:
                return StatementResult::ERROR;
        }
    }

    /**
     * @brief 用于打印StatementResult具体对应信息的函数 
     当StatementResult更新时需要更新该函数内容
     * 
     * @param result 需要打印的StatementResult
     * @return std::string 对应的信息内容字符串
     */
    [[nodiscard]]std::string PrintMessage(StatementResult result)
    {
        std::string Message;
        std::string Return  = "Statement ";
        switch(result)
        {
            case StatementResult::SUCCESS:
            Message = "\' "+statement+" \'" + " Execute SUCCESS!\n";
            Return += Message;
            break;
            case StatementResult::FAILED:
            Message = "\' "+statement+" \'" + " Execute FAILED!\n";
            Return += Message;
            break;
            case StatementResult::UNRECOGNIZED_COMMAND:
            Message = "\' " + statement + " \'" + " Unrecognized Statement\n";
            Return += Message;
            break;
            case StatementResult::ERROR:
             Message = "\' " + statement + " \'" + " Execute ERROR!\n";
            Return += Message;
            break;
            case StatementResult::TABLE_MAX:
             Message = "\' " + statement + " \'" + " Table is full!\n";
             Return += Message;
             break;
             case StatementResult::DUPLICATE_KEY:
             Message = "\' " + statement + " \'" + " The key is exist!\n";
             Return += Message;
             break;
            default:
             Message = "\' " + statement + " \'" + " Undefined behavior!\n";
            Return += Message;
            break;
        }

        return Return;
    }
};


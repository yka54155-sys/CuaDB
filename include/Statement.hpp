#pragma once
#include"../include/std.hpp"
#include"../include/Table.hpp"
#include <algorithm>
#include <functional>
#include <istream>
#include <sstream>
#include <unordered_map>
#include"Row.hpp"
enum class StatementResult
{
    NONE,
    SUCCESS,
    UNRECOGNIZED_COMMAND,
    FAILED,
    ERROR,
    TABLE_MAX
};
enum class StatementType
{
    SELECT,
    INSERT,
    DELETE,
    UPDATE,
    NONE
};
using st = StatementType;
inline std::unordered_map<std::string, StatementType> StatmentToken = 
{
    {"select",st::SELECT},
    {"delete",st::DELETE},
    {"insert",st::INSERT},
    {"update",st::UPDATE}
};
inline void print_row(Row* row)
{
    std::cout << row->id << " | " << row->username << " | " << row->email << '\n';
}
class Statment
{
    private:
    std::string statement;//接收的SQL语句
    StatementType type{st::NONE};//默认的SQL语句类型为NONE，用于防止未初始化SQL类型就执行
    Table& table;//接收已经设置好的表
    
    //SQL语句执行函数
    StatementResult SELECT_func()
    {
        //debug
        std::cout << "SELECT execute!" << '\n';
        for(auto r : table)
        {
            print_row(&r);
        }
        
        return StatementResult::SUCCESS;
    }
    StatementResult DELETE_func()
    {
        //debug
        std::cout << "DELETE execute!" << '\n';
        return StatementResult::SUCCESS;
    }
    StatementResult UPDATE_func()
    {
        //debug
        std::cout << "UPDATE execute!" << '\n';
        return StatementResult::SUCCESS;
    }
    StatementResult INSERT_func()
    {
        if(table.num_rows >= TABLE_MAX_ROWS)
        {
            return StatementResult::TABLE_MAX;
        }
        //debug
        std::cout << "INSERT execute!" << '\n';
        std::istringstream iis(statement);
        std::string token;
        Row row{};
        if(!(iis >> token >> row.id >> row.username >> row.email))
        {
            return StatementResult::FAILED;
        }
       
        table.end().write(row);

        return StatementResult::SUCCESS;
    }
    StatementResult TokenExist(const std::string& token)
    {
        auto p = StatmentToken.find(token);
        if(p == StatmentToken.end())
        {
            return StatementResult::UNRECOGNIZED_COMMAND;
        }
        
        return StatementResult::SUCCESS;
    }
    public:
    Statment(std::string s,StatementResult& result , Table& table) : statement(s) , table(table)
    {
        std::istringstream iss(s);
        std::string token;
        iss >> token;
        auto toLower = [](std::string& s)
        {
            std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c)
            {
                return static_cast<char>(tolower(c));
            });
        };
        toLower(token);
        result = TokenExist(token);
        if(result == StatementResult::SUCCESS)
        {
            type = StatmentToken[token];
        }
    }
    
    StatementResult Execute()
    {
        switch (type) 
        {
            case st::SELECT:
                return SELECT_func();
                break;
            case st::INSERT:
            return INSERT_func();break;
            case st::DELETE:
            return DELETE_func();break;
            case st::UPDATE:
            return UPDATE_func();break;
            default:
            return StatementResult::ERROR;break;
        }

        return StatementResult::ERROR;
    }

    std::string PrintMessage(StatementResult result)
    {
        std::string Message;
        std::string Return  = "Statement ";
        switch(result)
        {
            case StatementResult::SUCCESS:
            Message = "\' "+statement+" \'" + " Execute SECCESS!\n";
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
             Message = "\' " + statement + " \'" + " Table is full！\n";
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


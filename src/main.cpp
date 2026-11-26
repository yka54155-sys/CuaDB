#include"../include/std.hpp"
#include"../include/MetaCommand.hpp"
#include"../include/Statement.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>



int main(int argc,char* argv[])
{
    
    std::string input;
    if(argc < 2)
    {
        std::cerr << "usage:./CuaDB <DB file path>\n";
        return 1;
    }
    Table table(argv[1]);//初始化Table，设置页、表的大小
    auto print_getline = [&]()
    {
        std::cout << "CuaDB > ";
        return std::getline(std::cin,input).good();
    };//打印提示符的同时接收输入,用于解决EOF死循环问题
    while(print_getline())//读取命令行输入内容，处理EOF问题
    {
        std::istringstream iss(input);//处理最初的输入
        std::string keyword;
        iss >> keyword;
        if(keyword[0] == '.')//判断是否为元命令
        {
            MetaResult result;
            MetaCommand meta(input,result);//创建元命令处理对象
            if(result == MetaResult::UNRECOGNIZED_COMMAND)//如果创建结果为未知的元命令则重新读取命令行输入
            {
                std::cout << meta.MetaMessage(result);
                continue;
            }
            result = meta.CommandExecute(table);//执行元命令
            if(result == MetaResult::EXIT) break;
            // //debug
            // std::cout << meta.MetaMessage(result);//打印执行结果

        }
        else //执行SQL语句
        {
            
            try 
            {
                StatementResult result;//接收SQL执行结果
                Statement s(input,result,table);//处理SQL语句
                if(result != StatementResult::SUCCESS)//判断SQL语句执行结果
                {
                    std::cout << s.PrintMessage(result);//打印错误信息
                    continue;
                }
                result = s.Execute();
                // //debug
                std::cout << s.PrintMessage(result);//测试用
            } catch (const std::exception& e) 
            {
                std::cerr << e.what();
                continue;
            }
            
            
        }
        
    }
    

    return 0;
}

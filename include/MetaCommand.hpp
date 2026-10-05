#pragma once
#include"std.hpp"
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

enum class MetaResult
{
    NONE,
    SUCCESS,
    UNRECOGNIZED_COMMAND,
    FAILED,
    ERROR,
    EXIT
};

class MetaCommand
{
    private:
    std::string Command;
    
    std::optional<std::function<MetaResult()>> execute;

    static MetaResult MetaEXIT()
    {
        std::cout << "Exit CuaDB." << '\n';
        // std::exit(0);
        return MetaResult::EXIT;
    }
    /*
    static MetaResult MetaXXX()
    {
        
        return MetaResult::SUCCESS;
    }
    */

    static std::unordered_map<std::string, std::function<MetaResult()>> MetaReflect;
    
    public:
    
    explicit MetaCommand(std::string input,MetaResult& result) : Command(input)
    {
        result  = CommandExist() ? MetaResult::SUCCESS : MetaResult::UNRECOGNIZED_COMMAND;
    } 
    bool CommandExist()
    {
        auto p = MetaReflect.find(Command);
        if(p == MetaReflect.end())
        {
            execute = std::nullopt;
            return false;
        }
        execute = p->second;
        return true;
    }
    
    MetaResult CommandExecute()
    {
        if(!execute.has_value())
        {
            return MetaResult::FAILED;
        }
        
        return execute.value()();
    }

    std::string MetaMessage(MetaResult result)
    {
        std::string Message;
        std::string Return  = "Meta Command ";
        switch(result)
        {
            case MetaResult::SUCCESS:
            Message = "\' "+Command+" \'" + " Execute SECCESS!\n";
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
inline std::unordered_map<std::string, std::function<MetaResult()>>MetaCommand::MetaReflect = 
{
    {".exit",MetaEXIT},
};
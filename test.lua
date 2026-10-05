command  = {"insert 1 zaa @dasda","select",".exit"};
local function run_test(command)
    file = io.open("bin/input.tmp","w")
    file:write(table.concat(command,'\n')..'\n');
    file:close();
    file = io.open("bin/output.tmp","w")
    file:close();
    os.execute("cd bin && ./CuaDB test.db < input.tmp > output.tmp");
    local out = {}
    for line in io.lines("bin/output.tmp") do
        out[#out + 1]  = line
    end
    return out
end
local function expect_lines(actual,expect)
    assert(#actual == #expect,("预期%d行,实际%d行"):format(#actual,#expect))

end
local function print_out(out)
    for i = 1 ,  #out do
        print(out[i])
    end
end
local function full_test()
    local script = {}
    for i = 1, 1401 do
        script[#script + 1] = ("insert %d user%d person%d@example.com"):format(i, i, i)
    end
    script[#script + 1] = ".exit"
    local r = run_test(script)
    return r
end
local out = full_test()
print_out(out)

os.execute("rm -f bin/input.tmp bin/output.tmp")
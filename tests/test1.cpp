#include "CurrentThread.h"
#include <string>
#include <iostream>

using namespace reactor;

void func2()
{
    // 获取调用栈 (参数 true 通常代表开启 demangle 去混淆)
    std::string s = CurrentThread::stackTrace(true); 
    
    // 打印并使用 std::endl 强制刷新缓冲区！
    std::cout << "=== Current Stack Trace ===\n" << s << std::endl; 
    
    // 如果你要测试崩溃，放在刷新缓冲区之后
    int* p = nullptr;
    *p = 1; 
}

void func1() { func2(); }

int main() { func1(); }
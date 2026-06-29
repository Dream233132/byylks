// ============================================================
// main.cpp —— 小型编译程序命令行入口
// 负责解析命令行参数，调用编译器核心，输出各阶段结果。
//
// 用法：
//   mini_compiler <source> [-m output.med] [-a output.asm]
//                          [-t output.tok] [--no-asm]
// ============================================================

#include "Compiler.h"

#include <filesystem>  // filesystem::path，用于自动推导输出文件路径
#include <fstream>     // ofstream，用于写入文法信息文件
#include <iostream>    // cout、cerr，用于控制台输出
#include <string>      // string
#ifdef _WIN32
#include <windows.h>   // SetConsoleOutputCP / SetConsoleCP，用于 UTF-8 控制台支持
#endif

using namespace std;
namespace fs = filesystem;

namespace {

// printUsage：打印命令行用法说明。
// 当参数缺失或用户请求帮助时调用。
void printUsage() {
    cout << "用法: mini_compiler <source> [-m output.med] [-a output.asm]\n"
         << "                            [-t output.tok] [--no-asm]\n"
         << "                            [--grammar output.txt]\n"
         << "  --grammar  输出文法的 FIRST/FOLLOW 集和 LL(1) 预测分析表\n";
}

// defaultWithExtension：根据源文件路径自动生成默认输出路径。
// 只替换扩展名，保留目录和文件名主干。
// 例如：examples/pas.dat -> examples/pas.med
string defaultWithExtension(const string &source, const string &extension) {
    fs::path path(source);
    path.replace_extension(extension);
    return path.string();
}

} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
    // 将控制台输入输出代码页设置为 UTF-8，避免源码中中文字符串显示乱码。
    // Windows 默认代码页为 GBK（936），而源文件保存为 UTF-8，因此需要在启动时切换。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 参数不足时打印用法并退出
    if (argc < 2) {
        printUsage();
        return 1;
    }

    string source;      // 源程序文件路径（必须提供）
    string medium;      // 四元式输出路径（默认与源文件同目录，扩展名改为 .med）
    string assembly;    // 汇编输出路径（默认与源文件同目录，扩展名改为 .asm）
    string tokens;      // 词法二元式输出路径（可选，不指定则不输出）
    string grammarOut;  // 文法信息输出路径（可选，不指定则不输出）
    bool noAsm = false; // --no-asm 标志：只做必做的四元式生成，跳过汇编生成

    // 手写参数解析：
    //   第一个非选项参数作为源文件路径；
    //   -m/-a/-t 后必须紧跟输出路径；
    //   --no-asm 禁用汇编生成；
    //   -h/--help 打印用法。
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        }
        if (arg == "-m" || arg == "--medium") {
            if (++i >= argc) {
                cerr << "-m/--medium 缺少输出路径\n";
                return 1;
            }
            medium = argv[i];
        } else if (arg == "-a" || arg == "--asm") {
            if (++i >= argc) {
                cerr << "-a/--asm 缺少输出路径\n";
                return 1;
            }
            assembly = argv[i];
        } else if (arg == "-t" || arg == "--tokens") {
            if (++i >= argc) {
                cerr << "-t/--tokens 缺少输出路径\n";
                return 1;
            }
            tokens = argv[i];
        } else if (arg == "--no-asm") {
            noAsm = true;
        } else if (arg == "--grammar") {
            // --grammar 后跟输出文件路径；若省略路径则直接打印到控制台
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                grammarOut = argv[++i];
            } else {
                grammarOut = "__stdout__"; // 特殊标记：输出到控制台
            }
        } else if (source.empty()) {
            source = arg;  // 第一个非选项参数作为源文件路径
        } else {
            cerr << "未知参数: " << arg << "\n";
            return 1;
        }
    }

    // 源文件路径不能为空
    if (source.empty()) {
        printUsage();
        return 1;
    }

    // 未显式指定输出路径时，自动根据源文件路径推导默认值
    if (medium.empty()) {
        medium = defaultWithExtension(source, ".med");
    }
    if (!noAsm && assembly.empty()) {
        assembly = defaultWithExtension(source, ".asm");
    }

    try {
        // 调用编译器核心：词法分析 -> 语法分析 -> 四元式生成 -> 汇编生成（可选）
        const mini::CompileResult result = mini::compileFile(
            source,
            medium,
            noAsm ? "" : assembly,  // --no-asm 时传空字符串，跳过汇编写文件
            tokens
        );

        // 输出成功信息
        cout << "四元式生成成功: " << medium << "\n";
        cout << "四元式条数: " << result.quads.size() << "\n";
        if (!noAsm) {
            cout << "汇编生成成功: " << assembly << "\n";
        }
        if (!tokens.empty()) {
            cout << "词法结果生成成功: " << tokens << "\n";
        }

        // 可选：输出文法的 FIRST/FOLLOW 集和 LL(1) 预测分析表
        if (!grammarOut.empty()) {
            // 构造 GrammarInfo 对象（构造时自动完成计算）
            mini::GrammarInfo gi;
            const string content = gi.formatAll();
            if (grammarOut == "__stdout__") {
                // 直接打印到控制台
                cout << "\n" << content;
            } else {
                // 写入文件
                ofstream fout(grammarOut, ios::binary);
                if (!fout) {
                    cerr << "无法写入文法信息文件: " << grammarOut << "\n";
                    return 1;
                }
                fout << content;
                fout.close();
                cout << "文法信息已写入: " << grammarOut << "\n";
            }
        }
    } catch (const exception &ex) {
        // 编译失败：输出错误信息并以非 0 状态码退出
        cerr << "编译失败: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

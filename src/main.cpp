#include "Compiler.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

// 打印命令行用法。程序支持默认输出，也支持用户指定 .med/.asm/.tok 路径。
void printUsage() {
    std::cout << "用法: mini_compiler <source> [-m output.med] [-a output.asm] [-t output.tok] [--no-asm]\n";
}

// 未显式指定输出文件时，沿用源文件路径，只替换扩展名。
// 例如 examples/pas.dat -> examples/pas.med。
std::string defaultWithExtension(const std::string &source, const std::string &extension) {
    fs::path path(source);
    path.replace_extension(extension);
    return path.string();
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string source;
    std::string medium;
    std::string assembly;
    std::string tokens;
    bool noAsm = false;

    // 简单手写参数解析：
    // 第一个非选项参数作为源程序路径；-m/-a/-t 后必须跟输出路径；
    // --no-asm 表示只完成必做的四元式生成。
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        }
        if (arg == "-m" || arg == "--medium") {
            if (++i >= argc) {
                std::cerr << "-m/--medium 缺少输出路径\n";
                return 1;
            }
            medium = argv[i];
        } else if (arg == "-a" || arg == "--asm") {
            if (++i >= argc) {
                std::cerr << "-a/--asm 缺少输出路径\n";
                return 1;
            }
            assembly = argv[i];
        } else if (arg == "-t" || arg == "--tokens") {
            if (++i >= argc) {
                std::cerr << "-t/--tokens 缺少输出路径\n";
                return 1;
            }
            tokens = argv[i];
        } else if (arg == "--no-asm") {
            noAsm = true;
        } else if (source.empty()) {
            source = arg;
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            return 1;
        }
    }

    if (source.empty()) {
        printUsage();
        return 1;
    }

    // 根据用户输入补齐默认输出路径。
    if (medium.empty()) {
        medium = defaultWithExtension(source, ".med");
    }
    if (!noAsm && assembly.empty()) {
        assembly = defaultWithExtension(source, ".asm");
    }

    try {
        // compileFile 内部会完成词法分析、语法分析、四元式生成和可选汇编输出。
        const mini::CompileResult result = mini::compileFile(source, medium, noAsm ? "" : assembly, tokens);
        std::cout << "四元式生成成功: " << medium << "\n";
        std::cout << "四元式条数: " << result.quads.size() << "\n";
        if (!noAsm) {
            std::cout << "汇编生成成功: " << assembly << "\n";
        }
        if (!tokens.empty()) {
            std::cout << "词法结果生成成功: " << tokens << "\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "编译失败: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

#pragma once

// ============================================================
// AsmGenerator.h —— 汇编代码生成器类声明（选做部分）
// 将四元式序列翻译为 8086/8088 风格汇编文本。
// ============================================================

#include "Common.h"

namespace mini {

// ------------------------------------------------------------
// AsmGenerator —— 汇编代码生成器（选做部分）
// 将四元式序列翻译为 8086/8088 风格汇编文本。
// 采用 AX 作为主运算寄存器，乘法额外借用 BX。
// 输出格式：.MODEL SMALL / .STACK / .DATA / .CODE / END START
// ------------------------------------------------------------
class AsmGenerator {
public:
    // 构造函数：传入四元式列表和符号表
    AsmGenerator(std::vector<Quad> quads, std::vector<std::string> symbols);

    // 生成完整的汇编文本字符串
    std::string generate() const;

private:
    std::vector<Quad> quads_;          // 四元式序列
    std::vector<std::string> symbols_; // 符号表（用于 .DATA 段变量定义）

    // 将单条四元式翻译为一组汇编指令行
    std::vector<std::string> quadToAsm(const Quad &quad) const;
    // 将二元运算四元式（+、*、and、or）翻译为汇编指令序列
    std::vector<std::string> binaryToAsm(const Quad &quad) const;
    // 将操作数转换为汇编操作数格式（常量直接输出，变量转大写）
    std::string operand(const std::string &value) const;
    // 将跳转目标地址转换为汇编标签（程序出口用 LEND）
    std::string label(const std::string &address) const;
    // 将变量名转为大写（汇编符号惯例）
    static std::string asmSymbol(std::string value);
};

} // namespace mini

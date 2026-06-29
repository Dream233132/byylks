// ============================================================
// AsmGenerator.cpp —— 汇编代码生成器实现（选做部分）
// 将四元式序列翻译为 8086/8088 风格汇编文本。
// ============================================================

#include "AsmGenerator.h"

#include <algorithm>  // transform
#include <cctype>     // isdigit、toupper
#include <sstream>    // ostringstream

using namespace std;

namespace mini {
namespace {

// 判断字符串是否为十进制整数常量（立即数），用于区分变量和常量。
bool isNumber(const string &value) {
    return !value.empty() &&
           all_of(value.begin(), value.end(), [](unsigned char ch) { return isdigit(ch); });
}

// 返回四元式的格式化字符串，用于生成"不支持"注释行。
string formatQuadLocal(const Quad &quad) {
    ostringstream out;
    out << quad.address << " (" << quad.op << "," << quad.arg1
        << "," << quad.arg2 << "," << quad.result << ")";
    return out.str();
}

} // namespace

// 构造函数：将四元式序列和符号表移入成员变量，避免拷贝。
AsmGenerator::AsmGenerator(vector<Quad> quads, vector<string> symbols)
    : quads_(move(quads)), symbols_(move(symbols)) {}

// generate：将全部四元式翻译为完整的 8086/8088 汇编文本。
// 输出结构：
//   .MODEL SMALL / .STACK 100H
//   .DATA  —— 每个变量/临时变量声明为 DW 0（初值为 0）
//   .CODE START: 初始化数据段寄存器 DS
//   L<addr>: 每条四元式对应一个标签加若干指令
//   LEND:  程序出口，通过 INT 21H（功能 4CH）正常退出
string AsmGenerator::generate() const {
    ostringstream out;
    out << "; 8086/8088-style assembly generated from quadruples\n";
    out << ".MODEL SMALL\n.STACK 100H\n.DATA\n";
    // 在 .DATA 段为每个变量和临时变量分配一个字（DW 0）
    for (const auto &symbol : symbols_) {
        out << asmSymbol(symbol) << " DW 0\n";
    }
    out << ".CODE\nSTART:\n    MOV AX, @DATA\n    MOV DS, AX\n";
    // 为每条四元式生成对应的标签和汇编指令
    for (const Quad &quad : quads_) {
        out << "L" << quad.address << ":\n";
        for (const auto &line : quadToAsm(quad)) {
            out << line << "\n";
        }
    }
    out << "LEND:\n    MOV AH, 4CH\n    INT 21H\nEND START\n";
    return out.str();
}

// quadToAsm：将单条四元式翻译为一组汇编指令行。
// 分派规则：
//   +、*、and、or -> binaryToAsm（二元运算）
//   :=           -> MOV AX, src; MOV dst, AX（赋值）
//   j            -> JMP label（无条件跳转）
//   j<rel>       -> MOV AX, arg1; CMP AX, arg2; J<xx> label（条件跳转）
// 关系运算符到条件跳转指令：< JL，<= JLE，<> JNE，> JG，>= JGE，= JE
vector<string> AsmGenerator::quadToAsm(const Quad &quad) const {
    if (quad.op == "+" || quad.op == "*" || quad.op == "and" || quad.op == "or") {
        return binaryToAsm(quad);
    }
    if (quad.op == ":=") {
        // 赋值：先把源操作数装入 AX，再写回目标变量
        return {"    MOV AX, " + operand(quad.arg1),
                "    MOV " + operand(quad.result) + ", AX"};
    }
    if (quad.op == "j") {
        // 无条件跳转
        return {"    JMP " + label(quad.result)};
    }
    if (quad.op.size() > 1 && quad.op[0] == 'j') {
        // 条件跳转：(j<rel>, A, B, addr) -> MOV AX,A; CMP AX,B; J<xx> addr
        const string rel = quad.op.substr(1); // 提取关系运算符部分
        string instr;
        if      (rel == "<")  instr = "JL";
        else if (rel == "<=") instr = "JLE";
        else if (rel == "<>") instr = "JNE";
        else if (rel == ">")  instr = "JG";
        else if (rel == ">=") instr = "JGE";
        else if (rel == "=")  instr = "JE";
        else return {"    ; unsupported quad: " + formatQuadLocal(quad)};
        return {"    MOV AX, " + operand(quad.arg1),
                "    CMP AX, " + operand(quad.arg2),
                "    " + instr + " " + label(quad.result)};
    }
    // 未知操作符：输出注释行，不中断汇编文件生成
    return {"    ; unsupported quad: " + formatQuadLocal(quad)};
}

// binaryToAsm：将二元运算四元式翻译为汇编指令序列。
// 统一模式：MOV AX, arg1; <op> AX, arg2; MOV result, AX。
// 乘法使用 IMUL BX（需要先把 arg2 装入 BX），加法用 ADD，逻辑运算用 AND/OR。
vector<string> AsmGenerator::binaryToAsm(const Quad &quad) const {
    vector<string> lines = {"    MOV AX, " + operand(quad.arg1)}; // 左操作数 -> AX
    if (quad.op == "+") {
        lines.push_back("    ADD AX, " + operand(quad.arg2));
    } else if (quad.op == "*") {
        lines.push_back("    MOV BX, " + operand(quad.arg2)); // IMUL 需要寄存器操作数
        lines.push_back("    IMUL BX");                        // 有符号乘法，结果在 AX
    } else if (quad.op == "and") {
        lines.push_back("    AND AX, " + operand(quad.arg2));
    } else if (quad.op == "or") {
        lines.push_back("    OR AX, " + operand(quad.arg2));
    }
    lines.push_back("    MOV " + operand(quad.result) + ", AX"); // 结果写回变量
    return lines;
}

// operand：将操作数转换为汇编操作数格式。
// 纯数字（常量）直接作为立即数；变量和临时变量名转大写匹配 .DATA 段定义。
string AsmGenerator::operand(const string &value) const {
    return isNumber(value) ? value : asmSymbol(value);
}

// label：将四元式地址字符串转换为汇编标签。
// 若目标是最后一条四元式地址 +1（程序出口），则返回 "LEND"，
// 否则返回 "L<address>"，与 generate() 中的标签格式对应。
string AsmGenerator::label(const string &address) const {
    if (!quads_.empty() && address == to_string(quads_.back().address + 1)) {
        return "LEND";
    }
    return "L" + address;
}

// asmSymbol：将变量名转为全大写，与 .DATA 段中的标识符风格一致。
string AsmGenerator::asmSymbol(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(toupper(ch));
    });
    return value;
}

} // namespace mini

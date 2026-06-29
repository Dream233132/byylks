#pragma once

// ============================================================
// Common.h —— 公共数据类型定义
// 被 Lexer.h、Parser.h、AsmGenerator.h 共同包含。
// 定义了编译过程中各阶段共享的基础结构：
//   CompileError、TokenKind、Token、Quad、CompileResult
// ============================================================

#include <stdexcept>  // std::runtime_error
#include <string>     // std::string
#include <vector>     // std::vector

namespace mini {

// ------------------------------------------------------------
// CompileError —— 统一的编译错误类型
// 词法错误、语法错误、文件读写错误都统一抛出该异常，
// main() 捕获后打印"编译失败"并以非 0 状态码退出。
// ------------------------------------------------------------
struct CompileError : public std::runtime_error {
    explicit CompileError(const std::string &message) : std::runtime_error(message) {}
};

// ------------------------------------------------------------
// TokenKind —— 词法单元的种别枚举
// 将所有终结符细分为具体枚举值，使语法分析代码直观易读。
// 对应任务书表1中各符号的种别编码（用枚举代替数字）。
// ------------------------------------------------------------
enum class TokenKind {
    Identifier,  // 变量名，如 a、b、T1（种别 ident = 56）
    Number,      // 整型常量，如 1、2（种别 intconst = 57）
    Boolean,     // 布尔常量 true / false
    Keyword,     // 保留字：if/then/else/while/do/begin/end
    Assign,      // 赋值符号 :=（种别 becomes = 38）
    Relop,       // 关系运算符 < <= <> > >= =（种别 rop = 42）
    Plus,        // 算术加法运算符 +（种别 plus = 34）
    Star,        // 算术乘法运算符 *（种别 times = 36）
    Semicolon,   // 语句分隔符 ;（种别 semicolon = 8）
    LParen,      // 左括号 (（种别 lparent = 48）
    RParen,      // 右括号 )（种别 rparent = 49）
    And,         // 逻辑与 and（种别 op_and = 39）
    Or,          // 逻辑或 or（种别 op_or = 40）
    Not,         // 逻辑非 not（种别 op_not = 41）
    Hash,        // 程序结束标志 #（种别 jinghao = 10）
    Tilde,       // 程序结束标志 ~，与 # 配合使用
    End          // 输入结束哨兵，不对应源程序中的字符
};

// ------------------------------------------------------------
// Token —— 一个词法单元（二元式）
// 对应任务书规定的输出格式：(单词种别, 单词自身的值)
// line/column 记录该单词在源程序中的行列位置，用于出错定位。
// ------------------------------------------------------------
struct Token {
    TokenKind kind;    // 单词种别
    std::string value; // 单词自身的值（关键字统一转小写）
    int line;          // 所在行号（从 1 开始）
    int column;        // 所在列号（从 1 开始）
};

// ------------------------------------------------------------
// Quad —— 四元式结构
// 对应任务书中的四元式格式：(op, arg1, arg2, result)
// address 从 100 开始，每条四元式地址递增 1。
// 跳转指令的 result 字段保存跳转目标地址（字符串形式）；
// 赋值指令的 result 字段保存目标变量名。
// ------------------------------------------------------------
struct Quad {
    int address;        // 四元式地址（逻辑行号，从 100 开始）
    std::string op;     // 操作符，如 :=、+、*、j、j<、j>= 等
    std::string arg1;   // 第一操作数（变量名、常量或临时变量）
    std::string arg2;   // 第二操作数（同上，赋值时为空）
    std::string result; // 结果（目标变量名或跳转目标地址）
};

// ------------------------------------------------------------
// CompileResult —— 一次完整编译的所有产出
// 供命令行入口、测试脚本和文档生成工具统一复用。
// ------------------------------------------------------------
struct CompileResult {
    std::vector<Token> tokens;        // 词法分析产生的 Token 序列
    std::vector<Quad> quads;          // 语法制导翻译产生的四元式序列
    std::vector<std::string> symbols; // 符号表（变量和临时变量按出现顺序）
    std::string assembly;             // 汇编生成器产生的汇编文本（选做）
};

} // namespace mini

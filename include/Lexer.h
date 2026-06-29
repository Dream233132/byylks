#pragma once

// ============================================================
// Lexer.h —— 词法分析器类声明
// ============================================================

#include "Common.h"

namespace mini {

// ------------------------------------------------------------
// Lexer —— 词法分析器
// 职责：
//   1. 接收完整的源程序字符串；
//   2. 跳过空白符（空格、制表符、换行）和 C 风格块注释；
//   3. 识别标识符、整型常量、保留字、运算符、分隔符等；
//   4. 输出 Token 序列，末尾追加 End 作为语法分析器的结束标记。
// ------------------------------------------------------------
class Lexer {
public:
    // 构造函数：传入完整源程序字符串（以二进制读入，保留原始换行）
    explicit Lexer(std::string source);

    // 执行词法分析，返回识别出的全部 Token（含末尾 End 哨兵）
    std::vector<Token> tokenize();

private:
    std::string source_;        // 源程序字符串
    std::size_t pos_ = 0;       // 当前扫描位置（字节下标）
    int line_ = 1;              // 当前行号（从 1 开始）
    int column_ = 1;            // 当前列号（从 1 开始）
    std::vector<Token> tokens_; // 已识别的 Token 列表

    // 向前查看 offset 个字节处的字符，不移动 pos_
    char peek(std::size_t offset = 0) const;

    // 向前推进 count 个字节，同步更新 column_
    void advance(std::size_t count = 1);

    // 向前推进一个换行符，更新 line_ 并重置 column_
    void advanceLine();

    // 向 tokens_ 追加一个 Token，并推进 count 个字节
    void add(TokenKind kind, const std::string &value, std::size_t count = 1);

    // 识别标识符或保留字（包括 and/or/not/true/false）
    void identifier();

    // 识别十进制整型常量
    void number();

    // 跳过 /* ... */ 风格的块注释，内部换行正常更新行列号
    void skipBlockComment();
};

} // namespace mini

// ============================================================
// Lexer.cpp —— 词法分析器实现
// 负责将源程序字符串分解为 Token 序列。
// ============================================================

#include "Lexer.h"

#include <algorithm>  // transform
#include <cctype>     // isalpha、isdigit、tolower 等字符分类函数
#include <set>        // set（关键字集合查找）
#include <sstream>    // ostringstream（错误信息拼接）

using namespace std;

namespace mini {
namespace {

// 将 ASCII 字符串转为全小写，用于关键字的大小写不敏感比较。
// 源程序中 IF、If、if 都按 if 处理；变量名保留原样。
string lowerAscii(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return value;
}

// 判断字符是否可作标识符首字符：字母或下划线。
bool isIdentifierStart(char ch) {
    return isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

// 判断字符是否可作标识符后续字符：字母、数字或下划线。
bool isIdentifierBody(char ch) {
    return isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

} // namespace

// 构造函数：将源程序字符串移入成员变量，避免拷贝。
Lexer::Lexer(string source) : source_(move(source)) {}

// tokenize：主扫描循环，每次识别一个最长匹配的单词。
// 优先级规则：>= 优先于 >，:= 优先于 :，/* 优先于 /。
vector<Token> Lexer::tokenize() {
    while (pos_ < source_.size()) {
        const char ch = peek();
        // 跳过空白符（空格、制表符、回车）
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance();
        // 换行：行号 +1，列号重置为 1
        } else if (ch == '\n') {
            advanceLine();
        // /* ... */ 块注释：跳过整个注释体
        } else if (ch == '/' && peek(1) == '*') {
            skipBlockComment();
        // 标识符或保留字（含 and/or/not/true/false）
        } else if (isIdentifierStart(ch)) {
            identifier();
        // 十进制整数常量
        } else if (isdigit(static_cast<unsigned char>(ch))) {
            number();
        // 赋值符号 :=（双字符，需先判断）
        } else if (ch == ':' && peek(1) == '=') {
            add(TokenKind::Assign, ":=", 2);
        // 双字符关系运算符：<= 和 <>
        } else if (ch == '<' && (peek(1) == '=' || peek(1) == '>')) {
            add(TokenKind::Relop, string() + ch + peek(1), 2);
        // 双字符关系运算符：>=
        } else if (ch == '>' && peek(1) == '=') {
            add(TokenKind::Relop, ">=", 2);
        // 单字符关系运算符：< > =
        } else if (ch == '<' || ch == '>' || ch == '=') {
            add(TokenKind::Relop, string(1, ch));
        } else if (ch == '+') {
            add(TokenKind::Plus, "+");
        } else if (ch == '*') {
            add(TokenKind::Star, "*");
        } else if (ch == ';') {
            add(TokenKind::Semicolon, ";");
        } else if (ch == '(') {
            add(TokenKind::LParen, "(");
        } else if (ch == ')') {
            add(TokenKind::RParen, ")");
        } else if (ch == '#') {
            add(TokenKind::Hash, "#");
        } else if (ch == '~') {
            add(TokenKind::Tilde, "~");
        } else {
            // 遇到无法识别的字符，报错并终止词法分析
            ostringstream msg;
            msg << "非法字符 '" << ch << "'，位置 " << line_ << ":" << column_;
            throw CompileError(msg.str());
        }
    }
    // 末尾追加 End 哨兵，供语法分析器判断输入结束
    tokens_.push_back(Token{TokenKind::End, "", line_, column_});
    return tokens_;
}

// peek：向前查看 offset 个字节处的字符，不移动 pos_。
// 超出源程序范围时返回 '\0'，避免越界访问。
char Lexer::peek(size_t offset) const {
    const size_t index = pos_ + offset;
    return index < source_.size() ? source_[index] : '\0';
}

// advance：向前推进 count 个字节，同步更新列号。
void Lexer::advance(size_t count) {
    pos_ += count;
    column_ += static_cast<int>(count);
}

// advanceLine：处理换行符，行号 +1，列号重置为 1。
void Lexer::advanceLine() {
    ++pos_;
    ++line_;
    column_ = 1;
}

// add：向 tokens_ 追加一个 Token，然后推进 count 个字节。
void Lexer::add(TokenKind kind, const string &value, size_t count) {
    tokens_.push_back(Token{kind, value, line_, column_});
    advance(count);
}

// identifier：识别一个标识符或保留字。
// 策略：先贪心读入所有合法字符，再判断是否属于关键字/逻辑运算符/布尔常量。
// 好处：while1 整体识别为变量，而非 while + 1。
void Lexer::identifier() {
    const size_t start = pos_;       // 记录单词起始位置
    const int startColumn = column_; // 记录单词起始列号
    // 贪心读入标识符体字符
    while (pos_ < source_.size() && isIdentifierBody(source_[pos_])) {
        advance();
    }
    const string raw = source_.substr(start, pos_ - start);  // 原始字符串（保留大小写）
    const string lowered = lowerAscii(raw);                   // 小写版，用于关键字匹配
    // 七个保留字：if/then/else/while/do/begin/end
    static const set<string> keywords = {"if", "then", "else", "while", "do", "begin", "end"};
    if (keywords.count(lowered) != 0) {
        tokens_.push_back(Token{TokenKind::Keyword, lowered, line_, startColumn});
    } else if (lowered == "and") {
        tokens_.push_back(Token{TokenKind::And, raw, line_, startColumn});
    } else if (lowered == "or") {
        tokens_.push_back(Token{TokenKind::Or, raw, line_, startColumn});
    } else if (lowered == "not") {
        tokens_.push_back(Token{TokenKind::Not, raw, line_, startColumn});
    } else if (lowered == "true" || lowered == "false") {
        // 布尔常量统一转小写存储
        tokens_.push_back(Token{TokenKind::Boolean, lowered, line_, startColumn});
    } else {
        // 普通变量名，保留原始大小写
        tokens_.push_back(Token{TokenKind::Identifier, raw, line_, startColumn});
    }
}

// number：识别十进制整数常量，连续数字直接构成一个 NUM token。
void Lexer::number() {
    const size_t start = pos_;
    const int startColumn = column_;
    while (pos_ < source_.size() && isdigit(static_cast<unsigned char>(source_[pos_]))) {
        advance();
    }
    tokens_.push_back(Token{TokenKind::Number, source_.substr(start, pos_ - start), line_, startColumn});
}

// skipBlockComment：跳过 /* ... */ 风格的块注释。
// 注释内部的换行仍更新 line/column，保证后续报错位置准确。
// 如果注释未闭合则抛出 CompileError。
void Lexer::skipBlockComment() {
    advance(2); // 跳过 /*
    while (pos_ < source_.size()) {
        if (peek() == '*' && peek(1) == '/') {
            advance(2); // 跳过 */
            return;
        }
        if (peek() == '\n') {
            advanceLine();
        } else {
            advance();
        }
    }
    throw CompileError("块注释未闭合");
}

} // namespace mini

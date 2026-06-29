// ============================================================
// Parser.cpp —— 语法分析器（兼语法制导翻译）实现
// 采用递归下降分析，每识别一个语法结构立即生成四元式。
// 控制流语句使用"回填"技术处理未知跳转地址。
// ============================================================

#include "Parser.h"

#include <algorithm>  // find
#include <cctype>     // isdigit
#include <sstream>    // ostringstream
#include <utility>    // move

using namespace std;

namespace mini {
namespace {

// 判断字符串是否为十进制整数常量。
// 常量不加入符号表，在汇编中作立即数使用。
bool isNumber(const string &value) {
    return !value.empty() &&
           all_of(value.begin(), value.end(), [](unsigned char ch) { return isdigit(ch); });
}

// 返回 TokenKind 对应的可读种别名称，用于错误信息中描述期望的符号。
string tokenKindName(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier: return "ID";
    case TokenKind::Number:     return "NUM";
    case TokenKind::Boolean:    return "BOOL";
    case TokenKind::Keyword:    return "KW";
    case TokenKind::Assign:     return "ASSIGN";
    case TokenKind::Relop:      return "ROP";
    case TokenKind::Plus:       return "+";
    case TokenKind::Star:       return "*";
    case TokenKind::Semicolon:  return ";";
    case TokenKind::LParen:     return "(";
    case TokenKind::RParen:     return ")";
    case TokenKind::And:        return "AND";
    case TokenKind::Or:         return "OR";
    case TokenKind::Not:        return "NOT";
    case TokenKind::Hash:       return "#";
    case TokenKind::Tilde:      return "~";
    case TokenKind::End:        return "EOF";
    }
    return "UNKNOWN";
}

} // namespace

// 构造函数：将 token 序列移入成员变量，设置四元式起始地址。
Parser::Parser(vector<Token> tokens, int startAddress)
    : tokens_(move(tokens)), startAddress_(startAddress) {}

// parse：语法分析入口。
// 程序由一条语句（或 begin-end 复合语句）组成，末尾必须有 #~ 和 EOF。
pair<vector<Quad>, vector<string>> Parser::parse() {
    statement();              // 解析顶层语句
    expect(TokenKind::Hash);  // 期望程序结束标志 #
    expect(TokenKind::Tilde); // 期望程序结束标志 ~
    expect(TokenKind::End);   // 期望词法输入结束
    return {quads_, symbols_};
}

// statement：语句分析总入口，对应文法产生式：
//   S -> if B then S else S | while B do S | begin L end | i := E
void Parser::statement() {
    if (isKeyword("if")) {
        ifStatement();
    } else if (isKeyword("while")) {
        whileStatement();
    } else if (isKeyword("begin")) {
        compoundStatement();
    } else if (current().kind == TokenKind::Identifier) {
        assignment();
    } else {
        throw error("期望语句");
    }
}

// ifStatement：翻译 if-then-else 语句。
// 生成的四元式序列（回填技术）：
//   (j<rel>, left, right, [true_addr])  <- 条件成立跳到 then 分支
//   (j, , , [false_addr])               <- 条件不成立跳到 else 分支
//   ... then 分支代码 ...
//   (j, , , [end_addr])                 <- then 执行完跳过 else
//   ... else 分支代码 ...
void Parser::ifStatement() {
    expectKeyword("if");
    const BoolNode cond = condition();   // 翻译布尔条件
    expectKeyword("then");

    const int trueJump  = emit("j" + cond.op, cond.left, cond.right, nullopt);
    const int falseJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());      // 回填：条件成立跳到 then 起始
    statement();                         // 翻译 then 分支

    const int endJump = emit("j", "", "", nullopt);
    patch(falseJump, nextAddress());     // 回填：条件不成立跳到 else 起始
    expectKeyword("else");
    statement();                         // 翻译 else 分支
    patch(endJump, nextAddress());       // 回填：跳过 else 到 if 结束
}

// whileStatement：翻译 while-do 循环语句。
// 生成的四元式序列（回填技术）：
// loopStart:
//   (j<rel>, left, right, [body_addr])  <- 条件成立进入循环体
//   (j, , , [end_addr])                 <- 条件不成立退出循环
//   ... 循环体代码 ...
//   (j, , , loopStart)                  <- 无条件跳回循环条件
void Parser::whileStatement() {
    expectKeyword("while");
    const int loopStart = nextAddress(); // 循环条件起始地址，末尾跳回此处
    const BoolNode cond = condition();   // 翻译布尔条件
    expectKeyword("do");

    const int trueJump  = emit("j" + cond.op, cond.left, cond.right, nullopt);
    const int falseJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());      // 回填：条件成立跳到循环体起始
    statement();                         // 翻译循环体
    emit("j", "", "", to_string(loopStart)); // 无条件跳回循环条件
    patch(falseJump, nextAddress());     // 回填：条件不成立跳到循环结束
}

// compoundStatement：翻译复合语句（语句串）。
//   S -> begin L end，L -> S ; L | S
// 支持尾部多余分号（如 begin A:=1; end）。
void Parser::compoundStatement() {
    expectKeyword("begin");
    statement();                          // 至少处理第一条语句
    while (match(TokenKind::Semicolon)) { // 遇到分号则继续
        if (isKeyword("end")) {
            break;                        // 允许 end 前有多余分号
        }
        statement();
    }
    expectKeyword("end");
}

// assignment：翻译赋值语句 A -> i := E。
// 生成 (:=, val, , target) 四元式。
void Parser::assignment() {
    const string target = expect(TokenKind::Identifier).value;
    rememberSymbol(target);               // 左侧变量加入符号表
    expect(TokenKind::Assign);            // 消费 :=
    const string value = arithExpr();     // 翻译右侧算术表达式
    emit(":=", value, "", target);        // 生成赋值四元式
}

// condition：布尔表达式总入口，直接委托给 boolOr（优先级最低）。
Parser::BoolNode Parser::condition() {
    return boolOr();
}

// boolOr：处理逻辑或，优先级低于 and，左结合。
// A or B or C = (A or B) or C
Parser::BoolNode Parser::boolOr() {
    BoolNode left = boolAnd();
    while (match(TokenKind::Or)) {
        const BoolNode right = boolAnd();
        left = combineBool("or", left, right);
    }
    return left;
}

// boolAnd：处理逻辑与，优先级高于 or，低于 not，左结合。
Parser::BoolNode Parser::boolAnd() {
    BoolNode left = boolNot();
    while (match(TokenKind::And)) {
        const BoolNode right = boolNot();
        left = combineBool("and", left, right);
    }
    return left;
}

// boolNot：处理逻辑非，通过翻转关系运算符实现，支持递归连续取反。
// not A<B → A>=B；not not A<B → A<B
Parser::BoolNode Parser::boolNot() {
    if (match(TokenKind::Not)) {
        const BoolNode node = boolNot();  // 递归处理被取反的表达式
        return BoolNode{falseOp(node.op), node.left, node.right};
    }
    return boolPrimary();
}

// boolPrimary：布尔基本项。
//   B -> (B) | i rop i | i
// 单独变量 i 按 i<>0 解释（非零为真）。
Parser::BoolNode Parser::boolPrimary() {
    if (match(TokenKind::LParen)) {
        const BoolNode node = condition(); // 括号内递归解析
        expect(TokenKind::RParen);
        return node;
    }
    const string left = arithExpr();       // 解析左操作数
    if (current().kind == TokenKind::Relop) {
        const string op = advanceToken().value; // 消费关系运算符
        const string right = arithExpr();   // 解析右操作数
        return BoolNode{op, left, right};
    }
    return BoolNode{"<>", left, "0"};      // 单独变量按 i<>0 处理
}

// combineBool：将 and/or 两侧条件合并为一个新的 BoolNode。
// 先把左右条件转为 0/1 临时变量，再生成 (and/or,T1,T2,T3)。
Parser::BoolNode Parser::combineBool(const string &op, const BoolNode &left, const BoolNode &right) {
    const string leftTemp  = boolToTemp(left);
    const string rightTemp = boolToTemp(right);
    const string temp = newTemp();
    emit(op, leftTemp, rightTemp, temp);
    return BoolNode{"<>", temp, "0"};
}

// boolToTemp：将布尔条件转换为 0/1 临时变量。
// 生成的四元式序列：
//   (j<rel>, left, right, true_addr)  <- 条件成立跳到赋值 1 处
//   (:=, 0, , temp)                   <- 默认赋值 0
//   (j, , , end_addr)                 <- 跳过赋值 1
// true_addr:
//   (:=, 1, , temp)                   <- 条件成立赋值 1
// end_addr:
string Parser::boolToTemp(const BoolNode &node) {
    const string temp = newTemp();
    const int trueJump = emit("j" + node.op, node.left, node.right, nullopt);
    emit(":=", "0", "", temp);
    const int endJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());
    emit(":=", "1", "", temp);
    patch(endJump, nextAddress());
    return temp;
}

// falseOp：返回关系运算符的逻辑补，用于 not 运算的实现。
// 映射：< <-> >=，<= <-> >，<> <-> =，> <-> <=，>= <-> <，= <-> <>
string Parser::falseOp(const string &op) {
    if (op == "<")  return ">=";
    if (op == "<=") return ">";
    if (op == "<>") return "=";
    if (op == ">")  return "<=";
    if (op == ">=") return "<";
    if (op == "=")  return "<>";
    throw CompileError("未知关系运算符: " + op);
}

// arithExpr：翻译加法表达式，E -> T { + T }。
// 加法左结合，每遇到 + 生成一个新临时变量。
string Parser::arithExpr() {
    string left = arithTerm();
    while (match(TokenKind::Plus)) {
        const string right = arithTerm();
        const string temp = newTemp();
        emit("+", left, right, temp);
        left = temp;
    }
    return left;
}

// arithTerm：翻译乘法表达式，T -> F { * F }。
// 乘法优先级高于加法，通过调用层次自然保证。
string Parser::arithTerm() {
    string left = arithFactor();
    while (match(TokenKind::Star)) {
        const string right = arithFactor();
        const string temp = newTemp();
        emit("*", left, right, temp);
        left = temp;
    }
    return left;
}

// arithFactor：翻译算术因子，F -> id | num | bool | (E)。
// true/false 在算术上下文中按 1/0 处理。
string Parser::arithFactor() {
    const Token tok = current();
    if (tok.kind == TokenKind::Identifier) {
        advanceToken();
        rememberSymbol(tok.value);           // 变量加入符号表
        return tok.value;
    }
    if (tok.kind == TokenKind::Number) {
        advanceToken();
        return tok.value;
    }
    if (tok.kind == TokenKind::Boolean) {
        advanceToken();
        return tok.value == "true" ? "1" : "0";
    }
    if (match(TokenKind::LParen)) {
        const string value = arithExpr();
        expect(TokenKind::RParen);
        return value;
    }
    throw error("期望表达式因子");
}

// emit：生成一条四元式追加到 quads_，返回其 vector 下标（供 patch 使用）。
// result 为 nullopt 时置为空字符串，之后由 patch() 填入目标地址。
int Parser::emit(const string &op, const string &arg1, const string &arg2,
                 const optional<string> &result) {
    const int address = nextAddress();
    quads_.push_back(Quad{address, op, arg1, arg2, result.has_value() ? *result : ""});
    return static_cast<int>(quads_.size() - 1);
}

// patch：回填跳转目标地址。
// index 是 emit() 返回的下标，将对应四元式的 result 字段设为 targetAddress。
void Parser::patch(int index, int targetAddress) {
    quads_.at(static_cast<size_t>(index)).result = to_string(targetAddress);
}

// nextAddress：计算下一条四元式的逻辑地址。
// 逻辑地址 = startAddress_（默认 100）+ 已生成四元式数量。
int Parser::nextAddress() const {
    return startAddress_ + static_cast<int>(quads_.size());
}

// newTemp：分配新的临时变量（T1、T2、...），加入符号表。
string Parser::newTemp() {
    const string name = "T" + to_string(tempNo_++);
    rememberSymbol(name);
    return name;
}

// rememberSymbol：将变量名加入符号表，去重，保持首次出现顺序。
// 常量（纯数字）不加入符号表。
void Parser::rememberSymbol(const string &name) {
    if (!isNumber(name) && find(symbols_.begin(), symbols_.end(), name) == symbols_.end()) {
        symbols_.push_back(name);
    }
}

// current：返回当前位置的 Token，不移动 pos_。
const Token &Parser::current() const {
    return tokens_.at(pos_);
}

// advanceToken：返回当前 Token 并将 pos_ 前移一位（消费当前 Token）。
const Token &Parser::advanceToken() {
    return tokens_.at(pos_++);
}

// match：可选匹配。匹配成功则消费 Token 返回 true，否则不移动返回 false。
bool Parser::match(TokenKind kind, const optional<string> &value) {
    if (current().kind == kind && (!value.has_value() || current().value == *value)) {
        ++pos_;
        return true;
    }
    return false;
}

// expect：强制匹配。匹配成功则消费并返回 Token，失败则抛出带位置信息的异常。
Token Parser::expect(TokenKind kind, const optional<string> &value) {
    const Token tok = current();
    if (tok.kind == kind && (!value.has_value() || tok.value == *value)) {
        ++pos_;
        return tok;
    }
    ostringstream msg;
    msg << "期望 " << tokenKindName(kind);
    if (value.has_value()) {
        msg << "(" << *value << ")";
    }
    throw error(msg.str());
}

// isKeyword：检查当前 Token 是否为指定保留字，不消费 Token。
bool Parser::isKeyword(const string &value) const {
    return current().kind == TokenKind::Keyword && current().value == value;
}

// expectKeyword：强制匹配指定保留字，是 expect(Keyword, value) 的便捷封装。
Token Parser::expectKeyword(const string &value) {
    return expect(TokenKind::Keyword, value);
}

// error：构造带当前 Token 位置信息的 CompileError。
// 格式：<message>，当前位置 <kind>(<value>) at <line>:<column>
CompileError Parser::error(const string &message) const {
    const Token tok = current();
    ostringstream msg;
    msg << message << "，当前位置 " << tokenKindName(tok.kind)
        << "(" << tok.value << ") at " << tok.line << ":" << tok.column;
    return CompileError(msg.str());
}

} // namespace mini

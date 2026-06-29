// ============================================================
// Compiler.cpp —— 小型编译程序实现文件
// 包含词法分析器（Lexer）、语法分析器兼语义翻译器（Parser）、
// 汇编代码生成器（AsmGenerator）以及对外接口的完整实现。
// ============================================================

#include "Compiler.h"

#include <algorithm>  // transform、find、std::all_of
#include <cctype>     // std::isalpha、std::isdigit、tolower 等
#include <fstream>    // std::ifstream、std::ofstream（文件读写）
#include <set>        // std::set（关键字查找）
#include <sstream>    // ostringstream（字符串拼接）
#include <utility>    // move、pair

using namespace std;

namespace mini {
namespace {
// ============================================================
// 匿名命名空间：仅本翻译单元可见的工具函数
// ============================================================

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

// 判断字符串是否为十进制整数常量。
// 符号表只保存变量和临时变量，常量不入符号表。
bool isNumber(const string &value) {
    return !value.empty() &&
           all_of(value.begin(), value.end(), [](unsigned char ch) { return isdigit(ch); });
}

// 返回 TokenKind 对应的可读种别名称，用于词法二元式输出和错误信息。
string tokenKindName(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier:
        return "ID";
    case TokenKind::Number:
        return "NUM";
    case TokenKind::Boolean:
        return "BOOL";
    case TokenKind::Keyword:
        return "KW";
    case TokenKind::Assign:
        return "ASSIGN";
    case TokenKind::Relop:
        return "ROP";
    case TokenKind::Plus:
        return "+";
    case TokenKind::Star:
        return "*";
    case TokenKind::Semicolon:
        return ";";
    case TokenKind::LParen:
        return "(";
    case TokenKind::RParen:
        return ")";
    case TokenKind::And:
        return "AND";
    case TokenKind::Or:
        return "OR";
    case TokenKind::Not:
        return "NOT";
    case TokenKind::Hash:
        return "#";
    case TokenKind::Tilde:
        return "~";
    case TokenKind::End:
        return "EOF";
    }
    return "UNKNOWN";
}

// 以二进制模式读入源文件，避免 Windows 文本模式改写换行符或编码字节。
// 返回文件的完整字符串内容。
string readText(const string &path) {
    ifstream in(path, ios::binary);
    if (!in) {
        throw CompileError("无法打开输入文件: " + path);
    }
    ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// 以二进制模式写出文件内容。path 为空时静默跳过（用于禁用某个输出阶段）。
void writeText(const string &path, const string &content) {
    if (path.empty()) {
        return;
    }
    ofstream out(path, ios::binary);
    if (!out) {
        throw CompileError("无法写入输出文件: " + path);
    }
    out << content;
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

void Lexer::skipBlockComment() {
    // 支持 C 风格块注释，主要用于任务书样例中的说明性注释。
    // 注释内部的换行仍更新 line/column，保证后续报错位置准确。
    advance(2);
    while (pos_ < source_.size()) {
        if (peek() == '*' && peek(1) == '/') {
            advance(2);
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
//   S -> if B then S else S
//      | while B do S
//      | begin L end
//      | i := E
// 根据当前 token 的种类选择对应的分支处理函数。
void Parser::statement() {
    // 根据当前 token 选择语句产生式：
    // S -> if ... | while ... | begin ... end | assignment
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

// ifStatement：翻译 if-then-else 语句，对应文法产生式：
//   S -> if B then S1 else S2
//
// 生成的四元式序列（回填技术）：
//   (j<rel>, left, right, [true_addr])   <- trueJump，条件成立则跳到 then 分支
//   (j, , , [false_addr])                <- falseJump，条件不成立则跳到 else 分支
//   ... then 分支代码 ...
//   (j, , , [end_addr])                  <- endJump，then 执行完跳过 else
//   ... else 分支代码 ...
// 方括号内的地址在生成时未知，事后通过 patch() 回填。
void Parser::ifStatement() {
    expectKeyword("if");
    const BoolNode cond = condition();   // 翻译布尔条件，得到关系节点
    expectKeyword("then");

    // emit 条件跳转（目标暂空），记录下标用于之后 patch
    const int trueJump  = emit("j" + cond.op, cond.left, cond.right, nullopt);
    const int falseJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());      // 回填 true 跳转目标 = then 分支起始地址
    statement();                         // 翻译 then 分支

    // then 分支结束后，需要无条件跳过 else 分支
    const int endJump = emit("j", "", "", nullopt);
    patch(falseJump, nextAddress());     // 回填 false 跳转目标 = else 分支起始地址
    expectKeyword("else");
    statement();                         // 翻译 else 分支
    patch(endJump, nextAddress());       // 回填 end 跳转目标 = if 语句结束地址
}

// whileStatement：翻译 while-do 循环语句，对应文法产生式：
//   S -> while B do S
//
// 生成的四元式序列（回填技术）：
// loopStart:
//   (j<rel>, left, right, [body_addr])   <- trueJump，条件成立则进入循环体
//   (j, , , [end_addr])                  <- falseJump，条件不成立则退出循环
//   ... 循环体代码 ...
//   (j, , , loopStart)                   <- 无条件跳回循环条件检查处
// loopEnd: （falseJump 回填到此处）
void Parser::whileStatement() {
    expectKeyword("while");
    const int loopStart = nextAddress(); // 记录循环条件的起始地址，循环末尾无条件跳回此处
    const BoolNode cond = condition();   // 翻译布尔条件
    expectKeyword("do");

    const int trueJump  = emit("j" + cond.op, cond.left, cond.right, nullopt);
    const int falseJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());      // 回填 true 跳转目标 = 循环体起始地址
    statement();                         // 翻译循环体
    emit("j", "", "", to_string(loopStart)); // 无条件跳回循环条件检查处
    patch(falseJump, nextAddress());     // 回填 false 跳转目标 = 循环结束地址
}

// compoundStatement：翻译复合语句（语句串），对应文法产生式：
//   S -> begin L end
//   L -> S ; L | S
//
// 用循环而非递归处理分号分隔的语句串，支持尾部多余分号（如 begin A:=1; end）。
void Parser::compoundStatement() {
    expectKeyword("begin");
    statement();                         // 至少处理第一条语句
    while (match(TokenKind::Semicolon)) { // 遇到分号则继续解析下一条语句
        if (isKeyword("end")) {
            break;                       // 允许 end 前有一个多余分号
        }
        statement();
    }
    expectKeyword("end");
}

// assignment：翻译赋值语句 A -> i := E。
// 先解析左侧变量名，再解析右侧算术表达式，生成 (:=, val, , target) 四元式。
void Parser::assignment() {
    const string target = expect(TokenKind::Identifier).value;
    rememberSymbol(target);      // 左侧变量加入符号表
    expect(TokenKind::Assign);   // 消费 :=
    const string value = arithExpr(); // 翻译右侧表达式，返回结果操作数
    emit(":=", value, "", target);    // 生成赋值四元式
}

// condition：布尔表达式总入口，优先级最低（or 级别）。
// 直接委托给 boolOr，为语法分析提供统一入口。
Parser::BoolNode Parser::condition() {
    return boolOr();
}

// boolOr：处理逻辑或表达式，对应文法产生式：B -> B ∨ B。
// 优先级低于 and，采用左结合：A or B or C = (A or B) or C。
// 每遇到 or，先把两侧条件转为 0/1 临时变量，再 emit (or,T1,T2,T3)。
Parser::BoolNode Parser::boolOr() {
    BoolNode left = boolAnd();           // 先解析左侧（and 级别）
    while (match(TokenKind::Or)) {       // 遇到 or 则继续处理右侧
        const BoolNode right = boolAnd();
        left = combineBool("or", left, right); // 合并为新的 BoolNode
    }
    return left;
}

// boolAnd：处理逻辑与表达式，对应文法产生式：B -> B ∧ B。
// 优先级高于 or，低于 not，同样左结合。
Parser::BoolNode Parser::boolAnd() {
    BoolNode left = boolNot();           // 先解析左侧（not 级别）
    while (match(TokenKind::And)) {      // 遇到 and 则继续处理右侧
        const BoolNode right = boolNot();
        left = combineBool("and", left, right);
    }
    return left;
}

// boolNot：处理逻辑非，对应文法产生式：B -> ¬B。
// 通过将关系运算符替换为其逻辑补来实现取反，例如 not A<B → A>=B。
// 支持连续取反：not not A<B → A<B（递归处理）。
Parser::BoolNode Parser::boolNot() {
    if (match(TokenKind::Not)) {
        const BoolNode node = boolNot(); // 递归处理被取反的布尔表达式
        return BoolNode{falseOp(node.op), node.left, node.right}; // 翻转关系运算符
    }
    return boolPrimary();                // 无 not，直接解析基本布尔项
}

// boolPrimary：布尔基本项，对应文法产生式：
//   B -> (B) | i rop i | i
// 括号表达式递归调用 condition()；
// 关系表达式解析左右操作数和运算符；
// 单独变量 i 按 i<>0 解释（非零为真）。
Parser::BoolNode Parser::boolPrimary() {
    if (match(TokenKind::LParen)) {
        const BoolNode node = condition(); // 括号内递归解析布尔表达式
        expect(TokenKind::RParen);
        return node;
    }
    const string left = arithExpr();       // 解析左操作数（算术表达式）
    if (current().kind == TokenKind::Relop) {
        const string op = advanceToken().value; // 消费关系运算符
        const string right = arithExpr();   // 解析右操作数
        return BoolNode{op, left, right};
    }
    // 单独变量/常量：按 i<>0 处理
    return BoolNode{"<>", left, "0"};
}

// combineBool：将 and/or 两侧的布尔条件合并为一个新的 BoolNode。
// 由于四元式只支持简单关系表达式，复杂布尔值需先转为 0/1 临时变量，
// 再生成 (and/or, T_left, T_right, T_result) 四元式。
// 返回的 BoolNode 表示 "T_result <> 0"，供上层继续使用。
Parser::BoolNode Parser::combineBool(const string &op, const BoolNode &left, const BoolNode &right) {
    const string leftTemp  = boolToTemp(left);  // 将左条件转为 0/1 临时变量
    const string rightTemp = boolToTemp(right); // 将右条件转为 0/1 临时变量
    const string temp = newTemp();              // 分配结果临时变量
    emit(op, leftTemp, rightTemp, temp);        // 生成 (and/or, T1, T2, T3)
    return BoolNode{"<>", temp, "0"};           // 结果表示为 T3<>0
}

// boolToTemp：将一个布尔条件转换为 0/1 临时变量。
// 生成的四元式序列：
//   (j<rel>, left, right, true_addr)  <- 条件成立跳到赋值 1 处
//   (:=, 0, , temp)                   <- 默认赋值 0
//   (j, , , end_addr)                 <- 跳过赋值 1
// true_addr:
//   (:=, 1, , temp)                   <- 条件成立时赋值 1
// end_addr:
// 返回临时变量名，供 combineBool 使用。
string Parser::boolToTemp(const BoolNode &node) {
    const string temp = newTemp();
    const int trueJump = emit("j" + node.op, node.left, node.right, nullopt);
    emit(":=", "0", "", temp);               // 条件不成立：temp = 0
    const int endJump = emit("j", "", "", nullopt);
    patch(trueJump, nextAddress());          // 回填：条件成立跳到此处
    emit(":=", "1", "", temp);               // 条件成立：temp = 1
    patch(endJump, nextAddress());           // 回填：跳过赋值 1
    return temp;
}

// falseOp：返回关系运算符的逻辑补，用于实现 not 运算。
// 对应关系：< <-> >=，<= <-> >，<> <-> =，> <-> <=，>= <-> <，= <-> <>。
string Parser::falseOp(const string &op) {
    if (op == "<")  return ">=";
    if (op == "<=") return ">";
    if (op == "<>") return "=";
    if (op == ">")  return "<=";
    if (op == ">=") return "<";
    if (op == "=")  return "<>";
    throw CompileError("未知关系运算符: " + op);
}

// arithExpr：翻译加法表达式，对应文法产生式：E -> T { + T }。
// 每遇到一个 +，生成 (+, left, right, Tn) 四元式，Tn 作为新的左操作数继续累积。
// 加法左结合：A+B+C = (A+B)+C，由 while 循环自然实现。
string Parser::arithExpr() {
    string left = arithTerm();               // 先解析第一个乘法项
    while (match(TokenKind::Plus)) {         // 遇到 + 则继续
        const string right = arithTerm();    // 解析下一个乘法项
        const string temp = newTemp();       // 分配临时变量存放加法结果
        emit("+", left, right, temp);        // 生成加法四元式
        left = temp;                         // 以结果作为下一次加法的左操作数
    }
    return left;
}

// arithTerm：翻译乘法表达式，对应文法产生式：T -> F { * F }。
// 乘法优先级高于加法，通过调用层次（arithExpr 调用 arithTerm）自然保证。
string Parser::arithTerm() {
    string left = arithFactor();             // 先解析第一个因子
    while (match(TokenKind::Star)) {         // 遇到 * 则继续
        const string right = arithFactor();  // 解析下一个因子
        const string temp = newTemp();       // 分配临时变量存放乘法结果
        emit("*", left, right, temp);        // 生成乘法四元式
        left = temp;
    }
    return left;
}

// arithFactor：翻译算术因子，对应文法产生式：F -> id | num | bool | (E)。
// - 变量：直接返回变量名，并加入符号表
// - 整型常量：直接返回数字字符串
// - 布尔常量：true->1，false->0（在算术上下文中的值）
// - 括号表达式：递归调用 arithExpr()
string Parser::arithFactor() {
    const Token tok = current();
    if (tok.kind == TokenKind::Identifier) {
        advanceToken();
        rememberSymbol(tok.value);           // 变量加入符号表
        return tok.value;
    }
    if (tok.kind == TokenKind::Number) {
        advanceToken();
        return tok.value;                    // 整型常量直接返回
    }
    if (tok.kind == TokenKind::Boolean) {
        advanceToken();
        return tok.value == "true" ? "1" : "0"; // 布尔常量映射为 1/0
    }
    if (match(TokenKind::LParen)) {
        const string value = arithExpr();   // 括号内递归解析算术表达式
        expect(TokenKind::RParen);
        return value;
    }
    throw error("期望表达式因子");
}

// emit：生成一条四元式并追加到 quads_，返回其在 quads_ 中的下标（非地址）。
// 下标用于后续 patch() 回填跳转目标。
// result 为 nullopt 时表示跳转目标暂时未知，result 字段置为空字符串，
// 之后由 patch() 填入正确的目标地址字符串。
int Parser::emit(const string &op, const string &arg1, const string &arg2,
                 const optional<string> &result) {
    const int address = nextAddress();       // 计算本条四元式的逻辑地址
    quads_.push_back(Quad{address, op, arg1, arg2, result.has_value() ? *result : ""});
    return static_cast<int>(quads_.size() - 1); // 返回下标，供 patch 使用
}

// patch：回填跳转目标地址。
// index 是 emit() 返回的 quads_ 下标（不是四元式地址）。
// 将 quads_[index].result 设置为 targetAddress 的字符串表示。
void Parser::patch(int index, int targetAddress) {
    quads_.at(static_cast<size_t>(index)).result = to_string(targetAddress);
}

// nextAddress：计算下一条将要生成的四元式的逻辑地址。
// 逻辑地址 = startAddress_（默认 100）+ 已生成四元式数量。
int Parser::nextAddress() const {
    return startAddress_ + static_cast<int>(quads_.size());
}

// newTemp：分配一个新的临时变量，命名为 T1、T2、T3...
// 同时将临时变量加入符号表，确保汇编 .DATA 段中有对应的变量定义。
string Parser::newTemp() {
    const string name = "T" + to_string(tempNo_++); // tempNo_ 从 1 开始递增
    rememberSymbol(name);
    return name;
}

// rememberSymbol：将变量名加入符号表，保持首次出现的顺序，避免重复。
// 常量（纯数字字符串）不加入符号表，因为它们在汇编中作立即数使用。
void Parser::rememberSymbol(const string &name) {
    if (!isNumber(name) && find(symbols_.begin(), symbols_.end(), name) == symbols_.end()) {
        symbols_.push_back(name);
    }
}

// current：返回当前位置的 Token（不移动 pos_）。
// 用于在不消费 Token 的情况下查看当前单词种类和值。
const Token &Parser::current() const {
    return tokens_.at(pos_);
}

// advanceToken：返回当前 Token 并将 pos_ 前移一位（消费当前 Token）。
const Token &Parser::advanceToken() {
    return tokens_.at(pos_++);
}

// match：可选匹配。
// 若当前 Token 的 kind（和可选的 value）与参数匹配，则消费该 Token 并返回 true；
// 否则不移动 pos_，返回 false，不抛出异常。
// 常用于处理可选的语法成分（如分号、or/and 等循环迭代）。
bool Parser::match(TokenKind kind, const optional<string> &value) {
    if (current().kind == kind && (!value.has_value() || current().value == *value)) {
        ++pos_;
        return true;
    }
    return false;
}

// expect：强制匹配。
// 若当前 Token 与参数匹配，则消费并返回该 Token；
// 否则构造带有当前位置信息的错误消息并抛出 CompileError。
// 用于文法中必须出现的终结符（如 then、do、end、#、~ 等）。
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

// isKeyword：检查当前 Token 是否为指定的保留字，不消费 Token。
bool Parser::isKeyword(const string &value) const {
    return current().kind == TokenKind::Keyword && current().value == value;
}

// expectKeyword：强制匹配指定保留字，失败则抛出异常。
// 是 expect(TokenKind::Keyword, value) 的便捷封装。
Token Parser::expectKeyword(const string &value) {
    return expect(TokenKind::Keyword, value);
}

// error：构造带有当前 Token 位置信息的 CompileError。
// 错误消息格式：<message>，当前位置 <kind>(<value>) at <line>:<column>。
CompileError Parser::error(const string &message) const {
    const Token tok = current();
    ostringstream msg;
    msg << message << "，当前位置 " << tokenKindName(tok.kind)
        << "(" << tok.value << ") at " << tok.line << ":" << tok.column;
    return CompileError(msg.str());
}

// 构造函数：将四元式序列和符号表移入成员变量，避免拷贝。
AsmGenerator::AsmGenerator(vector<Quad> quads, vector<string> symbols)
    : quads_(move(quads)), symbols_(move(symbols)) {}

// generate：将全部四元式翻译为完整的 8086/8088 汇编文本。
// 输出结构：
//   .MODEL SMALL / .STACK 100H
//   .DATA  —— 每个变量/临时变量声明为 DW 0
//   .CODE START: 初始化 DS
//   L<addr>: 每条四元式对应一个标签 + 若干指令
//   LEND:  程序出口，INT 21H 退出
string AsmGenerator::generate() const {
    ostringstream out;
    out << "; 8086/8088-style assembly generated from quadruples\n";
    out << ".MODEL SMALL\n.STACK 100H\n.DATA\n";
    for (const auto &symbol : symbols_) {
        out << asmSymbol(symbol) << " DW 0\n";
    }
    out << ".CODE\nSTART:\n    MOV AX, @DATA\n    MOV DS, AX\n";
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
// 根据 op 字段分派到不同的指令模板：
//   +:*/and/or   -> binaryToAsm（二元运算）
//   :=           -> MOV AX, src; MOV dst, AX
//   j            -> JMP label（无条件跳转）
//   j<rel>       -> MOV AX, arg1; CMP AX, arg2; J<xx> label（条件跳转）
// 关系运算符到条件跳转指令的映射：< JL，<= JLE，<> JNE，> JG，>= JGE，= JE。
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
        // 条件跳转：(j<rel>, A, B, addr) -> CMP A,B; J<xx> addr
        const string rel = quad.op.substr(1); // 提取关系运算符部分
        string instr;
        if      (rel == "<")  instr = "JL";
        else if (rel == "<=") instr = "JLE";
        else if (rel == "<>") instr = "JNE";
        else if (rel == ">")  instr = "JG";
        else if (rel == ">=") instr = "JGE";
        else if (rel == "=")  instr = "JE";
        else return {"    ; unsupported quad: " + formatQuad(quad)};
        return {"    MOV AX, " + operand(quad.arg1),
                "    CMP AX, " + operand(quad.arg2),
                "    " + instr + " " + label(quad.result)};
    }
    // 未知操作符：输出注释行，不中断汇编
    return {"    ; unsupported quad: " + formatQuad(quad)};
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
        lines.push_back("    IMUL BX");                        // AX = AX * BX
    } else if (quad.op == "and") {
        lines.push_back("    AND AX, " + operand(quad.arg2));
    } else if (quad.op == "or") {
        lines.push_back("    OR AX, " + operand(quad.arg2));
    }
    lines.push_back("    MOV " + operand(quad.result) + ", AX"); // 结果写回变量
    return lines;
}

// operand：将操作数转换为汇编操作数格式。
// 纯数字（常量）直接作为立即数；变量和临时变量名转大写以匹配 .DATA 段定义。
string AsmGenerator::operand(const string &value) const {
    return isNumber(value) ? value : asmSymbol(value);
}

// label：将四元式地址字符串转换为汇编标签。
// 若目标是最后一条四元式地址 +1（即程序出口），则返回 "LEND"，
// 否则返回 "L<address>"，与 generate() 中生成的标签格式对应。
string AsmGenerator::label(const string &address) const {
    if (!quads_.empty() && address == to_string(quads_.back().address + 1)) {
        return "LEND";
    }
    return "L" + address;
}

// asmSymbol：将变量名转为全大写，统一与 .DATA 段中的标识符风格一致。
string AsmGenerator::asmSymbol(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(toupper(ch));
    });
    return value;
}

// compileSource：编译源程序字符串，返回完整的编译结果。
// 流水线：词法分析 -> 语法/语义分析 -> 四元式生成 -> 汇编生成。
// startAddress 指定四元式起始地址（默认 100，与任务书一致）。
CompileResult compileSource(const string &source, int startAddress) {
    vector<Token> tokens = Lexer(source).tokenize();   // 词法分析
    auto parsed = Parser(tokens, startAddress).parse(); // 语法分析 + 语义翻译
    CompileResult result;
    result.tokens = move(tokens);
    // 去掉末尾的 End 哨兵，输出词法结果时不需要它
    if (!result.tokens.empty() && result.tokens.back().kind == TokenKind::End) {
        result.tokens.pop_back();
    }
    result.quads    = move(parsed.first);   // 四元式序列
    result.symbols  = move(parsed.second);  // 符号表
    result.assembly = AsmGenerator(result.quads, result.symbols).generate(); // 汇编生成
    return result;
}

// compileFile：编译源文件并将各阶段结果写入指定文件。
// - sourcePath：源程序文件路径
// - mediumPath：四元式输出路径（.med）
// - asmPath：汇编输出路径（.asm），为空时跳过
// - tokenPath：词法二元式输出路径（.tok），为空时跳过
CompileResult compileFile(const string &sourcePath, const string &mediumPath, const string &asmPath,
                          const string &tokenPath) {
    CompileResult result = compileSource(readText(sourcePath)); // 读入并编译源文件

    // 将四元式序列格式化并写入 .med 文件
    ostringstream med;
    for (const auto &quad : result.quads) {
        med << formatQuad(quad) << "\n";
    }
    writeText(mediumPath, med.str());
    writeText(asmPath, result.assembly); // 写入汇编文件（asmPath 为空时自动跳过）

    // 可选：写入词法二元式文件
    if (!tokenPath.empty()) {
        ostringstream tokens;
        for (const auto &token : result.tokens) {
            tokens << formatToken(token) << "\n";
        }
        writeText(tokenPath, tokens.str());
    }
    return result;
}

// formatQuad：将四元式格式化为可读字符串。
// 输出格式：<address> (<op>,<arg1>,<arg2>,<result>)
// 例如：100 (j>,a,b,102)
string formatQuad(const Quad &quad) {
    ostringstream out;
    out << quad.address << " (" << quad.op << "," << quad.arg1
        << "," << quad.arg2 << "," << quad.result << ")";
    return out.str();
}

// formatToken：将词法单元格式化为二元式字符串。
// 输出格式：(<种别名称>,<单词值>)
// 例如：(ID,a)，(KW,while)，(ROP,>=)
string formatToken(const Token &token) {
    ostringstream out;
    out << "(" << tokenKindName(token.kind) << "," << token.value << ")";
    return out.str();
}

} // namespace mini

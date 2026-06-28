#include "Compiler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>

namespace mini {
namespace {

// 关键字大小写不敏感：源程序中 IF、If、if 都按 if 处理。
// 变量名本身保留原样，便于输出四元式时与输入保持一致。
std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// 标识符采用常见规则：字母或下划线开头，后续可含字母、数字、下划线。
bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierBody(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

// 判断字符串是否为十进制整数常量。符号表只保存变量和临时变量，不保存常量。
bool isNumber(const std::string &value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

// 输出词法二元式时使用的种别名称。
std::string tokenKindName(TokenKind kind) {
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

// 以二进制模式读写，避免 Windows 文本模式改写换行或编码字节。
std::string readText(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw CompileError("无法打开输入文件: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void writeText(const std::string &path, const std::string &content) {
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw CompileError("无法写入输出文件: " + path);
    }
    out << content;
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
    // 主扫描循环每次识别一个最长可用单词。
    // 例如 >= 必须优先于 >，:= 必须优先于 :。
    while (pos_ < source_.size()) {
        const char ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance();
        } else if (ch == '\n') {
            advanceLine();
        } else if (ch == '/' && peek(1) == '*') {
            skipBlockComment();
        } else if (isIdentifierStart(ch)) {
            identifier();
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            number();
        } else if (ch == ':' && peek(1) == '=') {
            add(TokenKind::Assign, ":=", 2);
        } else if (ch == '<' && (peek(1) == '=' || peek(1) == '>')) {
            add(TokenKind::Relop, std::string() + ch + peek(1), 2);
        } else if (ch == '>' && peek(1) == '=') {
            add(TokenKind::Relop, ">=", 2);
        } else if (ch == '<' || ch == '>' || ch == '=') {
            add(TokenKind::Relop, std::string(1, ch));
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
            std::ostringstream msg;
            msg << "非法字符 '" << ch << "'，位置 " << line_ << ":" << column_;
            throw CompileError(msg.str());
        }
    }
    // End 不是源程序中的真实字符，而是语法分析器判断输入结束的哨兵。
    tokens_.push_back(Token{TokenKind::End, "", line_, column_});
    return tokens_;
}

char Lexer::peek(std::size_t offset) const {
    // peek 不移动当前位置，便于判断双字符运算符和注释起始符。
    const std::size_t index = pos_ + offset;
    return index < source_.size() ? source_[index] : '\0';
}

void Lexer::advance(std::size_t count) {
    pos_ += count;
    column_ += static_cast<int>(count);
}

void Lexer::advanceLine() {
    ++pos_;
    ++line_;
    column_ = 1;
}

void Lexer::add(TokenKind kind, const std::string &value, std::size_t count) {
    tokens_.push_back(Token{kind, value, line_, column_});
    advance(count);
}

void Lexer::identifier() {
    // 先按标识符整体读入，再判断它是否属于保留字或逻辑运算符。
    // 这样 while1 会被识别为变量，而不是 while + 1。
    const std::size_t start = pos_;
    const int startColumn = column_;
    while (pos_ < source_.size() && isIdentifierBody(source_[pos_])) {
        advance();
    }
    const std::string raw = source_.substr(start, pos_ - start);
    const std::string lowered = lowerAscii(raw);
    static const std::set<std::string> keywords = {"if", "then", "else", "while", "do", "begin", "end"};
    if (keywords.count(lowered) != 0) {
        tokens_.push_back(Token{TokenKind::Keyword, lowered, line_, startColumn});
    } else if (lowered == "and") {
        tokens_.push_back(Token{TokenKind::And, raw, line_, startColumn});
    } else if (lowered == "or") {
        tokens_.push_back(Token{TokenKind::Or, raw, line_, startColumn});
    } else if (lowered == "not") {
        tokens_.push_back(Token{TokenKind::Not, raw, line_, startColumn});
    } else if (lowered == "true" || lowered == "false") {
        tokens_.push_back(Token{TokenKind::Boolean, lowered, line_, startColumn});
    } else {
        tokens_.push_back(Token{TokenKind::Identifier, raw, line_, startColumn});
    }
}

void Lexer::number() {
    // 本课程设计只要求整数常量，因此连续数字直接构成一个 NUM。
    const std::size_t start = pos_;
    const int startColumn = column_;
    while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
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

Parser::Parser(std::vector<Token> tokens, int startAddress)
    : tokens_(std::move(tokens)), startAddress_(startAddress) {}

std::pair<std::vector<Quad>, std::vector<std::string>> Parser::parse() {
    // 一个完整程序只允许一条语句或 begin-end 复合语句，之后必须以 #~ 结束。
    statement();
    expect(TokenKind::Hash);
    expect(TokenKind::Tilde);
    expect(TokenKind::End);
    return {quads_, symbols_};
}

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

void Parser::ifStatement() {
    expectKeyword("if");
    const BoolNode cond = condition();
    expectKeyword("then");

    // if 的四元式模板：
    //   j<rel> cond.true_target
    //   j     else_target
    // true_target 在 then 语句开始处即可确定，else_target 要等 then 分支结束后回填。
    const int trueJump = emit("j" + cond.op, cond.left, cond.right, std::nullopt);
    const int falseJump = emit("j", "", "", std::nullopt);
    patch(trueJump, nextAddress());
    statement();

    // then 执行完需要跳过 else 分支，目标地址要等 else 分支结束后回填。
    const int endJump = emit("j", "", "", std::nullopt);
    patch(falseJump, nextAddress());
    expectKeyword("else");
    statement();
    patch(endJump, nextAddress());
}

void Parser::whileStatement() {
    expectKeyword("while");
    // loopStart 是循环条件的第一条四元式地址，循环体末尾无条件跳回这里。
    const int loopStart = nextAddress();
    const BoolNode cond = condition();
    expectKeyword("do");

    // while 的四元式模板：
    // loopStart:
    //   j<rel> body_start
    //   j     loop_end
    // body_start:
    //   body
    //   j     loopStart
    const int trueJump = emit("j" + cond.op, cond.left, cond.right, std::nullopt);
    const int falseJump = emit("j", "", "", std::nullopt);
    patch(trueJump, nextAddress());
    statement();
    emit("j", "", "", std::to_string(loopStart));
    patch(falseJump, nextAddress());
}

void Parser::compoundStatement() {
    // L -> S ; L | S。这里用循环处理分号分隔的语句串。
    // 允许 end 前有一个多余分号，便于书写 begin A:=1; end。
    expectKeyword("begin");
    statement();
    while (match(TokenKind::Semicolon)) {
        if (isKeyword("end")) {
            break;
        }
        statement();
    }
    expectKeyword("end");
}

void Parser::assignment() {
    // A -> i := E。右侧表达式可能生成若干临时变量四元式，
    // 最终把表达式结果赋给左侧变量。
    const std::string target = expect(TokenKind::Identifier).value;
    rememberSymbol(target);
    expect(TokenKind::Assign);
    const std::string value = arithExpr();
    emit(":=", value, "", target);
}

Parser::BoolNode Parser::condition() {
    // condition 是布尔表达式入口，最低优先级为 or。
    return boolOr();
}

Parser::BoolNode Parser::boolOr() {
    // 左结合处理：A or B or C 被解释为 (A or B) or C。
    BoolNode left = boolAnd();
    while (match(TokenKind::Or)) {
        const BoolNode right = boolAnd();
        left = combineBool("or", left, right);
    }
    return left;
}

Parser::BoolNode Parser::boolAnd() {
    // and 优先级高于 or，因此 boolOr 的操作数是 boolAnd。
    BoolNode left = boolNot();
    while (match(TokenKind::And)) {
        const BoolNode right = boolNot();
        left = combineBool("and", left, right);
    }
    return left;
}

Parser::BoolNode Parser::boolNot() {
    // not 通过关系运算符取反实现，例如 not A<B 转换成 A>=B。
    if (match(TokenKind::Not)) {
        const BoolNode node = boolNot();
        return BoolNode{falseOp(node.op), node.left, node.right};
    }
    return boolPrimary();
}

Parser::BoolNode Parser::boolPrimary() {
    // 布尔基本项可以是括号表达式、关系表达式，或单独的变量/常量。
    // 单独变量 i 按 i<>0 解释。
    if (match(TokenKind::LParen)) {
        const BoolNode node = condition();
        expect(TokenKind::RParen);
        return node;
    }
    const std::string left = arithExpr();
    if (current().kind == TokenKind::Relop) {
        const std::string op = advanceToken().value;
        const std::string right = arithExpr();
        return BoolNode{op, left, right};
    }
    return BoolNode{"<>", left, "0"};
}

Parser::BoolNode Parser::combineBool(const std::string &op, const BoolNode &left, const BoolNode &right) {
    // 为了让 and/or 也能以普通四元式表示，先把左右条件各自转换成 0/1 临时变量，
    // 再生成 (and,T1,T2,T3) 或 (or,T1,T2,T3)。
    const std::string leftTemp = boolToTemp(left);
    const std::string rightTemp = boolToTemp(right);
    const std::string temp = newTemp();
    emit(op, leftTemp, rightTemp, temp);
    return BoolNode{"<>", temp, "0"};
}

std::string Parser::boolToTemp(const BoolNode &node) {
    // 将一个条件转换为临时布尔值：
    //   j<rel> true_part
    //   := 0 -> temp
    //   j end
    // true_part:
    //   := 1 -> temp
    // end:
    const std::string temp = newTemp();
    const int trueJump = emit("j" + node.op, node.left, node.right, std::nullopt);
    emit(":=", "0", "", temp);
    const int endJump = emit("j", "", "", std::nullopt);
    patch(trueJump, nextAddress());
    emit(":=", "1", "", temp);
    patch(endJump, nextAddress());
    return temp;
}

std::string Parser::falseOp(const std::string &op) {
    // not 运算需要把关系运算符替换成它的逻辑补。
    if (op == "<") return ">=";
    if (op == "<=") return ">";
    if (op == "<>") return "=";
    if (op == ">") return "<=";
    if (op == ">=") return "<";
    if (op == "=") return "<>";
    throw CompileError("未知关系运算符: " + op);
}

std::string Parser::arithExpr() {
    // E -> T { + T }，每遇到一个 + 就产生一个新的临时变量。
    std::string left = arithTerm();
    while (match(TokenKind::Plus)) {
        const std::string right = arithTerm();
        const std::string temp = newTemp();
        emit("+", left, right, temp);
        left = temp;
    }
    return left;
}

std::string Parser::arithTerm() {
    // T -> F { * F }，由调用层保证 * 的优先级高于 +。
    std::string left = arithFactor();
    while (match(TokenKind::Star)) {
        const std::string right = arithFactor();
        const std::string temp = newTemp();
        emit("*", left, right, temp);
        left = temp;
    }
    return left;
}

std::string Parser::arithFactor() {
    // F -> id | number | bool | (E)。
    // true/false 在算术上下文中分别按 1/0 处理。
    const Token tok = current();
    if (tok.kind == TokenKind::Identifier) {
        advanceToken();
        rememberSymbol(tok.value);
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
        const std::string value = arithExpr();
        expect(TokenKind::RParen);
        return value;
    }
    throw error("期望表达式因子");
}

int Parser::emit(const std::string &op, const std::string &arg1, const std::string &arg2,
                 const std::optional<std::string> &result) {
    // address 等于起始地址加当前四元式数量，因此第一条默认为 100。
    // result 为 nullopt 时表示目标地址暂时未知，稍后由 patch 回填。
    const int address = nextAddress();
    quads_.push_back(Quad{address, op, arg1, arg2, result.has_value() ? *result : ""});
    return static_cast<int>(quads_.size() - 1);
}

void Parser::patch(int index, int targetAddress) {
    // index 是 emit 返回的 vector 下标，不是四元式地址。
    quads_.at(static_cast<std::size_t>(index)).result = std::to_string(targetAddress);
}

int Parser::nextAddress() const {
    // 下一条四元式的逻辑地址。
    return startAddress_ + static_cast<int>(quads_.size());
}

std::string Parser::newTemp() {
    // 临时变量采用 T1、T2、...，并加入符号表，便于汇编 .DATA 段定义。
    const std::string name = "T" + std::to_string(tempNo_++);
    rememberSymbol(name);
    return name;
}

void Parser::rememberSymbol(const std::string &name) {
    // 符号表按首次出现顺序保存，避免重复定义。
    if (!isNumber(name) && std::find(symbols_.begin(), symbols_.end(), name) == symbols_.end()) {
        symbols_.push_back(name);
    }
}

const Token &Parser::current() const {
    return tokens_.at(pos_);
}

const Token &Parser::advanceToken() {
    return tokens_.at(pos_++);
}

bool Parser::match(TokenKind kind, const std::optional<std::string> &value) {
    // 可选匹配：成功则消费当前 token，失败不报错也不移动。
    if (current().kind == kind && (!value.has_value() || current().value == *value)) {
        ++pos_;
        return true;
    }
    return false;
}

Token Parser::expect(TokenKind kind, const std::optional<std::string> &value) {
    // 强制匹配：用于文法中必须出现的终结符，失败则抛出带位置的错误。
    const Token tok = current();
    if (tok.kind == kind && (!value.has_value() || tok.value == *value)) {
        ++pos_;
        return tok;
    }
    std::ostringstream msg;
    msg << "期望 " << tokenKindName(kind);
    if (value.has_value()) {
        msg << "(" << *value << ")";
    }
    throw error(msg.str());
}

bool Parser::isKeyword(const std::string &value) const {
    return current().kind == TokenKind::Keyword && current().value == value;
}

Token Parser::expectKeyword(const std::string &value) {
    return expect(TokenKind::Keyword, value);
}

CompileError Parser::error(const std::string &message) const {
    const Token tok = current();
    std::ostringstream msg;
    msg << message << "，当前位置 " << tokenKindName(tok.kind) << "(" << tok.value << ") at " << tok.line << ":"
        << tok.column;
    return CompileError(msg.str());
}

AsmGenerator::AsmGenerator(std::vector<Quad> quads, std::vector<std::string> symbols)
    : quads_(std::move(quads)), symbols_(std::move(symbols)) {}

std::string AsmGenerator::generate() const {
    // 输出一个完整的 8086/8088 风格汇编框架：
    // .DATA 定义变量，.CODE 初始化 DS，然后按四元式地址生成标签。
    std::ostringstream out;
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

std::vector<std::string> AsmGenerator::quadToAsm(const Quad &quad) const {
    // 根据四元式 op 分派到不同的指令模板。
    if (quad.op == "+" || quad.op == "*" || quad.op == "and" || quad.op == "or") {
        return binaryToAsm(quad);
    }
    if (quad.op == ":=") {
        return {"    MOV AX, " + operand(quad.arg1), "    MOV " + operand(quad.result) + ", AX"};
    }
    if (quad.op == "j") {
        return {"    JMP " + label(quad.result)};
    }
    if (quad.op.size() > 1 && quad.op[0] == 'j') {
        // 条件跳转四元式形如 (j>,A,B,102)，先 CMP，再输出对应条件跳转指令。
        const std::string rel = quad.op.substr(1);
        std::string instr;
        if (rel == "<") instr = "JL";
        else if (rel == "<=") instr = "JLE";
        else if (rel == "<>") instr = "JNE";
        else if (rel == ">") instr = "JG";
        else if (rel == ">=") instr = "JGE";
        else if (rel == "=") instr = "JE";
        else return {"    ; unsupported quad: " + formatQuad(quad)};
        return {"    MOV AX, " + operand(quad.arg1), "    CMP AX, " + operand(quad.arg2),
                "    " + instr + " " + label(quad.result)};
    }
    return {"    ; unsupported quad: " + formatQuad(quad)};
}

std::vector<std::string> AsmGenerator::binaryToAsm(const Quad &quad) const {
    // 二元运算统一把左操作数放入 AX，再与右操作数运算，最后写回结果变量。
    std::vector<std::string> lines = {"    MOV AX, " + operand(quad.arg1)};
    if (quad.op == "+") {
        lines.push_back("    ADD AX, " + operand(quad.arg2));
    } else if (quad.op == "*") {
        lines.push_back("    MOV BX, " + operand(quad.arg2));
        lines.push_back("    IMUL BX");
    } else if (quad.op == "and") {
        lines.push_back("    AND AX, " + operand(quad.arg2));
    } else if (quad.op == "or") {
        lines.push_back("    OR AX, " + operand(quad.arg2));
    }
    lines.push_back("    MOV " + operand(quad.result) + ", AX");
    return lines;
}

std::string AsmGenerator::operand(const std::string &value) const {
    // 常量直接作为立即数输出；变量和临时变量统一转大写，匹配 .DATA 段定义。
    return isNumber(value) ? value : asmSymbol(value);
}

std::string AsmGenerator::label(const std::string &address) const {
    // 跳到最后一条四元式后一位时，表示程序出口，用 LEND 标签承接。
    if (!quads_.empty() && address == std::to_string(quads_.back().address + 1)) {
        return "LEND";
    }
    return "L" + address;
}

std::string AsmGenerator::asmSymbol(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

CompileResult compileSource(const std::string &source, int startAddress) {
    // 编译流水线：词法分析 -> 语法/语义分析生成四元式 -> 汇编生成。
    std::vector<Token> tokens = Lexer(source).tokenize();
    auto parsed = Parser(tokens, startAddress).parse();
    CompileResult result;
    result.tokens = std::move(tokens);
    if (!result.tokens.empty() && result.tokens.back().kind == TokenKind::End) {
        result.tokens.pop_back();
    }
    result.quads = std::move(parsed.first);
    result.symbols = std::move(parsed.second);
    result.assembly = AsmGenerator(result.quads, result.symbols).generate();
    return result;
}

CompileResult compileFile(const std::string &sourcePath, const std::string &mediumPath, const std::string &asmPath,
                          const std::string &tokenPath) {
    // 文件级接口负责读取源程序并把不同阶段结果写到指定文件。
    CompileResult result = compileSource(readText(sourcePath));
    std::ostringstream med;
    for (const auto &quad : result.quads) {
        med << formatQuad(quad) << "\n";
    }
    writeText(mediumPath, med.str());
    writeText(asmPath, result.assembly);
    if (!tokenPath.empty()) {
        std::ostringstream tokens;
        for (const auto &token : result.tokens) {
            tokens << formatToken(token) << "\n";
        }
        writeText(tokenPath, tokens.str());
    }
    return result;
}

std::string formatQuad(const Quad &quad) {
    std::ostringstream out;
    out << quad.address << " (" << quad.op << "," << quad.arg1 << "," << quad.arg2 << "," << quad.result << ")";
    return out.str();
}

std::string formatToken(const Token &token) {
    std::ostringstream out;
    out << "(" << tokenKindName(token.kind) << "," << token.value << ")";
    return out.str();
}

} // namespace mini

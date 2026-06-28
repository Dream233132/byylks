#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini {

// 统一的编译错误类型。词法、语法、文件读写等错误都会抛出该异常，
// 命令行入口捕获后输出“编译失败”并返回非 0 状态码。
struct CompileError : public std::runtime_error {
    explicit CompileError(const std::string &message) : std::runtime_error(message) {}
};

// 词法单元种别。为了让语法分析代码更直接，关键字、关系运算符、
// 逻辑运算符、分隔符和结束符都被拆成明确的枚举值。
enum class TokenKind {
    Identifier,
    Number,
    Boolean,
    Keyword,
    Assign,
    Relop,
    Plus,
    Star,
    Semicolon,
    LParen,
    RParen,
    And,
    Or,
    Not,
    Hash,
    Tilde,
    End
};

// 一个词法单元。line/column 用于出错时定位源程序位置。
struct Token {
    TokenKind kind;
    std::string value;
    int line;
    int column;
};

// 四元式结构，对应任务书中的 (op, arg1, arg2, result)。
// address 从 100 开始递增，跳转指令的 result 保存目标地址。
struct Quad {
    int address;
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

// 一次完整编译的结果，便于命令行、测试和文档生成复用。
struct CompileResult {
    std::vector<Token> tokens;
    std::vector<Quad> quads;
    std::vector<std::string> symbols;
    std::string assembly;
};

// 词法分析器：
// 1. 输入源程序字符串；
// 2. 跳过空白和块注释；
// 3. 输出 Token 序列，最后追加 End 作为输入结束标记。
class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<Token> tokens_;

    char peek(std::size_t offset = 0) const;
    void advance(std::size_t count = 1);
    void advanceLine();
    void add(TokenKind kind, const std::string &value, std::size_t count = 1);
    void identifier();
    void number();
    void skipBlockComment();
};

// 递归下降语法分析器，同时承担语法制导翻译工作。
// 每识别出一个语法结构，就立即生成相应四元式；遇到控制流语句时，
// 先生成目标未知的跳转四元式，再在分支/循环体位置确定后回填地址。
class Parser {
public:
    explicit Parser(std::vector<Token> tokens, int startAddress = 100);
    std::pair<std::vector<Quad>, std::vector<std::string>> parse();

private:
    // 布尔条件的中间表示。简单关系如 A<B 保存为 op="<", left="A", right="B"；
    // 复杂布尔表达式会先转成临时变量，再表示为 Tn<>0。
    struct BoolNode {
        std::string op;
        std::string left;
        std::string right;
    };

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    std::vector<Quad> quads_;
    std::vector<std::string> symbols_;
    int tempNo_ = 1;
    int startAddress_;

    // 语句分析入口及四种语句形式。
    void statement();
    void ifStatement();
    void whileStatement();
    void compoundStatement();
    void assignment();

    // 布尔表达式按优先级拆分：or < and < not < primary。
    BoolNode condition();
    BoolNode boolOr();
    BoolNode boolAnd();
    BoolNode boolNot();
    BoolNode boolPrimary();
    BoolNode combineBool(const std::string &op, const BoolNode &left, const BoolNode &right);
    std::string boolToTemp(const BoolNode &node);
    static std::string falseOp(const std::string &op);

    // 算术表达式按优先级拆分：expr 处理 +，term 处理 *，factor 处理变量/常量/括号。
    std::string arithExpr();
    std::string arithTerm();
    std::string arithFactor();

    // 四元式辅助函数。emit 返回四元式下标，patch 根据该下标回填跳转目标。
    int emit(const std::string &op, const std::string &arg1 = "", const std::string &arg2 = "",
             const std::optional<std::string> &result = std::string(""));
    void patch(int index, int targetAddress);
    int nextAddress() const;
    std::string newTemp();
    void rememberSymbol(const std::string &name);

    const Token &current() const;
    const Token &advanceToken();
    bool match(TokenKind kind, const std::optional<std::string> &value = std::nullopt);
    Token expect(TokenKind kind, const std::optional<std::string> &value = std::nullopt);
    bool isKeyword(const std::string &value) const;
    Token expectKeyword(const std::string &value);
    CompileError error(const std::string &message) const;
};

// 选做部分：把四元式翻译为 8086/8088 风格汇编文本。
// 本实现采用 AX 作为主要运算寄存器，乘法额外使用 BX。
class AsmGenerator {
public:
    AsmGenerator(std::vector<Quad> quads, std::vector<std::string> symbols);
    std::string generate() const;

private:
    std::vector<Quad> quads_;
    std::vector<std::string> symbols_;

    std::vector<std::string> quadToAsm(const Quad &quad) const;
    std::vector<std::string> binaryToAsm(const Quad &quad) const;
    std::string operand(const std::string &value) const;
    std::string label(const std::string &address) const;
    static std::string asmSymbol(std::string value);
};

// 对外接口：编译字符串或编译文件。
CompileResult compileSource(const std::string &source, int startAddress = 100);
CompileResult compileFile(const std::string &sourcePath, const std::string &mediumPath,
                          const std::string &asmPath, const std::string &tokenPath);

std::string formatQuad(const Quad &quad);
std::string formatToken(const Token &token);

} // namespace mini

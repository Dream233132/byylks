#pragma once

// ============================================================
// Compiler.h —— 小型编译程序的头文件
// 包含所有数据结构定义、词法分析器、语法分析器、汇编生成器
// 以及对外暴露的编译接口声明。
// ============================================================

#include <optional>   // std::optional，用于可选的跳转目标
#include <stdexcept>  // std::runtime_error，编译错误基类
#include <string>     // std::string
#include <vector>     // std::vector

//  
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
    TokenKind kind;   // 单词种别
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
    std::string op;     // 操作符，如 :=、+、*、j、j<、j>=等
    std::string arg1;   // 第一操作数（变量名、常量或临时变量）
    std::string arg2;   // 第二操作数（同上，赋值时为空）
    std::string result; // 结果（目标变量名或跳转目标地址）
};

// ------------------------------------------------------------
// CompileResult —— 一次完整编译的所有产出
// 供命令行入口、测试脚本和文档生成工具统一复用。
// ------------------------------------------------------------
struct CompileResult {
    std::vector<Token> tokens;      // 词法分析产生的 Token 序列
    std::vector<Quad> quads;        // 语法制导翻译产生的四元式序列
    std::vector<std::string> symbols; // 符号表（变量和临时变量按出现顺序）
    std::string assembly;           // 汇编生成器产生的汇编文本（选做）
};

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
    std::string source_;          // 源程序字符串
    std::size_t pos_ = 0;         // 当前扫描位置（字节下标）
    int line_ = 1;                // 当前行号
    int column_ = 1;              // 当前列号
    std::vector<Token> tokens_;   // 已识别的 Token 列表

    // 向前查看 offset 个字节处的字符（不移动 pos_）
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

// ------------------------------------------------------------
// Parser —— 递归下降语法分析器（兼语法制导翻译）
// 文法（任务书给出）：
//   S  -> if B then S else S
//       | while B do S
//       | begin L end
//       | A
//   L  -> S ; L | S
//   A  -> i := E
//   B  -> B∧B | B∨B | ¬B | (B) | i rop i | i
//   E  -> E+E | E*E | (E) | i
//
// 翻译策略：
//   - 赋值语句直接生成 (:=, val, , target) 四元式；
//   - if/while 利用"回填"技术：先 emit 目标为空的跳转，
//     等分支/循环体翻译完毕后再 patch 填入正确目标地址；
//   - and/or 先把两侧布尔值转为 0/1 临时变量，再做二元运算。
// ------------------------------------------------------------
class Parser {
public:
    // 构造函数：传入词法序列和四元式起始地址（默认 100）
    explicit Parser(std::vector<Token> tokens, int startAddress = 100);

    // 执行语法分析和语义翻译，返回 <四元式列表, 符号表>
    std::pair<std::vector<Quad>, std::vector<std::string>> parse();

private:
    // ----------------------------------------------------------
    // BoolNode —— 布尔条件的中间表示
    // 简单关系表达式 A<B 存为 {op="<", left="A", right="B"}；
    // and/or 组合后的复杂条件会转成临时变量，存为 {op="<>", left="Tn", right="0"}。
    // ----------------------------------------------------------
    struct BoolNode {
        std::string op;    // 关系运算符（<、<=、<>、>、>=、=）
        std::string left;  // 左操作数（变量名或临时变量）
        std::string right; // 右操作数（变量名、常量或临时变量）
    };

    std::vector<Token> tokens_;        // 词法序列
    std::size_t pos_ = 0;              // 当前分析位置
    std::vector<Quad> quads_;          // 已生成的四元式
    std::vector<std::string> symbols_; // 符号表（保持首次出现顺序）
    int tempNo_ = 1;                   // 临时变量计数器（T1、T2、...）
    int startAddress_;                 // 四元式起始地址（默认 100）

    // ---------- 语句分析 ----------
    // 根据当前 Token 派发到对应的语句处理函数
    void statement();
    // if B then S else S —— 产生条件跳转四元式并回填
    void ifStatement();
    // while B do S —— 产生循环跳转四元式并回填
    void whileStatement();
    // begin L end —— 递归展开语句串
    void compoundStatement();
    // i := E —— 产生赋值四元式
    void assignment();

    // ---------- 布尔表达式分析（优先级：not > and > or）----------
    // 布尔表达式总入口，优先级最低
    BoolNode condition();
    // 处理 or（优先级最低的布尔运算符）
    BoolNode boolOr();
    // 处理 and（优先级高于 or）
    BoolNode boolAnd();
    // 处理 not（一元取反，优先级高于 and）
    BoolNode boolNot();
    // 处理括号布尔表达式或关系表达式（最高优先级）
    BoolNode boolPrimary();
    // 将 and/or 两侧的布尔条件合并为一个新的 BoolNode
    BoolNode combineBool(const std::string &op, const BoolNode &left, const BoolNode &right);
    // 将布尔条件转换为 0/1 临时变量（用于 and/or 的操作数）
    std::string boolToTemp(const BoolNode &node);
    // 返回关系运算符的逻辑补（用于 not 的实现）
    static std::string falseOp(const std::string &op);

    // ---------- 算术表达式分析（优先级：() > * > +）----------
    // 处理加法（最低优先级）：E -> T { + T }
    std::string arithExpr();
    // 处理乘法（高于加法）：T -> F { * F }
    std::string arithTerm();
    // 处理基本因子：F -> id | num | bool | (E)
    std::string arithFactor();

    // ---------- 四元式辅助 ----------
    // 生成一条四元式并追加到 quads_，返回其在 quads_ 中的下标
    int emit(const std::string &op, const std::string &arg1 = "", const std::string &arg2 = "",
             const std::optional<std::string> &result = std::string(""));
    // 回填跳转目标：将 quads_[index].result 设置为 targetAddress
    void patch(int index, int targetAddress);
    // 返回下一条四元式的逻辑地址（startAddress_ + 已生成条数）
    int nextAddress() const;
    // 分配一个新的临时变量（T1、T2、...），并加入符号表
    std::string newTemp();
    // 将变量名加入符号表（去重，保持首次出现顺序）
    void rememberSymbol(const std::string &name);

    // ---------- Token 操作辅助 ----------
    // 返回当前 Token（不移动位置）
    const Token &current() const;
    // 返回当前 Token 并将 pos_ 前移一位
    const Token &advanceToken();
    // 若当前 Token 与 kind（和可选的 value）匹配，消费并返回 true
    bool match(TokenKind kind, const std::optional<std::string> &value = std::nullopt);
    // 强制匹配当前 Token，成功则消费并返回；失败则抛出带位置信息的异常
    Token expect(TokenKind kind, const std::optional<std::string> &value = std::nullopt);
    // 判断当前 Token 是否为指定关键字
    bool isKeyword(const std::string &value) const;
    // 强制匹配指定关键字，失败抛出异常
    Token expectKeyword(const std::string &value);
    // 构造带当前 Token 位置信息的 CompileError
    CompileError error(const std::string &message) const;
};

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

// ------------------------------------------------------------
// 对外接口
// ------------------------------------------------------------

// 编译源程序字符串，返回完整的编译结果（含四元式和汇编）
// startAddress：四元式起始地址，默认 100
CompileResult compileSource(const std::string &source, int startAddress = 100);

// 编译源文件，将四元式、汇编、词法结果分别写入指定文件
// asmPath 为空时跳过汇编生成，tokenPath 为空时跳过词法输出
CompileResult compileFile(const std::string &sourcePath, const std::string &mediumPath,
                          const std::string &asmPath, const std::string &tokenPath);

// 将四元式格式化为字符串，如 "100 (:=,T1,,a)"
std::string formatQuad(const Quad &quad);

// 将 Token 格式化为二元式字符串，如 "(ID,a)"
std::string formatToken(const Token &token);

} // namespace mini

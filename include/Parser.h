#pragma once

// ============================================================
// Parser.h —— 递归下降语法分析器（兼语法制导翻译）类声明
// ============================================================

#include "Common.h"

#include <optional>  // std::optional，用于可选的跳转目标和匹配值

namespace mini {

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
    int emit(const std::string &op, const std::string &arg1 = "",
             const std::string &arg2 = "",
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

} // namespace mini

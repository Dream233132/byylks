#pragma once

// ============================================================
// GrammarInfo.h —— 文法信息计算与输出
// 根据课程设计任务书给出的文法，计算：
//   1. 各非终结符的 FIRST 集
//   2. 各非终结符的 FOLLOW 集
//   3. LL(1) 预测分析表
// 并提供格式化输出接口，供命令行或调试使用。
//
// 文法说明（已改写为非二义的 LL(1) 等价形式）：
//   S  -> if B then S else S
//       | while B do S
//       | begin L end
//       | i := E
//   L  -> S L'
//   L' -> ; S L' | ε
//   B  -> BA B'          (布尔 or，最低优先级)
//   B' -> or BA B' | ε
//   BA -> BN BA'         (布尔 and)
//   BA'-> and BN BA' | ε
//   BN -> not BN | BP    (布尔 not)
//   BP -> ( B ) | i rop i | i   (布尔基本项)
//   E  -> T E'           (算术加法)
//   E' -> + T E' | ε
//   T  -> F T'           (算术乘法)
//   T' -> * F T' | ε
//   F  -> i | ( E )
//
// 终结符集合（与 TokenKind 对应）：
//   if, then, else, while, do, begin, end  (保留字)
//   :=  +  *  (  )  ;  #  ~               (运算符和分隔符)
//   rop                                    (六种关系运算符的统称)
//   and  or  not                           (逻辑运算符)
//   i                                      (标识符/常量的统称)
//   $                                      (输入结束，对应 #~EOF)
// ============================================================

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mini {

// ------------------------------------------------------------
// GrammarInfo：封装文法的 FIRST、FOLLOW 集和 LL(1) 分析表
// ------------------------------------------------------------
class GrammarInfo {
public:
    // 构造时自动计算 FIRST 集、FOLLOW 集和预测分析表
    GrammarInfo();

    // 将 FIRST 集格式化为可读字符串
    std::string formatFirst() const;

    // 将 FOLLOW 集格式化为可读字符串
    std::string formatFollow() const;

    // 将 LL(1) 预测分析表格式化为可读字符串
    std::string formatTable() const;

    // 将三部分内容全部格式化输出（FIRST + FOLLOW + 分析表）
    std::string formatAll() const;

private:
    // 产生式：左部 -> 右部（右部是终结符/非终结符的字符串列表）
    struct Production {
        std::string lhs;                // 左部非终结符
        std::vector<std::string> rhs;   // 右部符号序列（空产生式用空 vector 表示）
        std::string desc;               // 可读描述，如 "S -> if B then S else S"
    };

    std::vector<std::string> nonterminals_;  // 非终结符列表（按定义顺序）
    std::vector<std::string> terminals_;     // 终结符列表
    std::vector<Production> productions_;    // 所有产生式

    // FIRST(X)：符号串 X 可以推导出的第一个终结符集合（含 ε）
    std::map<std::string, std::set<std::string>> first_;
    // FOLLOW(A)：紧跟在非终结符 A 之后的终结符集合
    std::map<std::string, std::set<std::string>> follow_;
    // 预测分析表：table_[A][a] = 产生式编号（-1 表示无该条目）
    std::map<std::string, std::map<std::string, int>> table_;

    // 初始化文法（定义产生式、终结符、非终结符）
    void initGrammar();

    // 计算单个符号的 FIRST 集（递归，处理 ε 传播）
    std::set<std::string> computeFirst(const std::string &symbol) const;

    // 计算符号串的 FIRST 集
    std::set<std::string> computeFirstOfSequence(const std::vector<std::string> &seq) const;

    // 计算所有非终结符的 FIRST 集
    void buildFirst();

    // 计算所有非终结符的 FOLLOW 集
    void buildFollow();

    // 根据 FIRST/FOLLOW 集构造 LL(1) 预测分析表
    void buildTable();
};

} // namespace mini

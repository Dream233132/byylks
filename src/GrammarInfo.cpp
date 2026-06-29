// ============================================================
// GrammarInfo.cpp —— FIRST/FOLLOW 集和 LL(1) 预测分析表计算
// ============================================================

#include "GrammarInfo.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace std;

namespace mini {

// ε 用特殊字符串 "ε" 表示，输入结束符用 "$"
static const string EPS = "ε";
static const string END_SYM = "$";

// ============================================================
// 构造函数：依次执行文法初始化、FIRST、FOLLOW、分析表构造
// ============================================================
GrammarInfo::GrammarInfo() {
    initGrammar();
    buildFirst();
    buildFollow();
    buildTable();
}

// ============================================================
// initGrammar：定义文法的非终结符、终结符和产生式
//
// 使用的非二义 LL(1) 等价文法（消除左递归和二义性）：
//
//  S   -> if B then S else S
//       | while B do S
//       | begin L end
//       | i := E
//  L   -> S L'
//  L'  -> ; S L' | ε
//  B   -> BA B'
//  B'  -> or BA B' | ε
//  BA  -> BN BA'
//  BA' -> and BN BA' | ε
//  BN  -> not BN | BP
//  BP  -> ( B ) | i rop i | i
//  E   -> T E'
//  E'  -> + T E' | ε
//  T   -> F T'
//  T'  -> * F T' | ε
//  F   -> i | ( E )
//
// 终结符用字符串表示，i 代表标识符/整常量/布尔常量，
// rop 代表 6 种关系运算符（< <= <> > >= =）的统称。
// ============================================================
void GrammarInfo::initGrammar() {
    // ---------- 非终结符（按文法层次顺序） ----------
    nonterminals_ = {"S", "L", "L'", "B", "B'", "BA", "BA'", "BN", "BP", "E", "E'", "T", "T'", "F"};

    // ---------- 终结符 ----------
    terminals_ = {
        "if", "then", "else", "while", "do", "begin", "end",  // 保留字
        ":=", "+", "*", "(", ")", ";",                          // 运算符/分隔符
        "rop", "and", "or", "not",                              // 关系/逻辑运算符
        "i",                                                    // 标识符/常量统称
        END_SYM                                                 // 输入结束符
    };

    // ---------- 产生式（编号从 0 开始） ----------
    // S 的产生式
    productions_.push_back({"S", {"if","B","then","S","else","S"},  "S -> if B then S else S"});  // 0
    productions_.push_back({"S", {"while","B","do","S"},             "S -> while B do S"});         // 1
    productions_.push_back({"S", {"begin","L","end"},                "S -> begin L end"});           // 2
    productions_.push_back({"S", {"i",":=","E"},                     "S -> i := E"});                // 3

    // L、L' 的产生式
    productions_.push_back({"L",  {"S","L'"},                        "L -> S L'"}); // 4
    productions_.push_back({"L'", {";","S","L'"},                    "L' -> ; S L'"});// 5
    productions_.push_back({"L'", {},                                "L' -> ε"});    // 6

    // B、B' 的产生式（or 层）
    productions_.push_back({"B",  {"BA","B'"},                       "B -> BA B'"});  // 7
    productions_.push_back({"B'", {"or","BA","B'"},                  "B' -> or BA B'"});// 8
    productions_.push_back({"B'", {},                                "B' -> ε"});     // 9

    // BA、BA' 的产生式（and 层）
    productions_.push_back({"BA",  {"BN","BA'"},                     "BA -> BN BA'"});  // 10
    productions_.push_back({"BA'", {"and","BN","BA'"},               "BA' -> and BN BA'"});// 11
    productions_.push_back({"BA'", {},                               "BA' -> ε"});       // 12

    // BN 的产生式（not 层）
    productions_.push_back({"BN", {"not","BN"},                      "BN -> not BN"});// 13
    productions_.push_back({"BN", {"BP"},                            "BN -> BP"});    // 14

    // BP 的产生式（基本布尔项）
    productions_.push_back({"BP", {"(","B",")"},                     "BP -> ( B )"});      // 15
    productions_.push_back({"BP", {"i","rop","i"},                   "BP -> i rop i"});    // 16
    productions_.push_back({"BP", {"i"},                             "BP -> i"});          // 17

    // E、E' 的产生式（加法层）
    productions_.push_back({"E",  {"T","E'"},                        "E -> T E'"});  // 18
    productions_.push_back({"E'", {"+","T","E'"},                    "E' -> + T E'"});// 19
    productions_.push_back({"E'", {},                                "E' -> ε"});    // 20

    // T、T' 的产生式（乘法层）
    productions_.push_back({"T",  {"F","T'"},                        "T -> F T'"});  // 21
    productions_.push_back({"T'", {"*","F","T'"},                    "T' -> * F T'"});// 22
    productions_.push_back({"T'", {},                                "T' -> ε"});    // 23

    // F 的产生式（因子）
    productions_.push_back({"F",  {"i"},                             "F -> i"});       // 24
    productions_.push_back({"F",  {"(","E",")"},                     "F -> ( E )"});   // 25
}

// ============================================================
// computeFirst：计算单个符号 X 的 FIRST 集。
// - X 是终结符：FIRST(X) = {X}
// - X 是非终结符：遍历 X 的所有产生式，递归收集
// - X 是 ε：返回 {ε}
// ============================================================
set<string> GrammarInfo::computeFirst(const string &symbol) const {
    set<string> result;
    // 终结符（含 ε 和 $）：FIRST = {symbol}
    if (find(nonterminals_.begin(), nonterminals_.end(), symbol) == nonterminals_.end()) {
        result.insert(symbol);
        return result;
    }
    // 非终结符：查已计算好的 first_ 表
    auto it = first_.find(symbol);
    if (it != first_.end()) {
        return it->second;
    }
    return result;
}

// ============================================================
// computeFirstOfSequence：计算符号串 α = X1 X2 ... Xn 的 FIRST 集。
// 算法：
//   1. 将 FIRST(X1) - {ε} 加入结果
//   2. 若 ε ∈ FIRST(X1)，继续处理 X2；否则停止
//   3. 若所有 Xi 都含 ε，则将 ε 加入结果
// ============================================================
set<string> GrammarInfo::computeFirstOfSequence(const vector<string> &seq) const {
    set<string> result;
    if (seq.empty()) {
        result.insert(EPS);
        return result;
    }
    bool allHaveEps = true;
    for (const auto &sym : seq) {
        set<string> fs = computeFirst(sym);
        // 将 FIRST(sym) 中非 ε 的元素加入结果
        for (const auto &f : fs) {
            if (f != EPS) result.insert(f);
        }
        // 若 FIRST(sym) 不含 ε，则停止传播
        if (fs.find(EPS) == fs.end()) {
            allHaveEps = false;
            break;
        }
    }
    if (allHaveEps) result.insert(EPS);
    return result;
}

// ============================================================
// buildFirst：迭代算法计算所有非终结符的 FIRST 集。
// 重复遍历所有产生式，直到所有 FIRST 集不再变化为止。
// ============================================================
void GrammarInfo::buildFirst() {
    // 初始化所有非终结符的 FIRST 集为空
    for (const auto &nt : nonterminals_) {
        first_[nt] = {};
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &prod : productions_) {
            // 计算产生式右部的 FIRST 集
            set<string> rhs_first;
            if (prod.rhs.empty()) {
                // 空产生式：FIRST 包含 ε
                rhs_first.insert(EPS);
            } else {
                bool allEps = true;
                for (const auto &sym : prod.rhs) {
                    set<string> symFirst;
                    // sym 是非终结符
                    if (first_.count(sym)) {
                        symFirst = first_[sym];
                    } else {
                        // sym 是终结符
                        symFirst.insert(sym);
                    }
                    for (const auto &f : symFirst) {
                        if (f != EPS) rhs_first.insert(f);
                    }
                    if (symFirst.find(EPS) == symFirst.end()) {
                        allEps = false;
                        break;
                    }
                }
                if (allEps) rhs_first.insert(EPS);
            }
            // 将计算出的 FIRST 并入 first_[lhs]
            size_t before = first_[prod.lhs].size();
            for (const auto &f : rhs_first) {
                first_[prod.lhs].insert(f);
            }
            if (first_[prod.lhs].size() > before) changed = true;
        }
    }
}

// ============================================================
// buildFollow：迭代算法计算所有非终结符的 FOLLOW 集。
// 规则：
//   1. FOLLOW(S) 包含 $（S 是开始符号）
//   2. 产生式 A -> αBβ：将 FIRST(β)-{ε} 加入 FOLLOW(B)
//   3. 产生式 A -> αB 或 A -> αBβ 且 ε∈FIRST(β)：将 FOLLOW(A) 加入 FOLLOW(B)
// ============================================================
void GrammarInfo::buildFollow() {
    // 初始化所有非终结符的 FOLLOW 集为空
    for (const auto &nt : nonterminals_) {
        follow_[nt] = {};
    }
    // 开始符号 S 的 FOLLOW 集包含 $
    follow_["S"].insert(END_SYM);

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &prod : productions_) {
            const string &A = prod.lhs;
            const vector<string> &rhs = prod.rhs;
            for (size_t i = 0; i < rhs.size(); ++i) {
                const string &B = rhs[i];
                // 只处理非终结符
                if (first_.find(B) == first_.end()) continue;
                // 计算 β（B 后面的符号串）的 FIRST 集
                vector<string> beta(rhs.begin() + i + 1, rhs.end());
                set<string> firstBeta = computeFirstOfSequence(beta);
                // 规则 2：FIRST(β)-{ε} 加入 FOLLOW(B)
                size_t before = follow_[B].size();
                for (const auto &f : firstBeta) {
                    if (f != EPS) follow_[B].insert(f);
                }
                // 规则 3：若 ε∈FIRST(β)，则将 FOLLOW(A) 加入 FOLLOW(B)
                if (firstBeta.count(EPS)) {
                    for (const auto &f : follow_[A]) {
                        follow_[B].insert(f);
                    }
                }
                if (follow_[B].size() > before) changed = true;
            }
        }
    }
}

// ============================================================
// buildTable：根据 FIRST/FOLLOW 集构造 LL(1) 预测分析表。
// 对每条产生式 A -> α：
//   对 FIRST(α)-{ε} 中每个终结符 a：table_[A][a] = 该产生式编号
//   若 ε∈FIRST(α)：对 FOLLOW(A) 中每个终结符 b：table_[A][b] = 该产生式编号
// ============================================================
void GrammarInfo::buildTable() {
    // 初始化表：所有格子为 -1（表示无产生式）
    for (const auto &nt : nonterminals_) {
        for (const auto &t : terminals_) {
            table_[nt][t] = -1;
        }
    }
    for (int i = 0; i < static_cast<int>(productions_.size()); ++i) {
        const auto &prod = productions_[i];
        const string &A = prod.lhs;
        // 计算产生式右部的 FIRST 集
        set<string> firstAlpha = computeFirstOfSequence(prod.rhs);
        // 对 FIRST(α)-{ε} 中每个终结符，填表
        for (const auto &a : firstAlpha) {
            if (a != EPS) {
                table_[A][a] = i;
            }
        }
        // 若 ε∈FIRST(α)，对 FOLLOW(A) 中每个终结符，填表
        if (firstAlpha.count(EPS)) {
            for (const auto &b : follow_[A]) {
                table_[A][b] = i;
            }
        }
    }
}

// ============================================================
// formatFirst：格式化输出所有非终结符的 FIRST 集
// ============================================================
string GrammarInfo::formatFirst() const {
    ostringstream out;
    out << "============================================================\n";
    out << "FIRST 集\n";
    out << "============================================================\n";
    for (const auto &nt : nonterminals_) {
        out << "FIRST(" << left << setw(4) << nt << ") = { ";
        bool first = true;
        for (const auto &f : first_.at(nt)) {
            if (!first) out << ", ";
            out << f;
            first = false;
        }
        out << " }\n";
    }
    return out.str();
}

// ============================================================
// formatFollow：格式化输出所有非终结符的 FOLLOW 集
// ============================================================
string GrammarInfo::formatFollow() const {
    ostringstream out;
    out << "============================================================\n";
    out << "FOLLOW 集\n";
    out << "============================================================\n";
    for (const auto &nt : nonterminals_) {
        out << "FOLLOW(" << left << setw(4) << nt << ") = { ";
        bool first = true;
        for (const auto &f : follow_.at(nt)) {
            if (!first) out << ", ";
            out << f;
            first = false;
        }
        out << " }\n";
    }
    return out.str();
}

// ============================================================
// formatTable：格式化输出 LL(1) 预测分析表
// 行：非终结符；列：终结符；格子：产生式编号和描述
// ============================================================
string GrammarInfo::formatTable() const {
    ostringstream out;
    out << "============================================================\n";
    out << "LL(1) 预测分析表\n";
    out << "格式：[非终结符, 输入符号] -> 产生式\n";
    out << "============================================================\n";
    for (const auto &nt : nonterminals_) {
        bool hasEntry = false;
        for (const auto &t : terminals_) {
            int idx = table_.at(nt).at(t);
            if (idx >= 0) {
                out << "  [" << left << setw(4) << nt << ", " << setw(6) << t << "] -> "
                    << productions_[idx].desc << "\n";
                hasEntry = true;
            }
        }
        if (hasEntry) out << "\n";
    }
    return out.str();
}

// ============================================================
// formatAll：输出 FIRST、FOLLOW 和分析表的全部内容
// ============================================================
string GrammarInfo::formatAll() const {
    return formatFirst() + "\n" + formatFollow() + "\n" + formatTable();
}

} // namespace mini

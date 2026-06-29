// ============================================================
// Compiler.cpp —— 对外编译接口实现
// 负责将各阶段（词法、语法、汇编）串联起来，提供文件级和字符串级接口。
// 具体实现分别位于 Lexer.cpp、Parser.cpp、AsmGenerator.cpp。
// ============================================================

#include "Compiler.h"

#include <fstream>   // ifstream、ofstream（文件读写）
#include <sstream>   // ostringstream（字符串拼接）

using namespace std;

namespace mini {
namespace {

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

// 返回 TokenKind 对应的可读种别名称，用于词法二元式输出。
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

// compileSource：编译源程序字符串，返回完整的编译结果。
// 流水线：Lexer（词法分析）-> Parser（语法分析+四元式生成）-> AsmGenerator（汇编生成）。
// startAddress 指定四元式起始地址，默认 100（与任务书样例一致）。
CompileResult compileSource(const string &source, int startAddress) {
    // 第一阶段：词法分析，源程序字符串 -> Token 序列
    vector<Token> tokens = Lexer(source).tokenize();

    // 第二阶段：语法分析 + 语义翻译，Token 序列 -> 四元式序列 + 符号表
    auto parsed = Parser(tokens, startAddress).parse();

    CompileResult result;
    result.tokens = move(tokens);
    // 去掉末尾的 End 哨兵，输出词法结果时不需要它
    if (!result.tokens.empty() && result.tokens.back().kind == TokenKind::End) {
        result.tokens.pop_back();
    }
    result.quads   = move(parsed.first);   // 四元式序列
    result.symbols = move(parsed.second);  // 符号表

    // 第三阶段（选做）：汇编代码生成，四元式序列 -> 汇编文本
    result.assembly = AsmGenerator(result.quads, result.symbols).generate();
    return result;
}

// compileFile：编译源文件并将各阶段结果写入指定文件。
// - sourcePath：源程序文件路径（.dat）
// - mediumPath：四元式输出路径（.med）
// - asmPath：汇编输出路径（.asm），为空时跳过
// - tokenPath：词法二元式输出路径（.tok），为空时跳过
CompileResult compileFile(const string &sourcePath, const string &mediumPath,
                          const string &asmPath, const string &tokenPath) {
    // 读入源文件并执行完整编译流水线
    CompileResult result = compileSource(readText(sourcePath));

    // 将四元式序列格式化并写入 .med 文件
    ostringstream med;
    for (const auto &quad : result.quads) {
        med << formatQuad(quad) << "\n";
    }
    writeText(mediumPath, med.str());

    // 写入汇编文件（asmPath 为空时 writeText 自动跳过）
    writeText(asmPath, result.assembly);

    // 可选：写入词法二元式文件
    if (!tokenPath.empty()) {
        ostringstream toks;
        for (const auto &token : result.tokens) {
            toks << formatToken(token) << "\n";
        }
        writeText(tokenPath, toks.str());
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

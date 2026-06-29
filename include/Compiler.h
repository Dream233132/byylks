#pragma once

// ============================================================
// Compiler.h —— 小型编译程序总入口头文件
// 包含所有子模块头文件，并声明对外暴露的编译接口。
// 外部代码只需 #include "Compiler.h" 即可使用全部功能。
// ============================================================

#include "AsmGenerator.h"  // 汇编代码生成器
#include "Common.h"        // 公共数据类型（Token、Quad、CompileResult 等）
#include "GrammarInfo.h"   // FIRST/FOLLOW 集和 LL(1) 预测分析表
#include "Lexer.h"         // 词法分析器
#include "Parser.h"        // 语法分析器（兼语义翻译）

namespace mini {

// ------------------------------------------------------------
// 对外编译接口
// ------------------------------------------------------------

// compileSource：编译源程序字符串，返回完整的编译结果（含四元式和汇编）
// startAddress：四元式起始地址，默认 100（与任务书样例一致）
CompileResult compileSource(const std::string &source, int startAddress = 100);

// compileFile：编译源文件，将各阶段结果写入指定文件
// - sourcePath：源程序文件路径
// - mediumPath：四元式输出路径（.med）
// - asmPath：汇编输出路径（.asm），为空时跳过汇编生成
// - tokenPath：词法二元式输出路径（.tok），为空时跳过词法输出
CompileResult compileFile(const std::string &sourcePath, const std::string &mediumPath,
                          const std::string &asmPath, const std::string &tokenPath);

// formatQuad：将四元式格式化为可读字符串，如 "100 (:=,T1,,a)"
std::string formatQuad(const Quad &quad);

// formatToken：将 Token 格式化为二元式字符串，如 "(ID,a)"
std::string formatToken(const Token &token);

} // namespace mini

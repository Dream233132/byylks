# 小型编译程序课程设计（C++）

本项目使用 C++17 实现课程设计任务中的必做与选做功能：

- 必做：将任务书规定的小型高级语言源程序翻译为四元式中间代码。
- 选做：将四元式翻译为 8086/8088 风格汇编语言目标程序。

## 项目结构

```
include/
  Common.h        —— 公共数据类型（CompileError、Token、Quad、CompileResult 等）
  Lexer.h         —— 词法分析器类声明
  Parser.h        —— 语法分析器类声明（兼语义翻译）
  AsmGenerator.h  —— 汇编代码生成器类声明
  Compiler.h      —— 总入口头文件（包含上述四个头文件 + 对外接口声明）

src/
  Lexer.cpp       —— 词法分析器实现：扫描 Token、识别保留字、处理注释
  Parser.cpp      —— 语法分析器实现：递归下降 + 回填技术生成四元式
  AsmGenerator.cpp—— 汇编生成器实现：四元式 → 8086/8088 汇编
  Compiler.cpp    —— 对外接口实现：串联三个阶段，提供文件级编译接口
  main.cpp        —— 命令行入口，解析参数并调用编译器核心
```

## 编译

需要将所有源文件一起编译：

```powershell
g++ -std=c++17 -Iinclude src\Lexer.cpp src\Parser.cpp src\AsmGenerator.cpp src\Compiler.cpp src\main.cpp -o build\mini_compiler.exe
```

## 运行

```powershell
build\mini_compiler.exe examples\pas.dat --tokens examples\pas.tok
```

默认生成：

- `examples\pas.med`：四元式文件
- `examples\pas.asm`：汇编文件
- `examples\pas.tok`：词法分析二元式文件，仅在传入 `--tokens` 时生成

只生成四元式（跳过汇编生成）：

```powershell
build\mini_compiler.exe examples\pas.dat --no-asm
```

## 自动测试

```powershell
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

测试覆盖任务书样例、表达式优先级、布尔表达式、汇编输出和错误输入。

# 小型编译程序课程设计（C++）

本项目使用 C++17 实现课程设计任务中的必做与选做功能：

- 必做：将任务书规定的小型高级语言源程序翻译为四元式中间代码。
- 选做：将四元式翻译为 8086/8088 风格汇编语言目标程序。

## 编译

```powershell
g++ -std=c++17 -Iinclude src\Compiler.cpp src\main.cpp -o build\mini_compiler.exe
```

## 运行

```powershell
build\mini_compiler.exe examples\pas.dat --tokens examples\pas.tok
```

默认生成：

- `examples\pas.med`：四元式文件
- `examples\pas.asm`：汇编文件
- `examples\pas.tok`：词法分析二元式文件，仅在传入 `--tokens` 时生成

只生成四元式：

```powershell
build\mini_compiler.exe examples\pas.dat --no-asm
```

## 自动测试

```powershell
tests\run_tests.bat
```

测试覆盖任务书样例、表达式优先级、布尔表达式、汇编输出和错误输入。

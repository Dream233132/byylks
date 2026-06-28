from __future__ import annotations

import html
import zipfile
from pathlib import Path


OUT = Path("docs/课程设计说明文档-C++版.docx")


def esc(text: str) -> str:
    return html.escape(text, quote=False)


def r(text: str, bold: bool = False, color: str | None = None, size: int | None = None) -> str:
    props = []
    if bold:
        props.append("<w:b/>")
    if color:
        props.append(f'<w:color w:val="{color}"/>')
    if size:
        props.append(f'<w:sz w:val="{size * 2}"/><w:szCs w:val="{size * 2}"/>')
    rpr = f"<w:rPr>{''.join(props)}</w:rPr>" if props else ""
    return f'<w:r>{rpr}<w:t xml:space="preserve">{esc(text)}</w:t></w:r>'


def p(text: str = "", style: str | None = None, align: str | None = None) -> str:
    props = []
    if style:
        props.append(f'<w:pStyle w:val="{style}"/>')
    if align:
        props.append(f'<w:jc w:val="{align}"/>')
    ppr = f"<w:pPr>{''.join(props)}</w:pPr>" if props else ""
    return f"<w:p>{ppr}{r(text)}</w:p>"


def title(text: str) -> str:
    return (
        '<w:p><w:pPr><w:jc w:val="center"/><w:spacing w:after="240"/></w:pPr>'
        f'{r(text, bold=True, color="0B2545", size=22)}</w:p>'
    )


def subtitle(text: str) -> str:
    return (
        '<w:p><w:pPr><w:jc w:val="center"/><w:spacing w:after="120"/></w:pPr>'
        f'{r(text, color="555555", size=12)}</w:p>'
    )


def bullet(text: str) -> str:
    return f'<w:p><w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/></w:numPr></w:pPr>{r(text)}</w:p>'


def numbered(text: str) -> str:
    return f'<w:p><w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="2"/></w:numPr></w:pPr>{r(text)}</w:p>'


def code(text: str) -> str:
    return (
        '<w:p><w:pPr><w:pStyle w:val="Code"/><w:spacing w:after="0"/></w:pPr>'
        '<w:r><w:rPr><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas" w:eastAsia="Consolas"/>'
        f'<w:sz w:val="19"/><w:szCs w:val="19"/></w:rPr><w:t xml:space="preserve">{esc(text)}</w:t></w:r></w:p>'
    )


def table(rows: list[list[str]], widths: list[int]) -> str:
    grid = "".join(f'<w:gridCol w:w="{w}"/>' for w in widths)
    xml = [
        '<w:tbl><w:tblPr><w:tblW w:w="9360" w:type="dxa"/><w:tblInd w:w="120" w:type="dxa"/>'
        '<w:tblBorders><w:top w:val="single" w:sz="4" w:color="D9E2EC"/>'
        '<w:left w:val="single" w:sz="4" w:color="D9E2EC"/><w:bottom w:val="single" w:sz="4" w:color="D9E2EC"/>'
        '<w:right w:val="single" w:sz="4" w:color="D9E2EC"/><w:insideH w:val="single" w:sz="4" w:color="D9E2EC"/>'
        '<w:insideV w:val="single" w:sz="4" w:color="D9E2EC"/></w:tblBorders>'
        '<w:tblCellMar><w:top w:w="80" w:type="dxa"/><w:left w:w="120" w:type="dxa"/>'
        '<w:bottom w:w="80" w:type="dxa"/><w:right w:w="120" w:type="dxa"/></w:tblCellMar></w:tblPr>'
        f"<w:tblGrid>{grid}</w:tblGrid>"
    ]
    for ridx, row in enumerate(rows):
        xml.append("<w:tr>")
        for text, width in zip(row, widths):
            fill = '<w:shd w:fill="F2F4F7"/>' if ridx == 0 else ""
            style = "TableHeader" if ridx == 0 else "TableBody"
            xml.append(f'<w:tc><w:tcPr><w:tcW w:w="{width}" w:type="dxa"/>{fill}</w:tcPr>{p(text, style)}</w:tc>')
        xml.append("</w:tr>")
    xml.append("</w:tbl>")
    return "".join(xml)


def read_lines(path: str) -> list[str]:
    return Path(path).read_text(encoding="utf-8").strip().splitlines()


def document_xml() -> str:
    pas_src = read_lines("examples/pas.dat")
    pas_med = read_lines("examples/pas.med")
    pas_asm = read_lines("examples/pas.asm")[:32]
    parts: list[str] = [
        title("编译技术课程设计说明文档"),
        subtitle("C++17 小型编译程序：高级语言源程序 -> 四元式 -> 8086/8088 风格汇编"),
        p("专业：__________    班级：__________    学号：__________    姓名：__________", align="center"),
        p("生成日期：2026 年 6 月 24 日", align="center"),
        p("一、课程设计目标", "Heading1"),
        p("本项目按照《课程设计任务.pdf》的要求，用 C++17 实现一个小型编译程序。必做部分完成高级语言源程序到四元式中间代码的翻译；选做部分继续完成四元式到 8086/8088 风格汇编目标程序的翻译。"),
        bullet("输入：任务书规定的小型高级语言源程序，程序以 #~ 结束。"),
        bullet("必做输出：四元式中间代码文件，地址从 100 开始。"),
        bullet("选做输出：8086/8088 风格汇编语言目标文件。"),
        bullet("工程要求：源码为 C++，可直接用 g++ 编译，并提供自动测试脚本。"),
        p("二、语言文法与处理策略", "Heading1"),
        p("任务书给出的语句文法如下："),
        code("S -> if B then S else S | while B do S | begin L end | A"),
        code("L -> S ; L | S"),
        code("A -> i := E"),
        code("B -> B and B | B or B | not B | (B) | i rop i | i"),
        code("E -> E + E | E * E | (E) | i"),
        p("表达式原文法存在二义性。实现中将其改写为按优先级分层的递归下降形式：算术表达式按 factor、term、expr 三层处理，使 * 高于 +；布尔表达式按 not、and、or 三层处理，使 not 高于 and，and 高于 or；关系运算位于算术运算之后、布尔运算之前。"),
        p("三、系统结构", "Heading1"),
        table(
            [
                ["模块", "职责", "对应文件/类"],
                ["词法分析", "扫描字符流，识别关键字、标识符、常量、关系运算符、逻辑运算符、分隔符和结束标记。", "src/Compiler.cpp / Lexer"],
                ["语法分析", "识别赋值、if、while、begin-end 语句，并在分析过程中触发语义动作。", "src/Compiler.cpp / Parser"],
                ["四元式生成", "维护四元式地址、临时变量、符号表和跳转回填。", "Quad、Parser::emit、Parser::patch"],
                ["汇编生成", "将赋值、算术、逻辑和跳转四元式翻译为 8086/8088 风格汇编。", "src/Compiler.cpp / AsmGenerator"],
                ["命令行入口", "解析参数，调用编译器，输出 .med、.asm、.tok 文件。", "src/main.cpp"],
                ["自动测试", "编译程序并校验样例四元式、表达式优先级、布尔表达式、汇编输出和错误处理。", "tests/run_tests.bat"],
            ],
            [1700, 5400, 2260],
        ),
        p("四、关键数据结构", "Heading1"),
        bullet("Token：保存单词种别、单词值、行号和列号，用于错误定位。"),
        bullet("Quad：保存 address、op、arg1、arg2、result，输出格式为 address (op,arg1,arg2,result)。"),
        bullet("CompileResult：集中保存词法结果、四元式、符号表和汇编文本。"),
        bullet("BoolNode：内部布尔条件结构，保存关系运算符和左右操作数，便于生成条件跳转。"),
        p("五、四元式生成方法", "Heading1"),
        p("赋值语句先分析右侧算术表达式。每个二元运算都会申请一个临时变量 Tn，并生成一条运算四元式；最后生成 (:=, value, , target)。while 语句记录循环入口地址，生成真跳转和假跳转，循环体末尾回跳到入口。if 语句先生成条件真跳转和假跳转，then 分支结束后生成跳过 else 的无条件跳转，再对各跳转目标进行回填。"),
        p("六、汇编生成方法", "Heading1"),
        p("汇编生成器以四元式为输入，输出 MASM/TASM 风格文本。所有变量和临时变量在 .DATA 段中定义为 DW 0。算术运算主要使用 AX，乘法额外使用 BX；关系跳转使用 MOV、CMP 和 JL/JLE/JNE/JG/JGE/JE；无条件跳转使用 JMP。"),
        p("七、编译与运行", "Heading1"),
        numbered("编译：g++ -std=c++17 -Iinclude src\\Compiler.cpp src\\main.cpp -o build\\mini_compiler.exe"),
        numbered("运行样例：build\\mini_compiler.exe examples\\pas.dat --tokens examples\\pas.tok"),
        numbered("只生成四元式：build\\mini_compiler.exe examples\\pas.dat --no-asm"),
        numbered("自动测试：tests\\run_tests.bat"),
        p("八、任务书样例验证", "Heading1"),
        p("输入源程序 examples/pas.dat："),
    ]
    parts.extend(code(line) for line in pas_src)
    parts.append(p("生成的四元式 examples/pas.med："))
    parts.extend(code(line) for line in pas_med)
    parts.append(p("生成的汇编文件 examples/pas.asm 前若干行："))
    parts.extend(code(line) for line in pas_asm)
    parts.extend(
        [
            p("九、自动测试结果", "Heading1"),
            table(
                [
                    ["测试项", "验证点", "结果"],
                    ["任务书样例", "17 条四元式与预期地址、运算符、操作数、跳转目标一致。", "通过"],
                    ["算术优先级", "A:=A+B*C 先生成乘法，再生成加法和赋值。", "通过"],
                    ["布尔表达式", "while A<C and B<D do ... 能生成 and 四元式和回跳。", "通过"],
                    ["not/or 组合", "if not A=B or C>D then ... else ... 能生成 or 四元式。", "通过"],
                    ["汇编输出", "汇编中包含 L100 标签和 INT 21H 退出代码。", "通过"],
                    ["错误处理", "缺少 #~ 时返回非零退出码并输出编译失败信息。", "通过"],
                ],
                [1800, 5960, 1600],
            ),
            p("实际测试命令 tests\\run_tests.bat 的输出结果："),
            code("PASSED: task sample quadruples"),
            code("PASSED: arithmetic precedence"),
            code("PASSED: boolean expression"),
            code("PASSED: not/or compilation"),
            code("PASSED: assembly output"),
            code("PASSED: syntax error handling"),
            code("All tests passed."),
            p("十、文件清单", "Heading1"),
            table(
                [
                    ["路径", "说明"],
                    ["include/Compiler.h", "编译器接口、Token、Quad、CompileResult、Lexer、Parser、AsmGenerator 声明。"],
                    ["src/Compiler.cpp", "词法分析、语法分析、四元式生成、汇编生成核心实现。"],
                    ["src/main.cpp", "命令行入口。"],
                    ["examples/pas.dat", "任务书样例源程序。"],
                    ["examples/pas.med", "任务书样例生成的四元式。"],
                    ["examples/pas.asm", "任务书样例生成的汇编。"],
                    ["tests/run_tests.bat", "Windows 下自动编译和测试脚本。"],
                    ["README.md", "项目运行说明。"],
                ],
                [3000, 6360],
            ),
            p("十一、总结与改进方向", "Heading1"),
            p("本实现重点完成了小型语言的词法分析、语法制导翻译、四元式地址回填和汇编目标代码生成。选择递归下降分析的原因是课程设计语言规模较小，递归下降能清晰表达语句结构与语义动作，也便于调试和展示。若后续继续扩展，可加入减法、除法、数组、过程调用、类型检查、错误恢复，以及真正可由 MASM/TASM 汇编运行的输入输出支持。"),
            p("十二、验证说明", "Heading1"),
            p("当前工作区已使用 g++ 14.2.0 编译通过，并执行 tests\\run_tests.bat，所有测试通过。说明文档由标准库生成 DOCX 包结构；当前环境未安装 LibreOffice/soffice，因此无法执行 DOCX 到 PNG 的视觉渲染检查，但已对 DOCX 的 XML 结构和主要文本内容进行程序化校验。"),
        ]
    )
    sect = (
        '<w:sectPr><w:pgSz w:w="12240" w:h="15840"/>'
        '<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440" w:header="708" w:footer="708" w:gutter="0"/>'
        "</w:sectPr>"
    )
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
        f"<w:body>{''.join(parts)}{sect}</w:body></w:document>"
    )


def styles_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Calibri" w:hAnsi="Calibri" w:eastAsia="宋体"/><w:sz w:val="22"/><w:szCs w:val="22"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after="120" w:line="264" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults>
  <w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/><w:pPr><w:spacing w:after="120" w:line="264" w:lineRule="auto"/></w:pPr><w:rPr><w:rFonts w:ascii="Calibri" w:hAnsi="Calibri" w:eastAsia="宋体"/><w:sz w:val="22"/><w:szCs w:val="22"/></w:rPr></w:style>
  <w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/><w:pPr><w:keepNext/><w:spacing w:before="320" w:after="160"/></w:pPr><w:rPr><w:b/><w:color w:val="2E74B5"/><w:sz w:val="32"/><w:szCs w:val="32"/></w:rPr></w:style>
  <w:style w:type="paragraph" w:styleId="Code"><w:name w:val="Code"/><w:basedOn w:val="Normal"/><w:pPr><w:spacing w:after="0" w:line="240" w:lineRule="auto"/><w:shd w:fill="F7F9FB"/></w:pPr><w:rPr><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas" w:eastAsia="Consolas"/><w:sz w:val="19"/><w:szCs w:val="19"/></w:rPr></w:style>
  <w:style w:type="paragraph" w:styleId="TableHeader"><w:name w:val="Table Header"/><w:basedOn w:val="Normal"/><w:pPr><w:spacing w:after="60"/></w:pPr><w:rPr><w:b/><w:color w:val="0B2545"/><w:sz w:val="20"/><w:szCs w:val="20"/></w:rPr></w:style>
  <w:style w:type="paragraph" w:styleId="TableBody"><w:name w:val="Table Body"/><w:basedOn w:val="Normal"/><w:pPr><w:spacing w:after="60"/></w:pPr><w:rPr><w:sz w:val="20"/><w:szCs w:val="20"/></w:rPr></w:style>
</w:styles>"""


def numbering_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:abstractNum w:abstractNumId="1"><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="bullet"/><w:lvlText w:val="•"/><w:lvlJc w:val="left"/><w:pPr><w:tabs><w:tab w:val="num" w:pos="720"/></w:tabs><w:ind w:left="720" w:hanging="360"/></w:pPr></w:lvl></w:abstractNum>
  <w:num w:numId="1"><w:abstractNumId w:val="1"/></w:num>
  <w:abstractNum w:abstractNumId="2"><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="decimal"/><w:lvlText w:val="%1."/><w:lvlJc w:val="left"/><w:pPr><w:tabs><w:tab w:val="num" w:pos="720"/></w:tabs><w:ind w:left="720" w:hanging="360"/></w:pPr></w:lvl></w:abstractNum>
  <w:num w:numId="2"><w:abstractNumId w:val="2"/></w:num>
</w:numbering>"""


def write_docx() -> None:
    OUT.parent.mkdir(exist_ok=True)
    files = {
        "[Content_Types].xml": """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/><Override PartName="/word/numbering.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"/><Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/><Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/></Types>""",
        "_rels/.rels": """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/></Relationships>""",
        "word/_rels/document.xml.rels": """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/></Relationships>""",
        "word/document.xml": document_xml(),
        "word/styles.xml": styles_xml(),
        "word/numbering.xml": numbering_xml(),
        "docProps/core.xml": """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><dc:title>编译技术课程设计说明文档 C++版</dc:title><dc:creator>Codex</dc:creator><dcterms:created xsi:type="dcterms:W3CDTF">2026-06-24T00:00:00Z</dcterms:created></cp:coreProperties>""",
        "docProps/app.xml": """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties"><Application>Python standard library</Application></Properties>""",
    }
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, content in files.items():
            zf.writestr(name, content)


if __name__ == "__main__":
    write_docx()
    print(OUT)

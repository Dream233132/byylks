$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

New-Item -ItemType Directory -Force -Path build | Out-Null
g++ -std=c++17 -Iinclude src\Compiler.cpp src\main.cpp -o build\mini_compiler.exe

function Assert-Equal {
    param(
        [string]$Actual,
        [string]$Expected,
        [string]$Name
    )
    $a = $Actual.Trim() -replace "`r`n", "`n"
    $e = $Expected.Trim() -replace "`r`n", "`n"
    if ($a -ne $e) {
        Write-Host "FAILED: $Name" -ForegroundColor Red
        Write-Host "Expected:"
        Write-Host $e
        Write-Host "Actual:"
        Write-Host $a
        exit 1
    }
    Write-Host "PASSED: $Name"
}

function Assert-Contains {
    param(
        [string]$Actual,
        [string]$Needle,
        [string]$Name
    )
    if (-not $Actual.Contains($Needle)) {
        Write-Host "FAILED: $Name" -ForegroundColor Red
        Write-Host "Missing: $Needle"
        exit 1
    }
    Write-Host "PASSED: $Name"
}

build\mini_compiler.exe examples\pas.dat --tokens examples\pas.tok | Out-Null

$expectedPas = @"
100 (j>,a,b,102)
101 (j,,,117)
102 (j>=,m,n,104)
103 (j,,,107)
104 (+,a,1,T1)
105 (:=,T1,,a)
106 (j,,,112)
107 (j=,k,h,109)
108 (j,,,112)
109 (+,x,2,T2)
110 (:=,T2,,x)
111 (j,,,107)
112 (+,m,y,T3)
113 (*,x,T3,T4)
114 (+,n,T4,T5)
115 (:=,T5,,m)
116 (j,,,100)
"@
Assert-Equal (Get-Content examples\pas.med -Raw) $expectedPas "task sample quadruples"

$expr = "A:=A+B*C#~"
Set-Content -Path build\expr.dat -Value $expr -NoNewline
build\mini_compiler.exe build\expr.dat --no-asm | Out-Null
$expectedExpr = @"
100 (*,B,C,T1)
101 (+,A,T1,T2)
102 (:=,T2,,A)
"@
Assert-Equal (Get-Content build\expr.med -Raw) $expectedExpr "arithmetic precedence"

$bool = "while A<C and B<D do A:=A+1#~"
Set-Content -Path build\bool.dat -Value $bool -NoNewline
build\mini_compiler.exe build\bool.dat | Out-Null
$boolMed = Get-Content build\bool.med -Raw
Assert-Contains $boolMed "(and," "boolean and quadruple"
Assert-Contains $boolMed "(j,,,100)" "while back jump"

$logic = "if not A=B or C>D then X:=1 else X:=2#~"
Set-Content -Path build\logic.dat -Value $logic -NoNewline
build\mini_compiler.exe build\logic.dat | Out-Null
$logicMed = Get-Content build\logic.med -Raw
Assert-Contains $logicMed "(or," "not/or compilation"

$asm = Get-Content examples\pas.asm -Raw
Assert-Contains $asm "L100:" "assembly labels"
Assert-Contains $asm "INT 21H" "assembly exit"

Set-Content -Path build\bad.dat -Value "A:=1" -NoNewline
$ErrorActionPreference = "Continue"
build\mini_compiler.exe build\bad.dat 2>$null | Out-Null
$ErrorActionPreference = "Stop"
if ($LASTEXITCODE -eq 0) {
    Write-Host "FAILED: syntax error handling" -ForegroundColor Red
    exit 1
}
Write-Host "PASSED: syntax error handling"

Write-Host "All tests passed."

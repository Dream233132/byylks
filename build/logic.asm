; 8086/8088-style assembly generated from quadruples
.MODEL SMALL
.STACK 100H
.DATA
A DW 0
B DW 0
C DW 0
D DW 0
T1 DW 0
T2 DW 0
T3 DW 0
X DW 0
.CODE
START:
    MOV AX, @DATA
    MOV DS, AX
L100:
    MOV AX, A
    CMP AX, B
    JNE L103
L101:
    MOV AX, 0
    MOV T1, AX
L102:
    JMP L104
L103:
    MOV AX, 1
    MOV T1, AX
L104:
    MOV AX, C
    CMP AX, D
    JG L107
L105:
    MOV AX, 0
    MOV T2, AX
L106:
    JMP L108
L107:
    MOV AX, 1
    MOV T2, AX
L108:
    MOV AX, T1
    OR AX, T2
    MOV T3, AX
L109:
    MOV AX, T3
    CMP AX, 0
    JNE L111
L110:
    JMP L113
L111:
    MOV AX, 1
    MOV X, AX
L112:
    JMP LEND
L113:
    MOV AX, 2
    MOV X, AX
LEND:
    MOV AH, 4CH
    INT 21H
END START

; Online ICG B2
format ELF executable 3
entry main

segment readable writeable
    var_a dd 0
    var_b dd 0

segment readable executable
main:
    PUSH EBP
    MOV  EBP, ESP
    SUB  ESP, 8
    ; Line 5: int x, y;
    ; Line 7: a = 10;
    MOV EAX, 10
    MOV [var_a], EAX
    ; Line 9: b = 20;
    MOV EAX, 20
    MOV [var_b], EAX
    ; Line 11: x = 100;
    MOV EAX, 100
    MOV [EBP-4], EAX
    ; Line 13: y = 200;
    MOV EAX, 200
    MOV [EBP-8], EAX
    ; Line 15: println(a, b, x, y);
    MOV EAX, [var_a]
    CALL OUTDEC
    MOV EAX, [var_b]
    CALL OUTDEC
    MOV EAX, [EBP-4]
    CALL OUTDEC
    MOV EAX, [EBP-8]
    CALL OUTDEC
    ; Line 17: println(a * 2 + b, x + y - 10, 3 + 3 * 7);
    MOV EAX, [var_b]
    PUSH EAX
    MOV EAX, 2
    PUSH EAX
    MOV EAX, [var_a]
    POP EBX
    MUL EBX
    POP EBX
    ADD EAX, EBX
    CALL OUTDEC
    MOV EAX, 10
    PUSH EAX
    MOV EAX, [EBP-8]
    PUSH EAX
    MOV EAX, [EBP-4]
    POP EBX
    ADD EAX, EBX
    POP EBX
    SUB EAX, EBX
    CALL OUTDEC
    MOV EAX, 7
    PUSH EAX
    MOV EAX, 3
    POP EBX
    MUL EBX
    PUSH EAX
    MOV EAX, 3
    POP EBX
    ADD EAX, EBX
    CALL OUTDEC
    ; Line 19: println(a == b, a && b, a++, -a);
    MOV EAX, [var_b]
    PUSH EAX
    MOV EAX, [var_a]
    POP EBX
    CMP EAX, EBX
    JE L0_true
    MOV EAX, 0
    JMP L0_end
L0_true:
    MOV EAX, 1
L0_end:
    CALL OUTDEC
    ; [Phase 2] logical && / || not implemented in Phase 1
    MOV EAX, 0
    CALL OUTDEC
    PUSH dword [var_a]
    INC dword [var_a]
    POP EAX
    CALL OUTDEC
    MOV EAX, [var_a]
    NEG EAX
    CALL OUTDEC
    ; Line 21: return 0;
    MOV EAX, 0
    JMP main_exit
main_exit:
    ADD  ESP, 8
    POP  EBP
    MOV EAX, 1
    XOR EBX, EBX
    INT 0x80


; ---- OUTDEC: prints the signed 32-bit integer in EAX as decimal + newline ----
OUTDEC:
    PUSH EBX
    PUSH ECX
    PUSH EDX
    PUSH ESI
    OR   EAX, EAX
    JGE  OUTDEC_POSITIVE
    NEG  EAX
    PUSH EAX
    SUB  ESP, 4
    MOV  byte [ESP], '-'
    MOV  EAX, 4
    MOV  EBX, 1
    MOV  ECX, ESP
    MOV  EDX, 1
    INT  0x80
    ADD  ESP, 4
    POP  EAX
OUTDEC_POSITIVE:
    XOR  ECX, ECX
    MOV  EBX, 10
OUTDEC_DIGIT_LOOP:
    XOR  EDX, EDX
    DIV  EBX
    ADD  DL, 30h
    PUSH EDX
    INC  ECX
    TEST EAX, EAX
    JNZ  OUTDEC_DIGIT_LOOP
    MOV  ESI, ECX
    MOV  EBX, 1
    MOV  EDX, 1
OUTDEC_PRINT_LOOP:
    TEST ESI, ESI
    JZ   OUTDEC_NEWLINE
    MOV  EAX, 4
    MOV  ECX, ESP
    INT  0x80
    ADD  ESP, 4
    DEC  ESI
    JMP  OUTDEC_PRINT_LOOP
OUTDEC_NEWLINE:
    SUB  ESP, 4
    MOV  byte [ESP], 10
    MOV  EAX, 4
    MOV  ECX, ESP
    INT  0x80
    ADD  ESP, 4
    POP  ESI
    POP  EDX
    POP  ECX
    POP  EBX
    RET

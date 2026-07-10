; factorial(5) = 120 using CALL/RET + pure stack ops
    PUSH 5
    CALL fact
    PRINTLN
    HALT

; fact: TOS=[n] -> TOS=[n!]
fact:
    DUP
    PUSH 1
    LE            ; n <= 1 ?
    JNZ base
    DUP           ; [n, n]
    PUSH 1
    SUB           ; [n, n-1]
    CALL fact     ; [n, (n-1)!]
    MUL           ; [n * (n-1)!]
    RET
base:
    POP           ; drop n (0 or 1)
    PUSH 1        ; 0! = 1, 1! = 1
    RET

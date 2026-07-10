; 计算 1+2+...+10 = 55
    PUSH 0       ; sum = 0
    STORE 0
    PUSH 1       ; i = 1
    STORE 1
loop:
    LOAD 1       ; i
    PUSH 10
    LE           ; i <= 10?
    JZ done
    LOAD 0       ; sum += i
    LOAD 1
    ADD
    STORE 0
    LOAD 1       ; i++
    PUSH 1
    ADD
    STORE 1
    JMP loop
done:
    LOAD 0
    PRINT
    HALT

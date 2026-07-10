\ demo.fth — forth 解释器示例
\ 运行: xl run forth.xl -- run demo.fth

\ --- 自定义词 ---
: square dup * ;
: cube dup square * ;
: fact dup 1 > if dup 1 - recurse * then ;

\ --- 算术 ---
3 square . cr          \ 9
2 cube . cr            \ 8

\ --- 递归 ---
5 fact . cr            \ 120
10 fact . cr           \ 3628800

\ --- if / else / then ---
: sign dup 0 > if drop 1 else dup 0 < if drop -1 else drop 0 then then ;
7 sign . cr            \ 1
-7 sign . cr           \ -1
0 sign . cr            \ 0

\ --- begin / until: 倒数到 0 ---
: countdown begin dup . 1 - dup 0 < until drop ;
3 countdown cr         \ 3 2 1 0

\ --- do / loop: 求和 1..5 ---
: sum1to 0 6 1 do i + loop ;
sum1to . cr            \ 15

\ --- do / loop + square: 平方和 ---
: sqsum 0 4 1 do i square + loop ;
sqsum . cr             \ 14  (1+4+9)

\ --- 栈操作 ---
1 2 swap . . cr        \ 1 2
1 2 over . . . cr      \ 1 2 1

\ --- 栈转储 ---
1 2 3 .s               \ <3> 1 2 3

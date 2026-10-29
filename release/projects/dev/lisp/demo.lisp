; demo.lisp - 演示 lisp 解释器能力
; 运行: lisp run demo.lisp
; 注：display 自动换行（简化实现），故无需额外 (newline)。

; 基本算术
(display (+ 1 2 3))

; 递归 factorial
(define (factorial n) (if (= n 0) 1 (* n (factorial (- n 1)))))
(display (factorial 10))

; 高阶函数：map
(define (map f lst) (if (null? lst) nil (cons (f (car lst)) (map f (cdr lst)))))
(display (map (lambda (x) (* x x)) '(1 2 3 4 5)))

; 闭包 + set!：计数器
(define (make-counter) (define n 0) (lambda () (set! n (+ n 1)) n))
(define c (make-counter))
(c)
(c)
(display (c))

; let 与 cond
(display (let ((x 5) (y 3)) (cond ((> x y) 'x-bigger) (else 'y-bigger))))

; fibonacci
(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(display (fib 20))

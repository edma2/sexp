(define empty? (lambda (x) (eq? x '())))
(define length (lambda (ls)
                 (if (empty? ls) 0
                   (+ 1 (length (cdr ls))))))
(define fib (lambda (n)
              (if (= n 0) 0
                (if (= n 1) 1
                  (+ (fib (- n 1)) (fib (- n 2)))))))

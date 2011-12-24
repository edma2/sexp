(define empty? (lambda (x) (eq? x '())))
(define length (lambda (ls)
                 (if (empty? ls) 0
                   (+ 1 (length (cdr ls))))) 

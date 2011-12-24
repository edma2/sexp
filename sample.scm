(define length (lambda (ls) 
                 (if (eq? ls '()) 0 
                   (+ 1 (length (cdr ls))))) 

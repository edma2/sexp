sexp: sexp.c
	gcc -Wall -g sexp.c -o sexp
test: sexp
	./sexp < sample.scm
clean:
	rm -f sexp

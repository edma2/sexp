#include "sexpr.h"

void repl(void);

int main(void) {
        init();
        repl();
        cleanup();
        return 0;
}

void repl(void) {
        SExpr *input, *result;

        eof = 0;
        while (!eof) {
                input = parse(stdin, 0);
                if (input == NULL)
                        continue;
                result = eval(input, global);
                if (result == NULL) {
                        dealloc(input);
                        continue;
                }
                result->refs++;
                print(result);
                printf("\n");
                dealloc(input);
                result->refs--;
                dealloc(result);
        }
}

#ifndef SEXPR_H
#define SEXPR_H
#include "sexpr.h"
#endif

int main(void) {
        SExpr *input, *result;

        init();
        eof = 0;
        while (!eof) {
                input = parse(stdin, 0);
                if (input == NULL)
                        continue;
                result = eval(input, &global);
                if (result == NULL)
                        continue;
                print(result);
                printf("\n");
        }
        return 0;
}

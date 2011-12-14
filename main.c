#include "sexpr.h"

int main(void) {
        SExpr *exp, *result;

        while (1) {
                exp = parse(stdin, 0);
                if (exp == NULL) {
                        if (eof)
                                break;
                        printf("Parse error!\n");
                        continue;
                }
                exp->refCount++;
                result = eval(exp, &global);
                if (result == NULL) {
                        printf("Evaluation error!\n");
                        release(exp);
                        continue;
                }
                result->refCount++;
                print(result);
                printf("\n");
                release(exp);
                release(result);
        }
        freeframe(&global);
        return 0;
}

#include "sexpr.h"

int main(void) {
        SExpr *exp, *result;

        while (1) {
                exp = parse(stdin);
                if (exp == NULL)
                        break;
                exp->refCount++;
                result = eval(exp, &global);
                if (result == NULL) {
                        release(exp);
                        break;
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

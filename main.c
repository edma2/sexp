#include "sexpr.h"

int main(void) {
        SExpr *exp, *result;

        while (1) {
                exp = parse(stdin);
                if (exp == NULL)
                        break;
                result = eval(exp);
                if (result == NULL) {
                        release(exp);
                        break;
                }
                print(result);
                printf("\n");
                release(exp);
                if (exp != result)
                        release(result);
        }
        return 0;
}

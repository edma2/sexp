#include "sexpr.h"

int main(void) {
        SExpr *input, *result;

        init();
        while (1) {
                input = parse(stdin, 0);
                if (input == NULL)
                        break;

                result = eval(input, &global);
                if (result == NULL) {
                        dealloc(result);
                        break;
                }
                result->refs++;
                print(result);
                printf("\n");

                dealloc(input);
                result->refs--;
                dealloc(result);
        }
        return 0;
}

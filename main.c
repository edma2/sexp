#include "sexpr.h"

int main(void) {
        SExpr *input, *result;

        while (1) {
                input = parse(stdin, 0);
                if (input == NULL)
                        break;
                result = eval(input, &global);
                if (result == NULL) {
                        dealloc(input);
                        break;
                }
                result->refs++;
                dealloc(input);
                print(result);
                printf("\n");
                result->refs--;
                dealloc(result);
        }
        return 0;
}

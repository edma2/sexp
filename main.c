#include "sexpr.h"

int main(void) {
        SExpr *parsedExp, *evaledExp;

        while (1) {
                parsedExp = parse(stdin, 0);
                if (parsedExp == NULL) {
                        fprintf(stderr, "Parse error!\n");
                        break;
                }
                parsedExp->refCount++;

                evaledExp = eval(parsedExp, &global);
                if (evaledExp == NULL) {
                        parsedExp->refCount--;
                        release(parsedExp);
                        fprintf(stderr, "Eval error!\n");
                        continue;
                }
                evaledExp->refCount++;

                print(evaledExp);
                printf("\n");

                evaledExp->refCount--;
                release(evaledExp);
                parsedExp->refCount--;
                release(parsedExp);
        }
        freeEnv(&global);
        return 0;
}

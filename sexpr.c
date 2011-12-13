#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL, 1};
int parensDepth;

static SExpr *parselist(FILE *f);

Frame *extend(Frame *env) {
        Frame *new;

        new = malloc(sizeof(Frame));
        if (new == NULL)
                return NULL;
        new->parent = env;
        memset(new->bindings, 0, SIZE);
        return new;
}

int add(SExpr *exp) {
        if (exp == &nil)
                return 0;
        return atoi(car(exp)->atom) + add(cdr(exp));
}

SExpr *evalmap(SExpr *exps) {
        if (exps == &nil)
                return exps;
        return cons(eval(car(exps)), evalmap(cdr(exps)));
}

SExpr *eval(SExpr *exp) {
        SExpr *list;

        if (exp->type == TYPE_ATOM)
                return exp;
        if (exp->type == TYPE_NIL)
                return exp;
        if (operator(exp)->atom[0] == '+') {
                list = evalmap(operands(exp));
                if (list == NULL)
                        return NULL;
                snprintf(buf, BUFLEN, "%d", add(list));
                release(list);
                return mkatom(buf);
        }
        return exp;
}

SExpr *operator(SExpr *exp) {
        return car(exp);
}

SExpr *operands(SExpr *exp) {
        return cdr(exp);
}

void release(SExpr *exp) {
        if (exp == &nil || exp->refCount > 0)
                return;
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else if (exp->type == TYPE_PAIR) {
                car(exp)->refCount--;
                cdr(exp)->refCount--;
                release(car(exp));
                release(cdr(exp));
        }
        free(exp);
}

SExpr *mkatom(char *str) {
        SExpr *exp;

        if (str == NULL)
                return NULL;
        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(str);
        if (exp->atom == NULL) {
                free(exp);
                return NULL;
        }
        exp->type = TYPE_ATOM;
        exp->refCount = 0;
        return exp;
}

SExpr *mkpair(SExpr *car, SExpr *cdr) {
        SExpr *exp;

        if (car == NULL || cdr == NULL) {
                if (car != NULL)
                        release(car);
                if (cdr != NULL)
                        release(cdr);
                return NULL;
        }
        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL) {
                release(car);
                release(cdr);
                return NULL;
        }
        car->refCount++;
        cdr->refCount++;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = TYPE_PAIR;
        exp->refCount = 0;
        return exp;
}

SExpr *parse(FILE *f) {
        int category;

        category = nexttok(f);
        if (category == ATOM) {
                return mkatom(buf);
        } else if (category == LPAREN) {
                parensDepth++;
                return parselist(f);
        } else if (category == RPAREN) {
                if (--parensDepth >= 0)
                        return &nil;
        } else if (category == QUOTE) {
                return cons(mkatom("quote"), cons(parse(f), &nil));
        }
        return NULL;
}

void print(SExpr *exp) {
        if (exp->type == TYPE_ATOM) {
                printf("%s", exp->atom);
        } else if (exp->type == TYPE_NIL) {
                printf("()");
        } else {
                printf("(");
                print(car(exp));
                printf(".");
                print(cdr(exp));
                printf(")");
        }
}

int nexttok(FILE *f) {
        char c;
        int i;

        while (1) {
                c = getc(f);
                if (c == EOF)
                        return END;
                if (isspace(c))
                        continue;
                if (c == '(')
                        return LPAREN;
                if (c == ')')
                        return RPAREN;
                if (c == '\'')
                        return QUOTE;
                break;
        }
        ungetc(c, f);
        for (i = 0; i < BUFLEN-1; i++) {
                c = getc(f);
                if (c == EOF)
                        return END;
                if (isreserved(c) || isspace(c))
                        break;
                buf[i] = c;
        }
        buf[i] = '\0';
        if (isreserved(c))
                ungetc(c, f);
        return ATOM;
}

static SExpr *parselist(FILE *f) {
        SExpr *exp;

        exp = parse(f);
        return exp == &nil ? exp : cons(exp, parselist(f));
}

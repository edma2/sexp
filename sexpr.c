#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL, 1};
int eof = 0;

void freeframe(Frame *f) {
        int i;
        Entry *ep, *tmp;

        for (i = 0; i < HSIZE; i++) {
                for (ep = f->bindings[i]; ep != NULL; ep = tmp) {
                        tmp = ep->next;
                        free(ep->key);
                        release(ep->value);
                        free(ep);
                }
        }
        if (f != &global)
                free(f);
}

Frame *extend(Frame *env) {
        Frame *new;

        new = malloc(sizeof(Frame));
        if (new == NULL)
                return NULL;
        new->parent = env;
        memset(new->bindings, 0, SIZE);
        return new;
}

SExpr *evaldefine(SExpr *exp, Frame *env) {
        SExpr *value;

        value = eval(caddr(exp), env);
        if (value == NULL)
                return NULL;
        if (define(cadr(exp), value, env) == NULL)
                return NULL;
        return mkatom("ok");
}

SExpr *define(SExpr *symbol, SExpr *exp, Frame *env) {
        Entry *e;

        e = find(env->bindings, symbol->atom);
        if (e == NULL) {
                e = insert(env->bindings, symbol->atom, exp);
                if (e == NULL)
                        return NULL;
        } else {
                release(e->value);
                e->value = exp;
        }
        exp->refCount++;
        return &nil;
}

SExpr *lookup(SExpr *symbol, Frame *env) {
        Frame *fp;
        Entry *e;

        for (fp = env; fp != NULL; fp = fp->parent) {
                e = find(env->bindings, symbol->atom);
                if (e != NULL)
                        return e->value;
        }
        printf("%s: undefined variable!\n", symbol->atom);
        return NULL;
}

int add(SExpr *exp) {
        if (exp == &nil)
                return 0;
        return atoi(car(exp)->atom) + add(cdr(exp));
}

SExpr *evalmap(SExpr *exps, Frame *env) {
        if (exps == &nil)
                return exps;
        return cons(eval(car(exps), env), evalmap(cdr(exps), env));
}

int isselfeval(SExpr *exp) {
        char *s;

        if (exp->type == TYPE_ATOM) {
                for (s = exp->atom; *s != '\0'; s++) {
                        if (!isdigit(*s))
                                return 0;
                }
                return 1;
        }
        return 0;
}

int issymbol(SExpr *exp) {
        return exp->type == TYPE_ATOM && !isselfeval(exp);
}

int isdefine(SExpr *exp) {
        return istaggedlist(exp, "define");
}

int isquoted(SExpr *exp) {
        return istaggedlist(exp, "quote");
}

int islambda(SExpr *exp) {
        return istaggedlist(exp, "lambda");
}

SExpr *evallambda(SExpr *exp, Frame *env) {
        exp->frame = env;
        return exp;
}

int istaggedlist(SExpr *exp, char *tag) {
        if (exp->type != TYPE_PAIR)
                return 0;
        if (car(exp)->type != TYPE_ATOM)
                return 0;
        return !strcmp(car(exp)->atom, tag);
}

SExpr *eval(SExpr *exp, Frame *env) {
        SExpr *op;

        if (isselfeval(exp))
                return exp;
        if (issymbol(exp))
                return lookup(exp, env);
        if (isdefine(exp))
                return evaldefine(exp, env);
        //op = eval(car(exp));


        /*
        if (operator(exp)->atom[0] == '+') {
                SExpr *list = evalmap(operands(exp), env);
                if (list == NULL)
                        return NULL;
                snprintf(buf, BUFLEN, "%d", add(list));
                release(list);
                return mkatom(buf);
        }
        */
        return exp;
}

SExpr *operator(SExpr *exp) {
        return car(exp);
}

SExpr *operands(SExpr *exp) {
        return cdr(exp);
}

void release(SExpr *exp) {
        if (exp == &nil || --(exp->refCount) > 0)
                return;
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else if (exp->type == TYPE_PAIR) {
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

SExpr *parse(FILE *f, int depth) {
        int category;
        SExpr *exp, *text;

        category = nexttok(f);
        if (category == END) {
                eof = 1;
                return NULL;
        } else if (category == LPAREN) {
                exp = parse(f, depth + 1);
        } else if (category == RPAREN) {
                return depth > 0 ? &nil : NULL;
        } else if (category == QUOTE) {
                text = parse(f, 0);
                exp = cons(mkatom("quote"), cons(text, &nil));
        } else {
                exp = mkatom(buf);
        }

        if (depth)
                exp = cons(exp, parse(f, depth));

        return exp;
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

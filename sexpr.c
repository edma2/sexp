#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL, 1};
int pdepth;

static SExpr *parselist(FILE *f);

void freeframe(Frame *f) {
        int i;
        Entry *ep, *tmp;

        for (i = 0; i < HSIZE; i++) {
                for (ep = f->bindings[i]; ep != NULL; ep = tmp) {
                        tmp = ep->next;
                        free(ep->key);
                        ((SExpr *)ep->value)->refCount--;
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

SExpr *evaldefinition(SExpr *exp, Frame *env) {
        define(cadr(exp), eval(caddr(exp), env), env);
        return cons(mkatom("quote"), cons(mkatom("ok"), &nil));
}

SExpr *define(SExpr *symbol, SExpr *exp, Frame *env) {
        Entry *e;

        e = find(env->bindings, symbol->atom);
        if (e == NULL) {
                e = insert(env->bindings, symbol->atom, exp);
                if (e == NULL)
                        return NULL;
        } else {
                ((SExpr *)(e->value))->refCount--;
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

int isselfevaluating(SExpr *exp) {
        char *s;

        if (exp->type == TYPE_NIL)
                return 1;
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
        return exp->type == TYPE_ATOM && !isselfevaluating(exp);
}

int isdefine(SExpr *exp) {
        return exp->type == TYPE_PAIR && !strcmp(car(exp)->atom, "define");
}

SExpr *eval(SExpr *exp, Frame *env) {
        if (isselfevaluating(exp))
                return exp;
        if (issymbol(exp))
                return lookup(exp, env);
        if (isdefine(exp))
                return evaldefinition(exp, env);
        if (operator(exp)->atom[0] == '+') {
                SExpr *list = evalmap(operands(exp), env);
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
                pdepth++;
                return parselist(f);
        } else if (category == RPAREN) {
                if (--pdepth >= 0)
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
                if (!strcmp(car(exp)->atom, "quote")) {
                        print(cadr(exp));
                } else {
                        printf("(");
                        print(car(exp));
                        printf(".");
                        print(cdr(exp));
                        printf(")");
                }
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

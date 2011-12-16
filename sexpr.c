#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL, 1};

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
        return define(cadr(exp), value, env);
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
        return mkatom("ok");
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
        SExpr *first, *rest, *exp;

        if (exps == &nil)
                return exps;
        first = eval(car(exps), env);
        if (first == NULL)
                return NULL;
        rest = evalmap(cdr(exps), env);
        if (rest == NULL) {
                release(first);
                return NULL;
        }
        exp = cons(first, rest);
        if (exp == NULL) {
                release(first);
                release(rest);
        }
        return exp;
}

int isselfeval(SExpr *exp) {
        return isnumber(exp);
}

int isnumber(SExpr *exp) {
        char *s;

        for (s = exp->atom; *s != '\0'; s++) {
                if (!isdigit(*s))
                        return 0;
        }
        return 1;
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
        if (isselfeval(exp))
                return exp;
        if (issymbol(exp))
                return lookup(exp, env);
        if (isdefine(exp))
                return evaldefine(exp, env);
        if (operator(exp)->atom[0] == '+') {
                SExpr *list = evalmap(operands(exp), env);
                if (list == NULL) {
                        release(exp);
                        return NULL;
                }
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

        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL)
                return NULL;
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
        SExpr *exp, *rest, *first;

        category = nexttok(f);
        if (category == LPAREN) {
                exp = parse(f, depth+1);
        } else if (category == RPAREN && depth) {
                return &nil;
        } else if (category == QUOTE) {
                rest = parse(f, 0);
                if (rest == NULL)
                        return NULL;
                exp = mkquote(rest);
                if (exp == NULL)
                        release(rest);
        } else if (category == ATOM) {
                exp = mkatom(buf);
        } else {
                /* Error or EOF */
                return NULL;
        }
        if (exp == NULL)
                return NULL;
        if (depth) {
                rest = parse(f, depth);
                if (rest == NULL) {
                        release(exp);
                        return NULL;
                }
                first = exp;
                exp = cons(first, rest);
                if (exp == NULL) {
                        release(first);
                        release(rest);
                }
        }
        return exp;
}

SExpr *mkquote(SExpr *text) {
        SExpr *tag, *list, *exp;

        tag = mkatom("quote");
        if (tag == NULL)
                return NULL;
        list = cons(text, &nil);
        if (list == NULL) {
                release(tag);
                return NULL;
        }
        exp = cons(tag, list);
        if (exp == NULL) {
                release(tag);
                release(list);
        }
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
        for (i = 0; i < BUFLEN-1; i++) {
                buf[i] = c;
                c = getc(f);
                if (c == EOF)
                        return END;
                if (isreserved(c) || isspace(c))
                        break;
        }
        buf[i+1] = '\0';
        if (isreserved(c))
                ungetc(c, f);
        return ATOM;
}

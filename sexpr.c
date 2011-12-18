#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL};

/* Returns a new atom with internal string value set to a newly allocated copy
 * of its argument. */
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

/* Create a new CONS cell from arguments, returning NULL on memory allocation
 * error. */
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

/* Check that arguments are non-NULL, otherwise release any remaining non-NULL
 * arguments and return NULL. Then call mkpair to return a new CONS cell. */
SExpr *cons(SExpr *car, SExpr *cdr) {
        if (car == NULL || cdr == NULL) {
                if (cdr != NULL)
                        release(cdr);
                if (car != NULL)
                        release(car);
                return NULL;
        }
        return mkpair(car, cdr);
}

/* Free resources associated with SExpr if its reference counter is greater
 * than zero. */
void release(SExpr *exp) {
        if (exp == &nil || exp->refCount > 0)
                return;
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else if (exp->type == TYPE_PAIR) {
                car(exp)->refCount--;
                release(car(exp));
                cdr(exp)->refCount--;
                release(cdr(exp));
        }
        free(exp);
}

/* Parse input stream into a single S-expression. */
SExpr *parse(FILE *f, int depth) {
        int category;
        SExpr *exp = NULL;

        category = nexttok(f);
        if (category == LPAREN)
                exp = parse(f, depth+1);
        else if (category == RPAREN && depth)
                return &nil;
        else if (category == QUOTE)
                exp = cons(mkatom("quote"), cons(parse(f, 0), &nil));
        else if (category == ATOM)
                exp = mkatom(buf);
        return (exp != NULL && depth) ? cons(exp, parse(f, depth)) : exp;
}

/* Map a new symbol to an S-expression in the environment.  Returns NULL on
 * error, or an "ok" atom otherwise. */
SExpr *define(SExpr *symbol, SExpr *exp, Frame *env) {
        Entry *e;
        int exists;

        e = find(env->bindings, symbol->atom);
        exists = (e != NULL);
        if (exists) {
                ((SExpr *)e->value)->refCount--;
                release(e->value);
                e->value = exp;
        } else {
                e = insert(env->bindings, symbol->atom, exp);
                if (e == NULL)
                        return NULL;
        }
        ((SExpr *)e->value)->refCount++;
        return mkatom("ok");
}

/* Return the value mapped to the symbol, or NULL if not found */
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

/* Evaluate every member of S-expression list. */
SExpr *evalmap(SExpr *exps, Frame *env) {
        return cons(eval(car(exps), env), evalmap(cdr(exps), env));
}

/* Evaluate S-expression in this environment. */
SExpr *eval(SExpr *exp, Frame *env) {
        if (exp == &nil)
                return &nil;
        if (isnumber(exp))
                return exp;
        if (issymbol(exp))
                return lookup(exp, env);
        if (isquoted(exp))
                return cadr(exp);
        if (isdefine(exp)) {
                SExpr *defval = eval(caddr(exp), env);
                if (defval == NULL)
                        return NULL;
                return define(cadr(exp), defval, env);
        }
        if (islambda(exp)) {
                exp->frame = env;
                return exp;
        }
        return NULL;
}

int isnumber(SExpr *exp) {
        char *s;

        if (exp->type != TYPE_ATOM)
                return 0;
        for (s = exp->atom; *s != '\0'; s++) {
                if (isdigit(*s))
                        return 1;
        }
        return 0;
}

int issymbol(SExpr *exp) {
        return exp->type == TYPE_ATOM && !isnumber(exp);
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

int istaggedlist(SExpr *exp, char *tag) {
        if (exp->type != TYPE_PAIR)
                return 0;
        if (car(exp)->type != TYPE_ATOM)
                return 0;
        return !strcmp(car(exp)->atom, tag);
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

void freeEnv(Frame *f) {
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

int primadd(SExpr *exps) {
        return exps == &nil ? 0 : atoi(car(exps)->atom) + primadd(cdr(exps));
}

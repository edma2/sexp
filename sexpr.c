#include "sexpr.h"

char buf[BUFLEN];
Frame global = {NULL, {0}};
SExpr nil = {{NULL}, TYPE_NIL};

SExpr *make_atom(char *s) {
        SExpr *exp;

        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(s);
        if (exp->atom == NULL) {
                free(exp);
                return NULL;
        }
        exp->type = TYPE_ATOM;
        exp->refs = 0;

        return exp;
}

SExpr *make_pair(SExpr *car, SExpr *cdr) {
        SExpr *exp;

        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL)
                return NULL;
        car->refs++;
        cdr->refs++;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = TYPE_PAIR;
        exp->refs = 0;

        return exp;
}

void dealloc(SExpr *exp) {
        if (exp->type == TYPE_NIL || exp->refs > 0)
                return;
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else {
                car(exp)->refs--;
                dealloc(car(exp));
                cdr(exp)->refs--;
                dealloc(cdr(exp));
        }
        free(exp);
}

SExpr *parse(FILE *f, int depth) {
        int category;
        SExpr *car, *cdr, *ls;

        category = read_token(f);
        if (category == LPAREN) {
                car = parse(f, depth+1);
        } else if (category == RPAREN) {
                return depth ? &nil : NULL;
        } else if (category == QUOTE) {
                car = make_atom("quote");
                if (car == NULL)
                        return NULL;
                cdr = parse(f, 0);
                if (cdr == NULL) {
                        dealloc(car);
                        return NULL;
                }
                ls = cons(cdr, &nil);
                if (ls == NULL) {
                        dealloc(car);
                        dealloc(cdr);
                        return NULL;
                }
                cdr = cons(car, ls);
                if (cdr == NULL) {
                        dealloc(car);
                        dealloc(ls);
                }
                car = cdr;
        } else if (category == ATOM) {
                car = make_atom(buf);
        } else {
                car = NULL;
        }
        if (!depth || car == NULL)
                return car;
        cdr = parse(f, depth);
        if (cdr == NULL) {
                dealloc(car);
                return NULL;
        }
        ls = cons(car, cdr);
        if (ls == NULL) {
                dealloc(car);
                dealloc(cdr);
        }
        return ls;
}

SExpr *eval(SExpr *exp, Frame *env) {
        if (is_nil(exp))
                return exp;
        if (is_number(exp))
                return exp;
        if (is_quoted(exp))
                return cadr(exp);
        if (is_symbol(exp))
                return env_lookup_symbol(exp->atom, env);
        if (is_define(exp)) {
                if (!env_bind_symbol(cadr(exp)->atom, caddr(exp), env))
                        return NULL;
                return &nil;
        }
        fprintf(stderr, "Unknown expression!\n");
        return NULL;
}

SExpr *env_lookup_symbol(char *sym, Frame *env) {
        Entry *en;

        en = find(env->bindings, sym);
        return en == NULL ? NULL : en->value;
}

int env_bind_symbol(char *sym, SExpr *val, Frame *env) {
        Entry *en;
        SExpr *old;

        en = find(env->bindings, sym);
        if (en == NULL) {
                en = insert(env->bindings, sym, val);
        } else {
                old = en->value;
                old->refs--;
                dealloc(old);
                en->value = val;
        }
        val->refs++;
        return en != NULL;
}

int atomic(SExpr *exp) {
        return exp->type == TYPE_ATOM;
}

int is_nil(SExpr *exp) {
        return exp == &nil;
}

int isnum(char *s) {
        for (; *s != '\0'; s++) {
                if (!isdigit(*s))
                        break;
        }
        return *s == '\0';
}

int is_number(SExpr *exp) {
        return atomic(exp) && isnum(exp->atom);
}

int is_symbol(SExpr *exp) {
        return atomic(exp) && !isnum(exp->atom);
}

int is_define(SExpr *exp) {
        return is_tagged_list(exp, "define");
}

int is_quoted(SExpr *exp) {
        return is_tagged_list(exp, "quote");
}

int is_lambda(SExpr *exp) {
        return is_tagged_list(exp, "lambda");
}

int is_tagged_list(SExpr *ls, char *tag) {
        if (ls->type != TYPE_PAIR)
                return 0;
        if (!atomic(car(ls)))
                return 0;
        return !strcmp(car(ls)->atom, tag);
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

int read_token(FILE *f) {
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

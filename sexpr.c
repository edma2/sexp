#include "sexpr.h"

char buf[BUFLEN];
Frame global = {.parent = NULL};
SExpr nil = {.type = TYPE_NIL};

SExpr Add = {.prim = prim_add, .type = TYPE_PRIM};
SExpr Sub = {.prim = prim_sub, .type = TYPE_PRIM};
SExpr Mult = {.prim = prim_mult, .type = TYPE_PRIM};
SExpr Div = {.prim = prim_div, .type = TYPE_PRIM};
SExpr Car = {.prim = prim_cdr, .type = TYPE_PRIM};
SExpr Cdr = {.prim = prim_car, .type = TYPE_PRIM};
SExpr Cons = {.prim = prim_cons, .type = TYPE_PRIM};

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
        exp->frame = NULL;
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
        exp->frame = NULL;
        return exp;
}

int unfreeable(SExpr *exp) {
        return primitive(exp) || exp == &nil;
}

void dealloc(SExpr *exp) {
        if (unfreeable(exp) || exp->refs > 0)
                return;
        if (atomic(exp)) {
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

SExpr *eval_list(SExpr *ls, Frame *env) {
        SExpr *car, *cdr, *exps;

        if (empty(ls))
                return &nil;
        car = eval(car(ls), env);
        if (car == NULL)
                return NULL;
        cdr = eval_list(cdr(ls), env);
        if (cdr == NULL) {
                dealloc(car);
                return NULL;
        }
        exps = cons(car, cdr);
        if (exps == NULL) {
                dealloc(car);
                dealloc(cdr);
        }
        return exps;
}

SExpr *eval(SExpr *exp, Frame *env) {
        SExpr *val, *op, *operands;

        if (empty(exp))
                return &nil;
        if (is_number(exp))
                return exp;
        if (is_quoted(exp))
                return cadr(exp);
        if (is_symbol(exp)) {
                val = env_lookup(exp->atom, env);
                if (val == NULL)
                        fprintf(stderr, "Unknown symbol!\n");
                return val;
        }
        if (is_define(exp)) {
                val = eval(caddr(exp), env);
                if (val == NULL)
                        return NULL;
                if (!env_bind(cadr(exp)->atom, val, env)) {
                        dealloc(val);
                        return NULL;
                }
                return &nil;
        }
        if (is_application(exp)) {
                op = eval(car(exp), env);
                if (op == NULL)
                        return NULL;
                operands = eval_list(cdr(exp), env);
                if (operands == NULL) {
                        dealloc(op);
                        return NULL;
                }
                val = apply(op, operands);
                dealloc(op);
                dealloc(operands);
                return val;
        }
        fprintf(stderr, "Unknown expression!\n");
        return NULL;
}

SExpr *apply(SExpr *op, SExpr *operands) {
        if (primitive(op))
                return op->prim(operands);
        return NULL;
}

SExpr *prim_add(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum += atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return make_atom(buf);
}

SExpr *prim_sub(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum -= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return make_atom(buf);
}

SExpr *prim_mult(SExpr *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args))
                prod *= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", prod);
        return make_atom(buf);
}

SExpr *prim_div(SExpr *args) {
        int quot;

        quot = atoi(car(args)->atom);
        args = cdr(args);
        for (; !empty(args); args = cdr(args))
                quot /= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", quot);
        return make_atom(buf);
}

SExpr *prim_cons(SExpr *args) {
        return cons(car(args), cadr(args));
}

SExpr *prim_car(SExpr *args) {
        return car(args);
}

SExpr *prim_cdr(SExpr *args) {
        return cdr(args);
}

void init(void) {
        insert(global.bindings, "+", &Add);
        insert(global.bindings, "-", &Sub);
        insert(global.bindings, "*", &Mult);
        insert(global.bindings, "/", &Div);
        insert(global.bindings, "car", &Car);
        insert(global.bindings, "cdr", &Cdr);
        insert(global.bindings, "cons", &Cons);
}

SExpr *env_lookup(char *sym, Frame *env) {
        Entry *en;

        en = find(env->bindings, sym);
        return en == NULL ? NULL : en->value;
}

int env_bind(char *sym, SExpr *val, Frame *env) {
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

int empty(SExpr *exp) {
        return exp->type == TYPE_NIL;
}

int pair(SExpr *exp) {
        return exp->type == TYPE_PAIR;
}

int primitive(SExpr *exp) {
        return exp->type == TYPE_PRIM;
}

int numeric(char *s) {
        for (; *s != '\0'; s++) {
                if (!isdigit(*s))
                        break;
        }
        return *s == '\0';
}

int is_application(SExpr *exp) {
        return pair(exp);
}

int is_number(SExpr *exp) {
        return atomic(exp) && numeric(exp->atom);
}

int is_symbol(SExpr *exp) {
        return atomic(exp) && !numeric(exp->atom);
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
        return pair(ls) && atomic(car(ls)) && !strcmp(car(ls)->atom, tag);
}

void print(SExpr *exp) {
        if (atomic(exp)) {
                printf("%s", exp->atom);
        } else if (empty(exp)) {
                printf("()");
        } else if (pair(exp)) {
                printf("(");
                print(car(exp));
                printf(".");
                print(cdr(exp));
                printf(")");
        } else if (primitive(exp)) {
                printf("PRIMITIVE");
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

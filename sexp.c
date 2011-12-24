#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BUFLEN 1024
#define POOLSIZE 256

#define isreserved(c) (c == ')' || c == '(' || c == '\'')

#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])
#define cadr(p) (car(cdr(p)))
#define caddr(p) (car(cdr(cdr(p))))
#define cadddr(p) (car(cdr(cdr(cdr(p)))))

enum category {QUOTE, LPAREN, RPAREN, STR, END};

typedef struct SExp SExp;
struct SExp {
        union {
                char *atom;
                SExp *pair[2];
                SExp *(*prim)(SExp *);
        };
        enum {ATOM, PAIR, NIL, PRIM} type;
        int live; /* gc flag */
};

/** Memory management */
SExp *alloc(void);
void gc(void);
void mark(SExp *exp);
void sweep(void);
void compact(void);
void reclaim(SExp *exp);

/** Constructors */
SExp *cons(SExp *car, SExp *cdr);
SExp *mkatom(char *str);
SExp *mkpair(SExp *car, SExp *cdr);
SExp *mkprim(SExp *(*prim)(SExp *));
SExp *mkproc(SExp *params, SExp *body, SExp *env);

/** I/O */
int readtoken(FILE *f);
SExp *parse(FILE *f, int depth);
void print(SExp *exp);

/** Evaluation */
SExp *apply(SExp *op, SExp *operands);
SExp *eval(SExp *exp, SExp *env);
SExp *evallist(SExp *ls, SExp *env);
int atomic(SExp *exp);
int compound(SExp *exp);
int empty(SExp *exp);
int number(SExp *exp);
int primproc(SExp *exp);
int tagged(SExp *ls, char *tag);

/** Environment */
SExp *envbind(SExp *var, SExp *val, SExp *env);
SExp *envlookup(SExp *var, SExp *env);
SExp *extend(SExp *args, SExp *params, SExp *env);

/** Primitives */
void init(void);
SExp *primadd(SExp *args);
SExp *primsub(SExp *args);
SExp *primmult(SExp *args);
SExp *primdiv(SExp *args);
SExp *primcons(SExp *args);
SExp *primcdr(SExp *args);
SExp *primcar(SExp *args);

char    buf[BUFLEN];            /* string buffer */
int     eof = 0;                /* end of file flag */
SExp   *pool[POOLSIZE];        /* heap references */
int     counter = 0;            /* next free node in pool */
SExp   *global;                /* global environment */
SExp   *nil;                   /* empty list */

void gc(void) {
        mark(global);
        mark(nil);
        sweep();
        compact();
}

SExp *mkatom(char *s) {
        SExp *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(s);
        if (exp->atom == NULL)
                return NULL;
        exp->type = ATOM;
        return exp;
}

SExp *mkpair(SExp *car, SExp *cdr) {
        SExp *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = PAIR;
        return exp;
}

SExp *mkprim(SExp *(*prim)(SExp *)) {
        SExp *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->prim = prim;
        exp->type = PRIM;
        return exp;
}

SExp *mknil(void) {
        SExp *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->type = NIL;
        return exp;
}

SExp *cons(SExp *car, SExp *cdr) {
        if (car == NULL || cdr == NULL)
                return NULL;
        return mkpair(car, cdr);
}

SExp *parse(FILE *f, int depth) {
        int category;
        SExp *car, *cdr;

        category = readtoken(f);
        if (category == END) {
                eof = 1;
                return NULL;
        } else if (category == LPAREN) {
                car = parse(f, depth+1);
        } else if (category == RPAREN) {
                return depth ? nil : NULL;
        } else if (category == QUOTE) {
                car = cons(mkatom("quote"), cons(parse(f, 0), nil));
        } else {
                car = mkatom(buf);
        }
        if (!depth)
                return car;
        cdr = parse(f, depth);
        return cons(car, cdr);
}

SExp *evallist(SExp *ls, SExp *env) {
        if (empty(ls))
                return nil;
        return cons(eval(car(ls), env), evallist(cdr(ls), env));
}

SExp *eval(SExp *exp, SExp *env) {
        SExp *op, *operands, *params, *body, *var, *val;

        if (empty(exp))
                return nil;
        if (atomic(exp)) {
                if (number(exp))
                        return exp;
                return envlookup(exp, env);
        }
        if (tagged(exp, "quote"))
                return cadr(exp);
        if (tagged(exp, "lambda")) {
                params = cadr(exp);
                body = caddr(exp);
                return mkproc(params, body, env);
        }
        if (tagged(exp, "define")) {
                var = cadr(exp);
                val = eval(caddr(exp), env);
                if (val == NULL)
                        return NULL;
                return envbind(var, val, env);
        }
        op = eval(car(exp), env);
        operands = evallist(cdr(exp), env);
        if (op == NULL || operands == NULL)
                return NULL;
        return apply(op, operands);
}

SExp *apply(SExp *op, SExp *operands) {
        SExp *body, *params, *env;

        if (primproc(op))
                return op->prim(operands);
        params = cadr(op);
        body = caddr(op);
        env = extend(params, operands, cadddr(op));
        if (env == NULL)
                return NULL;
        return eval(body, env);
}

SExp *extend(SExp *params, SExp *args, SExp *env) {
        SExp *frame = nil;

        for (; args != nil; args = cdr(args), params = cdr(params))
                frame = cons(cons(car(params), car(args)), frame);
        return cons(frame, env);
}

void reclaim(SExp *exp) {
        if (exp->type == ATOM)
                free(exp->atom);
        free(exp);
}

SExp *primadd(SExp *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum += atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExp *primsub(SExp *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum -= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExp *primmult(SExp *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args))
                prod *= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", prod);
        return mkatom(buf);
}

SExp *primdiv(SExp *args) {
        int quot;

        quot = atoi(car(args)->atom);
        args = cdr(args);
        for (; !empty(args); args = cdr(args))
                quot /= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", quot);
        return mkatom(buf);
}

SExp *primcons(SExp *args) {
        return cons(car(args), cadr(args));
}

SExp *primcar(SExp *args) {
        return car(car(args));
}

SExp *primcdr(SExp *args) {
        return cdr(car(args));
}

void init(void) {
        nil = mknil();
        global = cons(nil, nil);
        envbind(mkatom("+"), mkprim(primadd), global);
        envbind(mkatom("-"), mkprim(primsub), global);
        envbind(mkatom("*"), mkprim(primmult), global);
        envbind(mkatom("/"), mkprim(primdiv), global);
        envbind(mkatom("cons"), mkprim(primcons), global);
        envbind(mkatom("car"), mkprim(primcar), global);
        envbind(mkatom("cdr"), mkprim(primcdr), global);
}

int eq(SExp *e1, SExp *e2) {
        return !strcmp(e1->atom, e2->atom);
}

SExp *envlookup(SExp *var, SExp *env) {
        SExp *frame, *kv;

        for (; env != nil; env = cdr(env)) {
                for (frame = car(env); frame != nil; frame = cdr(frame)) {
                        kv = car(frame);
                        if (eq(var, car(kv)))
                                return cdr(kv);

                }
        }
        return NULL;
}

SExp *envbind(SExp *var, SExp *val, SExp *env) {
        SExp *frame, *kv;

        for (frame = car(env); frame != nil; frame = cdr(frame)) {
                kv = car(frame);
                if (eq(var, car(kv))) {
                        cdr(kv) = val;
                        return nil;
                }
        }
        car(env) = cons(cons(var, val), car(env));
        if (car(env) == NULL)
                return NULL;
        return nil;
}

int atomic(SExp *exp) {
        return exp->type == ATOM;
}

int compound(SExp *exp) {
        return exp->type == PAIR;
}

int empty(SExp *exp) {
        return exp->type == NIL;
}

int primproc(SExp *exp) {
        return exp->type == PRIM;
}

int number(SExp *exp) {
        char *s;

        if (!atomic(exp))
                return 0;
        for (s = exp->atom; *s != '\0'; s++) {
                if (!isdigit(*s))
                        return 0;
        }
        return 1;
}

int tagged(SExp *ls, char *tag) {
        return compound(ls) && atomic(car(ls)) && !strcmp(car(ls)->atom, tag);
}

void print(SExp *exp) {
        if (atomic(exp)) {
                printf("%s", exp->atom);
        } else if (empty(exp)) {
                printf("()");
        } else if (compound(exp)) {
                if (tagged(exp, "proc")) {
                        printf("PROC");
                } else {
                        printf("(");
                        print(car(exp));
                        printf(".");
                        print(cdr(exp));
                        printf(")");
                }
        } else if (primproc(exp)) {
                printf("<built-in>");
        }
}

int readtoken(FILE *f) {
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
        return STR;
}

SExp *alloc(void) {
        SExp *exp;

        if (counter == POOLSIZE)
                return NULL;
        exp = calloc(1, sizeof(SExp));
        if (exp == NULL)
                return NULL;
        pool[counter++] = exp;
        return exp;
}

void mark(SExp *exp) {
        if (exp->live)
                return;
        exp->live = 1;
        if (exp->type == PAIR) {
                mark(car(exp));
                mark(cdr(exp));
        }
}

SExp *mkproc(SExp *params, SExp *body, SExp *env) {
        return cons(mkatom("proc"), cons(params, cons(body, cons(env, nil))));
}

void sweep(void) {
        int i;
        SExp *exp;

        for (i = 0; i < counter; i++) {
                exp = pool[i];
                if (exp->live) {
                        exp->live = 0;
                } else {
                        reclaim(exp);
                        pool[i] = NULL;
                }
        }
}

void compact(void) {
        SExp *alive[counter];
        int i, j = 0;

        for (i = 0; i < counter; i++) {
                if (pool[i] != NULL)
                        alive[j++] = pool[i];
        }
        for (i = 0; i < j; i++)
                pool[i] = alive[i];
        counter = j;
}

int main(void) {
        SExp *input, *result;

        init();
        while (!eof) {
                input = parse(stdin, 0);
                if (input != NULL) {
                        result = eval(input, global);
                        if (result != NULL) {
                                print(result);
                                printf("\n");
                        } else {
                                fprintf(stderr, "Eval error!\n");
                        }
                } else {
                        if (eof)
                                break;
                        fprintf(stderr, "Parse error!\n");
                }
                gc();
        }
        sweep();
        return 0;
}

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Macros */
//{{{
#define BUFLEN 1024
#define MAXNODES 256

#define isreserved(c) (c == ')' || c == '(' || c == '\'')

#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])
#define cadr(p) (car(cdr(p)))
#define caddr(p) (car(cdr(cdr(p))))
#define cadddr(p) (car(cdr(cdr(cdr(p)))))

enum category {QUOTE, LPAREN, RPAREN, STR, END};
//}}}

/* Data structures */
//{{{
/* S-expressions */
typedef struct SExpr SExpr;
struct SExpr {
        union {
                char *atom;
                SExpr *pair[2];
                SExpr *(*prim)(SExpr *);
        };
        enum {ATOM, PAIR, NIL, PRIM} type;
        int live;
};

/* Garbage collection nodes */
typedef struct Node Node;
struct Node {
        void *data;
        enum {FRAME, SEXPR} type;
};
//}}}

/* Function declarations */
//{{{
void *alloc(void);
SExpr *apply(SExpr *op, SExpr *operands);
int atomic(SExpr *exp);
void compact(void);
int compound(SExpr *exp);
SExpr *cons(SExpr *car, SExpr *cdr);
int empty(SExpr *exp);
SExpr *envbind(SExpr *var, SExpr *val, SExpr *env);
SExpr *envlookup(SExpr *var, SExpr *env);
SExpr *evallist(SExpr *ls, SExpr *env);
SExpr *eval(SExpr *exp, SExpr *env);
SExpr *extend(SExpr *args, SExpr *params, SExpr *env);
void reclaim(SExpr *exp);
void gc(void);
void init(void);
int tagged(SExpr *ls, char *tag);
SExpr *mkatom(char *str);
SExpr *mkpair(SExpr *car, SExpr *cdr);
SExpr *mkprim(SExpr *(*prim)(SExpr *));
SExpr *mkproc(SExpr *params, SExpr *body, SExpr *env);
void mark(SExpr *exp);
int number(SExpr *exp);
SExpr *parse(FILE *f, int depth);
SExpr *primadd(SExpr *args);
SExpr *primsub(SExpr *args);
SExpr *primmult(SExpr *args);
SExpr *primdiv(SExpr *args);
SExpr *primcons(SExpr *args);
SExpr *primcdr(SExpr *args);
SExpr *primcar(SExpr *args);
void print(SExpr *exp);
int readtoken(FILE *f);
int primitive(SExpr *exp);
void sweep(void);
//}}}

/* Globals */
//{{{
char    buf[BUFLEN];            /* Token buffer */
int     eof = 0;                /* EOF flag */
SExpr   *global;                /* Global environment */
Node    Nodes[MAXNODES];        /* Heap references */
int     next_free_node = 0;     /* Next free Node */
SExpr   *nil;    /* Empty list */
SExpr   *roots[2];
//}}}

/* Function definitions */
//{{{
#if 0
void gc(void) {
        mark(global);
        sweep();
        compact();
}
#endif

SExpr *mkatom(char *s) {
        SExpr *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(s);
        if (exp->atom == NULL)
                return NULL;
        exp->type = ATOM;
        return exp;
}

SExpr *mkpair(SExpr *car, SExpr *cdr) {
        SExpr *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = PAIR;
        return exp;
}

SExpr *mkprim(SExpr *(*prim)(SExpr *)) {
        SExpr *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->prim = prim;
        exp->type = PRIM;
        return exp;
}

SExpr *mknil(void) {
        SExpr *exp;

        exp = alloc();
        if (exp == NULL)
                return NULL;
        exp->type = NIL;
        return exp;
}

SExpr *cons(SExpr *car, SExpr *cdr) {
        if (car == NULL || cdr == NULL)
                return NULL;
        return mkpair(car, cdr);
}

SExpr *parse(FILE *f, int depth) {
        int category;
        SExpr *car, *cdr;

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

SExpr *evallist(SExpr *ls, SExpr *env) {
        if (empty(ls))
                return nil;
        return cons(eval(car(ls), env), evallist(cdr(ls), env));
}

SExpr *eval(SExpr *exp, SExpr *env) {
        SExpr *op, *operands, *params, *body, *var, *val;

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

SExpr *apply(SExpr *op, SExpr *operands) {
        SExpr *body, *params, *env;

        if (primitive(op))
                return op->prim(operands);
        params = cadr(op);
        body = caddr(op);
        env = extend(params, operands, cadddr(op));
        if (env == NULL)
                return NULL;
        return eval(body, env);
}

SExpr *extend(SExpr *params, SExpr *args, SExpr *env) {
        SExpr *frame = nil;

        for (; args != nil; args = cdr(args), params = cdr(params))
                frame = cons(cons(car(params), car(args)), frame);
        return cons(frame, env);
}

void reclaim(SExpr *exp) {
        if (exp->type == ATOM)
                free(exp->atom);
        free(exp);
}

SExpr *primadd(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum += atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExpr *primsub(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum -= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExpr *primmult(SExpr *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args))
                prod *= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", prod);
        return mkatom(buf);
}

SExpr *primdiv(SExpr *args) {
        int quot;

        quot = atoi(car(args)->atom);
        args = cdr(args);
        for (; !empty(args); args = cdr(args))
                quot /= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", quot);
        return mkatom(buf);
}

SExpr *primcons(SExpr *args) {
        return cons(car(args), cadr(args));
}

SExpr *primcar(SExpr *args) {
        return car(car(args));
}

SExpr *primcdr(SExpr *args) {
        return cdr(car(args));
}

void init(void) {
        nil = mknil();
        global = cons(nil, nil);
        roots[0] = nil;
        roots[1] = global;
        envbind(mkatom("+"), mkprim(primadd), global);
        envbind(mkatom("-"), mkprim(primsub), global);
        envbind(mkatom("*"), mkprim(primmult), global);
        envbind(mkatom("/"), mkprim(primdiv), global);
        envbind(mkatom("cons"), mkprim(primcons), global);
        envbind(mkatom("car"), mkprim(primcar), global);
        envbind(mkatom("cdr"), mkprim(primcdr), global);
}

int eq(SExpr *e1, SExpr *e2) {
        return !strcmp(e1->atom, e2->atom);
}

SExpr *envlookup(SExpr *var, SExpr *env) {
        SExpr *frame, *kv;

        for (; env != nil; env = cdr(env)) {
                for (frame = car(env); frame != nil; frame = cdr(frame)) {
                        kv = car(frame);
                        if (eq(var, car(kv)))
                                return cdr(kv);

                }
        }
        return NULL;
}

SExpr *envbind(SExpr *var, SExpr *val, SExpr *env) {
        SExpr *frame, *kv;

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

int atomic(SExpr *exp) {
        return exp->type == ATOM;
}

int compound(SExpr *exp) {
        return exp->type == PAIR;
}

int empty(SExpr *exp) {
        return exp->type == NIL;
}

int primitive(SExpr *exp) {
        return exp->type == PRIM;
}

int number(SExpr *exp) {
        char *s;

        if (!atomic(exp))
                return 0;
        for (s = exp->atom; *s != '\0'; s++) {
                if (!isdigit(*s))
                        return 0;
        }
        return 1;
}

int tagged(SExpr *ls, char *tag) {
        return compound(ls) && atomic(car(ls)) && !strcmp(car(ls)->atom, tag);
}

void print(SExpr *exp) {
        if (atomic(exp)) {
                printf("%s", exp->atom);
        } else if (empty(exp)) {
                printf("()");
        } else if (compound(exp)) {
                if (tagged(exp, "proc")) {
                        /* HACK: This avoids infinite loops because a
                         * procedure's environment (which is stuck to the
                         * procedure structure) may contain a definition for
                         * the procedure itself. */
                        printf("PROC");
                } else {
                        printf("(");
                        print(car(exp));
                        printf(".");
                        print(cdr(exp));
                        printf(")");
                }
        } else if (primitive(exp)) {
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

void *alloc(void) {
        void *data;
        Node *n;

        if (next_free_node == MAXNODES)
                return NULL;
        data = calloc(1, sizeof(SExpr));
        if (data == NULL)
                return NULL;
        n = &Nodes[next_free_node++];
        n->data = data;
        n->type = SEXPR;
        return data;
}

#if 0
void mark(SExpr *exp) {
        if (exp->live)
                return;
        exp->live = 1;
        if (exp->type == PAIR) {
                mark(car(exp));
                mark(cdr(exp));
        }
}
#endif

SExpr *mkproc(SExpr *params, SExpr *body, SExpr *env) {
        return cons(mkatom("proc"), cons(params, cons(body, cons(env, nil))));
}

#if 0
void sweep(void) {
        int i;
        Node *n;

        for (i = 0; i < next_free_node; i++) {
                n = &Nodes[i];
                if (data_alive(n)) {
                        data_unmark(n);
                } else {
                        data_free(n);
                        n->data = NULL;
                }
        }
}

void compact(void) {
        Node tmp[next_free_node];
        int i, j = 0;

        for (i = 0; i < next_free_node; i++) {
                if (Nodes[i].data != NULL)
                        tmp[j++] = Nodes[i];
        }
        for (i = 0; i < j; i++)
                Nodes[i] = tmp[i];
        next_free_node = j;
}
#endif
//}}}

int main(void) {
        SExpr *input, *result;

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
                //gc();
        }
        //sweep();
        return 0;
}

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BUFLEN 1024
#define POOLSIZE 4096

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

/** Error handling */
void seterr(char *msg);

/** Evaluation */
SExp *apply(SExp *op, SExp *operands);
SExp *eval(SExp *exp, SExp *env);
SExp *evallist(SExp *ls, SExp *env);
SExp *evallookup(SExp *exp, SExp *env);
SExp *evalif(SExp *exp, SExp *env);
SExp *evallambda(SExp *exp, SExp *env);
SExp *evaldefine(SExp *exp, SExp *env);
SExp *evalset(SExp *exp, SExp *env);
SExp *evalbegin(SExp *exp, SExp *env);
SExp *evalapply(SExp *exp, SExp *env);
int atomic(SExp *exp);
int compound(SExp *exp);
int empty(SExp *exp);
int number(SExp *exp);
int primproc(SExp *exp);
int tagged(SExp *ls, char *tag);
int length(SExp *exp);

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
SExp *primeq(SExp *args);
SExp *primcmp(SExp *args, int type);
SExp *primlt(SExp *args);
SExp *primgt(SExp *args);
SExp *primlte(SExp *args);
SExp *primgte(SExp *args);
SExp *primeql(SExp *args);
SExp *primset(SExp *args, int type);
SExp *primsetcar(SExp *args);
SExp *primsetcdr(SExp *args);

char    buf[BUFLEN];    /* string buffer */
char   *err = NULL;     /* for displaying errors */
int     eof = 0;        /* end of file flag */
int     verbose = 0;
SExp   *pool[POOLSIZE]; /* heap references */
int     counter = 0;    /* next free node in pool */
SExp   *global;         /* global environment */
SExp   *nil;            /* empty list */
SExp   *true;
SExp   *false;

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

SExp *mkproc(SExp *params, SExp *body, SExp *env) {
        return cons(mkatom("proc"), cons(params, cons(body, cons(env, nil))));
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
                if (!depth) {
                        seterr("unexpected close paren");
                        return NULL;
                }
                return nil;
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
        if (empty(exp))
                return nil;
        if (atomic(exp)) {
                if (number(exp))
                        return exp;
                return evallookup(exp, env);
        }
        if (tagged(exp, "if"))
                return evalif(exp, env);
        if (tagged(exp, "quote"))
                return cadr(exp);
        if (tagged(exp, "lambda"))
                return evallambda(exp, env);
        if (tagged(exp, "define"))
                return evaldefine(exp, env);
        if (tagged(exp, "set!"))
                return evalset(exp, env);
        if (tagged(exp, "begin"))
                return evalbegin(exp, env);
        return evalapply(exp, env);
}

SExp *evallookup(SExp *exp, SExp *env) {
        SExp *kv;

        kv = envlookup(exp, env);
        if (kv == NULL)
                return NULL;
        return cdr(kv);
}

SExp *evalif(SExp *exp, SExp *env) {
        SExp *conditional, *truepart, *falsepart;

        if (length(exp) == 4) {
                conditional = eval(cadr(exp), env);
                truepart = caddr(exp);
                falsepart = cadddr(exp);
                if (conditional == false)
                        return eval(falsepart, env);
                return eval(truepart, env);
        }
        seterr("malformed if statement");
        return NULL;
}

SExp *evallambda(SExp *exp, SExp *env) {
        SExp *params, *body;

        if (length(exp) == 3) {
                params = cadr(exp);
                body = caddr(exp);
                if (compound(params) || empty(params))
                        return mkproc(params, body, env);
        }
        seterr("malformed lambda statement");
        return NULL;
}

SExp *evaldefine(SExp *exp, SExp *env) {
        SExp *var, *val;

        if (length(exp) == 3) {
                if (compound(cadr(exp))) {
                        var = car(cadr(exp));
                        val = mkproc(cdr(cadr(exp)), caddr(exp), env);
                } else {
                        var = cadr(exp);
                        val = eval(caddr(exp), env);
                }
                if (val == NULL)
                        return NULL;
                if (atomic(var) && !number(var))
                        return envbind(var, val, env);
        }
        seterr("malformed define statement");
        return NULL;
}

SExp *evalset(SExp *exp, SExp *env) {
        SExp *kv, *var, *val;

        if (length(exp) == 3) {
                var = cadr(exp);
                val = eval(caddr(exp), env);
                if (atomic(var) && !number(var)) {
                        kv = envlookup(var, env);
                        if (val == NULL || kv == NULL)
                                return NULL;
                        cdr(kv) = val;
                        return mkatom("ok");
                }
        }
        seterr("malformed set! statement");
        return NULL;
}

SExp *evalbegin(SExp *exp, SExp *env) {
        SExp *seq, *result;

        for (seq = cdr(exp); seq != nil; seq = cdr(seq)) {
                result = eval(car(seq), env);
                if (result == NULL)
                        return NULL;
        }
        return result;
}

SExp *evalapply(SExp *exp, SExp *env) {
        SExp *op, *operands;

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
        if (length(params) != length(operands)) {
                seterr("wrong number of arguments");
                return NULL;
        }
        body = caddr(op);
        env = extend(params, operands, cadddr(op));
        if (env == NULL)
                return NULL;
        return eval(body, env);
}

int length(SExp *exp) {
        int len;

        for (len = 0; exp != nil; exp = cdr(exp))
                len++;
        return len;
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

        for (sum = 0; !empty(args); args = cdr(args)) {
                if (!number(car(args))) {
                        seterr("invalid argument to +");
                        return NULL;
                }
                sum += atoi(car(args)->atom);
        }
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExp *primsub(SExp *args) {
        int sum, cnt = 0;

        for (sum = 0; !empty(args); args = cdr(args)) {
                if (!number(car(args))) {
                        seterr("invalid argument to -");
                        return NULL;
                }
                if (cnt++ == 1)
                        sum *= -1;
                sum -= atoi(car(args)->atom);
        }
        snprintf(buf, BUFLEN, "%d", sum);
        return mkatom(buf);
}

SExp *primmult(SExp *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args)) {
                if (!number(car(args))) {
                        seterr("invalid argument to *");
                        return NULL;
                }
                prod *= atoi(car(args)->atom);
        }
        snprintf(buf, BUFLEN, "%d", prod);
        return mkatom(buf);
}

SExp *primdiv(SExp *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args)) {
                if (!number(car(args))) {
                        seterr("invalid argument to /");
                        return NULL;
                }
                prod /= atoi(car(args)->atom);
        }
        snprintf(buf, BUFLEN, "%d", prod);
        return mkatom(buf);
}

SExp *primcons(SExp *args) {
        return cons(car(args), cadr(args));
}

SExp *primcar(SExp *args) {
        if (!compound(car(args))) {
                seterr("invalid argument to car");
                return NULL;
        }
        return car(car(args));
}

SExp *primcdr(SExp *args) {
        if (!compound(car(args))) {
                seterr("invalid argument to cdr");
                return NULL;
        }
        return cdr(car(args));
}

SExp *primeq(SExp *args) {
        SExp *exp = car(args);
        for (; args != nil; args = cdr(args)) {
                if (car(args) != exp)
                        return false;
        }
        return true;
}

enum {CMP_LT, CMP_GT, CMP_LTE, CMP_GTE, CMP_EQL};
SExp *primcmp(SExp *args, int type) {
        int lhs, rhs, result;

        for (; args != nil; args = cdr(args)) {
                if (!number(car(args))) {
                        seterr("invalid argument to compare");
                        return NULL;
                }
                if (cdr(args) != nil) {
                        lhs = atoi(car(args)->atom);
                        rhs = atoi(cadr(args)->atom);
                        if (type == CMP_LT)
                                result = lhs < rhs;
                        else if (type == CMP_GT)
                                result = lhs > rhs;
                        else if (type == CMP_LTE)
                                result = lhs <= rhs;
                        else if (type == CMP_GTE)
                                result = lhs >= rhs;
                        else
                                result = lhs == rhs;
                        if (!result)
                                return false;
                }
        }
        return true;
}

SExp *primlt(SExp *args) {
        return primcmp(args, CMP_LT);
}

SExp *primgt(SExp *args) {
        return primcmp(args, CMP_GT);
}

SExp *primlte(SExp *args) {
        return primcmp(args, CMP_LTE);
}

SExp *primgte(SExp *args) {
        return primcmp(args, CMP_GTE);
}

SExp *primeql(SExp *args) {
        return primcmp(args, CMP_EQL);
}

enum {SETCAR, SETCDR};
SExp *primset(SExp *args, int type) {
        SExp *pair, *val;

        if (length(args) != 3) {
                seterr("left side is atomic");
                return NULL;
        }
        pair = car(args);
        val = cadr(args);
        if (type == SETCAR)
                car(pair) = val;
        else
                cdr(pair) = val;
        return mkatom("ok");
}

SExp *primsetcar(SExp *args) {
        return primset(args, SETCAR);
}

SExp *primsetcdr(SExp *args) {
        return primset(args, SETCDR);
}

void init(void) {
        nil = mknil();
        global = cons(nil, nil);
        envbind(mkatom("#t"), true = mkatom("#t"), global);
        envbind(mkatom("#f"), false = mkatom("#f"), global);
        envbind(mkatom("+"), mkprim(primadd), global);
        envbind(mkatom("-"), mkprim(primsub), global);
        envbind(mkatom("*"), mkprim(primmult), global);
        envbind(mkatom("/"), mkprim(primdiv), global);
        envbind(mkatom("cons"), mkprim(primcons), global);
        envbind(mkatom("car"), mkprim(primcar), global);
        envbind(mkatom("cdr"), mkprim(primcdr), global);
        envbind(mkatom("eq?"), mkprim(primeq), global);
        envbind(mkatom("<"), mkprim(primlt), global);
        envbind(mkatom(">"), mkprim(primgt), global);
        envbind(mkatom("<="), mkprim(primlte), global);
        envbind(mkatom(">="), mkprim(primgte), global);
        envbind(mkatom("="), mkprim(primeql), global);
        envbind(mkatom("set-car!"), mkprim(primsetcar), global);
        envbind(mkatom("set-cdr!"), mkprim(primsetcdr), global);
}

SExp *envlookup(SExp *var, SExp *env) {
        SExp *frame, *kv;

        for (; env != nil; env = cdr(env)) {
                for (frame = car(env); frame != nil; frame = cdr(frame)) {
                        kv = car(frame);
                        if (!strcmp(var->atom, car(kv)->atom))
                                return kv;

                }
        }
        seterr("undefined variable");
        return NULL;
}

SExp *envbind(SExp *var, SExp *val, SExp *env) {
        SExp *frame, *kv;

        for (frame = car(env); frame != nil; frame = cdr(frame)) {
                kv = car(frame);
                if (!strcmp(var->atom, car(kv)->atom)) {
                        cdr(kv) = val;
                        return mkatom("ok");
                }
        }
        car(env) = cons(cons(var, val), car(env));
        if (car(env) == NULL)
                return NULL;
        return mkatom("ok");
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

void seterr(char *msg) {
        if (err == NULL)
                err = msg;
}

SExp *alloc(void) {
        SExp *exp;

        if (counter == POOLSIZE) {
                seterr("out of nodes");
                return NULL;
        }
        exp = calloc(1, sizeof(SExp));
        if (exp == NULL) {
                seterr("malloc failed");
                return NULL;
        }
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

void sweep(void) {
        int i;
        SExp *exp;
        int freed = 0;

        for (i = 0; i < counter; i++) {
                exp = pool[i];
                if (exp->live) {
                        exp->live = 0;
                } else {
                        freed++;
                        reclaim(exp);
                        pool[i] = NULL;
                }
        }
        if (verbose)
                fprintf(stderr, "Reclaimed %d nodes\n", freed);
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
        if (verbose)
                fprintf(stderr, "%d living nodes\n", counter);
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
                        }
                }
                if (err != NULL)
                        fprintf(stderr, "Error: %s\n", err);
                err = NULL;
                gc();
        }
        sweep();
        return 0;
}

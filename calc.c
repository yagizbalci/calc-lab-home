// Yagiz Balci 251ADB138
// https://github.com/yagizbalci/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: RTU lab instructions; OpenAI Codex (AI)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define NAME "yagiz"
#define LASTNAME "balci"
#define ID "251ADB138"

enum { END = 256, NUMBER, POWER, INVALID };
typedef struct { long long i; double d; int real; } Value;
typedef struct {
    char *line;
    size_t length, index, offset, pos, error;
    int token;
    Value value;
} Parser;

static void *resize(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q) { fprintf(stderr, "Not enough memory\n"); exit(1); }
    return q;
}

static void fail(Parser *p, size_t pos)
{
    if (!p->error) p->error = pos;
}

// AI (OpenAI Codex), prompt: implement the lab step by step, keeping to the teacher's requirements (translated from Turkish).
// Codex generated the tokenizer, parser, file handling and command-line code below in this conversation.
// The line offset counts all previous bytes, including comments and newlines, so positions stay file-relative.
static void next(Parser *p)
{
    char *s = p->line;
    while (p->index < p->length && (s[p->index] == ' ' || s[p->index] == '\t')) p->index++;
    p->pos = p->offset + p->index + 1;
    if (p->index == p->length) { p->token = END; return; }
    size_t start = p->index;
    unsigned char c = (unsigned char)s[p->index++];
    if (isdigit(c)) {
        while (p->index < p->length && isdigit((unsigned char)s[p->index])) p->index++;
        int real = 0;
        if (p->index + 1 < p->length && s[p->index] == '.' && isdigit((unsigned char)s[p->index + 1])) {
            real = 1;
            p->index++;
            while (p->index < p->length && isdigit((unsigned char)s[p->index])) p->index++;
        }
        char saved = s[p->index];
        s[p->index] = '\0';
        p->value = (Value){0, 0, real};
        if (real) p->value.d = strtod(s + start, NULL);
        else p->value.i = strtoll(s + start, NULL, 10);
        s[p->index] = saved;
        p->token = NUMBER;
        return;
    }
    if (c == '*' && p->index < p->length && s[p->index] == '*') {
        p->index++; p->token = POWER;
    } else if (c && strchr("+-*/()", c)) p->token = c;
    else p->token = INVALID;
}

// A float flag preserves integer division until either operand requires double arithmetic.
// Zero divisors and negative integer exponents point to the operand; non-finite results point to the operator.
static Value calculate(Parser *p, Value a, Value b, int op, size_t operator_pos, size_t right_pos)
{
    Value result = {0, 0, a.real || b.real};
    if (p->error) return result;
    if (op == '/' && (b.real ? b.d == 0 : b.i == 0)) { fail(p, right_pos); return result; }
    if (result.real) {
        double x = a.real ? a.d : (double)a.i;
        double y = b.real ? b.d : (double)b.i;
        if (op == '+') result.d = x + y;
        else if (op == '-') result.d = x - y;
        else if (op == '*') result.d = x * y;
        else if (op == '/') result.d = x / y;
        else result.d = pow(x, y);
        if (!isfinite(result.d)) fail(p, operator_pos);
    } else {
        if (op == '+') result.i = a.i + b.i;
        else if (op == '-') result.i = a.i - b.i;
        else if (op == '*') result.i = a.i * b.i;
        else if (op == '/') result.i = a.i / b.i;
        else {
            if (b.i < 0) { fail(p, right_pos); return result; }
            result.i = 1;
            while (b.i) {
                if (b.i % 2) result.i *= a.i;
                b.i /= 2;
                if (b.i) a.i *= a.i;
            }
        }
    }
    return result;
}

static Value expr(Parser *p);

static Value primary(Parser *p)
{
    Value v = {0, 0, 0};
    if (p->error) return v;
    if (p->token == NUMBER) { v = p->value; next(p); }
    else if (p->token == '(') {
        next(p); v = expr(p);
        if (!p->error) {
            if (p->token != ')') fail(p, p->pos);
            else next(p);
        }
    } else fail(p, p->pos);
    return v;
}

// Each level consumes the tighter level first. Recursing on the exponent makes 2**3**2 mean 2**(3**2).
static Value power(Parser *p)
{
    Value v = primary(p);
    if (!p->error && p->token == POWER) {
        size_t op = p->pos;
        next(p);
        size_t right = p->pos;
        Value b = power(p);
        v = calculate(p, v, b, POWER, op, right);
    }
    return v;
}

static Value term(Parser *p)
{
    Value v = power(p);
    while (!p->error && (p->token == '*' || p->token == '/')) {
        int op = p->token;
        size_t pos = p->pos;
        next(p);
        size_t right = p->pos;
        Value b = power(p);
        v = calculate(p, v, b, op, pos, right);
    }
    return v;
}

static Value expr(Parser *p)
{
    Value v = term(p);
    while (!p->error && (p->token == '+' || p->token == '-')) {
        int op = p->token;
        size_t pos = p->pos;
        next(p);
        size_t right = p->pos;
        Value b = term(p);
        v = calculate(p, v, b, op, pos, right);
    }
    return v;
}

static char *base_name(const char *path, int strip_txt)
{
    size_t end = strlen(path);
    while (end > 1 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start && path[start - 1] != '/') start--;
    size_t n = end - start;
    if (strip_txt && n >= 4 && memcmp(path + end - 4, ".txt", 4) == 0) n -= 4;
    char *s = resize(NULL, n + 1);
    memcpy(s, path + start, n); s[n] = '\0';
    return s;
}

static char *join(const char *a, const char *b)
{
    char *s = resize(NULL, strlen(a) + strlen(b) + 2);
    snprintf(s, strlen(a) + strlen(b) + 2, "%s/%s", a, b);
    return s;
}

// Doubling the reusable line buffer avoids copying the whole prefix for every new character.
// Results wait in a temporary file so a later syntax error can discard all earlier results.
static int process_file(const char *input, const char *outdir)
{
    FILE *in = fopen(input, "rb");
    if (!in) { perror(input); return 1; }
    FILE *results = tmpfile();
    if (!results) { perror("Temporary file"); fclose(in); return 1; }
    char *line = NULL;
    size_t capacity = 0, offset = 0, error = 0;
    int status = 0, c;
    do {
        size_t length = 0;
        while ((c = fgetc(in)) != EOF && c != '\n') {
            if (length + 1 >= capacity) {
                capacity = capacity ? capacity * 2 : 256;
                line = resize(line, capacity);
            }
            line[length++] = (char)c;
        }
        if (!line) { capacity = 256; line = resize(NULL, capacity); }
        line[length] = '\0';
        size_t first = 0;
        while (first < length && (line[first] == ' ' || line[first] == '\t')) first++;
        if (first < length && line[first] != '#') {
            Parser p = {0};
            p.line = line; p.length = length; p.offset = offset;
            next(&p);
            Value v = expr(&p);
            // An unexpected token, including end-of-line, is the point where parsing cannot continue.
            if (!p.error && p.token != END) fail(&p, p.pos);
            error = p.error;
            if (error) break;
            if (v.real) status = fprintf(results, "%.15g\n", v.d) < 0;
            else status = fprintf(results, "%lld\n", v.i) < 0;
            if (status) break;
        }
        offset += length + (c == '\n');
    } while (c != EOF);
    if (ferror(in)) status = 1;
    if (fclose(in)) status = 1;
    free(line);
    if (status) { fprintf(stderr, "Cannot read input or store results\n"); fclose(results); return 1; }
    char *base = base_name(input, 1);
    char *name = resize(NULL, strlen(base) + sizeof("_" NAME "_" LASTNAME "_" ID ".txt"));
    snprintf(name, strlen(base) + sizeof("_" NAME "_" LASTNAME "_" ID ".txt"), "%s_" NAME "_" LASTNAME "_" ID ".txt", base);
    char *path = join(outdir, name);
    FILE *out = fopen(path, "wb");
    if (!out) { perror(path); status = 1; }
    else {
        if (error) status = fprintf(out, "ERROR:%zu\n", error) < 0;
        else {
            if (fflush(results) || fseek(results, 0, SEEK_SET)) status = 1;
            char buffer[8192];
            size_t n;
            while (!status && (n = fread(buffer, 1, sizeof(buffer), results)) > 0)
                if (fwrite(buffer, 1, n, out) != n) status = 1;
            if (ferror(results)) status = 1;
        }
        if (fclose(out)) status = 1;
        if (status) fprintf(stderr, "Cannot write %s\n", path);
    }
    fclose(results);
    free(base); free(name); free(path);
    return status;
}

// AI assistance for file names and option handling uses the same lab prompt cited above.
int main(int argc, char **argv)
{
    const char *input = NULL, *dir = NULL, *outdir = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dir")) {
            if (dir || i + 1 == argc) goto usage;
            dir = argv[++i];
        } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output-dir")) {
            if (outdir || i + 1 == argc) goto usage;
            outdir = argv[++i];
        } else if (argv[i][0] == '-' || input) goto usage;
        else input = argv[i];
    }
    if ((!input && !dir) || (input && dir)) goto usage;
    struct stat info;
    const char *source = dir ? dir : input;
    if (stat(source, &info)) { perror(source); return 1; }
    if (dir ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)) {
        fprintf(stderr, "Wrong input type: %s\n", source); return 1;
    }
    char *generated = NULL;
    if (!outdir) {
        char *base = base_name(source, !dir);
        generated = resize(NULL, strlen(base) + sizeof("_" NAME "_" ID));
        snprintf(generated, strlen(base) + sizeof("_" NAME "_" ID), "%s_" NAME "_" ID, base);
        free(base); outdir = generated;
    }
    if (mkdir(outdir, 0777) && (errno != EEXIST || stat(outdir, &info) || !S_ISDIR(info.st_mode))) {
        perror(outdir); free(generated); return 1;
    }
    int status = 0;
    if (!dir) status = process_file(input, outdir);
    else {
        DIR *folder = opendir(dir);
        if (!folder) { perror(dir); free(generated); return 1; }
        struct dirent *entry;
        for (;;) {
            errno = 0;
            entry = readdir(folder);
            if (!entry) {
                if (errno) { perror(dir); status = 1; }
                break;
            }
            size_t n = strlen(entry->d_name);
            if (n < 4 || strcmp(entry->d_name + n - 4, ".txt")) continue;
            char *path = join(dir, entry->d_name);
            if (stat(path, &info)) { perror(path); status = 1; }
            else if (S_ISREG(info.st_mode) && process_file(path, outdir)) status = 1;
            free(path);
        }
        if (closedir(folder)) { perror(dir); status = 1; }
    }
    free(generated);
    return status;
usage:
    fprintf(stderr, "Usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return 1;
}

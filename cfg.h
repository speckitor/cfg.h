/*
 *  MIT License
 *
 *  Copyright (c) 2025 Pavel Loginov
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef CFG_H_
#define CFG_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Supported variable types
typedef enum {
    CFG_TYPE_NONE = 0, // If variable does not exist
    CFG_TYPE_INT = 1,
    CFG_TYPE_DOUBLE = 2,
    CFG_TYPE_BOOL = 4,
    CFG_TYPE_STRING = 8,
    CFG_TYPE_ARRAY = 16,
    CFG_TYPE_LIST = 32,
    CFG_TYPE_STRUCT = 64,
} Cfg_Type;

// Error types
typedef enum {
    CFG_ERROR_NONE = 0,
    CFG_ERROR_OPEN_FILE,
    CFG_ERROR_UNKNOWN_TOKEN,
    CFG_ERROR_UNEXPECTED_TOKEN,
    CFG_ERROR_VARIABLE_REDEFINITION,
    CFG_ERROR_VARIABLE_NOT_FOUND,
    CFG_ERROR_VARIABLE_WRONG_TYPE,
    CFG_ERROR_VARIABLE_PARSE,
    CFG_ERROR_COUNT,
} Cfg_Error_Type;

typedef struct Cfg_Variable Cfg_Variable;

struct Cfg_Variable {
    Cfg_Type type;
    char *name;
    char *value;
    Cfg_Variable *prev;
    Cfg_Variable *vars;
    size_t vars_len;
    size_t vars_cap;
};

// Public API functions declaration

// Loading file, returns 0 on success and 1 on fail
// If another file is already loaded it will automaticly unload it
int cfg_load_file(const char *file_path);
// Unload should be called to free allocated memory
// Make sure not to use any pointer after unloading (strings, arrays, lists, structs)
// If you want to save strings after unloading file you can use `strdup`
void cfg_unload(void);

// Get global context in file
Cfg_Variable *cfg_global_context(void);

// Get length of context
// Returns amount of inner variables
size_t cfg_get_context_len(Cfg_Variable *ctx);

// Get variable type by context and name/index
// Return CFG_TYPE_NONE on error
Cfg_Type cfg_get_type(Cfg_Variable *ctx, const char *name);
Cfg_Type cfg_get_type_elem(Cfg_Variable *ctx, size_t idx);

// Error information
Cfg_Error_Type cfg_get_error_type(void);
char *cfg_get_error_message(void);

// Get variables from provided context
// Context can be global or local (array, list, struct)
// Returns 0/0.0/false/NULL on error
int cfg_get_int(Cfg_Variable *ctx, const char *name);
double cfg_get_double(Cfg_Variable *ctx, const char *name);
bool cfg_get_bool(Cfg_Variable *ctx, const char *name);
char *cfg_get_string(Cfg_Variable *ctx, const char *name);
Cfg_Variable *cfg_get_array(Cfg_Variable *ctx, const char *name);
Cfg_Variable *cfg_get_list(Cfg_Variable *ctx, const char *name);
Cfg_Variable *cfg_get_struct(Cfg_Variable *ctx, const char *name);

// Safe verstions of functions above
// Return 0 on success, 1 on error
// To get more information about error see `cfg_get_error_type` and `cfg_get_error_message`
int cfg_get_int_safe(Cfg_Variable *ctx, const char *name, int *res);
int cfg_get_double_safe(Cfg_Variable *ctx, const char *name, double *res);
int cfg_get_bool_safe(Cfg_Variable *ctx, const char *name, bool *res);
int cfg_get_string_safe(Cfg_Variable *ctx, const char *name, char **res);
int cfg_get_array_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res);
int cfg_get_list_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res);
int cfg_get_struct_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res);

// Get variables by index
// Return 0/0.0/false/NULL on error
int cfg_get_int_elem(Cfg_Variable *ctx, size_t idx);
double cfg_get_double_elem(Cfg_Variable *ctx, size_t idx);
bool cfg_get_bool_elem(Cfg_Variable *ctx, size_t idx);
char *cfg_get_string_elem(Cfg_Variable *ctx, size_t idx);
Cfg_Variable *cfg_get_array_elem(Cfg_Variable *ctx, size_t idx);
Cfg_Variable *cfg_get_list_elem(Cfg_Variable *ctx, size_t idx);
Cfg_Variable *cfg_get_struct_elem(Cfg_Variable *ctx, size_t idx);

#endif // CFG_H_

#ifdef CFG_IMPLEMENTATION

// Private functions and types

#define INIT_VARIABLES_NUM 64
#define INIT_TOKENS_NUM 64
#define INIT_STRING_SIZE 64
#define INIT_STACK_SIZE 64
#define ERROR_MESSAGE_SIZE 256

typedef enum {
    // Types with string literal values
    CFG_TOKEN_EQ = 1,
    CFG_TOKEN_SEMICOLON = 2,
    CFG_TOKEN_COMMA = 4,
    CFG_TOKEN_LEFT_BRACKET = 8,
    CFG_TOKEN_RIGHT_BRACKET = 16,
    CFG_TOKEN_LEFT_PARENTHESIS = 32,
    CFG_TOKEN_RIGHT_PARENTHESIS = 64,
    CFG_TOKEN_LEFT_CURLY_BRACKET = 128,
    CFG_TOKEN_RIGHT_CURLY_BRACKET = 256,
    CFG_TOKEN_EOF = 512,
    // Types with dinamicly allocated values
    CFG_TOKEN_IDENTIFIER = 1024,
    CFG_TOKEN_INT = 2048,
    CFG_TOKEN_DOUBLE = 4096,
    CFG_TOKEN_BOOL = 8192,
    CFG_TOKEN_STRING = 16384,
} Cfg_Token_Type;

typedef struct {
    Cfg_Token_Type type;
    char *value;
    size_t line;
    size_t column;
} Cfg_Token;

typedef struct {
    char *values;
    size_t len;
    size_t cap;
} Cfg_Stack;

typedef struct {
    char *file;
    char *str_start;
    char *ch_current;
    Cfg_Token *tokens;
    size_t cur_token;
    size_t tokens_len;
    size_t tokens_cap;
    size_t line;
    size_t column;
    bool comment_eol;
    bool comment;
    Cfg_Stack stack;
} Cfg_Lexer;

typedef struct {
    char *message;
    Cfg_Error_Type type;
} Cfg_Error;

typedef struct {
    Cfg_Variable global;
    FILE *file;
    Cfg_Error err;
} Cfg_Config;

static Cfg_Config cfg;
static Cfg_Lexer lexer;

// Private functions forward declaration

// Custom malloc and realloc functions
static void *cfg__malloc(size_t size);
static void *cfg__realloc(void *prev, size_t size);

// Cfg_Config create and free
static void cfg__config_create(void);
static void cfg__config_free(void);

// Cfg_Lexer create and free
static void cfg__lexer_create(void);
static void cfg__lexer_free(void);

// Functions for parsing string from file
static void cfg__string_add_char(char **str, size_t *cap, char ch);
static char *cfg__lexer_parse_string(void);

// Add token to lexer
static void cfg__lexer_add_token(Cfg_Token_Type type, char *value);

// Stack functions for brakets and parenthesis evaluation
static void cfg__stack_add_char(char ch);
static void cfg__stack_pop_char(void);
static char cfg__stack_last_char(void);

// Cfg_Variable functions to add variable, free context or find variable
// `cfg__context_find_variable` return -1 on error
static void cfg__context_add_variable(Cfg_Variable *ctx, Cfg_Type type, char *name, char *value);
static void cfg__context_free(Cfg_Variable *ctx);
static int cfg__context_find_variable(Cfg_Variable *ctx, const char *name);

// File functions
// `cfg__file_open` and `cfg__file_parse` return 1 on error and 0 on success
// `cfg__file_get_str` return NULL on error and pointer to allocated string on success
static int cfg__file_open(const char *file_path);
static char *cfg__file_get_str(void);
static void cfg__file_tokenize(void);
static int cfg__file_parse(void);

// Private functions definition

static void cfg__config_create(void)
{
    cfg.global.vars = cfg__malloc(INIT_VARIABLES_NUM * sizeof(Cfg_Variable));
    cfg.global.name = NULL;
    cfg.global.value = NULL;
    cfg.global.prev = NULL;
    cfg.global.vars_len = 0;
    cfg.global.vars_cap = INIT_VARIABLES_NUM;
    cfg.err.type = CFG_ERROR_NONE;
    cfg.err.message = cfg__malloc(sizeof(char) * ERROR_MESSAGE_SIZE);
}

static void cfg__config_free(void)
{
    cfg__context_free(&cfg.global);
    if (cfg.err.message != NULL) free(cfg.err.message);
    if (cfg.file != NULL) fclose(cfg.file);
}

static void *cfg__malloc(size_t size)
{
    void *data = malloc(size);
    if (data == NULL) {
        perror("malloc");
        exit(1);
    }
    return data;
}

static void *cfg__realloc(void *prev, size_t size)
{
    void *data = realloc(prev, size);
    if (data == NULL) {
        perror("realloc");
        exit(1);
    }
    return data;
}

static void cfg__lexer_create(void)
{
    char *src = cfg__file_get_str();

    lexer.tokens = cfg__malloc(sizeof(Cfg_Token) * INIT_TOKENS_NUM);
    lexer.stack.values = cfg__malloc(sizeof(char) * INIT_STACK_SIZE);

    lexer.file = src;
    lexer.str_start = NULL;
    lexer.ch_current = src;

    lexer.cur_token = 0;
    lexer.tokens_len = 0;
    lexer.tokens_cap = INIT_TOKENS_NUM;

    lexer.line = 1;
    lexer.column = 1;

    lexer.comment_eol = false;
    lexer.comment = false;

    lexer.stack.cap = INIT_STACK_SIZE;
    lexer.stack.len = 0;
}

static void cfg__lexer_free(void)
{
    if (lexer.stack.values != NULL) free(lexer.stack.values);
    if (lexer.file != NULL) free(lexer.file);
    for (int i = 0; i < lexer.tokens_len; ++i) {
        if (lexer.tokens[i].type > CFG_TOKEN_EOF && lexer.tokens[i].value != NULL) {
            free(lexer.tokens[i].value);
        }
    }
    if (lexer.tokens != NULL) free(lexer.tokens);
}

static void cfg__lexer_add_token(Cfg_Token_Type type, char *value)
{
    if (lexer.tokens_len == lexer.tokens_cap) {
        lexer.tokens_cap *= 2;
        lexer.tokens = cfg__realloc(lexer.tokens, sizeof(Cfg_Token) * lexer.tokens_cap);
    }
    
    size_t idx = lexer.tokens_len++;
    memset(&lexer.tokens[idx], 0, sizeof(Cfg_Token));
    lexer.tokens[idx].type = type;
    lexer.tokens[idx].value = value;
    lexer.tokens[idx].line = lexer.line;
    lexer.tokens[idx].column = lexer.column - strlen(value);
}

static void cfg__stack_add_char(char ch)
{
    Cfg_Stack *stack = &lexer.stack;
    if (stack->len == stack->cap) {
        stack->cap *= 2;
        stack->values = cfg__realloc(stack->values, sizeof(char) * stack->cap);
    }
    stack->values[stack->len++] = ch;
}

static void cfg__stack_pop_char()
{
    Cfg_Stack *stack = &lexer.stack;
    if (stack->len != 0) {
        stack->values[--stack->len] = '\0';
    }
}

static char cfg__stack_last_char()
{
    Cfg_Stack *stack = &lexer.stack;
    if (stack->len == 0) return ' ';
    return stack->values[stack->len - 1];
}

static void cfg__string_add_char(char **str, size_t *cap, char ch)
{
    size_t len = strlen(*str);
    if (len + 2 > *cap) {
        *cap *= 2;
        *str = cfg__realloc(*str, sizeof(char) * (*cap));
    }
    (*str)[len] = ch;
    (*str)[len + 1] = '\0';
}

static char *cfg__lexer_parse_string(void)
{
    char *str = cfg__malloc(sizeof(char) * INIT_STRING_SIZE);
    str[0] = '\0';
    size_t cap = INIT_STRING_SIZE;

    char ch;
    bool backslash = false;
    while (*lexer.ch_current != '\0' && (*lexer.ch_current != '"' || backslash)) {
        if (*lexer.ch_current == '\\') {
            if (backslash) {
                ch = '\\';
                cfg__string_add_char(&str, &cap, ch);
                backslash = false;
                lexer.ch_current++;
                lexer.column++;
                continue;
            }
            backslash = true;
            lexer.ch_current++;
            lexer.column++;
            continue;
        }

        if (backslash) {
            switch (*lexer.ch_current) {
            case 'n':
                ch = '\n';
                break;
            case 't':
                ch = '\t';
                break;
            case '\"':
                ch = '\"';
                break;
            case '\'':
                ch = '\'';
                break;
            default:
                cfg__string_add_char(&str, &cap, '\\');
                ch = *lexer.ch_current;
                break;
            }
            backslash = false;
        } else {
            ch = *lexer.ch_current;
        }
        cfg__string_add_char(&str, &cap, ch);
        lexer.ch_current++;
        lexer.column++;
    }

    if (*lexer.ch_current == '\0') {
        free(str);
        return NULL;
    }

    lexer.ch_current++;
    lexer.column++;

    return str;
}

static void cfg__context_add_variable(Cfg_Variable *ctx, Cfg_Type type, char *name, char *value)
{
    if (ctx->vars_len == ctx->vars_cap) {
        ctx->vars_cap *= 2;
        ctx->vars = cfg__realloc(ctx->vars, sizeof(Cfg_Variable) * ctx->vars_cap);
        for (size_t i = 0; i < ctx->vars_len; ++i) {
            ctx->vars[i].prev = ctx;
        }
    }

    ctx->vars[ctx->vars_len].type = type;
    if (name != NULL) {
        for (size_t i = 0; i < ctx->vars_len; ++i) {
            if (strcmp(name, ctx->vars[i].name) == 0) {
                cfg.err.type = CFG_ERROR_VARIABLE_REDEFINITION;
                if (ctx->name != NULL) {
                    sprintf(cfg.err.message, "Redefined variable `%s` inside `%s` at line:%lu, column:%lu", name, ctx->name, lexer.tokens[lexer.cur_token - 3].line, lexer.tokens[lexer.cur_token - 3].column);
                } else {
                    sprintf(cfg.err.message, "Redefined variable `%s` at line:%lu, column:%lu", name, lexer.tokens[lexer.cur_token - 3].line, lexer.tokens[lexer.cur_token - 3].column);
                }
                return;
            }
        }
        ctx->vars[ctx->vars_len].name = strdup(name);
    } else {
        ctx->vars[ctx->vars_len].name = NULL;
    }
    if (value != NULL) {
        ctx->vars[ctx->vars_len].value = strdup(value);
    } else {
        ctx->vars[ctx->vars_len].value = NULL;
    }
    ctx->vars[ctx->vars_len].prev = ctx;
    if (type & CFG_TYPE_STRUCT || type & CFG_TYPE_ARRAY || type & CFG_TYPE_LIST) {
        ctx->vars[ctx->vars_len].vars = cfg__malloc(sizeof(Cfg_Variable) * INIT_VARIABLES_NUM);
        ctx->vars[ctx->vars_len].vars_cap = INIT_VARIABLES_NUM;
        ctx->vars[ctx->vars_len].vars_len = 0;
    } else {
        ctx->vars[ctx->vars_len].vars = NULL;
        ctx->vars[ctx->vars_len].vars_cap = 0;
        ctx->vars[ctx->vars_len].vars_len = 0;
    }
    ctx->vars_len++;
}

static int cfg__context_find_variable(Cfg_Variable *ctx, const char *name)
{
    for (size_t i = 0; i < ctx->vars_len; ++i) {
        if (strcmp(name, ctx->vars[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static void cfg__context_free(Cfg_Variable *ctx)
{
    if (ctx->vars != NULL) {
        for (size_t i = 0; i < ctx->vars_len; ++i) {
            cfg__context_free(&ctx->vars[i]);
        }
        free(ctx->vars);
    }
    if (ctx->name != NULL) free(ctx->name);
    if (ctx->value != NULL) free(ctx->value);
}

static int cfg__file_open(const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        cfg.err.type = CFG_ERROR_OPEN_FILE;
        sprintf(cfg.err.message, "Failed to open file: %s", file_path);
        return 1;
    }
    cfg.file = file;
    return 0;
}

static char *cfg__file_get_str(void)
{
    fseek(cfg.file, 0, SEEK_END);
    long size = ftell(cfg.file);
    fseek(cfg.file, 0, SEEK_SET);
    char *file_str = cfg__malloc(sizeof(char) * (size + 1));
    fread(file_str, sizeof(char), size, cfg.file);
    file_str[size] = '\0';
    return file_str;
}

static void cfg__file_tokenize(void)
{
    cfg__lexer_create();

    while (*lexer.ch_current != '\0') {
        if (*lexer.ch_current == '\n') {
            lexer.comment_eol = false;
            lexer.line++;
            lexer.column = 1;
            lexer.ch_current++;
            continue;
        }

        if (*lexer.ch_current == '/') {
            lexer.ch_current++;
            lexer.column++;
            if (*lexer.ch_current == '/') {
                lexer.comment_eol = true;
                lexer.ch_current++;
                lexer.column++;
                continue;
            } else if (*lexer.ch_current == '*') {
                lexer.comment = true;
                lexer.ch_current++;
                lexer.column++;
                continue;
            } else {
                cfg.err.type = CFG_ERROR_UNKNOWN_TOKEN;
                sprintf(cfg.err.message, "Unknown token at line:%lu, column:%lu", lexer.line, lexer.column);
                cfg__lexer_free();
                return;
            }
        }

        if (*lexer.ch_current == '*' && lexer.comment) {
            lexer.ch_current++;
            lexer.column++;
            if (*lexer.ch_current == '/') {
                lexer.comment = false;
                lexer.ch_current++;
                lexer.column++;
                continue;
            }
        }

        if (lexer.comment || lexer.comment_eol) {
            lexer.ch_current++;
            lexer.column++;
            continue;
        }

        switch (*lexer.ch_current) {
        case ' ':
            break;
        case '=':
            cfg__lexer_add_token(CFG_TOKEN_EQ, "=");
            break;
        case ';':
            cfg__lexer_add_token(CFG_TOKEN_SEMICOLON, ";");
            break;
        case ',':
            cfg__lexer_add_token(CFG_TOKEN_COMMA, ",");
            break;
        case '[':
            cfg__lexer_add_token(CFG_TOKEN_LEFT_BRACKET, "[");
            break;
        case ']':
            cfg__lexer_add_token(CFG_TOKEN_RIGHT_BRACKET, "]");
            break;
        case '(':
            cfg__lexer_add_token(CFG_TOKEN_LEFT_PARENTHESIS, "(");
            break;
        case ')':
            cfg__lexer_add_token(CFG_TOKEN_RIGHT_PARENTHESIS, ")");
            break;
        case '{':
            cfg__lexer_add_token(CFG_TOKEN_LEFT_CURLY_BRACKET, "{");
            break;
        case '}':
            cfg__lexer_add_token(CFG_TOKEN_RIGHT_CURLY_BRACKET, "}");
            break;
        default:
            if (isdigit(*lexer.ch_current)) {
                lexer.str_start = lexer.ch_current;

                size_t dots = 0;

                while (isdigit(*lexer.ch_current) || *lexer.ch_current == '.') {
                        if (*lexer.ch_current == '.') {
                            dots++;
                        }

                        lexer.ch_current++;
                        lexer.column++;
                }

                if (dots > 1) {
                    cfg.err.type = CFG_ERROR_UNKNOWN_TOKEN;
                    sprintf(cfg.err.message, "Unknown token at line:%lu, column:%lu", lexer.line, lexer.column);
                    cfg__lexer_free();
                    return;
                }

                size_t len = lexer.ch_current - lexer.str_start;
                char *value = cfg__malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer.str_start, len);

                if (dots < 1) {
                    cfg__lexer_add_token(CFG_TOKEN_INT, value);
                } else {
                    cfg__lexer_add_token(CFG_TOKEN_DOUBLE, value);
                }

                continue;
            } else if (*lexer.ch_current == '"') {
                lexer.str_start = ++lexer.ch_current;
                char *value = cfg__lexer_parse_string();
                cfg__lexer_add_token(CFG_TOKEN_STRING, value);
                continue;
            } else {
                lexer.str_start = lexer.ch_current;

                while (*lexer.ch_current != ' ' &&
                       *lexer.ch_current != '\n' &&
                       *lexer.ch_current != '=' &&
                       *lexer.ch_current != ';' &&
                       *lexer.ch_current != ',' &&
                       *lexer.ch_current != '[' &&
                       *lexer.ch_current != ']' &&
                       *lexer.ch_current != '(' &&
                       *lexer.ch_current != ')' &&
                       *lexer.ch_current != '{' &&
                       *lexer.ch_current != '}') {
                    lexer.ch_current++;
                    lexer.column++;
                }

                if (lexer.str_start == lexer.ch_current) {
                    lexer.ch_current++;
                    lexer.column++;
                    continue;
                }

                size_t len = lexer.ch_current - lexer.str_start;
                char *value = cfg__malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer.str_start, len);

                if (strcmp(value, "true") == 0 ||
                    strcmp(value, "false") == 0) {
                    cfg__lexer_add_token(CFG_TOKEN_BOOL, value);
                    continue;
                } else {
                    cfg__lexer_add_token(CFG_TOKEN_IDENTIFIER, value);
                }
            }
        }
        lexer.ch_current++;
        lexer.column++;
    }

    cfg__lexer_add_token(CFG_TOKEN_EOF, "\0");
}

static int cfg__file_parse(void)
{
    cfg__file_tokenize();

    if (cfg.err.type != CFG_ERROR_NONE) return 1;

    int prev_token = 0;
    int expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_EOF;
    int type = 0;
    char *name = NULL;
    char *value = NULL;
    Cfg_Token *tokens = lexer.tokens;
    Cfg_Stack *stack = &lexer.stack;
    Cfg_Variable *ctx = &cfg.global;
    for (size_t i = lexer.cur_token; i < lexer.tokens_len; ++i) {
        lexer.cur_token = i;
        if (tokens[i].type & expected_token) {
            switch (tokens[i].type) {
            case CFG_TOKEN_EQ:
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_PARENTHESIS |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING;
                break;
            case CFG_TOKEN_SEMICOLON:
                if (name != NULL && value != NULL) {
                    cfg__context_add_variable(ctx, type, name, value);
                    if (cfg.err.type != CFG_ERROR_NONE) {
                        cfg__lexer_free();
                        return 1;
                    }
                }
                name = NULL;
                value = NULL;
                expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_EOF;
                if (cfg__stack_last_char() == '{') {
                    expected_token |= CFG_TOKEN_RIGHT_CURLY_BRACKET;
                }
                break;
            case CFG_TOKEN_COMMA:
                if (cfg__stack_last_char() == '[' && ctx->vars_len > 0 && type != ctx->vars[0].type) {
                    cfg.err.type = CFG_ERROR_UNEXPECTED_TOKEN;
                    sprintf(cfg.err.message, "Wrong array member type line:%lu, column:%lu", tokens[i - 1].line, tokens[i - 1].column);
                    cfg__lexer_free();
                    return 1;
                };

                cfg__context_add_variable(ctx, type, name, value);
                if (cfg.err.type != CFG_ERROR_NONE) {
                    cfg__lexer_free();
                    return 1;
                }

                name = NULL;
                value = NULL;
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_PARENTHESIS |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token |= CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token |= CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    break;
                }
                break;
            case CFG_TOKEN_LEFT_BRACKET:
                cfg__stack_add_char('[');
                type = CFG_TYPE_ARRAY;
                value = NULL;
                cfg__context_add_variable(ctx, type, name, value);
                if (cfg.err.type != CFG_ERROR_NONE) {
                    cfg__lexer_free();
                    return 1;
                }
                name = NULL;
                ctx = &ctx->vars[ctx->vars_len - 1];
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_PARENTHESIS |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING |
                                 CFG_TOKEN_RIGHT_BRACKET;
                break;
            case CFG_TOKEN_RIGHT_BRACKET:
                if (value != NULL) {
                    if (type != ctx->vars[0].type) {
                        cfg.err.type = CFG_ERROR_UNEXPECTED_TOKEN;
                        sprintf(cfg.err.message, "Wrong array member type at line:%lu, column%lu", tokens[i - 1].line, tokens[i - 1].column);
                        cfg__lexer_free();
                        return 1;
                    };
                    cfg__context_add_variable(ctx, type, name, value);
                    if (cfg.err.type != CFG_ERROR_NONE) {
                        cfg__lexer_free();
                        return 1;
                    }
                    value = NULL;
                }
                cfg__stack_pop_char();
                ctx = ctx->prev;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_LEFT_PARENTHESIS:
                cfg__stack_add_char('(');
                type = CFG_TYPE_LIST;
                value = NULL;
                cfg__context_add_variable(ctx, type, name, value);
                if (cfg.err.type != CFG_ERROR_NONE) {
                    cfg__lexer_free();
                    return 1;
                }
                name = NULL;
                ctx = &ctx->vars[ctx->vars_len - 1];
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_PARENTHESIS |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING |
                                 CFG_TOKEN_RIGHT_PARENTHESIS;
                break;
            case CFG_TOKEN_RIGHT_PARENTHESIS:
                if (value != NULL) {
                    cfg__context_add_variable(ctx, type, name, value);
                    if (cfg.err.type != CFG_ERROR_NONE) {
                        cfg__lexer_free();
                        return 1;
                    }
                    value = NULL;
                }
                cfg__stack_pop_char();
                ctx = ctx->prev;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_LEFT_CURLY_BRACKET:
                cfg__stack_add_char('{');
                type = CFG_TYPE_STRUCT;
                value = NULL;
                cfg__context_add_variable(ctx, type, name, value);
                if (cfg.err.type != CFG_ERROR_NONE) {
                    cfg__lexer_free();
                    return 1;
                }
                name = NULL;
                ctx = &ctx->vars[ctx->vars_len - 1];
                expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_RIGHT_CURLY_BRACKET;
                break;
            case CFG_TOKEN_RIGHT_CURLY_BRACKET:
                cfg__stack_pop_char();
                ctx = ctx->prev;
                type = 0;
                name = NULL;
                value = NULL;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_IDENTIFIER:
                name = tokens[i].value;
                expected_token = CFG_TOKEN_EQ;
                break;
            case CFG_TOKEN_INT:
                type = CFG_TYPE_INT;
                value = tokens[i].value;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_DOUBLE:
                type = CFG_TYPE_DOUBLE;
                value = tokens[i].value;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_BOOL:
                type = CFG_TYPE_BOOL;
                value = tokens[i].value;
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_STRING:
                type = CFG_TYPE_STRING;
                if (prev_token & CFG_TOKEN_STRING) {
                    size_t new_size = sizeof(char) * (strlen(value) + strlen(tokens[i].value) + 1);
                    char *tmp = cfg__malloc(new_size);
                    strcpy(tmp, value);
                    strcat(tmp, tokens[i].value);
                    value = tmp;
                } else {
                    value = tokens[i].value;
                }
                switch (cfg__stack_last_char()) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                case '(':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_PARENTHESIS;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                expected_token |= CFG_TOKEN_STRING;
                break;
            default:
                break;
            }
        } else {
            cfg.err.type = CFG_ERROR_UNEXPECTED_TOKEN;
            sprintf(cfg.err.message, "Unexpected token at line:%lu, column:%lu", tokens[i].line, tokens[i].column);
            cfg__lexer_free();
            return 1;
        }
        prev_token = tokens[i].type;
    }

    cfg__lexer_free();

    return 0;
}


// Public API function definitions

int cfg_load_file(const char *file_path)
{
    if (cfg.global.vars != NULL) {
        cfg__config_free();
    }
    cfg__config_create();

    if ((cfg__file_open(file_path)) != 0 ||
        (cfg__file_parse() != 0)) {
        return 1;
    }
    return 0;
}

void cfg_unload(void)
{
    cfg__config_free();
}

Cfg_Variable *cfg_global_context(void)
{
    return &cfg.global;
}

int cfg_get_int(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_INT) {
        return 0;
    }

    int res;

    if (sscanf(ctx->vars[i].value, "%d", &res) != 1) {
        return 0;
    }

    return res;
}

double cfg_get_double(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_DOUBLE) {
        return 0.0;
    }

    double res;

    if (sscanf(ctx->vars[i].value, "%lf", &res) != 1) {
        return 0.0;
    }

    return res;
}

bool cfg_get_bool(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_BOOL) {
        return false;
    }

    if (strcmp(ctx->vars[i].value, "true") == 0) {
        return true;
    }

    return false;
}

char *cfg_get_string(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_STRING) {
        return NULL;
    }

    return ctx->vars[i].value;
}

Cfg_Variable *cfg_get_array(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_ARRAY) {
        return NULL;
    }

    return &ctx->vars[i];
}

Cfg_Variable *cfg_get_list(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_LIST) {
        return NULL;
    }

    return &ctx->vars[i];
}

Cfg_Variable *cfg_get_struct(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1 || ctx->vars[i].type != CFG_TYPE_STRUCT) {
        return NULL;
    }

    return &ctx->vars[i];
}

int cfg_get_int_safe(Cfg_Variable *ctx, const char *name, int *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_INT) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not int", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not int", name);
        }
        return 1;
    }

    if (sscanf(ctx->vars[i].value, "%d", res) != 1) {
        cfg.err.type = CFG_ERROR_VARIABLE_PARSE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Failed to parse variable `%s` in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Failed to parse variable `%s`", name);
        }
        return 1;
    }

    return 0;
}

int cfg_get_double_safe(Cfg_Variable *ctx, const char *name, double *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_DOUBLE) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not double", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not double", name);
        }
        return 1;
    }

    if (sscanf(ctx->vars[i].value, "%lf", res) != 1) {
        cfg.err.type = CFG_ERROR_VARIABLE_PARSE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Failed to parse variable `%s` in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Failed to parse variable `%s`", name);
        }
        return 1;
    }

    return 0;
}

int cfg_get_bool_safe(Cfg_Variable *ctx, const char *name, bool *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_BOOL) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not bool", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not bool", name);
        }
        return 1;
    }

    if (strcmp(ctx->vars[i].value, "true") == 0) {
        *res = true;
    } else {
        *res = false;
    }

    return 0;
}

int cfg_get_string_safe(Cfg_Variable *ctx, const char *name, char **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_STRING) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not string", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not string", name);
        }
        return 1;
    }

    *res = ctx->vars[i].value;
    return 0;
}

int cfg_get_array_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_ARRAY) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not array", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not array", name);
        }
        return 1;
    }

    *res = &ctx->vars[i];
    return 0;
}

int cfg_get_list_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_LIST) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not list", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not list", name);
        }
        return 1;
    }

    *res = &ctx->vars[i];
    return 0;
}

int cfg_get_struct_safe(Cfg_Variable *ctx, const char *name, Cfg_Variable **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        cfg.err.type = CFG_ERROR_VARIABLE_NOT_FOUND;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` not found in `%s`", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` not found", name);
        }
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_STRUCT) {
        cfg.err.type = CFG_ERROR_VARIABLE_WRONG_TYPE;
        if (ctx->name != NULL) {
            sprintf(cfg.err.message, "Variable `%s` in `%s` is not struct", name, ctx->name);
        } else {
            sprintf(cfg.err.message, "Variable `%s` is not struct", name);
        }
        return 1;
    }

    *res = &ctx->vars[i];
    return 0;
}

size_t cfg_get_context_len(Cfg_Variable *ctx)
{
    return ctx->vars_len;
}

int cfg_get_int_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_INT) return 0;

    int res;

    if (sscanf(ctx->vars[idx].value, "%d", &res) != 1) {
        return 0;
    }

    return res;
}

double cfg_get_double_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_DOUBLE) return 0.0;

    double res;

    if (sscanf(ctx->vars[idx].value, "%lf", &res) != 1) {
        return 0;
    }

    return res;
}

bool cfg_get_bool_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_BOOL) return false;

    if (strcmp(ctx->vars[idx].value, "true") == 0) {
        return true;
    }

    return false;
}

char *cfg_get_string_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_STRING) return NULL;

    return ctx->vars[idx].value;
}

Cfg_Variable *cfg_get_array_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_ARRAY) return NULL;

    return &ctx->vars[idx];
}

Cfg_Variable *cfg_get_list_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_LIST) return NULL;

    return &ctx->vars[idx];
}

Cfg_Variable *cfg_get_struct_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len || ctx->vars[idx].type != CFG_TYPE_STRUCT) return NULL;

    return &ctx->vars[idx];
}

Cfg_Type cfg_get_type(Cfg_Variable *ctx, const char *name)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) return CFG_TYPE_NONE;

    return ctx->vars[i].type;
}

Cfg_Type cfg_get_type_elem(Cfg_Variable *ctx, size_t idx)
{
    if (idx >= ctx->vars_len) return CFG_TYPE_NONE;

    return ctx->vars[idx].type;
}

Cfg_Error_Type cfg_get_error_type(void)
{
    return cfg.err.type;
}

char *cfg_get_error_message(void)
{
    if (cfg.err.type == CFG_ERROR_NONE) return NULL;

    return cfg.err.message;
}

#endif // CFG_IMPLEMENTATION

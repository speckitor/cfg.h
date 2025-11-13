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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


typedef enum {
    CFG_TYPE_INT = 1,
    CFG_TYPE_DOUBLE = 2,
    CFG_TYPE_BOOL = 4,
    CFG_TYPE_STRING = 8,
    CFG_TYPE_LIST = 16,
    CFG_TYPE_STRUCT = 32,
} Cfg_Type;

typedef enum {
    CFG_ERROR_NONE = 0,
    CFG_ERROR_OPEN_FILE,
    CFG_ERROR_UNKNOWN_TOKEN,
    CFG_ERROR_UNEXPECTED_TOKEN,
    CFG_ERROR_COUNT,
} Cfg_Error_Type;

typedef struct {
    char *message;
    Cfg_Error_Type type;
    char *str;
    size_t line;
    size_t column;
} Cfg_Error;

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

typedef struct {
    Cfg_Variable global;
    FILE *file;
    Cfg_Error err;
} Cfg_Config;

// Public API function declarations

Cfg_Config *cfg_init(void);
void cfg_destroy(Cfg_Config *cfg);

int cfg_load_file(Cfg_Config *cfg, const char *file_path);

Cfg_Variable *cfg_global_context(Cfg_Config *cfg);

int cfg_get_int(Cfg_Variable *ctx, const char *name, int *res);
int cfg_get_double(Cfg_Variable *ctx, const char *name, double *res);
int cfg_get_bool(Cfg_Variable *ctx, const char *name, bool *res);
int cfg_get_string(Cfg_Variable *ctx, const char *name, char **res);
int cfg_get_list(Cfg_Variable *ctx, const char *name, Cfg_Variable **res);
int cfg_get_struct(Cfg_Variable *ctx, const char *name, Cfg_Variable **res);

char *cfg_get_error(Cfg_Config *cfg);


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
    CFG_TOKEN_LEFT_CURLY_BRACKET = 32,
    CFG_TOKEN_RIGHT_CURLY_BRACKET = 64,
    CFG_TOKEN_EOF = 128,
    // Types with dinamicly allocated values
    CFG_TOKEN_IDENTIFIER = 256,
    CFG_TOKEN_INT = 512,
    CFG_TOKEN_DOUBLE = 1024,
    CFG_TOKEN_BOOL = 2048,
    CFG_TOKEN_STRING = 4096,
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

// Private functions forward declarations

static void *cfg__malloc(size_t size);
static void *cfg__realloc(void *prev, size_t size);

static Cfg_Lexer *cfg__lexer_create(Cfg_Config *cfg);
static void cfg__lexer_free(Cfg_Lexer *lexer);

static void cfg__string_add_char(char **str, size_t *cap, char ch);
static char *cfg__lexer_parse_string(Cfg_Lexer *lexer);

static void cfg__lexer_add_token(Cfg_Lexer *lexer, Cfg_Token_Type type, char *value);

static void cfg__stack_add_char(Cfg_Stack *stack, char ch);
static void cfg__stack_pop_char(Cfg_Stack *stack);
static char cfg__stack_last_char(Cfg_Stack *stack);

static void cfg__context_add_variable(Cfg_Variable *ctx, Cfg_Type type, char *name, char *value);
static void cfg__context_free(Cfg_Variable *ctx);
static int cfg__context_find_variable(Cfg_Variable *ctx, const char *name);

static int cfg__file_open(Cfg_Config *cfg, const char *file_path);
static char *cfg__file_get_str(Cfg_Config *cfg);
static Cfg_Lexer *cfg__file_tokenize(Cfg_Config *cfg);
static int cfg__file_parse(Cfg_Config *cfg);

// Private functions definition

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

static Cfg_Lexer *cfg__lexer_create(Cfg_Config *cfg)
{
    char *src = cfg__file_get_str(cfg);

    Cfg_Lexer *lexer = cfg__malloc(sizeof(Cfg_Lexer)); 
    lexer->tokens = cfg__malloc(sizeof(Cfg_Token) * INIT_TOKENS_NUM);
    lexer->stack.values = cfg__malloc(sizeof(char) * INIT_STACK_SIZE);

    lexer->file = src;
    lexer->str_start = NULL;
    lexer->ch_current = src;

    lexer->cur_token = 0;
    lexer->tokens_len = 0;
    lexer->tokens_cap = INIT_TOKENS_NUM;

    lexer->line = 0;
    lexer->column = 0;

    lexer->comment_eol = false;
    lexer->comment = false;

    lexer->stack.cap = INIT_STACK_SIZE;
    lexer->stack.len = 0;

    return lexer;
}

static void cfg__lexer_free(Cfg_Lexer *lexer)
{
    if (lexer->stack.values != NULL) free(lexer->stack.values);
    if (lexer->file != NULL) free(lexer->file);
    for (int i = 0; i < lexer->tokens_len; ++i) {
        if (lexer->tokens[i].type > CFG_TOKEN_EOF && lexer->tokens[i].value != NULL) {
            free(lexer->tokens[i].value);
        }
    }
    if (lexer->tokens != NULL)free(lexer->tokens);
    free(lexer);
}

static void cfg__lexer_add_token(Cfg_Lexer *lexer, Cfg_Token_Type type, char *value)
{
    if (lexer->tokens_len == lexer->tokens_cap) {
        lexer->tokens_cap *= 2;
        lexer->tokens = cfg__realloc(lexer->tokens, sizeof(Cfg_Token) * lexer->tokens_cap);
    }
    
    size_t idx = lexer->tokens_len++;
    memset(&lexer->tokens[idx], 0, sizeof(Cfg_Token));
    lexer->tokens[idx].type = type;
    lexer->tokens[idx].value = value;
    lexer->tokens[idx].line = lexer->line;
    if (strlen(value) > 1) {
        lexer->tokens[idx].column = lexer->column - strlen(value);
    } else {
        lexer->tokens[idx].column = lexer->column;
    }
}

static void cfg__stack_add_char(Cfg_Stack *stack, char ch)
{
    if (stack->len == stack->cap) {
        stack->cap *= 2;
        stack->values = cfg__realloc(stack->values, sizeof(char) * stack->cap);
    }
    stack->values[stack->len++] = ch;
}

static void cfg__stack_pop_char(Cfg_Stack *stack)
{
    if (stack->len != 0) {
        stack->values[--stack->len] = '\0';
    }
}

static char cfg__stack_last_char(Cfg_Stack *stack)
{
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

static char *cfg__lexer_parse_string(Cfg_Lexer *lexer)
{
    char *str = cfg__malloc(sizeof(char) * INIT_STRING_SIZE);
    str[0] = '\0';
    size_t cap = INIT_STRING_SIZE;

    char ch;
    bool backslash = false;
    while (*lexer->ch_current != '\0' && (*lexer->ch_current != '"' || backslash)) {
        if (*lexer->ch_current == '\\') {
            if (backslash) {
                ch = '\\';
                cfg__string_add_char(&str, &cap, ch);
                backslash = false;
                lexer->ch_current++;
                lexer->column++;
                continue;
            }
            backslash = true;
            lexer->ch_current++;
            lexer->column++;
            continue;
        }

        if (backslash) {
            switch (*lexer->ch_current) {
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
                ch = *lexer->ch_current;
                break;
            }
            backslash = false;
        } else {
            ch = *lexer->ch_current;
        }
        cfg__string_add_char(&str, &cap, ch);
        lexer->ch_current++;
        lexer->column++;
    }

    if (*lexer->ch_current == '\0') {
        free(str);
        return NULL;
    }

    lexer->ch_current++;
    lexer->column++;

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

    printf("name: %s, value: %s\n", name, value);

    ctx->vars[ctx->vars_len].type = type;
    if (name != NULL) {
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
    if (type & CFG_TYPE_STRUCT || type & CFG_TYPE_LIST) {
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

static int cfg__file_open(Cfg_Config *cfg, const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        cfg->err.type = CFG_ERROR_OPEN_FILE;
        cfg->err.str = "Failed to open file";
        return 1;
    }
    cfg->file = file;
    return 0;
}

static char *cfg__file_get_str(Cfg_Config *cfg)
{
    fseek(cfg->file, 0, SEEK_END);
    long size = ftell(cfg->file);
    fseek(cfg->file, 0, SEEK_SET);
    char *file_str = cfg__malloc(sizeof(char) * (size + 1));
    fread(file_str, sizeof(char), size, cfg->file);
    file_str[size] = '\0';
    return file_str;
}

static Cfg_Lexer *cfg__file_tokenize(Cfg_Config *cfg)
{
    Cfg_Lexer *lexer = cfg__lexer_create(cfg);

    while (*lexer->ch_current != '\0') {
        lexer->column++;

        if (*lexer->ch_current == '\n') {
            lexer->comment_eol = false;
            lexer->line++;
            lexer->column = 0;
            lexer->ch_current++;
            continue;
        }

        if (*lexer->ch_current == '/') {
            lexer->ch_current++;
            lexer->column++;
            if (*lexer->ch_current == '/') {
                lexer->comment_eol = true;
            } else if (*lexer->ch_current == '*') {
                lexer->comment = true;
            } else {
                cfg->err.type = CFG_ERROR_UNKNOWN_TOKEN;
                cfg->err.str = "Unknown token";
                cfg->err.line = lexer->line;
                cfg->err.column = lexer->column;
                cfg__lexer_free(lexer);
                return NULL;
            }
        }

        if (*lexer->ch_current == '*' && lexer->comment) {
            lexer->ch_current++;
            lexer->column++;
            if (*lexer->ch_current == '/') {
                lexer->comment = false;
                lexer->ch_current++;
                continue;
            }
        }

        if (lexer->comment || lexer->comment_eol) {
            lexer->ch_current++;
            lexer->column++;
            continue;
        }

        switch (*lexer->ch_current) {
        case ' ':
            break;
        case '=':
            cfg__lexer_add_token(lexer, CFG_TOKEN_EQ, "=");
            break;
        case ';':
            cfg__lexer_add_token(lexer, CFG_TOKEN_SEMICOLON, ";");
            break;
        case ',':
            cfg__lexer_add_token(lexer, CFG_TOKEN_COMMA, ",");
            break;
        case '[':
            cfg__lexer_add_token(lexer, CFG_TOKEN_LEFT_BRACKET, "[");
            break;
        case ']':
            cfg__lexer_add_token(lexer, CFG_TOKEN_RIGHT_BRACKET, "]");
            break;
        case '{':
            cfg__lexer_add_token(lexer, CFG_TOKEN_LEFT_CURLY_BRACKET, "{");
            break;
        case '}':
            cfg__lexer_add_token(lexer, CFG_TOKEN_RIGHT_CURLY_BRACKET, "}");
            break;
        default:
            if (isdigit(*lexer->ch_current)) {
                lexer->str_start = lexer->ch_current;

                size_t dots = 0;

                while (isdigit(*lexer->ch_current) || *lexer->ch_current == '.') {
                        if (*lexer->ch_current == '.') {
                            dots++;
                        }

                        lexer->ch_current++;
                        lexer->column++;
                }

                if (dots > 1) {
                    cfg->err.type = CFG_ERROR_UNKNOWN_TOKEN;
                    cfg->err.str = "Unknown token";
                    cfg->err.line = lexer->line;
                    cfg->err.column = lexer->column;
                    cfg__lexer_free(lexer);
                    return NULL;
                }

                size_t len = lexer->ch_current - lexer->str_start;
                char *value = cfg__malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer->str_start, len);

                if (dots < 1) {
                    cfg__lexer_add_token(lexer, CFG_TOKEN_INT, value);
                } else {
                    cfg__lexer_add_token(lexer, CFG_TOKEN_DOUBLE, value);
                }

                continue;
            } else if (*lexer->ch_current == '"') {
                lexer->str_start = ++lexer->ch_current;
                char *value = cfg__lexer_parse_string(lexer);
                cfg__lexer_add_token(lexer, CFG_TOKEN_STRING, value);
                continue;
            } else {
                lexer->str_start = lexer->ch_current;

                while (*lexer->ch_current != ' ' &&
                       *lexer->ch_current != '\n' &&
                       *lexer->ch_current != '=' &&
                       *lexer->ch_current != ';' &&
                       *lexer->ch_current != ',' &&
                       *lexer->ch_current != '[' &&
                       *lexer->ch_current != ']' &&
                       *lexer->ch_current != '{' &&
                       *lexer->ch_current != '}') {
                    lexer->ch_current++;
                    lexer->column++;
                }

                if (lexer->str_start == lexer->ch_current) {
                    continue;
                }

                size_t len = lexer->ch_current - lexer->str_start;
                char *value = cfg__malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer->str_start, len);

                if (strcmp(value, "true") == 0 ||
                    strcmp(value, "false") == 0) {
                    cfg__lexer_add_token(lexer, CFG_TOKEN_BOOL, value);
                    continue;
                } else {
                    cfg__lexer_add_token(lexer, CFG_TOKEN_IDENTIFIER, value);
                }
            }
        }
        lexer->ch_current++;
    }

    return lexer;
}

static int cfg__file_parse(Cfg_Config *cfg)
{
    Cfg_Lexer *lexer = cfg__file_tokenize(cfg);

    if (lexer == NULL) {
        return 1;
    }

    int prev_token = 0;
    int expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_EOF;
    int type = 0;
    char *name = NULL;
    char *value = NULL;
    Cfg_Token *tokens = lexer->tokens;
    Cfg_Stack *stack = &lexer->stack;
    Cfg_Variable *ctx = &cfg->global;
    for (size_t i = lexer->cur_token; i < lexer->tokens_len; ++i) {
        printf("Token: type = %d, value = %s\n", tokens[i].type, tokens[i].value);
        lexer->cur_token = i;
        if (tokens[i].type & expected_token) {
            switch (tokens[i].type) {
            case CFG_TOKEN_EQ:
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING;
                break;
            case CFG_TOKEN_SEMICOLON:
                if (name != NULL && value != NULL) {
                    cfg__context_add_variable(ctx, type, name, value);
                }
                name = NULL;
                value = NULL;
                expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_EOF;
                if (cfg__stack_last_char(stack) == '{') {
                    expected_token |= CFG_TOKEN_RIGHT_CURLY_BRACKET;
                }
                break;
            case CFG_TOKEN_COMMA:
                cfg__context_add_variable(ctx, type, name, value);
                name = NULL;
                value = NULL;
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING |
                                 CFG_TOKEN_RIGHT_BRACKET;
                break;
            case CFG_TOKEN_LEFT_BRACKET:
                cfg__stack_add_char(stack, '[');
                type = CFG_TYPE_LIST;
                value = NULL;
                cfg__context_add_variable(ctx, type, name, value);
                name = NULL;
                ctx = &ctx->vars[ctx->vars_len - 1];
                expected_token = CFG_TOKEN_LEFT_BRACKET |
                                 CFG_TOKEN_LEFT_CURLY_BRACKET |
                                 CFG_TOKEN_INT |
                                 CFG_TOKEN_DOUBLE |
                                 CFG_TOKEN_BOOL |
                                 CFG_TOKEN_STRING |
                                 CFG_TOKEN_RIGHT_CURLY_BRACKET;
                break;
            case CFG_TOKEN_RIGHT_BRACKET:
                if (value != NULL) {
                    cfg__context_add_variable(ctx, type, name, value);
                    value = NULL;
                }
                cfg__stack_pop_char(stack);
                ctx = ctx->prev;
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_LEFT_CURLY_BRACKET:
                cfg__stack_add_char(stack, '{');
                type = CFG_TYPE_STRUCT;
                value = NULL;
                cfg__context_add_variable(ctx, type, name, value);
                name = NULL;
                ctx = &ctx->vars[ctx->vars_len - 1];
                expected_token = CFG_TOKEN_IDENTIFIER | CFG_TOKEN_RIGHT_CURLY_BRACKET;
                break;
            case CFG_TOKEN_RIGHT_CURLY_BRACKET:
                cfg__stack_pop_char(stack);
                ctx = ctx->prev;
                type = 0;
                name = NULL;
                value = NULL;
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
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
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_DOUBLE:
                type = CFG_TYPE_DOUBLE;
                value = tokens[i].value;
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                break;
            case CFG_TOKEN_BOOL:
                type = CFG_TYPE_BOOL;
                value = tokens[i].value;
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
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
                switch (cfg__stack_last_char(stack)) {
                case '[':
                    expected_token = CFG_TOKEN_COMMA | CFG_TOKEN_RIGHT_BRACKET;
                    break;
                default:
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                }
                expected_token |= CFG_TOKEN_STRING;
                break;
            }
        } else {
            cfg->err.type = CFG_ERROR_UNEXPECTED_TOKEN;
            cfg->err.str = "Unexpected token";
            cfg->err.line = tokens[i].line;
            cfg->err.column = tokens[i].column;
            cfg__lexer_free(lexer);
            return 1;
        }
        prev_token = tokens[i].type;
    }

    cfg__lexer_free(lexer);

    return 0;
}


// Public API function definitions

Cfg_Config *cfg_init(void)
{
    Cfg_Config *cfg = cfg__malloc(sizeof(Cfg_Config));
    cfg->global.vars = cfg__malloc(INIT_VARIABLES_NUM * sizeof(Cfg_Variable));
    cfg->global.name = NULL;
    cfg->global.value = NULL;
    cfg->global.prev = NULL;
    cfg->global.vars_len = 0;
    cfg->global.vars_cap = INIT_VARIABLES_NUM;
    cfg->err.type = CFG_ERROR_NONE;
    cfg->err.message = NULL;
    cfg->err.str = NULL;
    return cfg;
}

void cfg_destroy(Cfg_Config *cfg)
{
    cfg__context_free(&cfg->global);
    if (cfg->err.message != NULL) free(cfg->err.message);
    if (cfg->file != NULL) fclose(cfg->file);
    free(cfg);
}

int cfg_load_file(Cfg_Config *cfg, const char *file_path)
{
    if ((cfg__file_open(cfg, file_path)) != 0 ||
        (cfg__file_parse(cfg) != 0)) {
        return 1;
    }
    return 0;
}

Cfg_Variable *cfg_global_context(Cfg_Config *cfg)
{
    return &cfg->global;
}

int cfg_get_int(Cfg_Variable *ctx, const char *name, int *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_INT) {
        return 2;
    }

    if (sscanf(ctx->vars[i].value, "%d", res) != 1) {
        return 3;
    }

    return 0;
}


int cfg_get_double(Cfg_Variable *ctx, const char *name, double *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_DOUBLE) {
        return 2;
    }

    if (sscanf(ctx->vars[i].value, "%lf", res) != 1) {
        return 3;
    }

    return 0;
}

int cfg_get_bool(Cfg_Variable *ctx, const char *name, bool *res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_BOOL) {
        return 2;
    }

    if (strcmp(ctx->vars[i].value, "true") == 0) {
        *res = true;
    } else {
        *res = false;
    }
    return 0;
}

int cfg_get_string(Cfg_Variable *ctx, const char *name, char **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_STRING) {
        return 2;
    }

    *res = ctx->vars[i].value;
    return 0;
}

int cfg_get_list(Cfg_Variable *ctx, const char *name, Cfg_Variable **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_LIST) {
        return 2;
    }

    *res = &ctx->vars[i];
    return 0;
}

int cfg_get_struct(Cfg_Variable *ctx, const char *name, Cfg_Variable **res)
{
    int i = cfg__context_find_variable(ctx, name);

    if (i == -1) {
        return 1;
    }

    if (ctx->vars[i].type != CFG_TYPE_STRUCT) {
        return 2;
    }

    *res = &ctx->vars[i];
    return 0;
}

char *cfg_get_error(Cfg_Config *cfg)
{
    if (cfg->err.type == CFG_ERROR_NONE) return NULL;

    if (cfg->err.message == NULL) cfg->err.message = malloc(sizeof(char) * ERROR_MESSAGE_SIZE);

    if (cfg->err.type == CFG_ERROR_OPEN_FILE) {
        sprintf(cfg->err.message, "%s", cfg->err.str);
    } else {
        sprintf(cfg->err.message, "%s at line:%lu column:%lu", cfg->err.str, cfg->err.line, cfg->err.column);
    }

    return cfg->err.message;
}


#endif // CFG_IMPLEMENTATION

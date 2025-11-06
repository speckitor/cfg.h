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
#include <time.h>

#define MAX_NAME_LENGTH 256
#define MAX_VALUE_LENGTH 256
typedef enum {
    CFG_ERROR_NONE = 0,
    CFG_ERROR_OPEN_FILE,
    CFG_ERROR_COUNT,
} Cfg_Error_Type;

typedef struct {
    Cfg_Error_Type type;
    char *message;
    size_t line;
    size_t column;
} Cfg_Error;

typedef struct Cfg_Variable Cfg_Variable;

struct Cfg_Variable {
    char *name;
    char *value;
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

int cfg_parse_int(Cfg_Variable *ctx, const char *name, int *res);
int cfg_parse_double(Cfg_Variable *ctx, const char *name, double *res);
int cfg_parse_bool(Cfg_Variable *ctx, const char *name, bool *res);
int cfg_parse_string(Cfg_Variable *ctx, const char *name, char **res);
int cfg_get_list(Cfg_Variable *ctx, const char *name, Cfg_Variable *res);
int cfg_get_struct(Cfg_Variable *ctx, const char *name, Cfg_Variable *res);

char *cfg_get_error_message(Cfg_Config *cfg);


// #ifdef CFG_IMPLEMENTATION

// Private functions and types

#define INIT_VARIABLES_NUM 32

#define INIT_TOKENS_NUM 32

typedef enum {
    // Types with string literal values
    CFG_TOKEN_EQ = 0,
    CFG_TOKEN_SEMICOLON,
    CFG_TOKEN_EOF,
    // Types with dinamicly allocated values
    CFG_TOKEN_IDENTIFIER,
    CFG_TOKEN_INT,
} Cfg_Token_Type;

typedef struct {
    Cfg_Token_Type type;
    char *value;
    size_t line;
    size_t column;
} Cfg_Token;

typedef struct {
    char *file;
    char *str_start;
    char *ch_current;
    Cfg_Token *tokens;
    size_t tokens_len;
    size_t tokens_cap;
    size_t line;
    size_t column;
} Cfg_Lexer;

// Private functions forward declarations

static Cfg_Lexer *cfg__lexer_create(Cfg_Config *cfg);
static void cfg__lexer_free(Cfg_Lexer *lexer);
static void cfg__lexer_add_token(Cfg_Lexer *lexer, Cfg_Token_Type type, char *value);

static int cfg__file_open(Cfg_Config *cfg, const char *file_path);
static char *cfg__file_get_str(Cfg_Config *cfg);
static Cfg_Lexer *cfg__file_tokenize(Cfg_Config *cfg);
static int cfg__file_parse(Cfg_Config *cfg);

// Private functions definition

static Cfg_Lexer *cfg__lexer_create(Cfg_Config *cfg)
{
    char *src = cfg__file_get_str(cfg);

    Cfg_Lexer *lexer = malloc(sizeof(Cfg_Lexer)); 
    lexer->tokens = malloc(sizeof(Cfg_Token) * INIT_TOKENS_NUM);

    if (!lexer || !lexer->tokens) {
        // TODO: add handlig error
    }

    lexer->file = src;
    lexer->str_start = NULL;
    lexer->ch_current = src;

    lexer->tokens_len = 0;
    lexer->tokens_cap = INIT_TOKENS_NUM;

    lexer->line = 0;
    lexer->column = 0;

    return lexer;
}

static void cfg__lexer_free(Cfg_Lexer *lexer)
{
    free(lexer->file);
    for (int i = 0; i < lexer->tokens_len; ++i) {
        if (lexer->tokens[i].type > CFG_TOKEN_EOF) {
            free(lexer->tokens[i].value);
        }
    }
    free(lexer->tokens);
    free(lexer);
}

static void cfg__lexer_add_token(Cfg_Lexer *lexer, Cfg_Token_Type type, char *value)
{
    if (lexer->tokens_len == lexer->tokens_cap) {
        lexer->tokens_cap *= 2;
        lexer->tokens = realloc(lexer->tokens, lexer->tokens_cap);
        if (!lexer->tokens) {
            // TODO: add handlig error
        }
    }
    
    size_t idx = lexer->tokens_len++;
    lexer->tokens[idx].type = type;
    lexer->tokens[idx].value = value;
    lexer->tokens[idx].line = lexer->line;
    lexer->tokens[idx].column = lexer->column;
}

static void cfg__context_add_variable(Cfg_Variable *ctx, char *name, char *value)
{
    if (ctx->vars_len == ctx->vars_cap) {
        ctx->vars_cap *= 2;
        ctx->vars = realloc(ctx->vars, ctx->vars_cap);
        if (!ctx->vars) {
            // TODO: add handlig error
        }
    }

    printf("adding variable: %s = %s\n", name, value);

    ctx->vars[ctx->vars_len].name = strdup(name);
    ctx->vars[ctx->vars_len].value = strdup(value);
    ctx->vars_len++;
}

static void cfg__context_free(Cfg_Variable *ctx)
{
    if (ctx->vars_len != 0) {
        for (size_t i = 0; i < ctx->vars_len; ++i) {
            cfg__context_free(&ctx->vars[i]);
        }
    }
    free(ctx->name);
    free(ctx->value);
    free(ctx->vars);
}

static int cfg__file_open(Cfg_Config *cfg, const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (!file) {
        cfg->err.type = CFG_ERROR_OPEN_FILE;
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
    char *file_str = malloc(sizeof(char) * (size + 1));
    if (!file_str) {
        // TODO: add handlig error
    }
    fread(file_str, sizeof(char), size, cfg->file);
    file_str[size] = '\0';
    return file_str;
}

static Cfg_Lexer *cfg__file_tokenize(Cfg_Config *cfg)
{
    Cfg_Lexer *lexer = cfg__lexer_create(cfg);

    printf("%s\n", lexer->file);

    printf("Tokenizer started\n");

    while (*lexer->ch_current != '\0') {
        printf("%lu, %lu: %c\n", lexer->line, lexer->column, *lexer->ch_current);
        lexer->column++;
        switch (*lexer->ch_current) {
        case ' ':
            break;
        case '\n':
            lexer->line++;
            lexer->column = 0;
            break;
        case '=':
            cfg__lexer_add_token(lexer, CFG_TOKEN_EQ, "=");
            break;
        case ';':
            cfg__lexer_add_token(lexer, CFG_TOKEN_SEMICOLON, ";");
            break;
        default:
            if (isdigit(*lexer->ch_current)) {
                lexer->str_start = lexer->ch_current;

                while (isdigit(*lexer->ch_current)) {
                        lexer->ch_current++;
                        lexer->column++;
                }

                size_t len = lexer->ch_current - lexer->str_start;
                char *value = malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer->str_start, len);

                cfg__lexer_add_token(lexer, CFG_TOKEN_INT, value);

                continue;
            } else {
                lexer->str_start = lexer->ch_current;

                while (*lexer->ch_current != ' ' && *lexer->ch_current != '=') {
                    lexer->ch_current++;
                    lexer->column++;
                }

                size_t len = lexer->ch_current - lexer->str_start;
                char *value = malloc(sizeof(char) * (len + 1));
                value[len] = '\0';
                strncpy(value, lexer->str_start, len);

                cfg__lexer_add_token(lexer, CFG_TOKEN_IDENTIFIER, value);
            }
        }
        lexer->ch_current++;
    }

    printf("File tokenized\n");

    for (size_t i = 0; i < lexer->tokens_len; ++i) {
        printf("%d\t", lexer->tokens[i].type);
        printf("%s\n", lexer->tokens[i].value);
    }
    
    return lexer;
}

static int cfg__file_parse(Cfg_Config *cfg)
{
    Cfg_Lexer *lexer = cfg__file_tokenize(cfg);

    int expected_token = CFG_TOKEN_IDENTIFIER;
    char *name = NULL;
    char *value = NULL;
    Cfg_Token *tokens = lexer->tokens;
    for (size_t i = 0; i < lexer->tokens_len; ++i) {
        if (tokens[i].type != expected_token) {
            if (tokens[i].type == CFG_TOKEN_EOF) {
                break;
            }
        } else {
            switch (expected_token) {
                case CFG_TOKEN_IDENTIFIER:
                    name = tokens[i].value;
                    expected_token = CFG_TOKEN_EQ;
                    break;
                case CFG_TOKEN_EQ:
                    expected_token = CFG_TOKEN_INT;
                    break;
                case CFG_TOKEN_INT:
                    value = tokens[i].value;
                    expected_token = CFG_TOKEN_SEMICOLON;
                    break;
                case CFG_TOKEN_SEMICOLON:
                    cfg__context_add_variable(&cfg->global, name, value);
                    name = NULL;
                    value = NULL;
                    expected_token = CFG_TOKEN_IDENTIFIER;
                    break;
            }
        }
    }

    for (int i = 0; i < cfg->global.vars_len; ++i) {
        printf("name: %s, value: %s\n", cfg->global.vars[i].name, cfg->global.vars[i].value);
    }

    cfg__lexer_free(lexer);
    return 0;
}


// Public API function definitions

Cfg_Config *cfg_init(void)
{
    Cfg_Config *cfg = malloc(sizeof(Cfg_Config));
    cfg->global.vars = malloc(INIT_VARIABLES_NUM * sizeof(Cfg_Variable));
    if (!cfg || !cfg->global.vars) {
        // TODO: add handlig error
    }
    cfg->global.vars_len = 0;
    cfg->global.vars_cap = INIT_VARIABLES_NUM;
    cfg->err.type = CFG_ERROR_NONE;
    cfg->err.message = NULL;
    return cfg;
}

void cfg_free(Cfg_Config *cfg)
{
    cfg__context_free(&cfg->global);
    if (cfg->file != NULL) {
        fclose(cfg->file);
    }
    if (cfg->err.message != NULL) {
        free(cfg->err.message);
    }
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

char *cfg_get_error(Cfg_Config *cfg)
{
    // TODO
}

// #endif // CFG_IMPLEMENTATION

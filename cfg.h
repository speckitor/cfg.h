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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


#define INITIAL_VARIABLES_NUM 32

#define PARSING_STACK_SIZE 256

typedef enum {
    CFG_TYPE_INT = 0,
    CFG_TYPE_DOUBLE,
    CFG_TYPE_BOOL,
    CFG_TYPE_STRING,
    CFG_TYPE_STRUCT,
    CFG_TYPE_LIST,
    CFG_TYPE_COUNT,
} Cfg_Value_Type;

typedef enum {
    CFG_ERROR_NONE = 0,
    CFG_ERROR_OPEN_FILE,
    CFG_ERROR_LOAD_FILE,
    CFG_ERROR_PARSE_VALUE,
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
    const char *name;
    const char *value;

    Cfg_Value_Type type;

    Cfg_Variable *vars; // Nested elements
    size_t vars_len;
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

// Private functions

static int _cfg_open_file(Cfg_Config *cfg, const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (!file) {
        cfg->err.type = CFG_ERROR_OPEN_FILE;
        return 1;
    }
    cfg->file = file;
    return 0;
}

static int _cfg_parse_file(Cfg_Config *cfg)
{
    char val_stack[PARSING_STACK_SIZE];
    size_t val_i = 0;
    char scope_stack[PARSING_STACK_SIZE];
    size_t scope_i = 0;

    // TODO

    return 0;
}


// Public API function definitions

Cfg_Config *cfg_init(void)
{
    Cfg_Config *cfg = malloc(sizeof(Cfg_Config));
    cfg->global.vars = malloc(INITIAL_VARIABLES_NUM * sizeof(Cfg_Variable));
    cfg->global.vars_len = 0;
    cfg->err.type = CFG_ERROR_NONE;
    cfg->err.message = NULL;
    return cfg;
}

void cfg_destroy(Cfg_Config *cfg)
{
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
    if ((_cfg_open_file(cfg, file_path)) != 0 ||
        (_cfg_parse_file(cfg) != 0)) {
        return 1;
    }
    return 0;
}

char *cfg_get_error(Cfg_Config *cfg)
{
    // TODO
}

// #endif // CFG_IMPLEMENTATION

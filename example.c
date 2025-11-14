#include <stdio.h>

#define CFG_IMPLEMENTATION
#include "cfg.h"

void print_vars(void)
{
    int res;

    int number;
    double floating;
    bool boolean;
    char *string;

    Cfg_Variable *structure;
    int structure_a;
    int structure_b;

    Cfg_Variable *structure_nested;
    double structure_nested_double;
    Cfg_Variable *structure_nested_list;

    Cfg_Variable *global = cfg_global_context();

    res = cfg_get_int(global, "number", &number);
    if (res == 0) {
        printf("number = %d\n", number);
    }

    res = cfg_get_double(global, "floating", &floating);
    if (res == 0) {
        printf("floating = %lf\n", floating);
    }

    cfg_get_bool(global, "boolean", &boolean);
    if (res == 0) {
        printf("boolean = %s\n", boolean ? "true" : "false");
    }

    cfg_get_string(global, "string", &string);
    if (res == 0) {
        printf("string = %s\n", string);
    }

    cfg_get_struct(global, "structure", &structure);
    if (res == 0) {
        printf("structure = {\n");
    }

    cfg_get_int(structure, "a", &structure_a);
    if (res == 0) {
        printf("\ta = %d\n", structure_a);
    }

    cfg_get_int(structure, "b", &structure_b);
    if (res == 0) {
        printf("\tb = %d\n", structure_b);
    }

    cfg_get_struct(structure, "nested", &structure_nested);
    if (res == 0) {
        printf("\tnested = {\n");
    }

    cfg_get_double(structure_nested, "double", &structure_nested_double);
    if (res == 0) {
        printf("\t\tb = %lf\n", structure_nested_double);
    }

    printf("\t}\n");

    printf("}\n");
}

int main(void)
{
    int res = cfg_load_file("./example.cfg");
    if (res != 0) {
        printf("%s\n", cfg_get_error());
        cfg_unload();
        return 1;
    }

    cfg_unload();

    return 0;
}

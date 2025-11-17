#include <stdio.h>

#define CFG_IMPLEMENTATION
#include "cfg.h"

void print_vars(void)
{
    // Getting global variables context
    // To describe a space that contains varables the word `context` is used
    Cfg_Variable *global = cfg_global_context();

    // Getting int, double, bool and string variables
    //
    // cfg_get_<type_name> functions return a value of specified name in provided context
    // If there is no variable with provided name
    // these functions return 0/0.0/NULL/false (depending on return type)
    int number = cfg_get_int(global, "number");
    printf("number = %d;\n", number);

    double Double = cfg_get_double(global, "double");
    printf("double = %lf;\n", Double);

    bool boolean = cfg_get_bool(global, "boolean");
    printf("boolean = %s;\n", boolean ? "true" : "false");

    char *string = cfg_get_string(global, "string");
    printf("string = %s;\n", string);

    // Getting arrays, lists and structs
    //
    // To get array, list or struct use get functions
    // which return context (Cfg_Variable *) to array, list or struct if it exists or NULL if not
    // You can use that context to get variables inside of it.
    //
    // cfg_get_context_len returns the number of variables inside of the context.
    // cfg_get_<type_name>_elem can be used to get variables by index

    // Arrays use brackets and can contain only variables of one type
    Cfg_Variable *array = cfg_get_array(global, "array");
    printf("array = [");

    int array_el;
    size_t array_len = cfg_get_context_len(array);
    for (size_t i = 0; i < array_len; ++i) {
        array_el = cfg_get_int_elem(array, i);
        printf("%d", array_el);
        if (i + 1 < array_len) {
            printf(", ");
        }
    }

    printf("];\n");

    // Structs use curly brackets and contain named variables
    Cfg_Variable *structure = cfg_get_struct(global, "structure");

    printf("structure = {\n");

    int structure_a = cfg_get_int(structure, "a");
    printf("\ta = %d;\n", structure_a);

    int structure_b = cfg_get_int(structure, "b");
    printf("\tb = %d;\n", structure_a);

    // Nested structure
    Cfg_Variable *nested = cfg_get_struct(structure, "nested");
    printf("\tnested = {\n");

    double nested_double = cfg_get_double(nested, "double");
    printf("\t\tdouble = %lf;\n", nested_double);

    // Nested array
    Cfg_Variable *nested_ints = cfg_get_array(nested, "ints");
    printf("\t\tints = [");

    int tmp;
    size_t ints_len = cfg_get_context_len(nested_ints);
    for (size_t i = 0; i < ints_len; ++i) {
        tmp = cfg_get_int_elem(nested_ints, i);
        printf("%d", tmp);
        if (i + 1 < ints_len) {
            printf(", ");
        }
    }

    printf("];\n");

    // Nested list
    Cfg_Variable *nested_list = cfg_get_list(nested, "list");
    printf("\t\tlist = (");

    int list_int = cfg_get_int_elem(nested_list, 0);
    printf("%d, ", list_int);
    char *list_string = cfg_get_string_elem(nested_list, 1);
    printf("%s, ", list_string);
    double list_double = cfg_get_double_elem(nested_list, 2);
    printf("%lf, ", list_double);
    bool list_bool = cfg_get_double_elem(nested_list, 3);
    printf("%s);\n", list_bool ? "true" : "false");

    printf("\t};\n");

    printf("};\n");
}

int main(void)
{
    int res = cfg_load_file("./example.cfg");
    if (res != 0) {
        printf("%s\n", cfg_get_error_message());
        cfg_unload();
        return 1;
    }
    cfg_load_file("./example.cfg");

    print_vars();

    cfg_unload();

    return 0;
}

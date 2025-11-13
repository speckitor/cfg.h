#define CFG_IMPLEMENTATION
#include "cfg.h"

void print_vars(Cfg_Config *cfg)
{
    printf("Printing\n");

    for (int i = 0; i < cfg->global.vars_len; ++i) {
        if (cfg->global.vars[i].type & CFG_TYPE_LIST) {
            printf("name: %s, value: %s\n", cfg->global.vars[i].name, cfg->global.vars[i].value);
            for (int j = 0; j < cfg->global.vars[i].vars_len; ++j) {
                printf("\tname:%s, index: %d, value: %s\n", cfg->global.vars[i].vars[j].name, j, cfg->global.vars[i].vars[j].value);
            }
        } else if (cfg->global.vars[i].type & CFG_TYPE_STRUCT) {
            printf("name: %s, value: %s\n", cfg->global.vars[i].name, cfg->global.vars[i].value);
            for (int j = 0; j < cfg->global.vars[i].vars_len; ++j) {
                printf("\tname: %s, value: %s\n", cfg->global.vars[i].vars[j].name, cfg->global.vars[i].vars[j].value);
            }
        } else {
            printf("name: %s, value: %s\n", cfg->global.vars[i].name, cfg->global.vars[i].value);
        }
    }

    printf("Printed\n");

}

int main(void)
{
    Cfg_Config *cfg = cfg_init();

    cfg_load_file(cfg, "./test.cfg");

    print_vars(cfg);

    cfg_destroy(cfg);

    return 0;
}

#define CFG_IMPLEMENTATION
#include "cfg.h"

#include <stdio.h>

int main(void)
{
    Cfg_Config *cfg = cfg_init();

    cfg_load_file(cfg, "./example.cfg");

    cfg_destroy(cfg);
    return 0;
}

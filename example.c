#define CFG_IMPLEMENTATION
#include "cfg.h"

int main(void)
{
    Cfg_Config *cfg = cfg_init();

    cfg_load_file(cfg, "./test.cfg");

    cfg_free(cfg);
    return 0;
}

#define CFG_IMPLEMENTATION
#include "cfg.h"

int main(void)
{
    Cfg_Config *cfg = cfg_init();
    cfg_destroy(cfg);
    return 0;
}

#include "sirius_common.h"
#include "sirius_errno.h"
#include "sirius_macro.h"

#include "./internal/sirius_internal_log.h"

static char sirius_version[16];
char *
sirius_get_version()
{
    strncpy(
        sirius_version,
        SIRIUS_VERSION,
        sizeof(sirius_version) - 1);

    return sirius_version;
}

static bool is_init = false;

void
sirius_deinit()
{
    if (!(is_init)) return;

    SIRIUS_INFO("deinitialization\n");
    sirius_log_deinit();

    is_init = false;
}

int
sirius_init(sirius_init_t *p_init)
{
    if (is_init) {
        SIRIUS_DEBG("repeat initialization\n");
        return SIRIUS_OK;
    }

    sirius_log_cr_t cr = {0};
    cr.log_lv = p_init->log_lv;
    cr.p_pipe = p_init->p_pipe;
    if (sirius_log_init(&cr)) return SIRIUS_ERR;

    is_init = true;

    SIRIUS_INFO("initialization\n");
    SIRIUS_INFO("sirius version: %s\n",
        sirius_get_version());
    return SIRIUS_OK;
}

#include "lot_version.h"

#include "sdkconfig.h"

const char *lot_version_get(void)
{
    return CONFIG_LOT_PROJECT_VERSION;
}

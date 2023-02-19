#include "OpenKNX/Module.h"
#include "OpenKNX/Common.h"
#include <knx.h>

namespace OpenKNX
{
    const std::string Module::version()
    {
        return "0.0";
    }

    uint16_t Module::flashSize()
    {
        return 0;
    }

    void Module::writeFlash()
    {}

    void Module::readFlash(const uint8_t *data, const uint16_t size)
    {}

    void Module::processAfterStartupDelay()
    {}

    void Module::processBeforeRestart()
    {}

    void Module::processBeforeTablesUnload()
    {}

    void Module::savePower()
    {}

    bool Module::restorePower()
    {
        return true;
    }

    bool Module::usesSecCore()
    {
        return false;
    }

} // namespace OpenKNX

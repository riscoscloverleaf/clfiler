#include "ProgInfo.h"
#include <cloverleaf/Logger.h>
#include "../global.h"

ProgInfo::ProgInfo(tbx::Object &obj) :
    _pi(obj)
{
    _pi.add_about_to_be_shown_listener(this);
}

void ProgInfo::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    Log_debug("ProgInfo::about_to_be_shown",1);
    _pi.version(g_app_state.app_version + " (" + g_app_state.app_date + ")");
    _pi.title(g_app_state.app_name);
}

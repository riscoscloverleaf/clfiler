//
// Created by lenz on 4/28/22.
//

#include <iterator>
#include <iostream>
#include <fstream>
#include <tbx/textarea.h>
#include <tbx/application.h>
#include <cloverleaf/Logger.h>
#include "../model/AppSettings.h"
#include "../utils.h"
#undef self_component
#include "CLDisclaimer.h"
#include <cloverleaf/CLUtils.h>

CLDisclaimer::CLDisclaimer():
    _win("WDisclaim")
{
    tbx::ActionButton(_win.gadget(1)).add_selected_listener(this);
    tbx::ActionButton(_win.gadget(2)).add_selected_listener(this);
    _win.add_has_been_hidden_listener(this);
    _win.show_centered();

    std::string txt = CLUtils::get_file_contents("<CLFiler$Dir>.disclaimer");
    tbx::TextArea ta = _win.gadget(0);
    ta.allow_selection(false);
    ta.wordwrap(false);
    ta.text(txt);
    ta.set_cursor_position(0, 1);
    Log_debug("txt=\n%s", txt.c_str());
}

CLDisclaimer::~CLDisclaimer() {
    _win.delete_object();
    if (!_accepted) {
        tbx::Application::instance()->quit();
    }
}

void CLDisclaimer::has_been_hidden(const tbx::EventInfo &event_info) {
    Log_debug("CLDisclaimer::has_been_hidden",1);
    delete this;
}

void CLDisclaimer::button_selected(tbx::ButtonSelectedEvent &event) {
    switch(event.id_block().self_component().id()) {
        case 2:
            g_app_settings.set_value("Startup", "DisclaimerAccepted", 1);
            _accepted = true;
            break;
        case 1:
            tbx::Application::instance()->quit();
            break;
    }
}

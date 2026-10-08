//
// Created by lenz on 12/13/21.
//

#include <tbx/application.h>
#include <tbx/path.h>
#include <tbx/button.h>
#include "../utils.h"
#include "../global.h"
#include "CLSplash.h"

CLSplash * CLSplash::_instance = nullptr;

CLSplash::CLSplash():
    _win("WSplash")
{
    tbx::ActionButton(_win.gadget(3)).add_selected_listener(this);
    tbx::Button(_win.gadget(5)).add_mouse_click_listener(this);
    tbx::Button(_win.gadget(1)).value("CLFiler ver."+ g_app_state.app_version);
    //tbx::Application::instance()->add_timer(500, this);
    //_win.show_centered();
}

CLSplash::~CLSplash() {
//    tbx::Application::instance()->remove_timer(this);
    _win.delete_object();
    _instance = nullptr;
}

void CLSplash::timer(unsigned int elapsed) {
    _win.hide();
}

void CLSplash::has_been_hidden(const tbx::EventInfo &event_info) {
    delete this;
}

void CLSplash::button_selected(tbx::ButtonSelectedEvent &event) {
    switch(event.id_block().self_component().id()) {
        case 3:
            open_browser_url("https://riscoscloverleaf.com/donate/");
            _win.hide();
            break;
    }
}

void CLSplash::mouse_click(tbx::MouseClickEvent &event) {
    open_browser_url("https://riscoscloverleaf.com/");
    _win.hide();
}

void CLSplash::open() {
    if (!_instance) {
        _instance = new CLSplash();
    }
    _instance->_win.show_centered();
}


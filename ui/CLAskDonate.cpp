//
// Created by slenz on 25.05.2022.
//

#include <tbx/application.h>
#include <tbx/path.h>
#include <tbx/button.h>
#include "../utils.h"
#include "../global.h"
#include "CLAskDonate.h"

CLAskDonate * CLAskDonate::_instance = nullptr;

CLAskDonate::CLAskDonate():
        _win("WAskDonate")
{
    tbx::ActionButton(_win.gadget(3)).add_selected_listener(this);
    tbx::Button(_win.gadget(0)).add_mouse_click_listener(this);
}

CLAskDonate::~CLAskDonate() {
    _win.delete_object();
    _instance = nullptr;
}

void CLAskDonate::has_been_hidden(const tbx::EventInfo &event_info) {
    delete this;
}

void CLAskDonate::button_selected(tbx::ButtonSelectedEvent &event) {
    switch(event.id_block().self_component().id()) {
        case 3:
            open_browser_url("https://riscoscloverleaf.com/donate/");
            _win.hide();
            break;
    }
}

void CLAskDonate::mouse_click(tbx::MouseClickEvent &event) {
    open_browser_url("https://riscoscloverleaf.com/");
    _win.hide();
}

void CLAskDonate::open() {
    if (!_instance) {
        _instance = new CLAskDonate();
    }
    _instance->_win.show_centered();
}


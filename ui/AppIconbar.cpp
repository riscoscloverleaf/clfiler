//
// Created by lenz on 3/16/20.
//

#include "AppIconbar.h"
#include "CLDirectoryMenu.h"
#include "../model/AppDataModel.h"
#include <tbx/application.h>
#include <cloverleaf/Logger.h>

MainMenu::MainMenu(tbx::Object object) {
    menu = tbx::Menu(object);
    menu.add_about_to_be_shown_listener(this);
    menu.add_selection_listener(this);
    Log_debug("MainMenu::MainMenu", 1);
}

void MainMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {

}

void MainMenu::menu_selection(const tbx::EventInfo &event) {
    if (event.id_block().self_component().id() == 4) {
        tbx::Application::instance()->os_cli("Filer_Run <CLFiler$Dir>.!Help");
    }
}

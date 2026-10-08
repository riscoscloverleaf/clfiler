//
// Created by lenz on 3/16/20.
//

#ifndef CLFILER_APPICONBAR_H
#define CLFILER_APPICONBAR_H

#include <tbx/menu.h>
#include <tbx/iconbar.h>
#include <tbx/loader.h>
#include <tbx/eventinfo.h>
#include <tbx/menuselectionlistener.h>
#include <tbx/abouttobeshownlistener.h>
#include <tbx/autocreatelistener.h>
#include "../model/AppDataModel.h"

class MainMenu : public tbx::AboutToBeShownListener, public tbx::MenuSelectionListener {
private:
    tbx::Menu menu;
public:
    MainMenu(tbx::Object object);
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;

    void menu_selection(const tbx::EventInfo &event) override;
};

#endif //CLFILER_APPICONBAR_H

//
// Created by lenz on 2/14/22.
//

#ifndef CLFILER_CLDIRSIZESCANWINDOW_H
#define CLFILER_CLDIRSIZESCANWINDOW_H

#include "CLDirectoryWindow.h"
#include "../model/AppDataModel.h"
#include <tbx/actionbutton.h>
#include <tbx/buttonselectedlistener.h>

class CLDirSizeScanWindow :
        public tbx::AboutToBeShownListener,
        public tbx::ButtonSelectedListener,
        public AppDataModelRefreshListener
{
private:
    tbx::Window _win;
    CLDirectoryItemData* _root_item;
    static CLDirSizeScanWindow* _instance;
    bool aborted = false;
public:
    CLDirSizeScanWindow();

    static void start(CLDirectoryItemData* item, bool force = false);
    static bool running();
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
    void on_refresh_finished(bool aborted, bool any_item_changed) override;
    void on_info_changed() override;
};


#endif //CLFILER_CLDIRSIZESCANWINDOW_H

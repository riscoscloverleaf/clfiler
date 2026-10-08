//
// Created by slenz on 09.02.2023.
//

#include "FSRemoveUI.h"
#include "../model/AppDataModel.h"

void FSRemoveConfirmCommand::execute() {
    CLDirectoryItemData *item = g_app_data_model.find_item_by_id(_item_id);
    if (item) {
        if (_selected_items.empty()) {
            item->fs_remove();
        } else {
            item->fs_remove_files(_selected_items);
        }
    }
    try {
        tbx::Window win = tbx::Window::from_handle(_win_handle);
        if (!win.null()) {
            win.focus();
        }
    } catch (std::exception &e) {}
}


void SetFocusBackCommand::execute() {
    try {
        tbx::Window win = tbx::Window::from_handle(_win_handle);
//            Log_debug("SetFocusBack wh=%d null=%d handle=%d", _win_handle, win.null(), win.handle());
        if (!win.null()) {
            win.focus();
        }
    } catch(std::exception& e) {}
}

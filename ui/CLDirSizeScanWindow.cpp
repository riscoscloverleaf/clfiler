//
// Created by lenz on 2/14/22.
//

#include "CLDirSizeScanWindow.h"

CLDirSizeScanWindow* CLDirSizeScanWindow::_instance = nullptr;

CLDirSizeScanWindow::CLDirSizeScanWindow() :
    _win("WSizeScan"),
    _root_item(nullptr)
{
    tbx::ActionButton(_win.gadget(4)).add_selected_listener(this);
    _win.add_about_to_be_shown_listener(this);
}

void CLDirSizeScanWindow::about_to_be_shown(tbx::AboutToBeShownEvent &event) {

}

void CLDirSizeScanWindow::button_selected(tbx::ButtonSelectedEvent &event) {
    aborted = true;
    g_app_data_model.abort_refresh();
}

void CLDirSizeScanWindow::on_refresh_finished(bool aborted, bool any_item_changed) {
    _win.hide();
    g_app_data_model.remove_refresh_listener(this);
//    if (!aborted) {
//        Log_debug("CLDirSizeScanWindow::on_refresh_finished (set CLDIRITEM_FLAG_FULL_SCANNED)",1);
//        _root_item->set_flag_recursive(CLDIRITEM_FLAG_FULL_SCANNED);
//    }
    _root_item = nullptr;
}

void CLDirSizeScanWindow::on_info_changed() {
    CLDirectoryItemData *data = g_app_data_model.get_refreshing_item();
    std::string scantxt = data->name();
    scantxt.append(" ");
    scantxt.append(data->dir_or_file_size_str());
    scantxt.append(" ");
    scantxt.append(data->dir_or_file_size_unit());
    tbx::Button(_win.gadget(0)).value(scantxt);

    scantxt = "Total: ";
    scantxt.append(" ");
    scantxt.append(_root_item->dir_or_file_size_str());
    scantxt.append(" ");
    scantxt.append(_root_item->dir_or_file_size_unit());

    tbx::Button(_win.gadget(2)).value(scantxt);
}

bool CLDirSizeScanWindow::running() {
    return (_instance && _instance->_root_item);
}

void CLDirSizeScanWindow::start(CLDirectoryItemData *item, bool force) {
    unsigned int options = CLREFRESH_WITH_YIELD;
    if (force || !item->is_full_scanned()) {
        options |= CLREFRESH_FORCE_CALC_SIZE;
    }
    if (item->is_full_scanned() && !force) {
        g_app_data_model.refresh_on_idle(item, 0, options);
    } else {
//        if (!_instance) {
//            _instance = new CLDirSizeScanWindow();
//        }
//        _instance->aborted = false;
//        if (_instance->_root_item) {
//            g_app_data_model.abort_refresh();
//        }
//        _instance->_root_item = item;
        options |= CLREFRESH_CALC_SIZE;
        g_app_data_model.refresh_on_idle(item, 1000, options);
//        g_app_data_model.add_refresh_listener(_instance);
//        _instance->_win.show();
    }
}


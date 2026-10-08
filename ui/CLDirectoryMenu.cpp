//
// Created by lenz on 10/14/21.
//
#include <time.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <oslib/os.h>
#include <tbx/res/resmenu.h>
#include <tbx/application.h>
#include <tbx/optionbutton.h>
#include <cloverleaf/CLClipboard.h>
#include <cloverleaf/CLImageCache.h>
#include "CLDirectoryMenu.h"
#include "CLImageViewWindow.h"
#include "../utils.h"

CLDirectoryMenu::CLDirectoryMenu(tbx::Object obj):
    _menu(obj)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
}

void CLDirectoryMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    std::string selection_txt = g_my_app->messages().message("Selection");
    std::string menu_clicked_item_txt = "File under mouse";
    //Log_debug("CLDirectoryMenu::about_to_be_shown %x", obj_id);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    Log_debug("CLDirectoryMenu::about_to_be_shown %x dir_win:%x", obj_id, dir_win);
    auto first_selected_item = dir_win->first_selected_item();
    auto menu_clicked_item = dir_win->menu_clicked_item();
    bool selection_fade = true;
    bool hovered_fade = true;

    if (dir_win->selection()->many()) {
        selection_fade = false;
    } else if (first_selected_item && first_selected_item != menu_clicked_item) {
        if (first_selected_item->file_type() == tbx::FILE_TYPE_APPLICATION) {
            selection_txt = "App '";
        } else if (first_selected_item->is_directory() || first_selected_item->is_image()) {
            selection_txt = "Dir '";
        } else {
            selection_txt = "File '";
        }
        selection_txt.append(first_selected_item->display_name());
        selection_txt.append("'");
        selection_fade = false;
    }
    if (menu_clicked_item) {
        if (menu_clicked_item->file_type() == tbx::FILE_TYPE_APPLICATION) {
            menu_clicked_item_txt = "App '";
        } else if (menu_clicked_item->is_directory() || menu_clicked_item->is_image()) {
            menu_clicked_item_txt = "Dir '";
        } else {
            menu_clicked_item_txt = "File '";
        }
        menu_clicked_item_txt.append(menu_clicked_item->display_name());
        menu_clicked_item_txt.append("'");
        hovered_fade = false;
    }

    _menu.item(0x10).text(selection_txt);
    _menu.item(0x10).fade(selection_fade); // selection

    _menu.item(0x110).text(menu_clicked_item_txt);
    _menu.item(0x110).fade(hovered_fade); // item hovered

    _menu.item(0x20).fade(dir_win->operation_mode() != OPERATION_MODE_DIR || g_app_state.selected_files.empty()); // paste
    _menu.item(0x40).fade(dir_win->operation_mode() != OPERATION_MODE_DIR); // select all
    _menu.item(0x50).fade(dir_win->selection()->empty()); // clear selection
    _menu.item(0x60).fade(dir_win->operation_mode() != OPERATION_MODE_DIR); // refresh
    _menu.item(0x70).fade(dir_win->operation_mode() != OPERATION_MODE_DIR); // set directory
}

void CLDirectoryMenu::menu_selection(const tbx::EventInfo &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    Log_debug("CLDirectoryMenu::menu_selection %x cmp_id", obj_id);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch(event.id_block().self_component().id()) {
        case 0x20: // paste
            dir_win->action_paste_items();
            break;
        case 0x40: //select all
            dir_win->select_all();
            break;
        case 0x50: //clear selection
            dir_win->clear_selection();
            break;
        case 0x60: //refresh
            dir_win->refresh_dir(true);
            break;
        case 0x70: //set directory
            dir_win->set_directory();
            break;
//        case 0x100: //refresh sizes
//            dir_win->refresh_directory_sizes(true);
//            break;
    }
}


CLDirectorySelectionMenu::CLDirectorySelectionMenu(tbx::Object obj):
        _menu(obj)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
    Log_debug("CLDirectorySelectionMenu::CLDirectorySelectionMenu",1);
}

void CLDirectorySelectionMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    int parent_component_id = event.id_block().parent_component().id();
    int parent_handle = event.id_block().parent_component().handle();
    Log_debug("CLDirectorySelectionMenu::about_to_be_shown %x parent_id=%x parent_handle=%x", obj_id, parent_component_id, parent_handle);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    bool is_hovered_action_item = event.id_block().parent_component().id() == 0x110;
    std::vector<CLDirectoryItemData*> action_items;
//    auto menu_clicked_item = dir_win->first_selected_item();
//    auto menu_clicked_item = dir_win->menu_clicked_item();
//    Log_debug("CLDirectorySelectionMenu::about_to_be_shown %x dir_win:%x menu_clicked_item:%x first_selected_item:%x", obj_id, dir_win, menu_clicked_item, first_selected_item);

//    if (!menu_clicked_item) {
//        menu_clicked_item = dir_win->menu_clicked_item();
//    }
    if (is_hovered_action_item) {
        if (dir_win->menu_clicked_item()) {
            action_items.push_back(dir_win->menu_clicked_item());
        }
    } else {
        action_items = dir_win->selected_items();
    }
    dir_win->set_action_items(action_items);

    if (!action_items.empty()) {
        if (action_items.size() > 1) {  // multiple selected items
            _menu.title(g_my_app->messages().message("Selection"));
            //_menu.item(2).fade(true); // open in new window
            _menu.item(3).fade(false); // copy
            _menu.item(4).fade(false); // cut
            _menu.item(5).fade(true); // rename
            _menu.item(6).fade(false); // delete
            _menu.item(7).fade(false); // access
            _menu.item(8).fade(false); // count
            _menu.item(9).fade(true); // help
            _menu.item(0xa).fade(true); // info
            _menu.item(0xb).fade(false); // set type
            _menu.item(0xc).fade(false); // stamp
            _menu.item(0xd).fade(true); // copy as
            _menu.item(0xe).fade(true); // view

            bool can_set_filetype_by_ext = false;
            for(auto &item : action_items) {
                if (item->can_set_filetype_by_ext()) {
                    can_set_filetype_by_ext = true;
                }
            }
            _menu.item(0xf).fade(!can_set_filetype_by_ext); // set type auto
        } else {// single clicked item
            std::string selection_label;
            CLDirectoryItemData* target_dir_item = action_items[0];
            if (target_dir_item->file_type() == tbx::FILE_TYPE_APPLICATION) {
                selection_label = "App '";
            } else if (target_dir_item->is_image() || target_dir_item->is_directory()) {
                selection_label = "Dir '";
            } else {
                selection_label = "File '";
            }
            selection_label.append(target_dir_item->display_name());
            selection_label.append("'");
            _menu.title(selection_label);
            //_menu.item(2).fade(false); // open in new window
            _menu.item(3).fade(false); // copy
            _menu.item(4).fade(false); // cut
            _menu.item(5).fade(dir_win->operation_mode() != OPERATION_MODE_DIR); // rename
            _menu.item(6).fade(false); // delete
            _menu.item(7).fade(false); // access
            _menu.item(8).fade(false); // count
            _menu.item(9).fade(!target_dir_item->has_help()); // help
            _menu.item(0xa).fade(false); // info
            _menu.item(0xb).fade(target_dir_item->is_directory() || target_dir_item->is_application()); // set type
            _menu.item(0xc).fade(false); // stamp
            _menu.item(0xd).fade(false); // copy as
            _menu.item(0xe).fade(!CLImageFactory::can_load(target_dir_item->file_type())); // view
            _menu.item(0xf).fade(!target_dir_item->is_file() || !target_dir_item->can_set_filetype_by_ext()); // set type auto
        }
    } else { // nothing selected
        //_menu.item(2).fade(true); // open in new window
        _menu.item(3).fade(true); // copy
        _menu.item(4).fade(true); // cut
        _menu.item(5).fade(true); // rename
        _menu.item(6).fade(true); // delete
        _menu.item(7).fade(true); // access
        _menu.item(8).fade(true); // count
        _menu.item(9).fade(true); // help
        _menu.item(0xa).fade(true); // info
        _menu.item(0xb).fade(true); // set type
        _menu.item(0xc).fade(true); // stamp
        _menu.item(0xd).fade(true); // copy as
        _menu.item(0xe).fade(true); // view
        _menu.item(0xf).fade(true); // set type auto
    }
}

void CLDirectorySelectionMenu::menu_selection(const tbx::EventInfo &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    Log_debug("CLDirectorySelectionMenu::menu_selection %x cmp_id", obj_id);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch(event.id_block().self_component().id()) {
//        case 2: { //open in new window
//                auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
//                if (menu_clicked_item->is_image() || menu_clicked_item->is_directory() || menu_clicked_item->is_application()) {
//                    instance->init(menu_clicked_item->path().name(), dir_win);
//                } else {
//                    instance->init(menu_clicked_item->path().parent().name(), dir_win);
//                }
//            }
//            break;
        case 3: // copy
            dir_win->action_copy_items();
            break;
        case 4: // cut
            dir_win->action_cut_items();
            break;
        case 5: //rename
            dir_win->start_rename_inplace();
            break;
        case 6: //delete
            dir_win->action_delete_items();
            break;
        case 9: // help
            dir_win->action_open_help();
            break;
        case 8: // count
            dir_win->action_count();
            break;
        case 0xa: // info
            dir_win->action_info();
            break;
        case 0xc: // stamp
            dir_win->action_stamp();
            break;
        case 0xe:
            dir_win->action_image_view();
            break;
        case 0xf:
            dir_win->action_set_filetype_by_ext();
            break;
    }
}

CLTreeMenu::CLTreeMenu(tbx::Object obj, CLDirectoryWindow* _dir_win):
        _menu(obj),
        dir_win(_dir_win)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
    Log_debug("CLTreeMenu::CLTreeMenu",1);
}

void CLTreeMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::PointerInfo where(true,false);
    tbx::ext::TreeView tree = dir_win->treeview();
    tbx::ext::TreeNodeId node_id = tree.find_node(where.mouse_x(), where.mouse_y());
    Log_debug("CLTreeMenu::about_to_be_shown dir_win:%x node_id:%x", dir_win, node_id);
    if (node_id) {
        CLDirectoryItemData* menu_clicked_item = dir_win->get_item_for_treeview_node_id(node_id);
        dir_win->set_action_item(menu_clicked_item);
        if (menu_clicked_item) {
            Log_debug("CLTreeMenu::about_to_be_shown dir_win:%x node_id:%x item:%s", dir_win, node_id, menu_clicked_item->name().c_str());
            std::string selection_label;
            if (menu_clicked_item->file_type() == tbx::FILE_TYPE_APPLICATION) {
                selection_label = "App '";
            } else if (menu_clicked_item->is_image() || menu_clicked_item->is_directory()) {
                selection_label = "Dir '";
            } else {
                selection_label = "File '";
            }
            selection_label.append(menu_clicked_item->display_name());
            selection_label.append("'");
            _menu.title(selection_label);
            _menu.item(2).fade(false); // open in new window
            _menu.item(3).fade(false); // copy
            _menu.item(4).fade(false); // cut
            _menu.item(5).fade(false); // rename
            _menu.item(6).fade(false); // delete
            _menu.item(7).fade(false); // access
            _menu.item(8).fade(menu_clicked_item->parent() == nullptr); // count
            _menu.item(9).fade(!menu_clicked_item->has_help()); // help
//            _menu.item(0xa).fade(true); // info
//            _menu.item(0xb).fade(menu_clicked_item->is_directory() || menu_clicked_item->is_application()); // set type
            _menu.item(0xc).fade(false); // stamp
            _menu.item(0xd).fade(false); // copy as
            _menu.item(0xe).fade(false); // set dir
            _menu.item(0xf).fade(!menu_clicked_item->is_directory()); // create new dir
            return;
        }
    } else {
        dir_win->set_action_item(nullptr);
    }
    _menu.title("No item");
    _menu.item(2).fade(true); // open in new window
    _menu.item(3).fade(true); // copy
    _menu.item(4).fade(true); // cut
    _menu.item(5).fade(true); // rename
    _menu.item(6).fade(true); // delete
    _menu.item(7).fade(true); // access
    _menu.item(8).fade(true); // count
    _menu.item(9).fade(true); // help
//    _menu.item(0xa).fade(true); // info
//    _menu.item(0xb).fade(true); // set type
    _menu.item(0xc).fade(true); // stamp
    _menu.item(0xd).fade(true); // copy as
    _menu.item(0xe).fade(true); // set dir
    _menu.item(0xf).fade(true); // create new dir
}

void CLTreeMenu::menu_selection(const tbx::EventInfo &event) {
//    if (!menu_clicked_item) {
//        Log_debug("CLTreeMenu::menu_selection no treeview item",1);
//        return;
//    }
    switch(event.id_block().self_component().id()) {
        case 2:
            dir_win->action_open_new_window();
//            CLDirectoryWindow::open_new(menu_clicked_item->path(), dir_win);
            break;
        case 3: // copy
            dir_win->action_copy_items();
//            copy_item(menu_clicked_item->path());
            break;
        case 4: // cut
            dir_win->action_cut_items();
//            cut_item(menu_clicked_item->path());
            break;
//        case 5: //rename
//            dir_win->start_rename_inplace(dir_win->menu_clicked_item_idx());
//            break;
        case 6: //delete
            dir_win->action_delete_items();
            break;
//        {
//            auto q = g_my_app->messages().message("DeleteDirConfirm", menu_clicked_item->name());
//            if (show_question(q.c_str(), g_my_app->messages().message("OKCancel").c_str()) == 3) {
//                menu_clicked_item->fs_remove();
//            }
//            break;
//        }
        case 8: // count
            dir_win->action_count();
            break;
//        {
//            if (menu_clicked_item->parent()) {
//                std::vector<std::string> dst;
//                dst.push_back(menu_clicked_item->name());
//                g_app_data_model.fs_count(menu_clicked_item->path().parent(), dst);
//            }
//            break;
//        }
        case 9: // help
            dir_win->action_open_help();
//            menu_clicked_item->fs_open_help();
            break;
        case 0xc: // stamp
            dir_win->action_stamp();
//            menu_clicked_item->fs_stamp();
            break;
        case 0xe: // set dir
            dir_win->action_set_dir();
            break;
    }
}

//void CLTreeMenu::cut_item(const tbx::Path& item) {
//    std::vector<std::string> src_files;
//    src_files.push_back(item.leaf_name());
//    g_app_state.selected_files = src_files;
//    g_app_state.selected_dir = item.parent().name();
//    g_app_state.selected_operation = SELECTED_OPERATION_CUT;
//    dir_win->set_status_line(g_my_app->messages().message("MovedIfPasteCommand", item.leaf_name()), true);
//}
//
//void CLTreeMenu::copy_item(const tbx::Path& item) {
//    std::vector<std::string> src_files;
//    src_files.push_back(item.leaf_name());
//    g_app_state.selected_files = src_files;
//    g_app_state.selected_dir = item.parent().name();
//    g_app_state.selected_operation = SELECTED_OPERATION_COPY;
//    dir_win->set_status_line(g_my_app->messages().message("CopyIfPasteCommand", item.leaf_name()), true);
//}

void CLTreeMenu::has_been_hidden(const tbx::EventInfo &event_info) {
    dir_win->set_action_item(nullptr);
//    menu_clicked_item = nullptr;
}


CLDirectoryDisplayMenu::CLDirectoryDisplayMenu(tbx::Object obj):
        _menu(obj)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
    Log_debug("CLDirectoryDisplayMenu::CLDirectoryDisplayMenu",1);
}

void CLDirectoryDisplayMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    _menu.item(1).tick(dir_win->toolbars_visible() == TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW);
    _menu.item(2).tick(dir_win->toolbars_visible() == TOOLBARS_VISIBLE_TOP_BOTTOM);
    _menu.item(3).tick(dir_win->toolbars_visible() == TOOLBARS_VISIBLE_TOP);
    _menu.item(4).tick(dir_win->toolbars_visible() == TOOLBARS_VISIBLE_NONE);
    _menu.item(0).fade(dir_win->operation_mode() != OPERATION_MODE_DIR);
}

void CLDirectoryDisplayMenu::menu_selection(const tbx::EventInfo &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch(event.id_block().self_component().id()) {
        case 1:
            dir_win->toggle_toolbars(TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW);
            break;
        case 2:
            dir_win->toggle_toolbars(TOOLBARS_VISIBLE_TOP_BOTTOM);
            break;
        case 3:
            dir_win->toggle_toolbars(TOOLBARS_VISIBLE_TOP);
            break;
        case 4:
            dir_win->toggle_toolbars(TOOLBARS_VISIBLE_NONE);
            break;
    }
}


CLDirectoryFolderDisplayMenu::CLDirectoryFolderDisplayMenu(tbx::Object obj):
        _menu(obj)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
}

void CLDirectoryFolderDisplayMenu::setup_menu(CLDirectoryWindow *dir_win) {
    unsigned int sort_order = dir_win->sort_order();

    _menu.item(1).tick(false);
    _menu.item(2).tick(false);
    _menu.item(3).tick(false);
    _menu.item(4).tick(false);
    _menu.item(4).fade(dir_win->operation_mode() == OPERATION_MODE_SEARCH);
    _menu.item(6).tick(dir_win->operation_mode() == OPERATION_MODE_SEARCH);
    _menu.item(9).tick(false);
    _menu.item(0xa).tick(false);
    _menu.item(0xb).tick(false);
    _menu.item(0xc).tick(false);

    _menu.item(0x20).tick(false);
    _menu.item(0x21).tick(false);
    _menu.item(0x22).tick(false);
    _menu.item(0x23).tick(false);

    _menu.item(0x30).tick(false);

    switch(dir_win->view_mode()) {
        case VIEW_MODE_TILE_BIG:
            _menu.item(1).tick(true);
            break;
        case VIEW_MODE_TILE_SMALL:
            _menu.item(2).tick(true);
            break;
        case VIEW_MODE_LIST:
            _menu.item(3).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_BIG:
            _menu.item(9).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_MEDIUM:
            _menu.item(0xa).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_SMALL:
            _menu.item(0xb).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_TINY:
            _menu.item(0xc).tick(true);
            break;
        case VIEW_MODE_DIR_SIZES_BARS:
            _menu.item(4).tick(true);
            break;
        case VIEW_MODE_BLOCKS:
            _menu.item(6).tick(true);
            break;
    }

    switch (sort_order & 0xfff0) {
        case SORT_NAME:
            _menu.item(0x20).tick(true);
            break;
        case SORT_NAME_DIRS:
            _menu.item(0x21).tick(true);
            break;
        case SORT_DATE:
            _menu.item(0x22).tick(true);
            break;
        case SORT_SIZE:
            _menu.item(0x23).tick(true);
            break;
    }

    if (sort_order & 0xf) {
        _menu.item(0x30).tick(true);
    }
}

void CLDirectoryFolderDisplayMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    setup_menu(dir_win);
}

void CLDirectoryFolderDisplayMenu::menu_selection(const tbx::EventInfo &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    unsigned int sort_direction =  dir_win->sort_order() & 0xf;
    switch(event.id_block().self_component().id()) {
        case 1:
            dir_win->view_mode(VIEW_MODE_TILE_BIG);
            break;
        case 2:
            dir_win->view_mode(VIEW_MODE_TILE_SMALL);
            break;
        case 3:
            dir_win->view_mode(VIEW_MODE_LIST);
            break;
        case 4:
            dir_win->view_mode(VIEW_MODE_DIR_SIZES_BARS);
            break;
        case 6:
            dir_win->view_mode(VIEW_MODE_BLOCKS);
            break;
        case 9:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_BIG);
            break;
        case 0xa:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_MEDIUM);
            break;
        case 0xb:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_SMALL);
            break;
        case 0xc:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_TINY);
            break;
        case 0x20:
            dir_win->sort_order(SORT_NAME | sort_direction);
            break;
        case 0x21:
            dir_win->sort_order(SORT_NAME_DIRS | sort_direction);
            break;
        case 0x22:
            dir_win->sort_order(SORT_DATE | sort_direction);
            break;
        case 0x23:
            dir_win->sort_order(SORT_SIZE | sort_direction);
            break;
        case 0x30:
            dir_win->sort_order((dir_win->sort_order() & 0xfff0) | (sort_direction ? 0 : 1));
            break;
    }
    setup_menu(dir_win);
}


CLDirectoryNewMenu::CLDirectoryNewMenu(tbx::Object obj):
        _menu(obj)
{
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_selection_listener(this);
}

void CLDirectoryNewMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    menu_clicked_item = dir_win->menu_clicked_item();
    if (!menu_clicked_item) {
        menu_clicked_item = dir_win->current_dir_item();
    }
    _menu.item(0x20).fade(dir_win->operation_mode() != OPERATION_MODE_DIR);
}

void CLDirectoryNewMenu::menu_selection(const tbx::EventInfo &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch(event.id_block().self_component().id()) {
        case 0x10: { // new window
                if (menu_clicked_item->is_image() || menu_clicked_item->is_directory() || menu_clicked_item->is_application()) {
                    CLDirectoryWindow::open_new(menu_clicked_item->path(), dir_win);
                } else {
                    CLDirectoryWindow::open_new(menu_clicked_item->path().parent(), dir_win);
                }
            }
            break;
        case 0x30: { // new window
                if (menu_clicked_item->is_image() || menu_clicked_item->is_directory() || menu_clicked_item->is_application()) {
                    dir_win->open_new_without_toolbars(menu_clicked_item->path());
                } else {
                    dir_win->open_new_without_toolbars(menu_clicked_item->path().parent());
                }
            }
            break;
    }
}


CLCreateDirWin::CLCreateDirWin(tbx::Object obj) :
        _win(obj)
{
    _dir_item_id = 0;
    _writable = _win.gadget(0);
    _win.add_about_to_be_shown_listener(this);
    _win.add_has_been_hidden_listener(this);
    tbx::ActionButton(_win.gadget(1)).add_selected_listener(this);
}

void CLCreateDirWin::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::Window win = event.id_block().ancestor_object();
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(win);
    _dir_win = dir_win;
    CLDirectoryItemData * _dir_item = nullptr;

    Log_debug("CLCreateDirWin::about_to_be_shown win:%x dir_win:%x", win.window_handle(), dir_win);

    if (dir_win->_win_treeview.window_handle() == win.window_handle()) {
        auto action_items = dir_win->get_action_items();
        if (!action_items.empty()) {
            _dir_item = action_items[0];
        }
    } else {
        _dir_item = dir_win->current_dir_item();
        std::string name = _dir_item->path().name();
        if (name.length() > 34) {
            _win.title("Create directory in: ..."+name.substr(name.length() - 34, 34));
        } else {
            _win.title("Create directory in: "+name);
        }
    }
    if (_dir_item) {
        _dir_item_id = _dir_item->id();

    }
    _writable.text("");
    Logger::info("CLCreateDirWin::about_to_be_shown item id=%ld", _dir_item_id);
}

void CLCreateDirWin::button_selected(tbx::ButtonSelectedEvent &event) {
    auto newname = _writable.text();
    if (!newname.empty()) {
        auto item = g_app_data_model.find_item_by_id(_dir_item_id);
        if (item) {
            if (!item->path().child(newname).exists()) {
                item->fs_create_directory(newname);
            } else {
                Logger::error("CLCreateDirWin::button_selected directory already exists: %s", item->path().child(newname).name().c_str());
            }
        } else {
            Logger::error("CLCreateDirWin::button_selected item id=%ld was not found", _dir_item_id);
        }
    }
}

void CLCreateDirWin::has_been_hidden(const tbx::EventInfo &event_info) {
    if (g_app_state.is_dir_window_exists(_dir_win)) {
        _dir_win->set_focus();
    }
}


CLCopyAsWin::CLCopyAsWin(tbx::Object obj) :
    _win(obj)
{
    _writable = _win.gadget(0);
    _icon = _win.gadget(3);
    _win.add_about_to_be_shown_listener(this);
    tbx::ActionButton(_win.gadget(1)).add_selected_listener(this);
    Log_debug("CLCopyAsWin::CLCopyAsWin",1);
}

void CLCopyAsWin::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::Window win = event.id_block().ancestor_object();
    CLDirectoryWindow* dir_win = CLDirectoryWindow::from_window(win);

    Log_debug("CLCopyAsWin::about_to_be_shown win:%x dir_win:%x", win.window_handle(), dir_win);
    auto action_items = dir_win->get_action_items();
    if (!action_items.empty()) {
        CLDirectoryItemData * dir_item = action_items[0];
        Log_debug("CLCopyAsWin::about_to_be_shown %s", dir_item->name().c_str());
        _icon.value(dir_item->sprite_name());
        _writable.text(dir_item->name());
        _dir_item_id = dir_item->id();
        _win.title("Copy as " + dir_item->name() + " (" + dir_item->file_type_str() + ")");
    } else {
        _dir_item_id = 0;
    }
}

void CLCopyAsWin::button_selected(tbx::ButtonSelectedEvent &event) {
    auto newname = _writable.text();
    if (_dir_item_id && !newname.empty()) {
        auto item = g_app_data_model.find_item_by_id(_dir_item_id);
        if (item && newname != item->name()) {
            item->fs_copy_local(newname);
        } else {
            Logger::error("CLCopyAsWin::button_selected item id=%ld was not found", _dir_item_id);
        }
    }
}


CLRenameWin::CLRenameWin(tbx::Object obj) :
        _win(obj)
{
    _writable = _win.gadget(0);
    _win.add_about_to_be_shown_listener(this);
    tbx::ActionButton(_win.gadget(1)).add_selected_listener(this);
}

void CLRenameWin::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::Window win = event.id_block().ancestor_object();
    CLDirectoryWindow* dir_win = CLDirectoryWindow::from_window(win);

    auto action_items = dir_win->get_action_items();
    if (!action_items.empty()) {
        CLDirectoryItemData * dir_item = action_items[0];
        Log_debug("CLRenameWin::about_to_be_shown %s", dir_item->name().c_str());
        _writable.text(dir_item->name());
        _win.title("Rename: " + dir_item->name());
        _dir_item_id = dir_item->id();
    } else {
        _dir_item_id = 0;
    }
}

void CLRenameWin::button_selected(tbx::ButtonSelectedEvent &event) {
    auto newname = _writable.text();
    if (_dir_item_id && !newname.empty()) {
        auto item = g_app_data_model.find_item_by_id(_dir_item_id);
        if (item) {
            item->fs_rename(newname);
        } else {
            Logger::error("CLRenameWin::button_selected item id=%ld was not found", _dir_item_id);
        }
    }
}


CLSetFileTypeWin::CLSetFileTypeWin(tbx::Object obj) :
        _win(obj)
{
    Log_debug("CLSetFileTypeWin::CLSetFileTypeWin0",1);
    _stringset = _win.gadget(2);
    _win.add_about_to_be_shown_listener(this);
    _win.add_has_been_hidden_listener(this);
    tbx::ActionButton(_win.gadget(1)).add_selected_listener(this);
    Log_debug("CLSetFileTypeWin::CLSetFileTypeWin",1);
}

void CLSetFileTypeWin::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    _dir_win = dir_win;
    CLDirectoryItemData* dir_item = nullptr;
    auto action_items = dir_win->get_action_items();
    file_types.clear();
    _dir_item_ids.clear();
    Log_debug("CLSetFileTypeWin::about_to_be_shown selitems:%d", action_items.size());
    for (auto item: action_items) {
        _dir_item_ids.push_back(item->id());
        Log_debug("CLSetFileTypeWin::about_to_be_shown id:%d", item->id());
        if (dir_item == nullptr) {
            dir_item = item;
        }
    }
    if (dir_item) {
        Log_debug("CLSetFileTypeWin::about_to_be_shown %s", dir_item->name().c_str());
        if (_dir_item_ids.size() > 1) {
            _win.title("Set file type for selected files");
        } else {
            _win.title("Set file type: " + dir_item->name());
        }

        std::string str_file_types;
        char val[200], label[200];
        int context = 0, len, var_type, file_type, item_file_type = dir_item->file_type(), selected_index = -1, i = 0;
        os_error *err;
        while(true) {
            err = xos_read_var_val("File$Type_*", val, sizeof(val) - 1, context, 0, &len, &context, &var_type);
            if (err) {
                break;
            }
            val[len] = 0;
            sscanf((char*)context, "File$Type_%X", &file_type);
            snprintf(label, sizeof(label)-1, "%s", val);
            file_types.push_back(CLFileTypeLabel{.label=label, .file_type=file_type});
        }

        std::sort(file_types.begin(), file_types.end(), [](const CLFileTypeLabel& a, const CLFileTypeLabel& b) {
            return (strcmp(a.label.c_str(), b.label.c_str()) < 0);
        });

        for(auto &ft : file_types) {
            if (!str_file_types.empty()) {
                str_file_types.append(",");
            }
            if (item_file_type == ft.file_type) {
                selected_index = i;
            }
            str_file_types.append(ft.label);
            i++;
        }

        _stringset.available(str_file_types);
        if (selected_index != -1) {
            _stringset.selected_index(selected_index);
        } else {
            sprintf(label, "&%X", item_file_type);
            _stringset.selected(label);
        }
    }
}

void CLSetFileTypeWin::button_selected(tbx::ButtonSelectedEvent &event) {
    if (!_dir_item_ids.empty()) {
        int idx = _stringset.selected_index(), tmp = 0, filetype = 0;
        if (idx == -1) {
            std::string needle = _stringset.selected();
            std::transform(needle.begin(), needle.end(),needle.begin(), ::toupper);
//            Log_debug("needle=%s", needle.c_str());
            for(auto &ft : file_types) {
                std::string haystack = ft.label;
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::toupper);
//                Log_debug("needle=%s haystack=%s", needle.c_str(), haystack.c_str());
                if (haystack == needle) {
                    filetype = ft.file_type;
                    Log_debug("Filetype found in the list %x", filetype);
                    break;
                }
            }
            if (!filetype) {
                for(auto &ft : file_types) {
                    std::string haystack = ft.label;
                    std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::toupper);
//                Log_debug("needle=%s haystack=%s", needle.c_str(), haystack.c_str());
                    if (haystack.find(needle) != std::string::npos) {
                        filetype = ft.file_type;
                        Log_debug("Filetype found in the list %x", filetype);
                        break;
                    }
                }
            }
            if (!filetype) {
                if (needle == "JPG") {
                    filetype = 0xc85;
                }
            }
            if (!filetype) {
                if (sscanf(needle.c_str(), "%X",  &tmp)) {
                    filetype = tmp;
                    Log_debug("Filetype recognized via scanf %x", filetype);
                } else if (sscanf(needle.c_str(), "&%X",  &tmp)) {
                    Log_debug("Filetype recognized via 2nd scanf %x", filetype);
                    filetype = tmp;
                }
            }
        } else {
            filetype = file_types[idx].file_type;
        }

        if (filetype) {
            for(auto item_id : _dir_item_ids) {
                auto item = g_app_data_model.find_item_by_id(item_id);
                if (item) {
                    if (item->is_directory() || item->is_application()) {
                        Log_debug("Set Filetype skipped for %s (it is directory or !app)", item->path().name().c_str());
                        continue;
                    }
                    item->fs_set_filetype(filetype);
                    Log_debug("Set Filetype %s %x", item->path().name().c_str(), filetype);
                } else {
                    Logger::error("CLSetFileTypeWin::button_selected item id=%ld was not found", item_id);
                }
            }
        } else {
            Logger::error("Filetype not recognized");
        }
    }
}

void CLSetFileTypeWin::has_been_hidden(const tbx::EventInfo &event_info) {
    if (g_app_state.is_dir_window_exists(_dir_win)) {
        _dir_win->set_focus();
    }
}

CLViewModeMenu::CLViewModeMenu(tbx::Object object) {
    menu = tbx::Menu(object);
    menu.add_selection_listener(this);
    menu.add_about_to_be_shown_listener(this);
    Log_debug("CLViewModeMenu::CLViewModeMenu",1);
}

void CLViewModeMenu::setup_menu(CLDirectoryWindow *dir_win) {
    tbx::MenuItem(menu.item(0)).tick(false);
    tbx::MenuItem(menu.item(1)).tick(false);
    tbx::MenuItem(menu.item(2)).tick(false);
    tbx::MenuItem(menu.item(3)).tick(false);
    tbx::MenuItem(menu.item(4)).tick(false);
    tbx::MenuItem(menu.item(5)).tick(false);
    tbx::MenuItem(menu.item(6)).tick(false);
    tbx::MenuItem(menu.item(7)).tick(false);
    tbx::MenuItem(menu.item(9)).tick(false);
    tbx::MenuItem(menu.item(0xa)).tick(false);
    tbx::MenuItem(menu.item(0xb)).tick(false);
    tbx::MenuItem(menu.item(0xc)).tick(false);

    tbx::MenuItem(menu.item(3)).fade(dir_win->operation_mode() == OPERATION_MODE_SEARCH);
    tbx::MenuItem(menu.item(4)).fade(dir_win->operation_mode() == OPERATION_MODE_SEARCH);

    switch (dir_win->view_mode()) {
        case VIEW_MODE_TILE_BIG:
            tbx::MenuItem(menu.item(0)).tick(true);
            break;
        case VIEW_MODE_TILE_SMALL:
            tbx::MenuItem(menu.item(1)).tick(true);
            break;
        case VIEW_MODE_LIST:
            tbx::MenuItem(menu.item(2)).tick(true);
            break;
        case VIEW_MODE_DIR_SIZES_BARS:
            tbx::MenuItem(menu.item(3)).tick(true);
            break;
        case VIEW_MODE_BLOCKS:
            tbx::MenuItem(menu.item(4)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_BIG:
            tbx::MenuItem(menu.item(9)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_MEDIUM:
            tbx::MenuItem(menu.item(0xa)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_SMALL:
            tbx::MenuItem(menu.item(0xb)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_TINY:
            tbx::MenuItem(menu.item(0xc)).tick(true);
            break;
    }
}

void CLViewModeMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    setup_menu(dir_win);
}

void CLViewModeMenu::menu_selection(const tbx::EventInfo &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch (event.id_block().self_component().id()) {
        case 0:
            dir_win->view_mode(VIEW_MODE_TILE_BIG);
            break;
        case 1:
            dir_win->view_mode(VIEW_MODE_TILE_SMALL);
            break;
        case 2:
            dir_win->view_mode(VIEW_MODE_LIST);
            break;
        case 3:
            dir_win->view_mode(VIEW_MODE_DIR_SIZES_BARS);
            break;
        case 4:
            dir_win->view_mode(VIEW_MODE_BLOCKS);
            break;
        case 9:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_BIG);
            break;
        case 0xa:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_MEDIUM);
            break;
        case 0xb:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_SMALL);
            break;
        case 0xc:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_TINY);
            break;
    }
    setup_menu(dir_win);
}

CLThumbnailsSubViewModeMenu::CLThumbnailsSubViewModeMenu(tbx::Object object) {
    menu = tbx::Menu(object);
    menu.add_selection_listener(this);
    menu.add_about_to_be_shown_listener(this);
    Log_debug("CLThumbnailsSubViewModeMenu::CLThumbnailsSubViewModeMenu", 1);
}

void CLThumbnailsSubViewModeMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    Log_debug("CLThumbnailsSubViewModeMenu::about_to_be_shown", 1);
    int obj_id = event.id_block().ancestor_object().handle();
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    setup_menu(dir_win);
}

void CLThumbnailsSubViewModeMenu::menu_selection(const tbx::EventInfo &event) {
    Log_debug("CLThumbnailsSubViewModeMenu::menu_selection", 1);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    switch (event.id_block().self_component().id()) {
        case 0x1:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_BIG);
            break;
        case 0x2:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_MEDIUM);
            break;
        case 0x3:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_SMALL);
            break;
        case 0x4:
            dir_win->view_mode(VIEW_MODE_THUMBNAILS_TINY);
            break;
    }
    setup_menu(dir_win);
}

void CLThumbnailsSubViewModeMenu::setup_menu(CLDirectoryWindow *dir_win) {
    tbx::MenuItem(menu.item(1)).tick(false);
    tbx::MenuItem(menu.item(2)).tick(false);
    tbx::MenuItem(menu.item(3)).tick(false);
    tbx::MenuItem(menu.item(4)).tick(false);

    switch (dir_win->view_mode()) {
        case VIEW_MODE_THUMBNAILS_BIG:
            tbx::MenuItem(menu.item(1)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_MEDIUM:
            tbx::MenuItem(menu.item(2)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_SMALL:
            tbx::MenuItem(menu.item(3)).tick(true);
            break;
        case VIEW_MODE_THUMBNAILS_TINY:
            tbx::MenuItem(menu.item(4)).tick(true);
            break;
    }
}


CLActionMenu::CLActionMenu(tbx::Object object) {
    menu = tbx::Menu(object);
    menu.add_selection_listener(this);
    menu.add_about_to_be_shown_listener(this);
    Log_debug("CLActionMenu::CLActionMenu", 1);
}

void CLActionMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::MenuItem(menu.item(0)).fade(true);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    CLDirectoryItemData *_dir_item = dir_win->menu_clicked_item();
    std::string selection_txt;
    tbx::Path p, dirpath;
    tbx::MenuItem(menu.item(0)).fade(false);
    tbx::MenuItem(menu.item(1)).fade(false);
    tbx::MenuItem(menu.item(2)).fade(false);

    if (_dir_item) {
        p = _dir_item->path();
        if (_dir_item->is_directory()) {
            dirpath = p;
        } else {
            dirpath = p.parent().name();
        }
    } else {
        if (dir_win->operation_mode() == OPERATION_MODE_DIR) {
            dirpath = p = dir_win->current_dir_path();
        } else {
            dirpath = p = "";
        }
    }
    Log_debug("CLActionMenu::about_to_be_shown file:%d type:%x", dirpath.file(), dirpath.file_type());
    if (!dirpath.name().empty()) {
        tbx::MenuItem(menu.item(0)).text(g_my_app->messages().message("CopyPathToClipboard", p.leaf_name()));

        if (g_app_data_model.is_favorite(dirpath.name())) {
            selection_txt = g_my_app->messages().message("RemoveFromFavorite", dirpath.leaf_name());
        } else {
            selection_txt = g_my_app->messages().message("AddToFavorite", dirpath.leaf_name());
        }
        tbx::MenuItem(menu.item(1)).text(selection_txt);

//        if (dirpath.file()) {
//            tbx::MenuItem(menu.item(0)).fade(true);
//            tbx::MenuItem(menu.item(2)).fade(true);
//        }
    } else {
        tbx::MenuItem(menu.item(0)).fade(true);
        tbx::MenuItem(menu.item(1)).fade(true);
    }

    if (dir_win->operation_mode() != OPERATION_MODE_DIR) {
        selection_txt = g_my_app->messages().message("ExportAsCSV", "Current contents");
    } else {
        selection_txt = g_my_app->messages().message("ExportAsCSV", "'"+dirpath.leaf_name()+"'");
    }
    tbx::MenuItem(menu.item(2)).text(selection_txt);
}

void CLActionMenu::menu_selection(const tbx::EventInfo &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    Log_debug("CLActionMenu::menu_selection %x", obj_id);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    CLDirectoryItemData *_dir_item = dir_win->menu_clicked_item();
    tbx::Path p, dirpath;
    if (_dir_item) {
        p =  tbx::Path(_dir_item->path());
        if (_dir_item->is_directory()) {
            dirpath = p;
        } else {
            dirpath = p.parent().name();
        }
    } else {
        dirpath = p = dir_win->current_dir_path();
    }
    switch (event.id_block().self_component().id()) {
        case 0: // copy to clipboard
            CLClipboard::instance()->clipboard_set(p.name().c_str());
            dir_win->set_status_line(g_my_app->messages().message("PathCopiedToClipboard", p.leaf_name()),false);
            break;
        case 1: // add/remove favorites
            if (g_app_data_model.is_favorite(dirpath.name())) {
                g_app_data_model.remove_favorite(dirpath.name());
            } else {
                g_app_data_model.add_favorite(dirpath.name());
            }
            break;

    }
}


CLMExpCSV::CLMExpCSV(tbx::Object object) {
    menu = tbx::Menu(object);
    menu.add_about_to_be_shown_listener(this);
    Log_debug("CLMExpCSV::CLMExpCSV", 1);
}

void CLMExpCSV::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    if (dir_win->operation_mode() == OPERATION_MODE_DIR) {
        tbx::MenuItem(menu.item(2)).fade(false);
        tbx::MenuItem(menu.item(3)).fade(false);
    } else {
        tbx::MenuItem(menu.item(2)).fade(true);
        tbx::MenuItem(menu.item(3)).fade(true);
    }
}


CLExportAsCSV::CLExportAsCSV(tbx::Object obj) {
    _saveas = obj;
    _saveas.add_about_to_be_shown_listener(this);
    _saveas.add_has_been_hidden_listener(this);
    _saveas.set_save_to_file_handler(this);
}

void CLExportAsCSV::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::SaveAs save_as = event.id_block().self_object();
    tbx::Window win = save_as.ancestor_object();
    parent_menu_id = event.id_block().parent_component().id();
    _dir_win = CLDirectoryWindow::from_window(win);
    if (_dir_win->operation_mode() != OPERATION_MODE_DIR) {
        save_as.title("Export Current contents as CSV");
        _export_path = "";
    } else {
        _export_path = _dir_win->current_dir_path();
        save_as.title("Export "+_export_path.name()+" as CSV");
    }

//    Log_debug("CLExportAsCSV::about_to_be_shown mitem:%d", mitem_id);
}

void CLExportAsCSV::has_been_hidden(const tbx::EventInfo &event_info) {
    if (g_app_state.is_dir_window_exists(_dir_win)) {
        _dir_win->set_focus();
    }
}

void CLExportAsCSV::saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) {
    g_hourglass_on();
    num_processed = 0;
    export_as_file(_export_path, file_name);
    saveas.file_save_completed(true, file_name);
    g_hourglass_off();
}

void CLExportAsCSV::generate_report(const tbx::Path& root, const tbx::Path& dir, std::vector<tbx::Path>& items) {
    int root_len = root.name().size();
    int file_type;
    std::string name;
    tbx::PathInfo pi;
    int percent_processed;
    for(tbx::PathInfo::Iterator it = tbx::PathInfo::begin(dir); it != tbx::PathInfo::end(); ++it) {
        num_processed++;
        percent_processed = std::min(100, num_processed / 100);
        g_hourglass_percentage(percent_processed);

        tbx::Path child = dir.child(it->name());
        items.push_back(dir.child(it->name()));
        if (it->directory()) {
            file_type = it->file_type();
            if (file_type == 0x2000) {
                if (parent_menu_id == 3) {
                    generate_report(root, child, items);
                }
            } else {
                if (parent_menu_id > 1) {
                    generate_report(root, child, items);
                }
            }
        }
    }
}

void CLExportAsCSV::export_as_file(const tbx::Path& start, const std::string& filename) {
    std::vector<tbx::Path> items;
    tbx::Path item;
    tbx::PathInfo pi;
    int root_len = start.name().length() + 1, file_type;
    int percent_processed;
    char buf[200];
    char tm_buf[100];
//    char out_str[4096];
    char *empty_parent = "";
    char *parent_dir;
    char *item_name;
    char* tm_format = "%CE%YR.%MN.%DY %24:%MI:%SE";
    std::string mtime;
    num_processed = 0;
    if (!g_app_state.is_dir_window_exists(_dir_win)) {
        Log_error("CLExportAsCSV::export_as_file parent direcotry window not exists anymore: %p", _dir_win);
        return;
    }
    if (_dir_win->operation_mode() != OPERATION_MODE_DIR) {
        for(auto dirit : _dir_win->current_dir_items()) {
            items.push_back(dirit->path());
        }
    } else {
        generate_report(start, start, items);
    }

    std::sort(items.begin(), items.end(), [](const tbx::Path& a, const tbx::Path& b) {
        return stricmp(a.name().c_str(), b.name().c_str()) < 0;
    });
    num_processed = 0;

    FILE *out = fopen(filename.c_str(), "w+");
    if (_dir_win->operation_mode() != OPERATION_MODE_DIR) {
        fprintf(out, "\"Export of current contents\",,,,\n");
    } else {
        fprintf(out, "\"Export of %s\",,,,\n", start.name().c_str());
    }
    fprintf(out, "\"Folder\",\"Name\",\"Type\",\"Size\",\"Date\"\n");
    for(auto item : items) {
        num_processed++;
        percent_processed = std::min(100, num_processed / 100);
        g_hourglass_percentage(percent_processed);

        std::string file_type_text;
        std::string sz;
        CLDirectoryItemData *dir_item;
        item.path_info(pi);
        file_type = pi.file_type();
        if (file_type == 0x1000) {
            file_type_text = "Directory";
            dir_item = g_app_data_model.find_item_by_path(item);
            sz = to_string(dir_item->dir_size());
        } else if (file_type == 0x2000) {
            file_type_text = "Application";
            dir_item = g_app_data_model.find_item_by_path(item);
            sz = to_string(dir_item->dir_size());
        } else {
            file_type_text = get_file_type_text(pi.file_type());
            if (file_type_text == "Unknown") {
                sprintf(buf,"&%x", pi.file_type());
                file_type_text = buf;
            }
            sz = to_string(pi.length());
        }
        if (_dir_win->operation_mode() != OPERATION_MODE_DIR) {
            parent_dir = (char*)item.parent().name().c_str();
        } else {
            if (item.parent().name().length() < root_len) {
                parent_dir = empty_parent;
            } else {
                parent_dir = (char*)item.parent().name().c_str() + root_len;
            }
        }
        item_name = (char*)pi.name().c_str();
        pi.modified_time().text(tm_format, tm_buf, sizeof(tm_buf) - 1);
//        snprintf(out_str, sizeof(out_str) - 1,"\"%s\",\"%s\",\"%s\",%s,%s\n", item_name, parent_dir,
//                 file_type_text.c_str(), sz.c_str(), tm_buf);
//        Log_debug("%s pl=%d rl=%d ep=%p p=%p", parent_dir, item.parent().name().length(), root_len, empty_parent, parent_dir);
//        Log_debug("%s", out_str);
//        fputs(out_str, out);
        fprintf(out, "\"%s\",\"%s\",\"%s\",%s,%s\n", parent_dir, item_name,
                file_type_text.c_str(), sz.c_str(), tm_buf);
    }
    fclose(out);
    tbx::Path(filename).file_type(0xdfe);
}


CLFavoriteMenu::CLFavoriteMenu(tbx::Menu& mnu, CLDirectoryWindow* dir_win) {
    _menu = mnu;
    _dir_win = dir_win;
    _menu.add_selection_listener(this);
    _menu.add_about_to_be_shown_listener(this);
    _menu.add_has_been_hidden_listener(this);
    Log_debug("CLFavoriteMenu::CLFavoriteMenu", 1);
}


void CLFavoriteMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    std::vector<std::string> favs = g_app_data_model.get_favorites();
    Log_debug("CLFavoriteMenu::about_to_be_shown", 1);
    if (!favs.empty()) {
        tbx::MenuItem menu_item = _menu.item(0);
        menu_item.text(favs[0]);
        Log_debug("CLFavoriteMenu::about_to_be_shown set text [%s]", favs[0].c_str());
        if (favs.size() > 1) {
            tbx::res::ResMenu res_menu = tbx::Application::instance()->resource("MFavorite");
            tbx::res::ResMenuItem res_item = res_menu.item(0);
            for(int i = 1; i < favs.size(); i++) {
                res_item.text(favs[i]);
                res_item.component_id(i);
                _menu.add(res_item);
            }
        }
    }
}

void CLFavoriteMenu::has_been_hidden(const tbx::EventInfo &event_info) {
    Log_debug("CLFavoriteMenu::~CLFavoriteMenu()", 1);
    _menu.delete_object();
    delete this;
}

void CLFavoriteMenu::menu_selection(const tbx::EventInfo &event) {
    int idx = event.id_block().self_component().id();
    tbx::PointerInfo where(true,false);
    std::vector<std::string> favs = g_app_data_model.get_favorites();
    CLDirectoryWindow *dw = _dir_win;
    Log_debug("CLFavoriteMenu::menu_selection %d menu:%d adjust:%d select:%d", idx, where.menu_down(), where.adjust_down(), where.select_down());
    if (where.adjust_down()) {
        g_app_data_model.remove_favorite(favs[idx]);
        _menu.hide();
    } else {
        CLDirectoryItemData *item = g_app_data_model.get_or_refresh_path(favs[idx]);
        if (item) {
            if (item->root()->name() != dw->_current_root) {
                dw->change_root_dir(item->root()->name());
            }
            dw->change_current_directory(item, true, true);
        }
    }
//    Log_debug("CLFavoriteMenu::menu_selection end", 1);
}


CLRootsMenu::CLRootsMenu(tbx::Object mnu) {
    _menu = mnu;
    _menu.add_selection_listener(this);
    _menu.add_about_to_be_shown_listener(this);
    Log_debug("CLRootsMenu::CLRootsMenu", 1);
}

void CLRootsMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int i;
    std::vector<CLDirectoryItemData*> roots = g_app_data_model.get_root_items();
//    Log_debug("CLRootsMenu::about_to_be_shown", 1);
    if (!roots.empty()) {
        tbx::MenuItem menu_item = _menu.item(0);
        menu_item.text(roots[0]->name());
        Log_debug("CLRootsMenu::about_to_be_shown set text [%s]", roots[0]->name().c_str());
        for(i = 1; i < _menu_entries_number; i++) {
            _menu.erase(i);
        }
        if (roots.size() > 1) {
            tbx::res::ResMenu res_menu = tbx::Application::instance()->resource("MRoots");
            tbx::res::ResMenuItem res_item = res_menu.item(0);
            for(i = 1; i < roots.size(); i++) {
                res_item.text(roots[i]->name());
                res_item.component_id(i);
                _menu.add(res_item);
            }
            _menu_entries_number = roots.size();
        }
    }
}

void CLRootsMenu::menu_selection(const tbx::EventInfo &event) {
    int idx = event.id_block().self_component().id();
    std::vector<CLDirectoryItemData*> roots = g_app_data_model.get_root_items();
    CLDirectoryWindow::open_new(roots[idx]->path());
}


CLAccessMenu::CLAccessMenu(tbx::Object mnu) {
    _menu = mnu;
    _menu.add_selection_listener(this);
    _menu.add_about_to_be_shown_listener(this);
    Log_debug("CLAccessMenu::CLAccessMenu", 1);
}

void CLAccessMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int i;
    tbx::Window win = event.id_block().ancestor_object();
    Log_debug("CLAccessMenu::about_to_be_shown ancestor_win=%x this=%p", win.handle(), this);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(win);
    CLDirectoryItemData *dir_item = nullptr;
    auto action_items = dir_win->get_action_items();
    _selected_items.clear();
    _dir_path.clear();
    if (!action_items.empty()) {
        _selected_items = g_app_data_model.items_names(action_items);
        _dir_path = action_items[0]->parent()->path().name();
        if (action_items.size() > 1) {
            dir_item = nullptr;
        } else {
            dir_item = action_items[0];
        }
    }

    if (dir_item) { // single clicked item
        _menu.title(std::string("Access '") + dir_item->name() + std::string("'"));
        _menu.item(0).tick(dir_item->is_protected());
        _menu.item(1).tick(dir_item->is_unprotected());
        _menu.item(2).tick(dir_item->is_public());
        _menu.item(3).tick(dir_item->is_private());
    } else { // nothing selected
        _menu.item(0).tick(false);
        _menu.item(1).tick(false);
        _menu.item(2).tick(false);
        _menu.item(3).tick(false);
    }
}

void CLAccessMenu::menu_selection(const tbx::EventInfo &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    Log_debug("CLCLAccessMenu::menu_selection %x cmp_id", obj_id);
    if (_dir_path.empty()) {
        Log_debug("CLAccessMenu::menu_selection no items selected",1);
        return;
    }
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    int leave_bits = 0xffff;
    int attrs = 0;

    switch (event.id_block().self_component().id()) {
        case 0x0: // protect
            attrs |= tbx::PathInfo::OWNER_LOCKED;
            attrs &= ~tbx::PathInfo::OWNER_WRITE;
            leave_bits &= ~(tbx::PathInfo::OWNER_LOCKED | tbx::PathInfo::OWNER_WRITE);
            break;
        case 0x1:// unprotect
            attrs &= ~tbx::PathInfo::OWNER_LOCKED;
            attrs |= tbx::PathInfo::OWNER_WRITE;
            leave_bits &= ~(tbx::PathInfo::OWNER_LOCKED |tbx::PathInfo::OWNER_WRITE);
            break;
        case 0x2: // public
            attrs |= tbx::PathInfo::OTHER_READ;
            leave_bits &= ~tbx::PathInfo::OTHER_READ;
            break;
        case 0x3: // private
            attrs &= ~tbx::PathInfo::OTHER_READ;
            leave_bits &= ~tbx::PathInfo::OTHER_READ;
            break;
    }
    g_app_data_model.fs_access(_dir_path, _selected_items, attrs, leave_bits, true);
}

CLAccessWin::CLAccessWin(tbx::Object obj) :
    _win(obj)
{
    _win.add_about_to_be_shown_listener(this);
    tbx::ActionButton(_win.gadget(0xd)).add_selected_listener(this);
    Log_debug("CLAccessWin::CLAccessWin %p", this);
}

void CLAccessWin::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    int i;
    tbx::Window win = event.id_block().ancestor_object();
    Log_debug("CLAccessWin::about_to_be_shown ancestor_win=%x this=%p", win.handle(), this);
    CLDirectoryWindow *dir_win = CLDirectoryWindow::from_window(win);
    CLDirectoryItemData *dir_item = nullptr;
    _selected_items.clear();
    _dir_path.clear();
    auto action_items = dir_win->get_action_items();
    if (!action_items.empty()) {
        _selected_items = g_app_data_model.items_names(action_items);
        _dir_path = action_items[0]->parent()->path().name();
        if (action_items.size() > 1) {
            dir_item = nullptr;
        } else {
            dir_item = action_items[0];
        }
    }

    Log_debug("CLAccessWin::about_to_be_shown dirwin:%s", dir_win->current_dir_path().c_str());
    int leave_bits = 0xffff;
    int attrs = 0;
    tbx::RadioButton(_win.gadget(0x100)).on(true);
    tbx::RadioButton(_win.gadget(0x101)).on(true);
    tbx::RadioButton(_win.gadget(0x102)).on(true);
    tbx::RadioButton(_win.gadget(0x103)).on(true);
    tbx::RadioButton(_win.gadget(0x104)).on(true);
    if (dir_item) {
        tbx::OptionButton(_win.gadget(0xc)).on(!dir_item->is_file());
        attrs = dir_item->attrs();
        Log_debug("CLAccessWin::about_to_be_shown %s attrs = %x", dir_item->path().name().c_str(), attrs);
        tbx::RadioButton(_win.gadget(0x80)).on(attrs & tbx::PathInfo::OWNER_LOCKED);
        tbx::RadioButton(_win.gadget(0x81)).on(attrs & tbx::PathInfo::OWNER_READ);
        tbx::RadioButton(_win.gadget(0x82)).on(attrs & tbx::PathInfo::OWNER_WRITE);
        tbx::RadioButton(_win.gadget(0x83)).on(attrs & tbx::PathInfo::OTHER_READ);
        tbx::RadioButton(_win.gadget(0x84)).on(attrs & tbx::PathInfo::OTHER_WRITE);
        if (dir_item->is_file()) {
            tbx::RadioButton(_win.gadget(0x90)).on(!(attrs & tbx::PathInfo::OWNER_LOCKED));
            tbx::RadioButton(_win.gadget(0x91)).on(!(attrs & tbx::PathInfo::OWNER_READ));
            tbx::RadioButton(_win.gadget(0x92)).on(!(attrs & tbx::PathInfo::OWNER_WRITE));
            tbx::RadioButton(_win.gadget(0x93)).on(!(attrs & tbx::PathInfo::OTHER_READ));
            tbx::RadioButton(_win.gadget(0x94)).on(!(attrs & tbx::PathInfo::OTHER_WRITE));
        }
    }
}

void CLAccessWin::button_selected(tbx::ButtonSelectedEvent &event) {
    int i;
    Log_debug("CLAccessWin::button_selected this=%p", this);
    int leave_bits = 0xffff;
    int attrs = 0;

    if (tbx::RadioButton(_win.gadget(0x80)).on()) {
        attrs |= tbx::PathInfo::OWNER_LOCKED;
        leave_bits &= ~tbx::PathInfo::OWNER_LOCKED;
    } else if (tbx::RadioButton(_win.gadget(0x90)).on()) {
        leave_bits &= ~tbx::PathInfo::OWNER_LOCKED;
    }

    if (tbx::RadioButton(_win.gadget(0x81)).on()) {
        attrs |= tbx::PathInfo::OWNER_READ;
        leave_bits &= ~tbx::PathInfo::OWNER_READ;
    } else if (tbx::RadioButton(_win.gadget(0x91)).on()) {
        leave_bits &= ~tbx::PathInfo::OWNER_READ;
    }

    if (tbx::RadioButton(_win.gadget(0x82)).on()) {
        attrs |= tbx::PathInfo::OWNER_WRITE;
        leave_bits &= ~tbx::PathInfo::OWNER_WRITE;
    } else if (tbx::RadioButton(_win.gadget(0x92)).on()) {
        leave_bits &= ~tbx::PathInfo::OWNER_WRITE;
    }

    if (tbx::RadioButton(_win.gadget(0x83)).on()) {
        attrs |= tbx::PathInfo::OTHER_READ;
        leave_bits &= ~tbx::PathInfo::OTHER_READ;
    } else if (tbx::RadioButton(_win.gadget(0x93)).on()) {
        leave_bits &= ~tbx::PathInfo::OTHER_READ;
    }

    if (tbx::RadioButton(_win.gadget(0x84)).on()) {
        attrs |= tbx::PathInfo::OTHER_WRITE;
        leave_bits &= ~tbx::PathInfo::OTHER_WRITE;
    } else if (tbx::RadioButton(_win.gadget(0x94)).on()) {
        leave_bits &= ~tbx::PathInfo::OTHER_WRITE;
    }

    g_app_data_model.fs_access(_dir_path, _selected_items, attrs, leave_bits,
                               tbx::OptionButton(_win.gadget(0xc)).on());
}

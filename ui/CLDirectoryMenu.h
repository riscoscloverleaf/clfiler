//
// Created by lenz on 10/14/21.
//

#ifndef CLFILER_DIRITEMMENU_H
#define CLFILER_DIRITEMMENU_H

#include "CLDirectoryWindow.h"
#include <tbx/menu.h>
#include <tbx/menuselectionlistener.h>
#include <tbx/keylistener.h>
#include <tbx/writablefield.h>
#include <tbx/actionbutton.h>
#include <tbx/buttonselectedlistener.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/saveas.h>

class CLDirectoryMenu :
        public tbx::AboutToBeShownListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;

public:
    CLDirectoryMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLDirectorySelectionMenu :
        public tbx::AboutToBeShownListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;
public:
    CLDirectorySelectionMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLTreeMenu :
        public tbx::AboutToBeShownListener,
        public tbx::HasBeenHiddenListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;
    CLDirectoryWindow* dir_win;
public:
    CLTreeMenu(tbx::Object obj, CLDirectoryWindow* dir_win);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void menu_selection(const tbx::EventInfo &event) override;

    void cut_item(const tbx::Path& item);
    void copy_item(const tbx::Path& item);
};


class CLDirectoryDisplayMenu :
        public tbx::AboutToBeShownListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;

public:
    CLDirectoryDisplayMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLDirectoryFolderDisplayMenu :
        public tbx::AboutToBeShownListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;

public:
    CLDirectoryFolderDisplayMenu(tbx::Object obj);

    void setup_menu(CLDirectoryWindow *dir_win);
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};

class CLDirectoryNewMenu :
        public tbx::AboutToBeShownListener,
        public tbx::MenuSelectionListener
{
private:
    tbx::Menu _menu;
    CLDirectoryItemData *menu_clicked_item = nullptr;

public:
    CLDirectoryNewMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};

class CLRenameWin :
        public tbx::ButtonSelectedListener,
        public tbx::AboutToBeShownListener
{
private:
    tbx::WritableField _writable;
    tbx::Window _win;
    unsigned int _dir_item_id;
public:
    CLRenameWin(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
};


class CLCreateDirWin :
        public tbx::ButtonSelectedListener,
        public tbx::AboutToBeShownListener,
        public tbx::HasBeenHiddenListener
{
private:
    tbx::WritableField _writable;
    tbx::Window _win;
    unsigned int _dir_item_id;
    CLDirectoryWindow *_dir_win;
public:
    CLCreateDirWin(tbx::Object obj);

    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
};


class CLCopyAsWin :
        public tbx::ButtonSelectedListener,
        public tbx::AboutToBeShownListener
{
private:
    tbx::WritableField _writable;
    tbx::Button _icon;
    tbx::Window _win;
    unsigned int _dir_item_id;
public:
    CLCopyAsWin(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
};


class CLSetFileTypeWin :
        public tbx::ButtonSelectedListener,
        public tbx::AboutToBeShownListener,
        public tbx::HasBeenHiddenListener
{
public:
    struct CLFileTypeLabel {
        std::string label;
        int file_type;
    };
    CLSetFileTypeWin(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;

    void has_been_hidden(const tbx::EventInfo &event_info) override;

private:
    tbx::StringSet _stringset;
    tbx::Window _win;
    std::vector<unsigned int> _dir_item_ids;
    std::vector<CLFileTypeLabel> file_types;
    CLDirectoryWindow *_dir_win;
};

class CLViewModeMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener {
private:
    tbx::Menu menu;
public:
    CLViewModeMenu(tbx::Object object);

    void setup_menu(CLDirectoryWindow *dir_win);
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};

class CLThumbnailsSubViewModeMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener {
private:
    tbx::Menu menu;
public:
    CLThumbnailsSubViewModeMenu(tbx::Object object);

    void setup_menu(CLDirectoryWindow *dir_win);
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLExportAsCSV : tbx::SaveAsSaveToFileHandler,
                      tbx::AboutToBeShownListener,
                      tbx::HasBeenHiddenListener
{
private:
    tbx::SaveAs _saveas;
    tbx::Path _export_path;
    CLDirectoryWindow* _dir_win;
    int parent_menu_id;
    int num_processed;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) override;

    void has_been_hidden(const tbx::EventInfo &event_info) override;

public:
    CLExportAsCSV(tbx::Object obj);

    void generate_report(const tbx::Path& root, const tbx::Path& dir, std::vector<tbx::Path>& items);
    void export_as_file(const tbx::Path& start, const std::string& filename);
};

class CLActionMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener {
private:
    tbx::Menu menu;

public:
    CLActionMenu(tbx::Object object);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLMExpCSV : public tbx::AboutToBeShownListener {
private:
    tbx::Menu menu;

public:
    CLMExpCSV(tbx::Object object);
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};


class CLFavoriteMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener, public tbx::HasBeenHiddenListener {
private:
    CLDirectoryWindow* _dir_win;
    tbx::Menu _menu;
public:
    CLFavoriteMenu(tbx::Menu& mnu, CLDirectoryWindow* dir_win);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void menu_selection(const tbx::EventInfo &event) override;
};

class CLRootsMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener {
private:
    tbx::Menu _menu;
    int _menu_entries_number = 1;
public:
    CLRootsMenu(tbx::Object mnu);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};

class CLAccessMenu : public tbx::MenuSelectionListener, public tbx::AboutToBeShownListener {
private:
    std::string _dir_path;
    std::vector<std::string> _selected_items;
    tbx::Menu _menu;
public:
    CLAccessMenu(tbx::Object mnu);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLAccessWin :
        public tbx::ButtonSelectedListener,
        public tbx::AboutToBeShownListener
{
private:
    tbx::Window _win;
    std::string _dir_path;
    std::vector<std::string> _selected_items;
public:
    CLAccessWin(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
};

#endif //CLFILER_DIRITEMMENU_H

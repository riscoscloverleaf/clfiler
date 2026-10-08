//
// Created by lenz on 9/2/21.
//

#ifndef CLFILER_CLDIRECTORYWINDOW_H
#define CLFILER_CLDIRECTORYWINDOW_H

#include <tbx/ext/treeview.h>
#include <tbx/ext/treeviewlisteners.h>
#include <tbx/view/selection.h>
#include <tbx/scrollrequestlistener.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/abouttobeshownlistener.h>
#include <tbx/keylistener.h>
#include <tbx/writablefield.h>
#include <tbx/textchangedlistener.h>
#include <tbx/view/iconitemrenderer.h>
#include <tbx/view/viewitems.h>
#include <tbx/view/tileview.h>
#include <tbx/view/reportview.h>
#include <tbx/path.h>
#include <tbx/loader.h>
#include <tbx/caretlistener.h>
#include <tbx/pointerlistener.h>
#include <tbx/fileraction.h>
#include <tbx/sprite.h>
#include <tbx/radiobutton.h>
#include <tbx/button.h>
#include <tbx/stringset.h>
#include <tbx/saver.h>
#include <tbx/size.h>
#include <tbx/application.h>
#include <tbx/optionbuttonstatelistener.h>
#include <tbx/buttonselectedlistener.h>
#include <tbx/colourmenu.h>
#include <tbx/timer.h>
#include <cstdio>
#include <cstdlib>
#include <cloverleaf/Logger.h>
#include <cloverleaf/CLGraphics.h>
#include <tbx/actionbutton.h>
#include <tbx/colourdbox.h>
#include "../utils.h"
#include "../global.h"
#include "../model/AppDataModel.h"
#include "CLDirectoryMenu.h"
#include "CLDirectoryMenu.h"

#undef self_component
#undef parent_component

#define VIEW_MODE_TILE_BIG 0x10
#define VIEW_MODE_TILE_SMALL 0x20
#define VIEW_MODE_LIST 0x30
#define VIEW_MODE_DIR_SIZES_BARS 0x31
#define VIEW_MODE_BLOCKS 0x40
#define VIEW_MODE_THUMBNAILS 0x50
#define VIEW_MODE_THUMBNAILS_BIG 0x51
#define VIEW_MODE_THUMBNAILS_MEDIUM 0x52
#define VIEW_MODE_THUMBNAILS_SMALL 0x53
#define VIEW_MODE_THUMBNAILS_TINY 0x54

#define OPERATION_MODE_DIR 1
#define OPERATION_MODE_SEARCH 2
#define OPERATION_MODE_TIMELINE 3

#define VIEW_SCROLL_TOP 1
#define VIEW_SCROLL_BOTTOM 2

#define SORT_NAME 0x10
#define SORT_NAME_DESC 0x11
#define SORT_NAME_DIRS 0x20
#define SORT_NAME_DIRS_DESC 0x21
#define SORT_DATE 0x30
#define SORT_DATE_DESC 0x31
#define SORT_SIZE 0x40
#define SORT_SIZE_DESC 0x41
#define SORT_FILETYPE 0x50
#define SORT_FILETYPE_DESC 0x51

#define TOOLBARS_VISIBLE_NONE 0
#define TOOLBARS_VISIBLE_TOP 1
#define TOOLBARS_VISIBLE_TOP_BOTTOM 2
#define TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW 3

class CLDirectoryWindow;
class CLBlocksView;
class CLRenameInplace;
class CLTreeMenu;
class CLRenameWin;
class CLCopyAsWin;
class CLFavoriteMenu;
class CLCreateDirWin;
class CLSaveCachedThumbnailsSizesCmd;
class CLAccessWin;
class CLAccessMenu;
class CLDirectorySelectionMenu;

class SmallSpriteAndNameRenderer : public tbx::view::IconItemRenderer
{
    CLDirectoryWindow *_me;
    bool _use_display_name;
public:
    SmallSpriteAndNameRenderer(CLDirectoryWindow *cldirview, bool use_display_name) :
            _me(cldirview), _use_display_name(use_display_name), tbx::view::IconItemRenderer(nullptr, nullptr, false) {};
    virtual ~SmallSpriteAndNameRenderer() {};
    virtual std::string text(unsigned int index) const;
    virtual std::string sprite_name(unsigned int index) const;
    virtual void render(const tbx::view::ItemRenderer::Info &info);
    bool hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const override;
    virtual tbx::Size size(unsigned int index) const;
    CLBaseImage* small_sprite_image(unsigned int index);
};

class BigSpriteAndNameRenderer : public tbx::view::IconItemRenderer {
    CLDirectoryWindow *_me;
public:
    BigSpriteAndNameRenderer(CLDirectoryWindow *cldirview) :
            _me(cldirview), tbx::view::IconItemRenderer(nullptr, nullptr, false) {};
    virtual ~BigSpriteAndNameRenderer() {};
    virtual std::string text(unsigned int index) const;
    virtual std::string sprite_name(unsigned int index) const;
    bool hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const override;
    virtual tbx::Size size(unsigned int index) const;
    void render(const ItemRenderer::Info &info) override;
};

class ThumbnailRenderer : public tbx::view::ItemRenderer {
    CLDirectoryWindow *_me;
    static tbx::TranslationTable tt;
    int _thumb_width;
    int _thumb_height;
public:
    ThumbnailRenderer(CLDirectoryWindow *cldirview, int thumb_width, int thumb_height) :
            _me(cldirview),
            _thumb_width(thumb_width),
            _thumb_height(thumb_height),
            tbx::view::ItemRenderer() {};
    virtual ~ThumbnailRenderer() {};

    void render(const tbx::view::ItemRenderer::Info &info) override;
    bool hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const override;
    bool intersects(unsigned int index, const tbx::Size &size, const tbx::BBox &box) const override;
    tbx::Size size(unsigned int index) const override;
    unsigned int width(unsigned int index) const override {
        return size(index).width;
    }
    unsigned int height(unsigned int index) const override {
        return size(index).height;
    }
    bool can_make_thumbnail(CLDirectoryItemData* item) const;
    CLBaseImage * thumb_img(CLDirectoryItemData* item) const;
    static int text_split_and_get_height(CLDirectoryItemData *dir_item, int cell_width, std::list<std::string>& lines);
};

class CLListWimpFontRenderer : public tbx::view::WimpFontItemRenderer {
    CLDirectoryWindow *_me;
    int _flags = 0;
public:
    CLListWimpFontRenderer(CLDirectoryWindow *cldirview, int flags, tbx::view::ItemViewValue<std::string> *vv) :
            _me(cldirview), _flags(flags), tbx::view::WimpFontItemRenderer(vv) {}

    void render(const ItemRenderer::Info &info) override;
};

class CLItemSizeBarRenderer : public tbx::view::ItemRenderer {
    CLDirectoryWindow *_me;
public:
    CLItemSizeBarRenderer(CLDirectoryWindow *cldirview) :
            _me(cldirview) {}
    virtual ~CLItemSizeBarRenderer() {};

    virtual void render(const ItemRenderer::Info &info);
    virtual unsigned int width(unsigned int index) const { return 300; };
    virtual unsigned int height(unsigned int index) const { return 10; };
    virtual tbx::Size size(unsigned int index) const { return tbx::Size(10, 300); };
};

class CLReportView : public tbx::view::ReportView {
protected:
    CLDirectoryWindow *_me;
public:
    explicit CLReportView(CLDirectoryWindow *cldirwin);
    virtual void setup_columns();
    void redraw(const tbx::RedrawEvent &event) override;
    void update_window_extent() override;
    void refresh() override;
};

class CLTileView : public tbx::view::TileView {
friend CLDirectoryWindow;
private:
    CLDirectoryWindow *_me;
    tbx::Size _max_thumb;
    bool _is_thumbs_size_calculated;

public:
    CLTileView(CLDirectoryWindow *cldirwin, tbx::view::ItemRenderer *itemRenderer);

    void init_thumbs_size() { _is_thumbs_size_calculated = false; _max_thumb.width = 0; _max_thumb.height = 0; }
    void thumbs_size_calculated() { _is_thumbs_size_calculated = true; }
    bool is_thumbs_size_calculated() { return _is_thumbs_size_calculated; }
    void set_max_thumb_size(int thumb_w, int thumb_h);

    const tbx::Size& get_max_thumb_size() { return _max_thumb; }
    int cols_per_row() { return _cols_per_row; };
    void redraw(const tbx::RedrawEvent &event) override;
    bool recalc_layout(const tbx::BBox &visible_area) override;
};

//class CLDirectoryItemIconData {
//private:
//    tbx::WimpSprite *_sprite_image = nullptr;
//    std::string _name;
//    std::string _display_name;
//    std::string _mtime_str;
//    std::string _file_type_str;
//    std::string _file_size_str;
//    std::string _attrs_str;
//    int _file_type;
//    size_t _file_size;
//    bool _directory_like;
//public:
//    CLDirectoryItemIconData(tbx::PathInfo& info);
//
//    ~CLDirectoryItemIconData();
//
//    void name(const std::string& newname);
//
//    std::string sprite_name() { return _sprite_image->name(); }
//    const std::string &name() const { return _name; }
//    const std::string &display_name() const { return _display_name; }
//    const std::string &file_type_str() const { return _file_type_str; }
//    const std::string &file_size_str() const { return _file_size_str; }
//    const std::string &attrs_str() const { return _attrs_str; }
//    const std::string &mtime_str() const { return _mtime_str; }
//    tbx::WimpSprite* sprite() { return _sprite_image; }
//    int file_type() { return _file_type; }
//    size_t file_size() { return _file_size; }
//    bool is_image_or_directory() { return _directory_like; }
//};

class CLDirectoryWindow :
        public tbx::HasBeenHiddenListener,
        public tbx::MouseClickListener,
        public tbx::GainCaretListener,
        public tbx::LoseCaretListener,
        public tbx::Loader,
        public tbx::DragHandler,
        public tbx::OpenWindowListener,
        public tbx::AboutToBeShownListener,
        public tbx::KeyListener,
        public tbx::Timer,
        public tbx::view::ItemViewClickListener,
        public tbx::ext::TreeViewNodeSelectedListener,
        public tbx::ext::TreeViewNodeExpandedListener,
        public tbx::ext::TreeViewNodeDraggedListener,
        public tbx::view::SelectionListener,
        public CLSearchModelListener,
        public AppDataModelChangedListener,
        public AppDataModelRefreshListener
{
private:
    friend CLTileView;
    friend CLReportView;
    friend CLListWimpFontRenderer;
    friend SmallSpriteAndNameRenderer;
    friend BigSpriteAndNameRenderer;
    friend ThumbnailRenderer;
    friend CLItemSizeBarRenderer;
    friend CLRenameInplace;
    friend CLRenameWin;
    friend CLCopyAsWin;
    friend CLBlocksView;
    friend CLFavoriteMenu;
    friend CLCreateDirWin;
    friend CLSaveCachedThumbnailsSizesCmd;
    friend CLAccessWin;
    friend CLAccessMenu;
    friend CLDirectorySelectionMenu;

    class IdleCurrentDirRefresher : public tbx::Command, public tbx::Timer {
        CLDirectoryWindow *_me;
        bool refresh_pending = false;
        bool fast_refresh = false;
    public:
        IdleCurrentDirRefresher(CLDirectoryWindow *view) : _me(view) {};

        void refresh_on_idle(int delay, bool fast_refresh_) {
            if (!refresh_pending) {
                refresh_pending = true;
                fast_refresh = fast_refresh_;
                if (delay) {
                    tbx::Application::instance()->add_timer(delay, this);
                } else {
                    tbx::Application::instance()->add_idle_command(this);
                }
            }
        }
        inline bool is_refresh_pending() { return refresh_pending; }
        void execute() override {
            refresh_pending = false;
            tbx::Application::instance()->remove_idle_command(this);
            if (fast_refresh) {
                _me->fast_refresh_current_view();
            } else {
                _me->refresh_current_view(true, true);
            }
        }

        void timer(unsigned int elapsed) override {
            refresh_pending = false;
            tbx::Application::instance()->remove_timer(this);
            if (fast_refresh) {
                _me->fast_refresh_current_view();
            } else {
                _me->refresh_current_view(true, true);
            }
        }
    } _idle_view_refresher;

    class DirectoryItemSizeText : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemSizeText(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const {
            return _me->_current_dir_items[index]->dir_or_file_size_str();
        }
    } _item_size_text;

    class DirectoryItemSizeUnitText : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemSizeUnitText(CLDirectoryWindow *view) : _me(view) {};

       virtual std::string value(unsigned int index) const {
           return _me->_current_dir_items[index]->dir_or_file_size_unit();
       }
    } _item_size_unit_text;

    class DirectoryItemDirOrFileSizeText : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemDirOrFileSizeText(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const {
            return _me->_current_dir_items[index]->dir_or_file_size_str();
        }
    } _item_dir_or_file_size_text;

    class DirectoryItemDirOrFileSizeUnitText : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemDirOrFileSizeUnitText(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const {
            return _me->_current_dir_items[index]->dir_or_file_size_unit();
        }
    } _item_dir_or_file_size_unit_text;

//    class DirectoryItemDirOrFileSize : public tbx::view::ItemViewValue<int64_t> {
//        CLDirectoryWindow *_me;
//    public:
//        DirectoryItemDirOrFileSize(CLDirectoryWindow *view) : _me(view) {};
//
//        virtual int64_t value(unsigned int index) const { return _me->_current_dir_items[index]->dir_or_file_size(); }
//    } _item_dir_or_file_size;

    class DirectoryItemFileType : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemFileType(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const { return _me->_current_dir_items[index]->file_type_str(); }
    } _item_file_type_text;

    class DirectoryItemMtime : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemMtime(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const { return _me->_current_dir_items[index]->mtime_short_str(); }
    } _item_mtime_text;

    class DirectoryItemAttrs : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemAttrs(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const { return _me->_current_dir_items[index]->attrs_str(); }
    } _item_attrs_text;

    class DirectoryItemPath : public tbx::view::ItemViewValue<std::string> {
        CLDirectoryWindow *_me;
    public:
        DirectoryItemPath(CLDirectoryWindow *view) : _me(view) {};

        virtual std::string value(unsigned int index) const {
//            Log_debug("DirectoryItemPath %s", _me->_current_dir_items[index]->path().name().c_str());
            return _me->_current_dir_items[index]->path().name();
        }
    } _item_path_text;

    class RootSelectorListener : public tbx::TextChangedListener, public tbx::StringSetAboutToBeShownListener {
        CLDirectoryWindow *_me;
    public:
        RootSelectorListener(CLDirectoryWindow *view) : _me(view) {};

        void text_changed(tbx::TextChangedEvent &event) override {
            tbx::StringSet roots_stringset = tbx::StringSet(event.id_block().self_component());
            auto root_str = roots_stringset.selected();
            if (_me->change_root_dir(root_str)) {
                CLDirectoryItemData* root_item = g_app_data_model.get_root_item(root_str);
                _me->change_current_directory(root_item, true, true);
            } else {
                roots_stringset.selected(_me->_current_root);
            }
        }

        void stringset_about_to_be_shown(const tbx::EventInfo &event) override {
            _me->set_status_line("Detecting drives...", true);
            g_my_app->yield();
            g_app_data_model.refresh_roots(true);
            _me->set_status_line("", true);
        }

    } _toolbar_root_selector_listener;

    friend RootSelectorListener;

    class IdleTopToolbarScroller :
            public tbx::PointerLeavingListener,
            public tbx::PointerEnteringListener,
            public tbx::Timer {
        CLDirectoryWindow *_me;
        bool _scrolling = false;
        int _scroll_x = 0;
    public:
        IdleTopToolbarScroller(CLDirectoryWindow *view) : _me(view) {};

        void timer(unsigned int elapsed) override;
        void pointer_leaving(const tbx::EventInfo &ev) override;
        void pointer_entering(const tbx::EventInfo &ev) override;
        void reset_scroll();
    } _idle_top_toolbar_scroller;

    class ToolbarButtonNavClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        ToolbarButtonNavClickListener(CLDirectoryWindow *view) : _me(view) {};

        void mouse_click(tbx::MouseClickEvent &event);
    } _toolbar_button_nav_listener;

    class ToolbarButtonRefreshListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        ToolbarButtonRefreshListener(CLDirectoryWindow *view) : _me(view) {};

        void mouse_click(tbx::MouseClickEvent &event) override {
            if (tbx::Button(_me->_win_toolbar.gadget(0x12)).validation() == "R1;Sstop") {
                g_app_data_model.abort_refresh();
            } else {
                _me->refresh_roots();
            }
        }
    } _toolbar_button_refresh_listener;

    class SelectColorClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        SelectColorClickListener(CLDirectoryWindow *view) : _me(view) {};

        void mouse_click(tbx::MouseClickEvent &event) override {
            tbx::ColourMenu("MDirColor").show_as_menu();
        }
    } _toolbar_button_select_color_listener;

    class SearchPanelToggleListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        SearchPanelToggleListener(CLDirectoryWindow *view) : _me(view) {};

        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_search_toggle_listener;

    class TreeviewToggleListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        TreeviewToggleListener(CLDirectoryWindow *view) : _me(view) {};

        void mouse_click(tbx::MouseClickEvent &event) override {
            _me->toggle_toolbars();
        }
    } _toolbar_treeview_toggle_listener;

    class FavoriteClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        FavoriteClickListener(CLDirectoryWindow *view) : _me(view) {};
        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_button_favorite_listener;

    class RenameInplaceKeyListener : public tbx::KeyListener {
        CLDirectoryWindow *_me;
    public:
        RenameInplaceKeyListener(CLDirectoryWindow *view) : _me(view) {};

        void key(tbx::KeyEvent &event) override {
            Log_debug("RenameInplaceKeyListener key:%d", event.key());
            switch(event.key()) {
                case wimp_KEY_ESCAPE:
                    _me->stop_rename_inplace();
                    event.key_used();
                    break;
                case wimp_KEY_RETURN:
                    if (_me->is_index_valid(_me->_renaming_item_idx)) {
                        CLDirectoryItemData* item = _me->_current_dir_items[_me->_renaming_item_idx];
                        item->fs_rename(tbx::WritableField(_me->_win_inplace.gadget(0)).text());
                        _me->stop_rename_inplace();
                        _me->refresh_current_view(true, true);
                    }
                    event.key_used();
                    break;
            }
        }
    } _rename_inplace_key_listener;

    class SearchTextOrOptionChangedListener : public tbx::TextChangedListener,
            public tbx::OptionButtonStateListener, public tbx::ButtonSelectedListener
    {
    public:
        std::string search_text;
        CLDirectoryWindow *_me;

        SearchTextOrOptionChangedListener(CLDirectoryWindow *view) : _me(view), search_text("") {};

        void text_changed(tbx::TextChangedEvent &event) override {
            if (search_text != event.text()) {
                search_text = event.text();
                _me->start_stop_search();
            }
        }

        void option_button_state_changed(tbx::OptionButtonStateEvent &event) override {
            _me->start_stop_search();
        }

        void button_selected(tbx::ButtonSelectedEvent &event) override {
            _me->toggle_pause_search();
        }
    } _toolbar_search_text_or_option_changed_listener;

    class OpenWindowRequestListener : public tbx::Command {
        CLDirectoryWindow *_me;
    public:
        OpenWindowRequestListener(CLDirectoryWindow *view) : _me(view) {};

        void execute() override {
//            tbx::BBox winb = _me->_win_main.bounds();
//            Log_debug(("OpenWindowRequestListener %d %d prev %d %d",
//                          winb.size().width, winb.size().height,
//                          _me->_prev_win_size.width, _me->_prev_win_size.height);
//            if (_me->_prev_win_size != _me->_win_main.bounds().size()) {
//            _me->setup_subwindows();

            _me->update_treeview_bounds();
            //_me->_view->refresh();
//            }
            _me->_handling_open_window_request = false;
            tbx::Application::instance()->remove_idle_command(this);
        }
    } _open_window_request_listener;

    class SortOrderChangedListener : public tbx::TextChangedListener,
                                     public tbx::MouseClickListener
    {
        CLDirectoryWindow *_me;
    public:
        SortOrderChangedListener(CLDirectoryWindow *view) : _me(view) {};

        void text_changed(tbx::TextChangedEvent &event) override {
            auto order = tbx::StringSet(event.id_block().self_component()).selected_index();
            order += 1;
            _me->sort_order((order << 4) | (_me->sort_order() & 0xf));
        }
        void mouse_click(tbx::MouseClickEvent &event) override {
            int sort_toggle = (_me->sort_order() & 0xf) ? 0 : 1;
            _me->sort_order((_me->sort_order() & 0xfff0) | sort_toggle);
        }

    } _toolbar_sort_order_changed_listener;

    class TimelineToggleClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        TimelineToggleClickListener(CLDirectoryWindow *view) : _me(view) {};
        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_button_timeline_toggle_listener;

    class TimelineFoldersClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        TimelineFoldersClickListener(CLDirectoryWindow *view) : _me(view) {};
        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_button_timeline_folders_listener;

    class TimelineFilesClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        TimelineFilesClickListener(CLDirectoryWindow *view) : _me(view) {};
        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_button_timeline_files_listener;

    class TimelineAppsClickListener : public tbx::MouseClickListener {
        CLDirectoryWindow *_me;
    public:
        TimelineAppsClickListener(CLDirectoryWindow *view) : _me(view) {};
        void mouse_click(tbx::MouseClickEvent &event) override;
    } _toolbar_button_timeline_apps_listener;

    static CLDirectoryWindow *_latest_instance;

    CLTreeMenu* _tree_menu;

    tbx::Button _toolbar_btn_up;
    tbx::Button _toolbar_btn_select_color;

    CLSearchModel *_search_model;
    bool _search_paused = false;

    std::vector<CLDirectoryItemData*> _current_dir_items;
// using id instead of pointer because pointed item might be deleted already
//    CLDirectoryItemData* _current_dir_item = nullptr;
    int _saved_view_mode = VIEW_MODE_TILE_BIG;
    bool _initialized = false;
    bool _min_layout = false;
    bool _refreshing_at_change_dir = false;

    std::vector<unsigned long> _action_item_ids;

    std::vector<std::string> _backward_dir_items;
    std::vector<std::string> _forward_dir_items;

    CLDirectoryItemData* _current_dir_item = nullptr;
    std::string _current_dir_path;
    std::string _current_root;
//    std::map<unsigned int, tbx::ext::TreeNodeId> _item_to_treeview_node;
//    std::map<tbx::ext::TreeNodeId, CLDirectoryItemData*> _treeview_node_to_item;

    CLRenameInplace* _rename_inplace = nullptr;
    bool _handling_open_window_request = false;
    bool _caret_gained = false;
    bool _handling_click_event = false;
    bool _search_by_prefix_not_found = false;
    bool _is_initial_refresh = true;
//    bool _save_cached_thumbnails_pending = false;
//    bool _treeview_visible = true;
//    bool _top_toolbar_visible = true;
//    bool _bottom_toolbar_visible = true;
    int _toolbars_visible = 3;
//    os_t _last_click_time = 0;
    os_t _last_keypress_time = 0;
//    int _last_click_item_idx = -1;
    std::string _search_by_prefix;
    tbx::Size _prev_win_size;
    tbx::Window _win_main;
    tbx::Window _win_toolbar;
    tbx::Window _win_statusbar;
    tbx::Window _win_treeview;
    tbx::Window _win_inplace;
    tbx::Window _treeview_underlying_window;
    tbx::Colour _bg_color = tbx::Colour::no_colour;
    tbx::WimpColour _fg_color = tbx::WimpColour::black;
    tbx::ext::TreeView _treeview;
    tbx::ext::TreeNodeId _treeview_root_node_id = 0;

    static std::map<std::string, tbx::SpriteArea*> _app_sprites;
//    std::map<std::string, CLThumbSize> _thumb_sizes;
//    bool _thumb_sizes_changed;

    tbx::Margin _margin;
    BigSpriteAndNameRenderer _tile_big_icon_and_name_renderer;
    SmallSpriteAndNameRenderer _tile_small_icon_and_name_renderer;
    SmallSpriteAndNameRenderer _list_icon_and_name_renderer;
    ThumbnailRenderer _tile_thumbnail_big_renderer;
    ThumbnailRenderer _tile_thumbnail_small_renderer;
    ThumbnailRenderer _tile_thumbnail_tiny_renderer;
    ThumbnailRenderer _tile_thumbnail_medium_renderer;
    CLListWimpFontRenderer _item_size_renderer;
    CLListWimpFontRenderer _item_size_unit_renderer;
    CLListWimpFontRenderer _item_dir_or_file_size_renderer;
    CLListWimpFontRenderer _item_dir_or_file_size_unit_renderer;
    CLListWimpFontRenderer _item_mtime_renderer;
    CLListWimpFontRenderer _item_file_type_renderer;
    CLListWimpFontRenderer _item_attrs_renderer;
    CLListWimpFontRenderer _item_path_renderer;
    CLItemSizeBarRenderer _item_size_bar_renderer;

    unsigned int _dragged_item_id = 0;
    tbx::Window _drag_src_win;

    int _menu_clicked_item_idx = -1;
    int _renaming_item_idx = -1;
    int _drop_item_idx = -1;
    int _shift_selected_first_idx = -1;
    tbx::ext::TreeNodeId _drop_tree_current_node_id = 0;
    tbx::ext::TreeNodeId _selected_node_id = 0;

    tbx::view::Selection *_selection;
    tbx::view::ItemView *_view = nullptr;

    std::vector<std::string> _loading_files;
    std::map<std::string, std::string> _saving_files;
    std::string _loading_dropped_file_name;
    std::string _loading_dropped_dest_path;
    std::string _status_message;

    int _view_mode = -1;
    int _operation_mode = OPERATION_MODE_DIR;
    unsigned int _sort_order = SORT_NAME;
    int _timeline_filter = TIMELINE_FILTER_FOLDERS;
    int64_t _max_dir_item_size = -1;

    void select_item_by_idx(int idx);
    void select_item_by_prefix();

    void refresh_roots_widget();
    inline void refresh_current_view_on_idle(int delay = 0, bool fast_refresh = false) { _idle_view_refresher.refresh_on_idle(delay, fast_refresh); }
    inline bool is_idle_refresh_pending() { _idle_view_refresher.is_refresh_pending(); }
//    void clear_directory_items();
//    void treeview_scan_current_directory(const tbx::Path& path);
//    void treeview_add_current_dir_items(bool select_node = false);
//    tbx::ext::TreeNodeId treeview_set_current_node_for_path(const tbx::Path& path, bool select_node = true, bool expand_node = true);
//    tbx::Path get_path_from_current_node();
//    tbx::SpriteArea* make_treeview_sprite(tbx::WimpSprite* wimpsprite);
    void assign_node_sprite(tbx::ext::TreeViewCurrentNode& node, CLDirectoryItemData *item);
    void set_current_dir_items(const std::vector<CLDirectoryItemData*> &items);

public:
    explicit CLDirectoryWindow(tbx::Window main_win);
    ~CLDirectoryWindow() override;
    static CLDirectoryWindow* from_window(tbx::Window win);
    static void open_new(const tbx::Path& path, CLDirectoryWindow* parent= nullptr);
    void open_new_without_toolbars(const tbx::Path& p);

    void init(const tbx::Path& dir, CLDirectoryWindow* parent = nullptr, bool min_size_no_toolbars = false);
//    void set_current_directory(const tbx::Path& path, bool select_treeview_node = true, bool do_view_refresh = true);
    void change_current_directory(unsigned int current_dir_item_id, bool select_treeview_node = true, bool do_view_refresh = true);
    void change_current_directory(CLDirectoryItemData* dir_item, bool select_treeview_node = true, bool do_view_refresh = true, bool with_history = true);
    void change_current_directory(std::string &dpath);
    bool change_root_dir(const std::string &new_root);
    void change_current_dir_without_history(CLDirectoryItemData* item);

    inline unsigned int sort_order() { return _sort_order; };
    void sort_order(unsigned int sort, bool save_and_refresh = true);
    void view_mode(int view_mode, bool save_and_refresh = true);
    inline int view_mode() { return _view_mode; };
    inline int main_view_mode() { return _view_mode & 0xfff0; };
    void go_up_level();
    void go_backward();
    void go_forward();
    void scroll_to_first_selected(int scroll_top_or_bottom);
    void toggle_toolbars(int value = -1);
//    void toggle_top_toolbar();

    void treeview_make_node(CLDirectoryItemData* dir_item, bool create_child);
    void treeview_remove_node(CLDirectoryItemData* dir_item);
    void treeview_delete_empty_child();
    void treeview_update_node_contents(tbx::ext::TreeNodeId node_id);
    void treeview_full_update();
    tbx::ext::TreeNodeId treeview_set_current_node_for_item(CLDirectoryItemData* current_dir_item, bool select_node, bool expand_node);
//    tbx::ext::TreeNodeId get_treeview_node_id_for_item(CLDirectoryItemData* dir_item);
    CLDirectoryItemData* get_item_for_treeview_node_id(tbx::ext::TreeNodeId);

    void refresh_current_view(bool keep_scroll, bool do_view_refresh);
    void fast_refresh_current_view();
//    void refresh_directory(const tbx::Path& dir, bool refresh_tree=true);
    void setup_top_toolbar(int scroll_x = 0);
    void setup_subwindows();
    void update_treeview_bounds();
    void set_focus() { _win_main.focus(); }
    void fs_open_or_run(int idx);
    void fs_copy_move(const tbx::Path &src_dir, const std::vector<std::string> &src_files_vec, const tbx::Path &dst_dir);

    void set_action_item(CLDirectoryItemData* item);
    void set_action_items(const std::vector<CLDirectoryItemData*>& items);
    std::vector<CLDirectoryItemData*> get_action_items();

    void action_cut_items();
    void action_copy_items();
    void action_paste_items();
    void action_delete_items();
    void action_open_help();
    void action_count();
    void action_info();
    void action_stamp();
    void action_set_filetype_by_ext();
    void action_image_view();
    void action_set_dir();
    void action_open_new_window();
    void start_rename_inplace();

    void clear_selected_items();
    void set_status_line(const std::string& status, bool all_windows = false);
    std::string get_status_line() { return _status_message; }
    void select_all();
    void clear_selection();
    void stop_rename_inplace();
    void drag_start(const tbx::MouseClickEvent& event, CLDirectoryItemData* item);
    void drag_stop(bool canceled = false);
    void drag_stop_cleanup();
    void change_operation_mode(int mode);
    inline int operation_mode() { return _operation_mode; };
    void start_stop_search();
    void toggle_pause_search();
    void refresh_roots();
    void refresh_dir(bool force);
    void set_directory();
//    void refresh_directory_sizes(bool force);
    void update_status_line();

//    tbx::Path current_dir() { return _current_dir; }
    const std::vector<CLDirectoryItemData*> & current_dir_items() { return _current_dir_items; }
    inline CLDirectoryItemData* get_item(int idx) { return is_index_valid(idx) ? _current_dir_items[idx] : nullptr; }
    CLDirectoryItemData* current_dir_item() { return _current_dir_item; }
    unsigned int current_dir_item_id() { return _current_dir_item->id(); }
    inline const std::string& current_dir_path() { return  _current_dir_path; }
    inline bool is_index_valid(int idx) { return (idx > -1 && idx < _current_dir_items.size()); }
    inline tbx::view::Selection* selection() { return _selection; }
    std::vector<CLDirectoryItemData*> selected_items();
    std::vector<std::string> selected_item_names();
    CLDirectoryItemData* first_selected_item();
    inline CLDirectoryItemData* menu_clicked_item() { return (is_index_valid(_menu_clicked_item_idx) ? _current_dir_items[_menu_clicked_item_idx] : nullptr); }
    CLDirectoryItemData* tree_menu_clicked_item();
    inline int menu_clicked_item_idx() { return _menu_clicked_item_idx; }
    inline bool is_treeview_visible() { return _toolbars_visible == TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW; }
    inline bool is_top_toolbar_visible() { return _toolbars_visible >= TOOLBARS_VISIBLE_TOP; }
    inline int toolbars_visible() { return _toolbars_visible; }
    int find_item_index(CLDirectoryItemData* item);
    const tbx::ext::TreeView& treeview() { return _treeview; }
//    inline CLDirectoryItemData*  dragged_item() { return is_index_valid(_dragged_item_id) ? _current_dir_items[_dragged_item_id] : nullptr; }

//    bool get_cached_thumbnail_size(CLDirectoryItemData* item, int &w, int &h);
//    void set_cached_thumbnail_size(CLDirectoryItemData* item, int w, int h);
//    bool is_cached_thumbnail_size(CLDirectoryItemData* item);
//    void load_cached_thumbnail_sizes();
//    void save_cached_thumbnails_sizes();
//    void clear_cached_thumbnails_sizes();

    tbx::Colour bg_color() { return _bg_color; }
    void change_bg_color(unsigned int bg_color, bool save_and_refresh = true);
    void load_and_set_current_directory_view_config();

    void treeview_node_selected(const tbx::ext::TreeViewNodeSelectedEvent &event) override;
    void treeview_node_expanded(const tbx::ext::TreeViewNodeExpandedEvent &event) override;
    void treeview_node_dragged(const tbx::ext::TreeViewNodeDraggedEvent &event) override;
    void itemview_clicked(const tbx::view::ItemViewClickEvent &event) override;
    void selection_changed(const tbx::view::SelectionChangedEvent &event) override;
    void open_window(tbx::OpenWindowEvent &event) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void key(tbx::KeyEvent &event) override;
    bool accept_file(tbx::LoadEvent &event) override;
    std::string destination_save_path(tbx::LoadEvent &event) override;
    CLDirectoryItemData* get_drop_dest_item(tbx::LoadEvent &event);
    bool load_file(tbx::LoadEvent &event) override;
    void lose_caret(tbx::CaretEvent &event) override;
    void gain_caret(tbx::CaretEvent &event) override;
    void mouse_click(tbx::MouseClickEvent &event) override;
    void drag_finished(const tbx::BBox &final) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void timer(unsigned int elapsed) override;
    void timer_handler(const tbx::PointerInfo& where);

    void on_dir_item_added(CLDirectoryItemData *item) override ;
    void on_dir_item_removed(CLDirectoryItemData *item) override ;
    void on_dir_item_updated(CLDirectoryItemData *item, int what) override ;
    void on_root_item_added(CLDirectoryItemData *item) override ;
    void on_root_item_removed(CLDirectoryItemData *item) override ;
    void on_search_state_callback(unsigned int state, CLDirectoryItemData* item) override;
    void on_refresh_finished(bool aborted, bool any_item_changed) override;
    void on_refresh_started() override;
    void on_info_changed() override;
    void on_favorites_changed() override;
};


class CLSaverSaveToFileHandler : public tbx::SaverSaveToFileHandler, public tbx::SaverFinishedHandler {
private:
    CLDirectoryWindow *_me;
    tbx::Path _src_file;
public:
    CLSaverSaveToFileHandler(CLDirectoryWindow *me, const tbx::Path& src) : _me(me), _src_file(src) {};
    void saver_save_to_file(tbx::Saver saver, std::string file_name) override;
    void saver_finished(const tbx::SaverFinishedEvent &finished) override;
};

class CLBgColorMenu : public  tbx::ColourMenuSelectionListener, public tbx::AboutToBeShownListener {
public:

    CLBgColorMenu(tbx::Object obj);;

    void colourmenu_selection(tbx::ColourMenu colour_menu, tbx::WimpColour colour) override;

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

class CLBgColorDialog : public tbx::AboutToBeShownListener,
//        public tbx::ColourDboxDialogueCompletedListener,
        public tbx::ColourSelectedListener
{
    CLDirectoryWindow *me;
    tbx::ColourDbox dbox;
public:

    CLBgColorDialog(tbx::Object obj);;

//    void colourdbox_dialogue_completed(tbx::ColourDbox colour_dbox, bool colour_selected) override;

    void colour_selected(const tbx::ColourSelectedEvent &event) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

#endif //CLFILER_CLDIRECTORYWINDOW_H

//
// Created by slenz on 22.12.2022.
//
#include <oslib/osspriteop.h>
#include <kernel.h>
#include <swis.h>
#include <tbx/messagewindow.h>
#include <tbx/numberrange.h>
#include <cloverleaf/CLUtils.h>
#include <tbx/optionbutton.h>
#include <tbx/textarea.h>
#include <tbx/questionwindow.h>
#include <tbx/swixcheck.h>
#include <tbx/iconpainter.h>
#include <tbx/messagewindow.h>
#include "cloverleaf/tinct.h"
#include "cloverleaf/IdleTask.h"
#include "cloverleaf/CLImageMagickLoader.h"
#include "CLImageViewWindow.h"
#include "FSRemoveUI.h"
#include "cloverleaf/CLImageMagickLoader.h"

#undef parent_component

static void save_completed(tbx::SaveAs &saveas, bool result, const std::string& file_name) {
    saveas.file_save_completed(result, file_name);
    if (result) {
        CLDirectoryItemData *diritem = g_app_data_model.find_item_by_path(tbx::Path(file_name).parent().name());
        if (diritem) {
            g_app_data_model.refresh_on_idle(diritem, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
        }
    }
}

class FSRemoveImageCommand : public tbx::Command {
private:
    CLImageViewWindow *_imgview;
    tbx::Path _image_path;

public:
    FSRemoveImageCommand(CLImageViewWindow *imgview) : _imgview(imgview)
    {
        _image_path = imgview->get_image_path();
    }

    void execute() override {
        _imgview->delete_image(_image_path);
    }
};

CLImageViewWindow::CLImageViewWindow(CLDirectoryItemData* item, const std::vector<CLDirectoryItemData*> & items, CLDirectoryWindow *dir_win) :
    _dir_win(dir_win),
    _win("WImageView")
{
    int i = 0, image_index = 0;
    for(auto it : items) {
        if (CLImageFactory::can_load(it->file_type())) {
            _image_files.push_back(it->path());
            if (it->id() == item->id()) {
                image_index = i;
            }
            i++;
        }
    }
    Log_debug("CLImageViewWindow::CLImageViewWindow images: %d idx:%d", _image_files.size(), image_index);
    _win.add_has_been_hidden_listener(this);
    _win.add_mouse_click_listener(this);
    _win.add_key_listener(this);
    _win.add_redraw_listener(this);
    _win.add_open_window_listener(this);
    _win.client_handle(this);
    _win.add_scroll_request_listener(this);
    show_image(image_index);
}

CLImageViewWindow::~CLImageViewWindow() {
    Log_debug("CLImageViewWindow::~CLImageViewWindow",1);
    delete _image;
    delete _sprite;
}

void CLImageViewWindow::show_image(int image_index) {
    if (image_index >= _image_files.size() || image_index < 0) {
        Logger::error("Image [%s] is corrupt or in the wrong format", _image_files[image_index].name().c_str());
        tbx::show_message("Image index is wrong");
        return;
    }

    _image_index = image_index;
    delete _image;

    tbx::Hourglass hg;
    hg.on();
    _image = CLImageFactory::load_image(_image_files[image_index]);
    hg.off();
    if (!_image) {
        Logger::error("Image [%s] is corrupt or in the wrong format", _image_files[image_index].name().c_str());
        tbx::show_message("Image is corrupt or in the wrong format");
        return;
    }

    int xwind_limit, ywind_limit, scr_width, scr_height, zoom_level;
    xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XWIND_LIMIT,
                           &xwind_limit, 0);
    xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YWIND_LIMIT,
                           &ywind_limit, 0);
    scr_width = (xwind_limit + 1) << CLBaseImage::get_vdu_info().xeig;
    scr_height = (ywind_limit + 1) << CLBaseImage::get_vdu_info().yeig;
    auto max_win_size = tbx::Size(scr_width - 32,  scr_height - 64);
    zoom_level =  _image->get_percent_scale_to_fit_in_box(max_win_size);
    tbx::BBox ext = tbx::BBox(0, -( _image->height() * zoom_level / 100), ( _image->width() * zoom_level / 100), 0);
    Log_debug("CLImageViewWindow::CLImageViewWindow maxw w:%d h:%d ext w:%d h:%d zoom:%d", max_win_size.width, max_win_size.height,
                  ext.width(), ext.height(), zoom_level);

    if (ext.width() < 200 && ext.height() < 200) {
        ext.min.y = -200;
        ext.max.x = 200;
    }
    _win.extent(ext);
    _win.size(ext.size());
    _win.show_centered();
    set_zoom(zoom_level);
    Log_debug("CLImageViewWindow::show_image idx=%d %s", image_index, _image_files[image_index].name().c_str());
}

void CLImageViewWindow::set_zoom(int zoom) {
    if (!_image) {
        return;
    }
    _zoom_level = std::min(std::max(zoom, 5), 500);
    tbx::Path image_path = _image_files[_image_index];
    std::string win_title = CLImageFactory::get_image_type_str(image_path.file_type())
            + ": " + image_path.name()+ " " + to_string(_zoom_level) +"%";
    _win.title(win_title);
    tbx::Size ws = _win.size();
    tbx::BBox ext(0,
                  -std::max( _image->height() * _zoom_level / 100, ws.height),
                  std::max( _image->width() *_zoom_level / 100, ws.width),
                  0);
    _win.extent(ext);
    _win.scroll(0,0);
    refresh_sprite();
    _win.force_redraw(ext);
    _win.focus();
}

void CLImageViewWindow::recalc_layout(const tbx::BBox &visible_area) {
    if (!_image) {
        return;
    }
    tbx::BBox prev_ext = _win.extent();
    tbx::BBox ext(0,
      -std::max( _image->height() * _zoom_level / 100, visible_area.height()),
       std::max( _image->width() *_zoom_level / 100, visible_area.width()),
        0);
//    Log_debug("CLImageViewWindow::recalc vw=%d vh=%d extw=%d exth=%d prevw=%d prevh=%d", visible_area.width(), visible_area.height(), ext.width(), ext.height(), prev_ext.width(), prev_ext.height());
    if (abs(prev_ext.height()  - ext.height()) > 2 || abs(prev_ext.width()  - ext.width()) > 2) {
        _win.extent(ext);
        _win.force_redraw(ext);
    }
}

void CLImageViewWindow::has_been_hidden(const tbx::EventInfo &event_info) {
    Log_debug("CLImageViewWindow::has_been_hidden", 1);
    _win.remove_all_listeners();
    _win.delete_object();
    if (g_app_state.is_dir_window_exists(_dir_win)) {
        _dir_win->set_focus();
    }
    delete this;
}

CLUserSpriteImage* CLImageViewWindow::refresh_sprite() {
    int width_px = _image->width_px() * _zoom_level / 100;
    int height_px = _image->height_px() * _zoom_level / 100;
    g_hourglass_on();
    _sprite = CLImageFactory::resize_image(width_px, height_px, _image);
    g_hourglass_off();
    return _sprite;
}


void CLImageViewWindow::redraw(const tbx::RedrawEvent &e) {
    tbx::UserSprite spr = tbx::Application::instance()->sprite_area()->get_sprite("checkb");
    CLGraphics g(e.visible_area());
    tbx::BBox win_ext = _win.extent();
//    tbx::IconPainter ip;
//    tbx::SpriteArea ar((int *)_sprite_image->get_area_pointer(), false);
//    ip.bounds() = win_ext;
//    ip.sprite_area(&ar);
//    ip.sprite("bitmap");
//    ip.redraw(e);
//    if (_sprite_image) {
//        g.draw_image(*_sprite_image, 0, -win_ext.height());
//    }
//    Log_debug("sprw=%d sprh=%d winext w=%d h=%d", sprw,sprh, win_ext.width(), win_ext.height());
//        Log_debug("CLImageViewWindow::redraw %p w=%d h=%d", _sprite_image->get_area_pointer(), win_ext.width(), win_ext.height());

//    _swix(Tinct_Plot, _INR(2, 4) | _IN(7),
//                  ((unsigned char *) spr.pointer()),
//                  0, 0,
//                  tinct_FILL_HORIZONTALLY | tinct_FILL_VERTICALLY);

    if (!_image) {
//        g.foreground(0);
//        g.tbx::Graphics::fill_rectangle(win_ext);
        return;
    }

    g.foreground(tbx::Colour::white);
    g.tbx::Graphics::fill_rectangle(win_ext);
    if (_sprite) {
        int x = (win_ext.width() - _sprite->width()) / 2;
        int y = -win_ext.height() + ((win_ext.height() - _sprite->height()) / 2);
        g.draw_image(*_sprite, x, y, tbx::Colour::white);
    } else {
        g.draw_image_scaled_centered(*_image, _zoom_level, win_ext);
    }
//    _swix(Tinct_Plot, _INR(2, 4) | _IN(7),
//          ((unsigned char *) spr.pointer()),
//          0, 0,
//          tinct_FILL_HORIZONTALLY | tinct_FILL_VERTICALLY);
//    g.draw_image_scaled_centered(*_image, _zoom_level, win_ext);
}

void CLImageViewWindow::increase_zoom() {
    if (_zoom_level >= 100) {
        set_zoom(((_zoom_level / 50) + 1) * 50);
    } else if (_zoom_level >= 50) {
        set_zoom(((_zoom_level / 25) + 1) * 25);
    } else {
        set_zoom(((_zoom_level / 10) + 1) * 10);
    }
}

void CLImageViewWindow::decrease_zoom() {
    if (_zoom_level > 100) {
        set_zoom(((_zoom_level / 50) - 1) * 50);
    } else if (_zoom_level > 50) {
        set_zoom(((_zoom_level / 25) - 1) * 25);
    } else {
        set_zoom(((_zoom_level / 10) - 1) * 10);
    }
}

void CLImageViewWindow::key(tbx::KeyEvent &event) {
    Log_debug("CLImageViewWindow::key key:%d", event.key());
    switch (event.key()) {
        case '*':
            set_zoom(100);
            event.key_used();
            break;
        case '+':
            increase_zoom();
            event.key_used();
            break;
        case '-':
            decrease_zoom();
            event.key_used();
            break;
        case wimp_KEY_LEFT:
            show_prev();
            event.key_used();
            break;
        case wimp_KEY_RIGHT:
            show_next();
            event.key_used();
            break;
        case wimp_KEY_ESCAPE:
            _win.hide();
            event.key_used();
            break;
        case wimp_KEY_DELETE:
            if (g_app_config.ClipboardKeys == 2) {
                delete_image_confirm();
                event.key_used();
            }
            break;
        case 0xb: // Ctrl-K
            if (g_app_config.ClipboardKeys == 1) {
                delete_image_confirm();
                event.key_used();
            }
            break;
    }
}

bool _is_dragged = false;
void CLImageViewWindow::mouse_click(tbx::MouseClickEvent &event) {
    _win.focus();
    if (event.is_select_drag()) {
        if (_is_dragged) {
            drag_stop();
        }
        g_my_app->add_timer(5, this);
        _is_dragged = true;
        tbx::PointerInfo where(true,false);
        _prev_mouse_x = where.mouse_x();
        _prev_mouse_y = where.mouse_y();
        _win.drag_point(this);
        Log_debug("CLImageViewWindow::mouse_click drag_start", 1);
    }
}

void CLImageViewWindow::timer(unsigned int elapsed) {
    if (_is_dragged) {
        tbx::PointerInfo where(true,false);
        auto sc = _win.scroll();
        sc.x += _prev_mouse_x - where.mouse_x();
        sc.y += _prev_mouse_y - where.mouse_y();
        _win.scroll(sc);
        _prev_mouse_x = where.mouse_x();
        _prev_mouse_y = where.mouse_y();
    }
}

void CLImageViewWindow::drag_stop() {
    if (_is_dragged) {
        Log_debug("CLImageViewWindow::mouse_click drag_stop", 1);
        g_my_app->remove_timer(this);
        _is_dragged = false;
    }
}

void CLImageViewWindow::drag_finished(const tbx::BBox &final) {
    drag_stop();
}

void CLImageViewWindow::drag_cancelled() {
    drag_stop();
}

void CLImageViewWindow::open_window(tbx::OpenWindowEvent &event) {
//    Log_debug("CLImageViewWindow::open_window", 1);
    recalc_layout(event.visible_area());
}

static int _last_time_scroll_event = 0;
void CLImageViewWindow::scroll_request(const tbx::ScrollRequestEvent &event) {

    if ((os_read_monotonic_time() - _last_time_scroll_event) > 25) {
        // read control key state
        _kernel_swi_regs regs;
        regs.r[0] = 121;
        regs.r[1] = 0x1 ^ 0x80;
        tbx::swix_check(_kernel_swi(0x6, &regs, &regs)); // 0x6 is OS_Byte
        bool ctrl_pressed = regs.r[1] == 0xff;
        if (ctrl_pressed) {
            Log_debug("CLImageViewWindow::scroll_request x:%d y:%d", event.x_scroll(), event.y_scroll());
            switch (event.y_scroll()) {
                case tbx::ScrollRequestEvent::DOWN:
                case tbx::ScrollRequestEvent::PAGE_DOWN:
                    increase_zoom();
                    break;
                case tbx::ScrollRequestEvent::UP:
                case tbx::ScrollRequestEvent::PAGE_UP:
                    decrease_zoom();
                    break;
            }
            _last_time_scroll_event = os_read_monotonic_time();
        }
    }
}

void CLImageViewWindow::show_next() {
    if (_image_index < _image_files.size() - 1) {
        show_image(_image_index + 1);
    } else {
        show_image(0);
    }
}

void CLImageViewWindow::show_prev() {
    if (_image_index > 0) {
        show_image(_image_index - 1);
    } else {
        show_image(_image_files.size() - 1);
    }
}

void CLImageViewWindow::delete_image_confirm() {
    if (!_image_files.empty()) {
        const tbx::Path img_item = get_image_path();
        auto q = g_my_app->messages().message("DeleteFileConfirm", img_item.name());
        tbx::show_question(q,"Remove?", new FSRemoveImageCommand(this), new SetFocusBackCommand(_win.window_handle()), true);
    }
}

void CLImageViewWindow::delete_image(tbx::Path& image_path) {
    image_path.remove();
    CLDirectoryItemData *dir_item = g_app_data_model.find_item_by_path(image_path.parent().name());
    if (dir_item) {
        g_app_data_model.refresh_on_idle(dir_item, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
    }
    Log_debug("CLImageViewWindow::delete_image %s", image_path.name().c_str());
    if (!_image_files.empty()) {
        for(auto it = _image_files.begin(); it != _image_files.end(); it++) {
            if (it->name() == image_path.name()) {
                Log_debug("CLImageViewWindow::delete_image %s erased cnt:%d", it->name().c_str(), _image_files.size());
                _image_files.erase(it);
                break;
            }
        }
        Log_debug("CLImageViewWindow::delete_image cnt:%d", _image_files.size());
        if (_image_files.empty()) {
            _win.hide();
        } else {
            show_next();
            _win.focus();
            Log_debug("CLImageViewWindow::delete_image next", _image_files.size());
        }
    }
}

CLImageViewWindow *CLImageViewWindow::from_window(tbx::Window win) {
    return reinterpret_cast<CLImageViewWindow *>(win.client_handle());
}



CLSaveAsImgMenu::CLSaveAsImgMenu(tbx::Object obj) {
    tbx::Menu mnu = tbx::Menu(obj);
    mnu.add_about_to_be_shown_listener(this);
    Log_debug("CLSaveAsImgMenu::CLSaveAsImgMenu", 1);
}

void CLSaveAsImgMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::Menu mnu = event.id_block().self_object();
    tbx::Window win = event.id_block().ancestor_object();
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(win);
    bool can_save = img_view->get_image();
    if (can_save) {
        mnu.item(1).fade(false);
        mnu.item(2).fade(false);
        mnu.item(3).fade(false);
        mnu.item(4).fade(false);
        mnu.item(5).fade(false);
        mnu.item(6).fade(false);
    } else {
        // disable save if no valid image displayed
        mnu.item(1).fade(true);
        mnu.item(2).fade(true);
        mnu.item(3).fade(true);
        mnu.item(4).fade(true);
        mnu.item(5).fade(true);
        mnu.item(6).fade(true);
    }
}

CLMainImageViewMenu::CLMainImageViewMenu(tbx::Object obj) {
    tbx::Menu mnu = tbx::Menu(obj);
    mnu.add_about_to_be_shown_listener(this);
    mnu.add_selection_listener(this);
    Log_debug("CLMainImageViewMenu::CLMainImageViewMenu", 1);
}

void CLMainImageViewMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::Menu mnu = event.id_block().self_object();
    tbx::Window win = event.id_block().ancestor_object();
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(win);
    bool can_info = false;
    CLBaseImage *img = img_view->get_image();
    if (img) {
        switch(img_view->get_image_path().file_type()) {
            case FILE_TYPE_PNG:
            case FILE_TYPE_JPEG:
            case FILE_TYPE_ICO:
            case FILE_TYPE_GIF:
            case FILE_TYPE_SPRITE:
            case FILE_TYPE_WEBP:
            case FILE_TYPE_PSD:
            case FILE_TYPE_TIFF:
            case FILE_TYPE_SVG:
            case FILE_TYPE_PDF:
                can_info = true;
                break;
        }
    }
    mnu.item(0).fade(!can_info);
    if (g_app_config.ClipboardKeys == 1) {
        mnu.item(2).text("Delete...   Ctrl-K");
    } else {
        mnu.item(2).text("Delete...   Del");
    }
}

void CLMainImageViewMenu::menu_selection(const tbx::EventInfo &event) {
    int obj_id = event.id_block().ancestor_object().handle();
    Log_debug("CLMainImageViewMenu::menu_selection %x cmp_id", obj_id);
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(event.id_block().ancestor_object());
    switch(event.id_block().self_component().id()) {
        case 0x2:
            img_view->delete_image_confirm();
            break;
    }
}


class ImageMagickInstalledMessage : public tbx::Command {
public:
    void execute() override {
        tbx::show_message("You need to have Imagemagick installed and activated (loaded) to use save function");
        tbx::Application::instance()->remove_idle_command(this);
    }
} _image_magick_not_installed;

CLSaveAsViaImageMagic::CLSaveAsViaImageMagic(tbx::Object obj) {
    tbx::SaveAs save_as(obj);
    save_as.add_about_to_be_shown_listener(this);
    save_as.add_has_been_hidden_listener(this);
    save_as.set_save_to_file_handler(this);
    tmpl_win = save_as.window();
    Log_debug("CLSaveAsViaImageMagic::CLSaveAsViaImageMagic obj=%x tmpl_win=%s",obj.handle(), tmpl_win.title().c_str());
    tbx::WritableField(tmpl_win.gadget(0x10)).add_key_listener(this);
    tbx::WritableField(tmpl_win.gadget(0x11)).add_key_listener(this);
    try {
        tbx::WritableField(tmpl_win.gadget(0xb)).add_key_listener(this);
        tbx::Button(tmpl_win.gadget(0xc)).add_mouse_click_listener(this);
    } catch (const std::exception&) {
        Log_debug("CLSaveAsViaImageMagic::CLSaveAsViaImageMagic() bg_color component seems not present", 1);
    }
    tmpl_win.add_mouse_click_listener(this);
}

void CLSaveAsViaImageMagic::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    tbx::SaveAs save_as = event.id_block().self_object();
    tbx::Window win = save_as.ancestor_object();
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(win);
    img = img_view->get_image();
    save_as.file_size(10000000); // Estimated size of output
    Log_debug("CLSaveAsViaImageMagic::about_to_be_shown file:%s",src.leaf_name().c_str());

    if (img) {
        src = img_view->get_image_path();
        tbx::WritableField(tmpl_win.gadget(0x10)).text(to_string(img->width_px()));
        tbx::WritableField(tmpl_win.gadget(0x11)).text(to_string(img->height_px()));
        save_as.file_name(src.name());
        Log_debug("CLSaveAsViaImageMagic::about_to_be_shown1 file:%s",src.leaf_name().c_str());
        //save_as.file_type(FILE_TYPE_PNG);
        //Log_debug("CLSaveAsViaImageMagic::about_to_be_shown2 file:%s",src.leaf_name().c_str());
    }
    if (!CLImageMagickLoader::check_imagemagick_installed()) {
        tbx::Application::instance()->add_idle_command(&_image_magick_not_installed);
        return;
    }
}

void CLSaveAsViaImageMagic::has_been_hidden(const tbx::EventInfo &event_info) {
    tbx::Window(event_info.id_block().ancestor_object()).focus();
}

void CLSaveAsViaImageMagic::get_common_options(int* width, int *height, int *quality, std::string& bg_color) {
    std::string w = tbx::WritableField(tmpl_win.gadget(0x10)).text();
    std::string h = tbx::WritableField(tmpl_win.gadget(0x11)).text();
    std::string color_opts;
    std::string resize_opts;
    *width = string_to_int(w);
    *height = string_to_int(h);

    try {
        *quality = tbx::NumberRange(tmpl_win.gadget(0x6)).value();
    } catch (const std::exception&) {
        Log_debug("get_common_options() quality component seems not present", 1);
    }

    try {
        std::string c = tbx::WritableField(tmpl_win.gadget(0xb)).text();
        if (!c.empty() && c != "FFFFFFFF") {
            bg_color.assign(c.substr(4,2)+c.substr(2,2)+c.substr(0,2));
        }
    } catch (const std::exception&) {
        Log_debug("get_common_options() bg_color component seems not present", 1);
    }
}

void CLSaveAsViaImageMagic::key(tbx::KeyEvent &event) {
    std::string fieldstr;
    switch (event.id_block().self_component().id()) {
        case 0x11:
        case 0x10: {
            int val;
            fieldstr = tbx::WritableField(event.id_block().self_component()).text();
            val = string_to_int(fieldstr);
            if (event.id_block().self_component().id() == 0x10) {
                val = val * img->height() / img->width();
                tbx::WritableField(tmpl_win.gadget(0x11)).text(to_string(val));
            } else {
                val = val * img->width() / img->height();
                tbx::WritableField(tmpl_win.gadget(0x10)).text(to_string(val));
            }
            break;
        }
        case 0xb: {
            fieldstr = tbx::WritableField(event.id_block().self_component()).text();
            unsigned int bgrx = hex_string_to_int(fieldstr);
            tbx::Colour col(bgrx);
            char validation[60];
            snprintf(validation, sizeof(validation), "R1;C/%02x%02x%02x", col.blue(), col.green(), col.red());
            Log_debug("%s", validation);
            tbx::Button(tmpl_win.gadget(0xc)).validation(validation);
            break;
        }
    }
}


void CLSaveAsViaImageMagic::mouse_click(tbx::MouseClickEvent &event) {
    if (event.id_block().self_component().id() == 0xc) {
        CLSaveAsBGColorDialog::show(tmpl_win);
    } else {
        tmpl_win.focus();
    }
}


CLSaveAsBGColorDialog* CLSaveAsBGColorDialog::_instance = nullptr;

CLSaveAsBGColorDialog::CLSaveAsBGColorDialog() {
    _dbox = tbx::ColourDbox("DBgColor");
    _dbox.add_colour_selected_listener(this);
    _dbox.add_about_to_be_shown_listener(this);
    Log_debug("CLSaveAsBGColorDialog", 1);
}

void CLSaveAsBGColorDialog::show(tbx::Window tmpl_win) {
    if (!_instance) {
        _instance = new CLSaveAsBGColorDialog();
    }
    _instance->_tmpl_win = tmpl_win;
    _instance->_dbox.show_at_pointer();
}

void CLSaveAsBGColorDialog::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    unsigned int val = hex_string_to_int(tbx::WritableField(_tmpl_win.gadget(0xb)).text());
    tbx::Colour clr = val;
    Log_debug("CLSaveAsBGColorDialog::about_to_be_shown %08x %02x %02x %02x", val, clr.red(), clr.green(), clr.blue());
    _dbox.colour(tbx::Colour(val));
}

void CLSaveAsBGColorDialog::colour_selected(const tbx::ColourSelectedEvent &event) {
//    Log_debug("CLSaveAsBGColorDialog::colour_selected %08x", ((event.red() << 24) | (event.green() << 16) | (event.blue() << 8));
    if (event.none()) {
        tbx::WritableField(_tmpl_win.gadget(0xb)).text("FFFFFFFF");
        tbx::Button(_tmpl_win.gadget(0xc)).validation("R2;Shatch");
        tbx::Button(_tmpl_win.gadget(0xc)).validation("R1;Shatch");
        Log_debug("CLSaveAsBGColorDialog::colour_selected transparent, set 'hatch' icon for color button", 1);
    } else {
        char val[60];
        snprintf(val, sizeof(val), "%02X%02X%02X00", event.blue(), event.green(), event.red());
        tbx::WritableField(_tmpl_win.gadget(0xb)).text(val);
        snprintf(val, sizeof(val), "R1;C/%02x%02x%02x", event.blue(), event.green(), event.red());
        Log_debug("CLSaveAsBGColorDialog::colour_selected %s", val);
        tbx::Button(_tmpl_win.gadget(0xc)).validation(val);
    }
    _tmpl_win.focus();
}


void CLSaveAsImage::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLSaveAsViaImageMagic::about_to_be_shown(event);
    tbx::SaveAs save_as = event.id_block().self_object();
    tbx::Window win = save_as.ancestor_object();
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(win);
    int menu_cmp_id = event.id_block().parent_component().id();
    if (img_view->get_image()) {
        src = img_view->get_image_path();
        save_as.file_name(src.name());
        Log_debug("CLSaveAsImage::about_to_be_shown file:%s menu_cmp_id:%d",src.leaf_name().c_str(), menu_cmp_id);
        switch(menu_cmp_id) {
            case 1: // JPG
                save_as.file_type(FILE_TYPE_JPEG);
                break;
            case 2: // PNG
                save_as.file_type(FILE_TYPE_PNG);
                break;
            case 3: // TIFF
                save_as.file_type(FILE_TYPE_TIFF);
                break;
            case 4: // GIF
                save_as.file_type(FILE_TYPE_GIF);
                break;
            case 5: // WEBP
                save_as.file_type(FILE_TYPE_WEBP);
                break;
            case 6: // Sprite
                save_as.file_type(FILE_TYPE_SPRITE);
                break;
            default:
                save_as.file_type(FILE_TYPE_JPEG);
                break;
        }
    }
}

void CLSaveAsImage::saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) {
    bool result = false;
    int filetype = saveas.file_type(),
        src_filetype = src.file_type(),
        iw, ih, quality;
    char options[200];
    std::string bg_color, color_opts, resize_opts;
    file_name = CLUtils::get_unique_numbered_filename(file_name);
    tbx::Hourglass hg;
    hg.on();
    get_common_options(&iw, &ih, &quality, bg_color);
    Log_debug("CLSaveAsImage::saveas_save_to_file saveto:%s type:%x w:%d h:%d bg_color:%s comp:%d", file_name.c_str(), filetype, iw, ih, bg_color.c_str(), quality);
    if (!bg_color.empty()) {
        color_opts = "-background '#"+bg_color+"' -alpha remove -alpha off";
    }
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype) || iw != img->width_px() && ih != img->height_px()) {
        resize_opts = "-resize "+to_string(iw)+"x"+to_string(ih);
    }
    snprintf(options, sizeof(options)-1, "%s %s", resize_opts.c_str(), color_opts.c_str());
    switch(src.file_type()) {
        case FILE_TYPE_SPRITE: {
            CLUserSpriteImage img;
            img.load(src);
            if (img.is_valid()) {
                result = CLImageFactory::save_image(&img, file_name, filetype, options);
            }
            break;
        }
        case FILE_TYPE_DRAW: {
            CLDrawImage img;
            img.load(src);
            if (img.is_valid()) {
                result = CLImageFactory::save_image(&img, file_name, filetype, options);
            }
            break;
        }
        default:
            if (CLImageMagickLoader::can_direct_convert_from(src_filetype)) {
                result = CLImageMagickLoader::convert(src, file_name, filetype, options);
            } else {
                if (iw > img->width_px() && ih > img->height_px()) {
                    result = CLImageFactory::save_image(img, file_name, filetype, options, iw, ih);
                } else {
                    result = CLImageFactory::save_image(img, file_name, filetype, options);
                }
            }
            break;
    }
    save_completed(saveas, result, file_name);
    if (result) {
        CLDirectoryItemData *diritem = g_app_data_model.find_item_by_path(tbx::Path(file_name).parent().name());
        if (diritem) {
            g_app_data_model.refresh_on_idle(diritem, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
        }
    }
    hg.off();
    Log_debug("CLSaveAsImage::saveas_save_to_file saveto:%s result:%d", file_name.c_str(), result);
}


void CLSaveAsPNG::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLSaveAsViaImageMagic::about_to_be_shown(event);
    tbx::SaveAs save_as = event.id_block().self_object();
    save_as.file_type(FILE_TYPE_PNG);
    Log_debug("CLSaveAsPNG::about_to_be_shown2 file:%s",src.leaf_name().c_str());
}

void CLSaveAsPNG::saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) {
    bool result = false;
    file_name = CLUtils::get_unique_numbered_filename(str_replace_all(file_name," ","\xA0"));
    int filetype = saveas.file_type(),
        src_filetype = src.file_type(),
        iw, ih, quality;
    std::string bg_color, color_opts, resize_opts;
    char options[200];
    tbx::Hourglass hg;
    hg.on();
    get_common_options(&iw, &ih, &quality, bg_color);
    Log_debug("CLSaveAsPNG::saveas_save_to_file saveto:%s type:%x w:%d h:%d bg_color:%s comp:%d", file_name.c_str(), filetype, iw, ih, bg_color.c_str(), quality);
    if (!bg_color.empty()) {
        color_opts = "-background '#"+bg_color+"' -alpha remove -alpha off";
    }
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype) || iw != img->width_px() && ih != img->height_px()) {
        resize_opts = "-resize "+to_string(iw)+"x"+to_string(ih);
    }

    snprintf(options, sizeof(options)-1, "%s -quality %d5 %s", resize_opts.c_str(), quality, color_opts.c_str());
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype)) {
        result = CLImageMagickLoader::convert(src, file_name, filetype, options);
    } else {
        if (iw > img->width_px() && ih > img->height_px()) {
            result = CLImageFactory::save_image(img, file_name, filetype, options, iw, ih);
        } else {
            result = CLImageFactory::save_image(img, file_name, filetype, options);
        }
    }
    saveas.file_save_completed(result, file_name);
    if (result) {
        CLDirectoryItemData *diritem = g_app_data_model.find_item_by_path(tbx::Path(file_name).parent().name());
        if (diritem) {
            g_app_data_model.refresh_on_idle(diritem, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
        }
    }
    hg.off();
    Log_debug("CLSaveAsPNG::saveas_save_to_file saveto:%s result:%d", file_name.c_str(), result);
}


void CLSaveAsJPEG::saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) {
    bool result = false;
    file_name = CLUtils::get_unique_numbered_filename(str_replace_all(file_name," ","\xA0"));
    int filetype = saveas.file_type(),
        src_filetype = src.file_type(),
        iw, ih, quality;
    std::string bg_color, color_opts, resize_opts;
    char options[200];
    tbx::Hourglass hg;
    hg.on();
    get_common_options(&iw, &ih, &quality, bg_color);
    Log_debug("CLSaveAsJPEG::saveas_save_to_file saveto:%s type:%x w:%d h:%d bg_color:%s comp:%d", file_name.c_str(), filetype, iw, ih, bg_color.c_str(), quality);
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype) || iw != img->width_px() && ih != img->height_px()) {
        resize_opts = "-resize "+to_string(iw)+"x"+to_string(ih);
    }
    if (!bg_color.empty()) {
        color_opts = "-background '#"+bg_color+"' -alpha remove -alpha off";
    }

    snprintf(options, sizeof(options)-1, "%s %s -quality %d", resize_opts.c_str(), color_opts.c_str(), quality);
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype)) {
        result = CLImageMagickLoader::convert(src, file_name, filetype, options);
    } else {
        if (iw > img->width_px() && ih > img->height_px()) {
            result = CLImageFactory::save_image(img, file_name, filetype, options, iw, ih);
        } else {
            result = CLImageFactory::save_image(img, file_name, filetype, options);
        }
    }

    save_completed(saveas, result, file_name);

    if (result) {
        CLDirectoryItemData *diritem = g_app_data_model.find_item_by_path(tbx::Path(file_name).parent().name());
        if (diritem) {
            g_app_data_model.refresh_on_idle(diritem, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
        }
    }

    hg.off();
    Log_debug("CLSaveAsJPEG::saveas_save_to_file saveto:%s result:%d", file_name.c_str(), result);
}

void CLSaveAsJPEG::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLSaveAsViaImageMagic::about_to_be_shown(event);
    tbx::SaveAs save_as = event.id_block().self_object();
    save_as.file_type(FILE_TYPE_JPEG);
    Log_debug("CLSaveAsJPEG::about_to_be_shown file:%s",src.leaf_name().c_str());
}


void CLSaveAsWEBP::saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) {
    bool result = false, lossless;
    file_name = CLUtils::get_unique_numbered_filename(str_replace_all(file_name," ","\xA0"));
    int filetype = saveas.file_type(),
            src_filetype = src.file_type(),
            iw, ih, quality, speed;
    std::string bg_color, color_opts, resize_opts;
    char options[200];
    tbx::Hourglass hg;
    hg.on();
    get_common_options(&iw, &ih, &quality, bg_color);
    speed = tbx::NumberRange(tmpl_win.gadget(0x14)).value();
    lossless = tbx::OptionButton(tmpl_win.gadget(0x15)).on();

    Log_debug("CLSaveAsWEBP::saveas_save_to_file saveto:%s type:%x w:%d h:%d bg_color:%s comp:%d", file_name.c_str(), filetype, iw, ih, bg_color.c_str(), quality);
    if (CLImageMagickLoader::can_direct_convert_from(src_filetype) || iw != img->width_px() && ih != img->height_px()) {
        resize_opts = "-resize "+to_string(iw)+"x"+to_string(ih);
    }
    if (!bg_color.empty()) {
        color_opts = "-background '#"+bg_color+"' -alpha remove -alpha off";
    }

    snprintf(options, sizeof(options)-1, "-quality %d -define webp:method=%d %s %s %s",
            quality, speed, resize_opts.c_str(), color_opts.c_str(),
             (lossless ? "-define webp:lossless=true" : ""));

    if (CLImageMagickLoader::can_direct_convert_from(src_filetype)) {
        result = CLImageMagickLoader::convert(src, file_name, filetype, options);
    } else {
        if (iw > img->width_px() && ih > img->height_px()) {
            result = CLImageFactory::save_image(img, file_name, filetype, options, iw, ih);
        } else {
            result = CLImageFactory::save_image(img, file_name, filetype, options);
        }
    }
    save_completed(saveas, result, file_name);

    if (result) {
        CLDirectoryItemData *diritem = g_app_data_model.find_item_by_path(tbx::Path(file_name).parent().name());
        if (diritem) {
            g_app_data_model.refresh_on_idle(diritem, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
        }
    }

    hg.off();
    Log_debug("CLSaveAsWEBP::saveas_save_to_file saveto:%s result:%d", file_name.c_str(), result);
}

void CLSaveAsWEBP::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLSaveAsViaImageMagic::about_to_be_shown(event);
    tbx::SaveAs save_as = event.id_block().self_object();
    save_as.file_type(FILE_TYPE_WEBP);
    Log_debug("CLSaveAsWEBP::about_to_be_shown file:%s",src.leaf_name().c_str());
}


CLImageInfoWindow::CLImageInfoWindow(tbx::Object obj) {
    tbx::Window win(obj);
    win.add_about_to_be_shown_listener(this);
}

void CLImageInfoWindow::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    std::string file_info;
    g_hourglass_on();
    tbx::Window win = event.id_block().self_object().ancestor_object();
    tbx::Window my_window = event.id_block().self_object();
    CLImageViewWindow *img_view = CLImageViewWindow::from_window(win);
    if (img_view->get_image()) {
        tbx::Path img_path = img_view->get_image_path();
        file_info = CLImageMagickLoader::info(img_path);
        my_window.title("Info: "+img_path.name());
    } else {
        file_info = "Image is not loaded.";
        my_window.title(file_info);
    }
    Log_debug("File info:%s", file_info.substr(0, 200).c_str());
    tbx::TextArea ta(my_window.gadget(0));
    ta.set_colour(tbx::Colour(0), tbx::Colour(0xEF, 0xED, 0xED));
    ta.text(file_info);
    ta.set_cursor_position(0, 1);

    g_hourglass_off();
}

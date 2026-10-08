//
// Created by slenz on 22.12.2022.
//

#ifndef CLFILER_CLIMAGEVIEWWINDOW_H
#define CLFILER_CLIMAGEVIEWWINDOW_H

#include <tbx/saveas.h>
#include <tbx/keylistener.h>
#include <tbx/menuselectionlistener.h>
#include "CLDirectoryWindow.h"
#include "../model/AppDataModel.h"

class CLImageViewWindow:
        public tbx::HasBeenHiddenListener,
        public tbx::KeyListener,
        public tbx::MouseClickListener,
        public tbx::RedrawListener,
        public tbx::OpenWindowListener,
        public tbx::Timer,
        public tbx::DragHandler,
        public tbx::ScrollRequestListener
{
private:
    tbx::Window _win;
    CLDirectoryWindow* _dir_win = nullptr;
    CLBaseImage *_image = nullptr;
    CLUserSpriteImage* _sprite = nullptr;
    std::vector<tbx::Path> _image_files;
    int _image_index;
    int _zoom_level = 100;
    int _prev_mouse_x = 0;
    int _prev_mouse_y = 0;
public:

    CLImageViewWindow(CLDirectoryItemData* item, const std::vector<CLDirectoryItemData*> & items, CLDirectoryWindow* dir_win);
    ~CLImageViewWindow();

    void set_zoom(int zoom);
    void recalc_layout(const tbx::BBox &visible_area);

    void scroll_request(const tbx::ScrollRequestEvent &event) override;
    void key(tbx::KeyEvent &event) override;
    void open_window(tbx::OpenWindowEvent &event) override;
    void mouse_click(tbx::MouseClickEvent &event) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void redraw(const tbx::RedrawEvent &e) override;
    void drag_finished(const tbx::BBox &final) override;
    void drag_cancelled() override;
    void timer(unsigned int elapsed) override;
    void drag_stop();

    void show_image(int image_index);
    void show_next();
    void show_prev();
    void increase_zoom();
    void decrease_zoom();
    CLUserSpriteImage* refresh_sprite();
    void delete_image_confirm();
    void delete_image(tbx::Path& image_path);
    const std::vector<tbx::Path>& get_image_files() { return _image_files; };
    tbx::Window get_win() { return _win; };
    CLBaseImage* get_image() { return _image; }
    const tbx::Path& get_image_path() { return _image_files[_image_index]; }
    static CLImageViewWindow* from_window(tbx::Window win);
};


class CLImageInfoWindow:
        tbx::AboutToBeShownListener
{
public:
    CLImageInfoWindow(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

class CLMainImageViewMenu:
        tbx::AboutToBeShownListener,
        tbx::MenuSelectionListener
{
public:
    CLMainImageViewMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;

private:
    void menu_selection(const tbx::EventInfo &event) override;
};


class CLSaveAsImgMenu:
        tbx::AboutToBeShownListener
{
public:
    CLSaveAsImgMenu(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};


class CLSaveAsViaImageMagic :
        tbx::SaveAsSaveToFileHandler,
        tbx::AboutToBeShownListener,
        tbx::HasBeenHiddenListener,
        tbx::KeyListener,
        tbx::MouseClickListener
{
protected:
    tbx::Path src;
    CLBaseImage *img;
    tbx::Window tmpl_win;
public:
    CLSaveAsViaImageMagic(tbx::Object obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void key(tbx::KeyEvent &event) override;
    void mouse_click(tbx::MouseClickEvent &event) override;
    void get_common_options(int* width, int *height, int *quality, std::string& bg_color);
};

class CLSaveAsBGColorDialog : tbx::ColourSelectedListener, tbx::AboutToBeShownListener
{
private:
    tbx::ColourDbox _dbox;
    tbx::Window _tmpl_win;
    static CLSaveAsBGColorDialog* _instance;
public:

    CLSaveAsBGColorDialog();

    void colour_selected(const tbx::ColourSelectedEvent &event) override;

private:
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;

public:
    static void show(tbx::Window tmpl_win);
};

class CLSaveAsPNG : public CLSaveAsViaImageMagic
{
public:
    CLSaveAsPNG(tbx::Object obj) : CLSaveAsViaImageMagic(obj) { Log_debug("CLSaveAsPNG",1); };

    void saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

class CLSaveAsJPEG : public CLSaveAsViaImageMagic
{
public:
    CLSaveAsJPEG(tbx::Object obj) : CLSaveAsViaImageMagic(obj) { Log_debug("CLSaveAsJPEG",1); };

    void saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

class CLSaveAsWEBP : public CLSaveAsViaImageMagic
{
public:
    CLSaveAsWEBP(tbx::Object obj) : CLSaveAsViaImageMagic(obj) { Log_debug("CLSaveAsWEBP",1); };

    void saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) override;
    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};

class CLSaveAsImage : public CLSaveAsViaImageMagic
{
public:
    CLSaveAsImage(tbx::Object obj) : CLSaveAsViaImageMagic(obj) { Log_debug("CLSaveAsImage",1); };

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
    void saveas_save_to_file(tbx::SaveAs saveas, bool selection, std::string file_name) override;
};

#endif //CLFILER_CLIMAGEVIEWWINDOW_H

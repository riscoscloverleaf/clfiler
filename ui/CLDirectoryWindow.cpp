//
//
// Created by lenz on 9/2/21.
//
#include <set>
#include <algorithm>
#include <list>
#include <iomanip>
#include <cstdlib>
#include <errno.h>
#include <memory.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/abouttobeshownlistener.h>
#include <tbx/application.h>
#include <tbx/messagewindow.h>
#include <tbx/oserror.h>
#include <tbx/radiobutton.h>
#include <tbx/font.h>
#include <tbx/button.h>
#include <tbx/sprite.h>
#include <tbx/iconpainter.h>
#include <tbx/stringset.h>
#include <tbx/swixcheck.h>
#include <tbx/res/reswindow.h>
#include <tbx/res/reswritablefield.h>
#include <tbx/writablefield.h>
#include <tbx/osgraphics.h>
#include <tbx/questionwindow.h>
#include <tbx/optionbutton.h>
#include <tbx/fileinfo.h>
#include <oslib/osfscontrol.h>
#include <oslib/filecore.h>
#include <oslib/taskmanager.h>
#include <backward/strstream>
#include <cloverleaf/CLUtils.h>
#include "../global.h"
#include "../utils.h"
#include "CLDirectoryWindow.h"
#include "CLDirectoryMenu.h"
#include "CLDirSizeScanWindow.h"
#include "CLAskDonate.h"
#include "CLBlocksView.h"
#include "CopyMoveConfirm.h"
#include "CLImageViewWindow.h"
#include "FSRemoveUI.h"
#include "cloverleaf/Logger.h"
#include "cloverleaf/CLImageCache.h"
#include "cloverleaf/CLImage.h"
#include "cloverleaf/CLUtf8.h"
#include "cloverleaf/IdleTask.h"
#include "../model/AppDataModel.h"
#include "../model/AppSettings.h"

#undef xmin
#undef xmax
#undef ymin
#undef ymax

#define CONTENT_MARGIN_TOP 72
#define CONTENT_MARGIN_LEFT 480
#define CONTENT_MARGIN_BOTTOM 60

#define TOOLBAR_BTN_UP_ID       0x1a
#define TOOLBAR_BTN_BACK_ID     0x1b
#define TOOLBAR_BTN_FORW_ID     0x1c
#define TOOLBAR_BTN_SEARCH_ID   0xf
#define TOOLBAR_BTN_TREEVIEW_ID 0x10
#define TOOLBAR_BTN_REFRESH_ID         0x12
#define TOOLBAR_BTN_FAVORITE_ID         0x19
#define TOOLBAR_BTN_BACK_ID       0x1b
#define TOOLBAR_BTN_FORW_ID       0x1c
#define TOOLBAR_BTN_TIMELINE_TOGGLE       0x22
#define TOOLBAR_BTN_TIMELINE_FOLDERS       0x23
#define TOOLBAR_BTN_TIMELINE_FILES       0x24
#define TOOLBAR_BTN_TIMELINE_APPS       0x25
#define TOOLBAR_STRINGSET_ROOT          0x6
#define TOOLBAR_SEARCH_WRITEABLE_ID     0xb
#define TOOLBAR_SEARCH_OPTION_APPS_ID   0xc
#define TOOLBAR_SEARCH_OPTION_IMAGES_ID 0xd
#define TOOLBAR_SEARCH_OPTION_CASE_ID   0x13
#define TOOLBAR_SEARCH_PAUSE_ID         0x11

#define ITEM_CELL_MARGIN 6
#define THUMBNAIL_CELL_TEXT_LINE_HEIGHT 32
#define THUMBNAIL_CELL_MAX_TEXT_WIDTH 320
#define THUMBNAIL_ITEM_CELL_PADDING 4

#define DESKTOP_FONT_SIZE 30
CLDesktopFontStyle *_name_style = nullptr;

CLDirectoryWindow* CLDirectoryWindow::_latest_instance = nullptr;

static CLDesktopFontStyle *get_name_style() {
    if (!_name_style) {
        _name_style = new CLDesktopFontStyle(rufl_WEIGHT_400, DESKTOP_FONT_SIZE);
    }
    return _name_style;
}

void SmallSpriteAndNameRenderer::render(const ItemRenderer::Info &info)
{
    char validation_string[100];
    tbx::Colour bg_color = _me->_bg_color;
    tbx::BBox content(info.bounds);
    content.inflate(-ITEM_CELL_MARGIN); // Reduce size for border
    // WIMP sprite can be done with one Icon
    tbx::IconPainter ip;
    CLBaseImage *small_spr = small_sprite_image(info.index);
    std::string spr_name;
    if (small_spr && small_spr->get_image_type() == CL_IMAGE_TYPE_WIMP_SPRITE) {
//        Log_debug("use small sprite %s for %s text:%s", small_spr->name().c_str(), _me->_current_dir_items[info.index]->name().c_str(), text(info.index).c_str());
        spr_name = ((CLWimpSpriteImage*)small_spr)->name_str();
    } else {
//        Log_debug("use big sprite %s for %s text:%s", sprite_name(info.index).c_str(),_me->_current_dir_items[info.index]->name().c_str(), text(info.index).c_str());
        spr_name = sprite_name(info.index);
        ip.half_sprite(true);
    }
    ip.sprite(spr_name);
    ip.text(text(info.index));
    ip.text_12_sprite_left();
    ip.inverted(info.selected);
    ip.bounds() = content;
    if (bg_color != tbx::Colour::no_colour) {
        sprintf(validation_string, "S%s;C/%02x%02x%02x", spr_name.c_str(), bg_color.blue(), bg_color.green(), bg_color.red());
//        Log_debug("valid str:%s", validation_string);
        ip.validation(validation_string);
    } else {
        ip.background(1);
    }
    ip.foreground(_me->_fg_color);
    ip.redraw(info.redraw);
//    Log_debug("SmallSpriteAndNameRenderer %d [%s] [%s]", info.index, text(info.index).c_str(), spr_name.c_str());
    if (_me->_view_mode == VIEW_MODE_TILE_SMALL && _me->_drop_item_idx == info.index) {
        tbx::BBox box = info.redraw.visible_area().screen(info.bounds);
        box.inflate(-2);
        tbx::OSGraphics g;
        g.foreground(_me->_fg_color.colour());
        g.rectangle(box.min.x, box.min.y, box.max.x, box.max.y);
//        Log_debug("SmallSpriteAndNameRenderer::render refresh %d %d,%d %d,%d", info.index, box.min.x, box.max.y, box.max.x, box.min.y);
    }
}


/**
 * Called to get the size of an item.
 */
tbx::Size SmallSpriteAndNameRenderer::size(unsigned int index) const {
    std::string t = text(index);
    int text_width = 0;
    if (!t.empty())
    {
        tbx::WimpFont font;
        text_width = font.string_width_os(t);
    }

    return tbx::Size(text_width + 68 + ITEM_CELL_MARGIN*2, 32+ITEM_CELL_MARGIN*2);
}

std::string SmallSpriteAndNameRenderer::text(unsigned int index) const {
    if (_use_display_name) {
        return _me->_current_dir_items[index]->display_name();
    } else {
        return _me->_current_dir_items[index]->name();
    }
}

std::string SmallSpriteAndNameRenderer::sprite_name(unsigned int index) const {
    return _me->_current_dir_items[index]->sprite_name();
}

CLBaseImage* SmallSpriteAndNameRenderer::small_sprite_image(unsigned int index) {
    return _me->_current_dir_items[index]->small_sprite_image();
}

bool SmallSpriteAndNameRenderer::hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const {
    return !(pos.x < 10 || pos.y < 10
             || pos.x > size.width - 40
             || pos.y > size.height - 10);
}

bool BigSpriteAndNameRenderer::hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const {
    return !(pos.x < 20 || pos.y < 20
             || pos.x > size.width - 40
             || pos.y > size.height - 20);
}

void BigSpriteAndNameRenderer::render(const tbx::view::ItemRenderer::Info &info) {
    char validation_string[100];
    tbx::BBox content(info.bounds);
    tbx::Colour bg_color = _me->_bg_color;
    content.inflate(-ITEM_CELL_MARGIN); // Reduce size for border
    int total_height = content.height(),
        text_height = total_height - 64;

    // WIMP sprite can be done with one Icon
    tbx::IconPainter ip;
    // draw icon
    ip.sprite(sprite_name(info.index));
    ip.vcentred(false).hcentred(true);
    ip.inverted(info.selected);
    content.min.y = content.max.y - 64;
    ip.bounds() = content;
    if (bg_color != tbx::Colour::no_colour) {
        sprintf(validation_string, "S%s;C/%02x%02x%02x", sprite_name(info.index).c_str(), bg_color.blue(), bg_color.green(), bg_color.red());
//        Log_debug("BigSpriteAndNameRenderer::render validstr [%s]", validation_string);
//        ip.validation(validation_string);
    } else {
        ip.background(1);
    }
    ip.foreground(_me->_fg_color);
    ip.redraw(info.redraw);

    std::list<std::string> lines;
    CLGraphics g(info.redraw.visible_area());
    g.split_text_to_width(text(info.index), content.width(), *get_name_style(), lines);
    int y = content.max.y - 30 - 64;
    for(auto &line : lines) {
        if (info.selected) {
            if (_me->_fg_color == -1) {
                g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), bg_color, tbx::Colour::white, false, true);
            } else {
                g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), bg_color, _me->_fg_color.colour(), false, true);
            }
        } else {
            g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), _me->_fg_color.colour(), bg_color, false);
        }
        y -= 32;
    }

    if (_me->_drop_item_idx == info.index) {
//        Log_debug("BigSpriteAndNameRenderer::render refresh %d %d,%d %d,%d", info.index, info.bounds.min.x, info.bounds.max.y, info.bounds.max.x, info.bounds.min.y);
        tbx::BBox box = info.redraw.visible_area().screen(info.bounds);
        box.inflate(-2);
        tbx::OSGraphics g;
        g.foreground(_me->_fg_color.colour());
        g.rectangle(box.min.x, box.min.y, box.max.x, box.max.y);
    }
}

/**
 * Called to get the size of an item.
 */
tbx::Size BigSpriteAndNameRenderer::size(unsigned int index) const
{
    std::string t = text(index);
    int text_width = 0, text_height = 0;
    if (!t.empty())
    {
        text_width = std::max(CLGraphics::get_text_width(t, *get_name_style()) + 4, 68);
        text_height = ((text_width / 320) + 1) * 32;
//        Log_debug("text_width=%d text_height=%d txt=%s", text_width, text_height, t.c_str());
    } else {
        text_width = 68;
    }

    return tbx::Size(std::min(text_width, 320) + ITEM_CELL_MARGIN*2, text_height + 70 + ITEM_CELL_MARGIN*2);
}

std::string BigSpriteAndNameRenderer::text(unsigned int index) const {
    return riscos_local_to_utf8(str_replace_all(str_replace_all(_me->_current_dir_items[index]->name(), "\xC2\xA0", " "), "\xA0", " "));
}

std::string BigSpriteAndNameRenderer::sprite_name(unsigned int index) const {
    return _me->_current_dir_items[index]->sprite_name();
}

int ThumbnailRenderer::text_split_and_get_height(CLDirectoryItemData *dir_item, int cell_width, std::list<std::string>& lines) {
    std::string name = riscos_local_to_utf8(str_replace_all(str_replace_all(dir_item->name(), "\xC2\xA0", " "), "\xA0", " "));
    CLGraphics::split_text_to_width(name, cell_width, *get_name_style(), lines);
//    Log_debug("text_split_and_get_height [%s] h=%d lines=%d", name.c_str(), lines.size() * THUMBNAIL_CELL_TEXT_LINE_HEIGHT, lines.size());
    return lines.size() * THUMBNAIL_CELL_TEXT_LINE_HEIGHT;
}

tbx::TranslationTable ThumbnailRenderer::tt;
void ThumbnailRenderer::render(const ItemRenderer::Info &info)
{
    CLDirectoryItemData *dir_item = _me->_current_dir_items[info.index];
    int file_type = dir_item->file_type(), thumb_box_width, thumb_box_height;
    CLBaseImage *tmb_img = g_thumbnailer.get_thumbnail(dir_item);
//    tbx::WimpSprite *spr = nullptr;
    char validation_string[100];
    tbx::Colour bg_color = _me->_bg_color;
    std::list<std::string> lines;
    CLGraphics g(info.redraw.visible_area());
    tbx::BBox content(info.bounds);
    content.inflate(-(ITEM_CELL_MARGIN + THUMBNAIL_ITEM_CELL_PADDING)); // Reduce size for border


    Log_debug("ThumbnailRenderer::render content w=%d h=%d name=%s", content.width(), content.height(), dir_item->name().c_str());
    int txt_height = text_split_and_get_height(dir_item, content.width(), lines);
    int y = content.min.y + txt_height - THUMBNAIL_CELL_TEXT_LINE_HEIGHT;
//    Log_debug("maxy=%d y=%d miny=%d th=%d", content.max.y, y, content.min.y, txt_height);

    for(auto &line : lines) {
        if (info.selected) {
            if (_me->_fg_color == -1) {
                g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), bg_color, tbx::Colour::white, false, true);
            } else {
                g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), bg_color, _me->_fg_color.colour(), false, true);
            }
        } else {
            g.draw_text_centered(content.min.x, y, content.width(), line, *get_name_style(), _me->_fg_color.colour(), bg_color, false);
        }
        y -= THUMBNAIL_CELL_TEXT_LINE_HEIGHT;
    }

//    Log_debug("box maxy=%d miny=%d w=%d h=%d th=%d", fit_in_box.max.y, fit_in_box.min.y, fit_in_box.width(), fit_in_box.height(), txt_height);
    tbx::BBox img_box, fit_in_box = content;
    if (tmb_img) {
        fit_in_box.min.y = content.min.y + THUMBNAIL_CELL_TEXT_LINE_HEIGHT;
        img_box = tmb_img->get_scaled_and_centered_box(fit_in_box);
        if (img_box.min.y < content.min.y + txt_height) {
            fit_in_box.min.y = content.min.y + txt_height;
            img_box = tmb_img->get_scaled_and_centered_box(fit_in_box);
        }
    }
    if (info.selected) {
        g.foreground(_me->_fg_color.colour());
        g.fill_rectangle(img_box.min.x - THUMBNAIL_ITEM_CELL_PADDING - 2,
                         img_box.min.y - THUMBNAIL_ITEM_CELL_PADDING - 2,
                         img_box.max.x + THUMBNAIL_ITEM_CELL_PADDING,
                         img_box.max.y + THUMBNAIL_ITEM_CELL_PADDING);
    }

    if (tmb_img) {
        g.draw_image_scaled(*tmb_img, img_box.min.x, img_box.min.y, img_box.width(), img_box.height(), bg_color);
//    } else {
//        if (spr) {
//            CLSpriteImage img(spr);
//            g.draw_image_fit_box(img, img_box, bg_color, 100);
//        }
    }


    if (_me->_drop_item_idx == info.index) {
        content = info.bounds;
        content.inflate(-2);
        g.foreground(_me->_fg_color.colour());
        g.rectangle(content.min.x, content.min.y, content.max.x, content.max.y);
    }
}

tbx::Size ThumbnailRenderer::size(unsigned int index) const {
    int max_thumb_w = _thumb_width * 2;
    int max_thumb_h = _thumb_height * 2;
    int thumb_w = max_thumb_w, thumb_h = max_thumb_h;
    CLDirectoryItemData *item = _me->_current_dir_items[index];
    std::string name = item->name();
    std::list<std::string> lines;
    int txt_width = 0, txt_height = 0;

    if (((CLTileView *) _me->_view)->is_thumbs_size_calculated()) {
        thumb_w = ((CLTileView *) _me->_view)->get_max_thumb_size().width;
        thumb_h = ((CLTileView *) _me->_view)->get_max_thumb_size().height;
    } else {
        if (!CLImageFactory::can_load(item->file_type())) {
            CLBaseImage* _thumb_img = item->sprite_image();
            thumb_h = _thumb_img->height();
            thumb_w = _thumb_img->width();
        } else {
            if (!g_thumbnailer.get_cached_thumbnail_size(item, thumb_w, thumb_h)) {
                // another special case for Stefan's (stupid?) request. If file is sprite then its dimensions probably small
                if (item->file_type() == FILE_TYPE_SPRITE && item->file_size() < 100000) {
                    CLBaseImage *tmb_img = g_thumbnailer.create_thumbnail(item);
                    if (tmb_img) {
                        thumb_w = tmb_img->width();
                        thumb_h = tmb_img->height();
                    }
                }
            }

//    if (!g_thumbnailer.get_cached_thumbnail_size(item, thumb_w, thumb_h)) {
//        } else {
//            // special case for Stefan's (stupid?) request. Load and measure sprites if that images will be displayed anyway
//            // just to make adaptive size mode works always
//            if ((item->file_type() == FILE_TYPE_SPRITE && item->file_size() < 100000)
//                || (item->file_type() == FILE_TYPE_JPEG && item->file_size() < 10000)
//                || (item->file_type() == FILE_TYPE_PNG && item->file_size() < 10000)) {
//                tbx::Size _win_size = _me->_win_main.bounds().size();
//                _win_size.width -= (_me->_view->margin().left + _me->_view->margin().right);
//                _win_size.height -= (_me->_view->margin().top + _me->_view->margin().bottom);
//                int max_cells_shown = (_win_size.width / max_thumb_w) * ((_win_size.height / max_thumb_h));
//                switch(_me->_view_mode) {
//                    case VIEW_MODE_THUMBNAILS_BIG:
//                        max_cells_shown *= 10;
//                        break;
//                    case VIEW_MODE_THUMBNAILS_MEDIUM:
//                        max_cells_shown *= 6;
//                        break;
//                    case VIEW_MODE_THUMBNAILS_SMALL:
//                        max_cells_shown *= 3;
//                        break;
//                    case VIEW_MODE_THUMBNAILS_TINY:
//                        max_cells_shown *= 2;
//                        break;
//                }
//                if (_me->_current_dir_items.size() <= max_cells_shown) {
//                    _thumb_img = thumb_img(item);
//                    if (_thumb_img) {
//                        thumb_w = _thumb_img->width();
//                        thumb_h = _thumb_img->height();
//                    }
//                }
//            }
//        }
        }
//    Log_debug("ThumbnailRenderer::size idx:%d %s th:%d tw:%d", index, name.c_str(), thumb_h, thumb_w);

        if (thumb_w > thumb_h) {
            if (thumb_w > max_thumb_w) { // fit to max width
                thumb_h = max_thumb_h *  thumb_h / thumb_w;
                thumb_w = max_thumb_w;
            }
        } else {
            if (thumb_h > max_thumb_h) { // fit to max height
                thumb_w = max_thumb_w * thumb_w / thumb_h;
                thumb_h = max_thumb_h;
            }
        }
        ((CLTileView *) _me->_view)->set_max_thumb_size(thumb_w, thumb_h);
    }

//   Log_debug("size name=%s thumb=%d %d %d", name.c_str(), thumb_cell_width, tsz.width, tsz.height);
    if (!name.empty())
    {
        txt_width = CLGraphics::get_text_width(name, *get_name_style()) + 10;
        if (txt_width > THUMBNAIL_CELL_MAX_TEXT_WIDTH) {
            txt_width = std::max(thumb_w, THUMBNAIL_CELL_MAX_TEXT_WIDTH);
            txt_height = text_split_and_get_height(item, txt_width, lines);
        } else {
            txt_height = THUMBNAIL_CELL_TEXT_LINE_HEIGHT;
        }
//        Log_debug("text h=%d w=%d", text_height, txt_width);
    }
    tbx::Size result;
    result.width = std::max(thumb_w, std::min(txt_width, THUMBNAIL_CELL_MAX_TEXT_WIDTH)) + THUMBNAIL_ITEM_CELL_PADDING * 2 + ITEM_CELL_MARGIN * 2;
    result.height = std::min(_thumb_height * 2, thumb_h) + txt_height + THUMBNAIL_ITEM_CELL_PADDING*2 + ITEM_CELL_MARGIN*2;
//    Log_debug("size item=%s result=%dx%d", item->name().c_str(), result.width, result.height);
    return result;

//    return tbx::Size(
//            std::max((_thumb_width * 2) + THUMBNAIL_ITEM_CELL_PADDING*2, 320) + ITEM_CELL_MARGIN * 2,
//            (_thumb_height * 2) + 32 + THUMBNAIL_ITEM_CELL_PADDING*2 + ITEM_CELL_MARGIN * 2);
}

bool ThumbnailRenderer::hit_test(unsigned int index, const tbx::Size &size, const tbx::Point &pos) const {
    if (pos.x <(THUMBNAIL_ITEM_CELL_PADDING + ITEM_CELL_MARGIN) || pos.y <(THUMBNAIL_ITEM_CELL_PADDING + ITEM_CELL_MARGIN)
                                                                          || pos.x > (size.width - (THUMBNAIL_ITEM_CELL_PADDING + ITEM_CELL_MARGIN))
               || pos.y > (size.height - (THUMBNAIL_ITEM_CELL_PADDING + ITEM_CELL_MARGIN)))
        return false;
    return true;
}

bool ThumbnailRenderer::intersects(unsigned int index, const tbx::Size &size, const tbx::BBox &box) const {
    // Quick test intersects if mid point is in box
    return box.contains(size.width/2, size.height/2);
}


void CLListWimpFontRenderer::render(const tbx::view::ItemRenderer::Info &info)  {
    std::string t = _value_provider->value(info.index);
    int x = info.screen.x;
    if (!t.empty()) {
        tbx::WimpFont font;

        if (info.selected) {
            font.set_colours(_me->bg_color(), _me->_fg_color.colour());
        } else {
            font.set_colours(_me->_fg_color.colour(), _me->bg_color());
        }
        if (_flags & (1<<31)) {
            x += (info.bounds.width() - 6);
        }
        font.paint(x, info.screen.y+14, t, _flags);
    }
}

void CLItemSizeBarRenderer::render(const ItemRenderer::Info &info)
{
    tbx::BBox box = info.redraw.visible_area().screen(info.bounds);
    box.inflate(-8);
    int bounds_width = box.width();
    int width = _me->_current_dir_items[info.index]->dir_or_file_size() * bounds_width / _me->_max_dir_item_size;

    tbx::OSGraphics g;
    g.foreground(tbx::Colour(10, 200, 10));
    g.fill_rectangle(box.min.x, box.min.y, box.min.x + width, box.max.y);

//    if (_me->_drop_item_idx == info.index) {
////        Log_debug("BigSpriteAndNameRenderer::render refresh %d %d,%d %d,%d", info.index, info.bounds.min.x, info.bounds.max.y, info.bounds.max.x, info.bounds.min.y);
//        tbx::BBox box = info.redraw.visible_area().screen(info.bounds);
//        box.inflate(-2);
//        tbx::OSGraphics g;
//        g.foreground(_me->_fg_color.colour());
//        g.rectangle(box.min.x, box.min.y, box.max.x, box.max.y);
//    }
}

CLReportView::CLReportView(CLDirectoryWindow *cldirwin) :
        ReportView(cldirwin->_win_main), _me(cldirwin)
{
    menu_selects(true);
    add_click_listener(_me);
    margin(_me->_margin);
    setup_columns();
}

void CLReportView::setup_columns() {
    _columns.clear();
    Log_debug("CLReportView::setup_columns", 1);
    switch (_me->_operation_mode) {
        case OPERATION_MODE_DIR:
            if (_me->_view_mode == VIEW_MODE_DIR_SIZES_BARS) {
                add_column(&_me->_list_icon_and_name_renderer);
                add_column(&_me->_item_dir_or_file_size_renderer, 70);
                add_column(&_me->_item_dir_or_file_size_unit_renderer, 50);
                add_column(&_me->_item_size_bar_renderer, 500);
            } else {
                add_column(&_me->_list_icon_and_name_renderer);
                add_column(&_me->_item_attrs_renderer, 110);
                add_column(&_me->_item_size_renderer, 70);
                add_column(&_me->_item_size_unit_renderer, 50);
                add_column(&_me->_item_file_type_renderer, 164);
                add_column(&_me->_item_mtime_renderer, 8000);
            }
            break;
        case OPERATION_MODE_TIMELINE:
            add_column(&_me->_list_icon_and_name_renderer);
            if (_me->_timeline_filter == TIMELINE_FILTER_FILES) {
                add_column(&_me->_item_file_type_renderer, 164);
                add_column(&_me->_item_mtime_renderer, 246);
            }
            add_column(&_me->_item_path_renderer, 8000);
            break;
        default:
            add_column(&_me->_list_icon_and_name_renderer);
            add_column(&_me->_item_attrs_renderer, 110);
            add_column(&_me->_item_size_renderer, 70);
            add_column(&_me->_item_size_unit_renderer, 50);
            add_column(&_me->_item_file_type_renderer, 164);
            add_column(&_me->_item_mtime_renderer, 246);
            add_column(&_me->_item_path_renderer, 8000);
            break;
    }
}

void CLReportView::redraw(const tbx::RedrawEvent &event) {
//    Log_debug("CLReportView::redraw dirwin:%p", _me);
    tbx::BBox clip = event.clip();
    tbx::OSGraphics g;

    if (_me->_bg_color != tbx::Colour::no_colour) {
        g.foreground(_me->_bg_color);
        g.fill_rectangle(clip.min.x, clip.min.y, clip.max.x, clip.max.y);
    }

    tbx::BBox work_clip = event.visible_area().work(event.clip());
    int first_row = (-work_clip.max.y - _margin.top) / _height;
    int last_row =  (-work_clip.min.y - _margin.top) / _height;

    if (first_row < 0) first_row = 0;
    if (last_row < 0) return; // Nothing to draw

    if (first_row >= _count) return; // Nothing to draw
    if (last_row >= _count) last_row = _count - 1;
//    Log_debug("CLReportView::redraw _count:%d last_row:%d", _count, last_row);

    unsigned int first_col = column_from_x(work_clip.min.x);
    unsigned int last_col = column_from_x(work_clip.max.x);

    if (first_col >= column_count()) return; // Nothing to redraw
    if (last_col >= column_count()) last_col = column_count() - 1;

    tbx::view::ItemRenderer::Info cell_info(event);

    cell_info.bounds.max.y = -first_row * _height - _margin.top;
    cell_info.screen.y = event.visible_area().screen_y(cell_info.bounds.max.y);
    int first_col_x = x_from_column(first_col);
    int first_col_scr_x = event.visible_area().screen_x(first_col_x);

    for (unsigned int row = first_row; row <= last_row; row++)
    {
        cell_info.bounds.min.y = cell_info.bounds.max.y - _height;
        cell_info.bounds.min.x = first_col_x;
        cell_info.screen.y -= _height;
        cell_info.screen.x = first_col_scr_x;
        cell_info.index = row;

        cell_info.selected = (_selection != 0 && _selection->selected(row));

        if (cell_info.selected || _me->_drop_item_idx == row)
        {
            // Fill background with selected colour
            int sel_right;
            if (last_col < column_count()-1) sel_right = x_from_column(last_col+1);
            else sel_right = _width + _margin.left;
            g.foreground(_me->_fg_color.colour());
            if (cell_info.selected) {
//                Log_debug("cell_info.selected %d,%d,%d,%d", first_col_scr_x, cell_info.screen.y + _height - 1, first_col_scr_x + sel_right - first_col_x - 1, cell_info.screen.y);
                g.fill_rectangle(first_col_scr_x, cell_info.screen.y,
                                 first_col_scr_x + sel_right - first_col_x - 1,
                                 cell_info.screen.y + _height - 1);
            } else {
                g.rectangle(first_col_scr_x, cell_info.screen.y,
                                 first_col_scr_x + sel_right - first_col_x - 1,
                                 cell_info.screen.y + _height - 1);
            }
        }

        for (unsigned int col = first_col; col <= last_col; col++)
        {
            cell_info.bounds.max.x = cell_info.bounds.min.x + _columns[col].width;
            if (_columns[col].width > 0)
            {
                _columns[col].renderer->render(cell_info);
            }
            cell_info.screen.x += cell_info.bounds.max.x - cell_info.bounds.min.x + _column_gap;
            cell_info.bounds.min.x = cell_info.bounds.max.x + _column_gap;
        }
        cell_info.bounds.max.y = cell_info.bounds.min.y;
    }
}

void CLReportView::update_window_extent()
{
    if (!updates_enabled()) return;

    int width = _width + _margin.left + _margin.right;
    int height = _count * _height + _margin.top + _margin.bottom;

    tbx::WindowState state;
    _window.get_state(state);

    if (width < state.visible_area().bounds().width())
        width = state.visible_area().bounds().width();

    if (height < state.visible_area().bounds().height() && (!_me->_min_layout || _count == 0))
        height = state.visible_area().bounds().height();

    tbx::BBox extent(0,-height, width, 0);
    _window.extent(extent);
}

void CLReportView::refresh()
{
    if (updates_enabled())
    {
//        BBox all(_margin.left, -_margin.top - _count * _height,
//                 _margin.left + _width, -_margin.top);
        _window.force_redraw(_window.extent());
    }
}

CLTileView::CLTileView(CLDirectoryWindow *cldirwin, tbx::view::ItemRenderer *itemRenderer) :
        TileView(cldirwin->_win_main, itemRenderer), _me(cldirwin)
{
    margin(_me->_margin);
    menu_selects(true);
    add_click_listener(_me);
    init_thumbs_size();
}

void CLTileView::set_max_thumb_size(int thumb_w, int thumb_h) {
    _max_thumb.width = std::max(_max_thumb.width, thumb_w);
    _max_thumb.height = std::max(_max_thumb.height, thumb_h);
}

void CLTileView::redraw(const tbx::RedrawEvent &event) {
//    Log_debug("CLTileView::redraw dirwin:%p dir:%s items:%d clip min_x:%d min_y:%d max_x:%d max_y:%d bgcolor:%x", _me, _me->_current_dir_path.c_str(), _me->_current_dir_items.size(), event.clip().min.x, event.clip().min.y, event.clip().max.x, event.clip().max.y, _me->_bg_color);
    if (_me->_bg_color != tbx::Colour::no_colour) {
        tbx::BBox clip = event.clip();
        tbx::OSGraphics g;
        g.foreground(_me->_bg_color);
        g.fill_rectangle(clip.min.x, clip.min.y, clip.max.x, clip.max.y);
    }
//    auto start = os_read_monotonic_time();
    TileView::redraw(event);
//    auto diff = os_read_monotonic_time() - start;
//    if (diff > 100) {
//        Log_debug("CLTileView::redraw takes much time:%d will refresh status bar", diff);
//        auto refresh_statusbar = [this]() {
//            Log_debug("Refreshing statusbar", 1);
//            _me->_win_statusbar.scroll(0,0);
//            tbx::BBox statusbar_extent = _me->_win_statusbar.extent();
//            _me->_win_statusbar.force_redraw(statusbar_extent);
//        };
//        g_idle_task.run_at_next_idle(refresh_statusbar);
//    }
}

bool CLTileView::recalc_layout(const tbx::BBox &visible_area) {
    int cols_per_row = (visible_area.width() - _margin.left - _margin.right) / _tile_size.width;
    bool cols_per_row_changed = false;
    if (cols_per_row < 1) cols_per_row = 1;

    if (cols_per_row != _cols_per_row)
    {
        // Need to re do the layout
        _cols_per_row = cols_per_row;
        cols_per_row_changed = true;
    }

    int rows = (_count + cols_per_row - 1)/cols_per_row;
    int height = rows * _tile_size.height + _margin.top + _margin.bottom;
    if (height < visible_area.height() && (!_me->_min_layout || height <= (_me->_margin.top + _margin.bottom)))
        height = visible_area.height();
    //Log_debug("CLTileView::recalc_layout w:%d h:%d", visible_area.width(), height);
    tbx::BBox extent(0,-height, visible_area.width(), 0);
    tbx::BBox old_extent = _window.extent();
    if (old_extent != extent) {
        _window.extent(extent);
    }

    return cols_per_row_changed;
}

class CLFileInfo : public tbx::HasBeenHiddenListener {
    tbx::FileInfo fi;
public:
    CLFileInfo(const tbx::Path& f) {
        fi = tbx::FileInfo("FInfo");
        fi.file_type(f.file_type());
        fi.file_size(f.file_size());
        fi.date(f.modified_time());
        fi.file_name(f.leaf_name());
        fi.title(std::string("About ") + f.leaf_name());
        fi.add_has_been_hidden_listener(this);
        fi.show_as_menu();
    }

    void has_been_hidden(const tbx::EventInfo &event_info) override {
        fi.delete_object();
        delete this;
    }
};

static tbx::Point win_initial_position(-1,-1);

CLDirectoryWindow::CLDirectoryWindow(tbx::Window win):
        _win_main(win),
        _win_toolbar("SWFilerTBar"),
        _win_treeview("SWDirTree"),
        _win_statusbar("SWStatusTB"),
        _win_inplace("WInplace"),
        _open_window_request_listener(this),
        _idle_view_refresher(this),
        _idle_top_toolbar_scroller(this),
        _rename_inplace_key_listener(this),
        _toolbar_root_selector_listener(this),
        _toolbar_button_nav_listener(this),
        _toolbar_button_refresh_listener(this),
        _toolbar_search_toggle_listener(this),
        _toolbar_search_text_or_option_changed_listener(this),
        _toolbar_treeview_toggle_listener(this),
        _toolbar_button_select_color_listener(this),
        _toolbar_button_favorite_listener(this),
        _toolbar_sort_order_changed_listener(this),
        _toolbar_button_timeline_toggle_listener(this),
        _toolbar_button_timeline_folders_listener(this),
        _toolbar_button_timeline_files_listener(this),
        _toolbar_button_timeline_apps_listener(this),
        _item_size_text(this),
        _item_size_unit_text(this),
        _item_dir_or_file_size_text(this),
        _item_dir_or_file_size_unit_text(this),
        _item_file_type_text(this),
//        _item_dir_or_file_size(this),
        _item_mtime_text(this),
        _item_attrs_text(this),
        _item_path_text(this),
        _tile_big_icon_and_name_renderer(this),
        _tile_small_icon_and_name_renderer(this, true),
        _tile_thumbnail_big_renderer(this, THUMBNAIL_IMAGE_WIDTH, THUMBNAIL_IMAGE_HEIGHT),
        _tile_thumbnail_medium_renderer(this, THUMBNAIL_IMAGE_WIDTH/2, THUMBNAIL_IMAGE_HEIGHT/2),
        _tile_thumbnail_small_renderer(this, THUMBNAIL_IMAGE_WIDTH/4, THUMBNAIL_IMAGE_HEIGHT/4),
        _tile_thumbnail_tiny_renderer(this, THUMBNAIL_IMAGE_WIDTH/8, THUMBNAIL_IMAGE_HEIGHT/8),
//        _tile_thumbnail_small_renderer(this, 36, 36),
//        _tile_thumbnail_tiny_renderer(this, 18, 18),
        _list_icon_and_name_renderer(this, false),
        _item_size_renderer(this, (1<<31), &_item_size_text),
        _item_dir_or_file_size_renderer(this, (1 << 31), &_item_dir_or_file_size_text),
        _item_size_unit_renderer(this, 0, &_item_size_unit_text),
        _item_dir_or_file_size_unit_renderer(this, 0, &_item_dir_or_file_size_unit_text),
        _item_size_bar_renderer(this),
        _item_attrs_renderer(this, 0, &_item_attrs_text),
        _item_mtime_renderer(this, 0, &_item_mtime_text),
        _item_path_renderer(this, 0, &_item_path_text),
        _item_file_type_renderer(this, 0, &_item_file_type_text)
{
    _selection = new tbx::view::MultiSelection();
    _selection->add_listener(this);
//    Log_debug("__selection = %p", _selection);

    _treeview = _win_treeview.gadget(0);
    _treeview_underlying_window = _treeview.underlying_window();
    _treeview.add_node_selected_listener(this);
    _treeview.add_node_expanded_listener(this);
    _treeview.add_node_dragged_listener(this);
    tbx::Menu mnu = tbx::Menu("MTreeView");
    _tree_menu = new CLTreeMenu(mnu, this);
    _treeview.menu(mnu);
    _win_main.add_open_window_listener(this);
    _win_main.add_about_to_be_shown_listener(this);
    _win_main.add_has_been_hidden_listener(this);
    _win_main.add_key_listener(this);
    _win_main.add_mouse_click_listener(this);
    _win_main.add_gain_caret_listener(this);
    _win_main.add_lose_caret_listener(this);
    _win_main.add_loader(this);
    _win_main.client_handle(this);
    _win_treeview.client_handle(this);
    _win_toolbar.client_handle(this);
    _win_toolbar.add_key_listener(this);
    _win_toolbar.add_mouse_click_listener(this);
    _win_toolbar.add_pointer_entering_listener(&_idle_top_toolbar_scroller);
    _win_toolbar.add_pointer_leaving_listener(&_idle_top_toolbar_scroller);
    _win_statusbar.client_handle(this);
    _win_statusbar.add_key_listener(this);
    _win_statusbar.add_mouse_click_listener(this);

    _treeview.all_events(true);
    _treeview_underlying_window.add_all_mouse_click_listener(this);
    _treeview_underlying_window.add_loader(this);


    tbx::StringSet(_win_toolbar.gadget(6)).add_text_changed_listener(&_toolbar_root_selector_listener);
    tbx::StringSet(_win_toolbar.gadget(6)).add_about_to_be_shown_listener(&_toolbar_root_selector_listener);

    tbx::WritableField(_win_inplace.gadget(0)).add_key_listener(&_rename_inplace_key_listener);
    _win_inplace.add_key_listener(&_rename_inplace_key_listener);
    tbx::StringSet(_win_toolbar.gadget(0x16)).add_text_changed_listener(&_toolbar_sort_order_changed_listener);
    tbx::Button(_win_toolbar.gadget(0x17)).add_mouse_click_listener(&_toolbar_sort_order_changed_listener);

    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_UP_ID)).add_mouse_click_listener(&_toolbar_button_nav_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_BACK_ID)).add_mouse_click_listener(&_toolbar_button_nav_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_FORW_ID)).add_mouse_click_listener(&_toolbar_button_nav_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TREEVIEW_ID)).add_mouse_click_listener(&_toolbar_treeview_toggle_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_REFRESH_ID)).add_mouse_click_listener(&_toolbar_button_refresh_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_FAVORITE_ID)).add_mouse_click_listener(&_toolbar_button_favorite_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_SEARCH_ID)).add_mouse_click_listener(&_toolbar_search_toggle_listener);

    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).add_mouse_click_listener(&_toolbar_button_timeline_toggle_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).add_mouse_click_listener(&_toolbar_button_timeline_apps_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).add_mouse_click_listener(&_toolbar_button_timeline_files_listener);
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).add_mouse_click_listener(&_toolbar_button_timeline_folders_listener);

    tbx::WritableField(_win_toolbar.gadget(TOOLBAR_SEARCH_WRITEABLE_ID)).add_text_changed_listener(&_toolbar_search_text_or_option_changed_listener);
    tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_APPS_ID)).add_state_listener(&_toolbar_search_text_or_option_changed_listener);
    tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_IMAGES_ID)).add_state_listener(&_toolbar_search_text_or_option_changed_listener);
    tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_CASE_ID)).add_state_listener(&_toolbar_search_text_or_option_changed_listener);
    tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).add_selected_listener(&_toolbar_search_text_or_option_changed_listener);

    tbx::Button(_win_statusbar.gadget(1)).add_mouse_click_listener(this);

//    _toolbar_btn_select_color = _win_toolbar.gadget(8);
//    _toolbar_btn_select_color.add_mouse_click_listener(&_toolbar_button_select_color_listener);
    _search_model = new CLSearchModel(this);

    Log_debug("CLDirectoryWindow::CLDirectoryWindow end current_node_id=%d winhandle=%x this:%p", _treeview.current_node().node_id(), _win_main.handle(), this);

    g_app_state.dir_windows.push_back(this);
    g_app_data_model.add_change_listener(this);
    g_app_data_model.add_refresh_listener(this);
    view_mode(g_app_state.view_mode);
    _initialized = true;
    set_status_line("");
}

CLDirectoryWindow::~CLDirectoryWindow() {
    Log_debug("CLDirectoryWindow::~CLDirectoryWindow start %p", this);
    g_app_data_model.remove_change_listener(this);
    g_app_data_model.remove_refresh_listener(this);

    delete _tree_menu;
    _treeview.remove_node_expanded_listener(this);
    _treeview.remove_node_selected_listener(this);
    _treeview_underlying_window.remove_mouse_click_listener(this);
    _treeview_underlying_window.remove_loader(this);
    _treeview.auto_update(false);
    _treeview.clear();
    _selection->remove_listener(this);

    delete _view;
    delete _selection;
    delete _search_model;

    for(auto dirit = g_app_state.dir_windows.begin(); dirit != g_app_state.dir_windows.end(); ++dirit) {
        if (*dirit == this) {
            g_app_state.dir_windows.erase(dirit);
            Log_debug("Erase window %x this:%p from g_app_state.dir_windows", _win_main.handle(), this);
            break;
        }
    }

    _win_toolbar.delete_object();
    _win_treeview.delete_object();
    _win_main.delete_object();
    Log_debug("CLDirectoryWindow::~CLDirectoryWindow %p", this);
}

void CLDirectoryWindow::has_been_hidden(const tbx::EventInfo &event_info) {
    Log_debug("CLDirectoryWindow::has_been_hidden %p", this);
    tbx::PointerInfo where(true,false);
    if (where.adjust_down()) {
        open_new(tbx::Path(_current_dir_path).parent(), this);
    }
    delete this;
}

void CLDirectoryWindow::init(const tbx::Path& dir, CLDirectoryWindow* parent, bool min_size_no_toolbars) {
    tbx::Path canonical_dir = g_app_data_model.find_real_existing_dir_or_parent(dir);
    std::vector<std::string> parts;
    const std::vector<CLDirectoryItemData*> &root_items = g_app_data_model.get_root_items();
    Log_debug("CLDirectoryWindow::init path:%s init_path:%s roots_count:%d", canonical_dir.name().c_str(), dir.name().c_str(), root_items.size());

    _latest_instance = this;

    const os_VDU_VAR_LIST(5) var_list =
            {{
                 os_MODEVAR_XEIG_FACTOR,
                 os_MODEVAR_YEIG_FACTOR,
                 os_MODEVAR_XWIND_LIMIT,
                 os_MODEVAR_YWIND_LIMIT,
                 -1,
             }};
    int vdu_info[4];
    os_read_vdu_variables((const os_vdu_var_list *) &var_list,
                          vdu_info);


    if (win_initial_position.y == -1) {
        _win_main.show_centered();
        tbx::Application::instance()->yield();
        win_initial_position = _win_main.bounds().bottom_left();
    } else {
        _win_main.show();
    }
    tbx::WindowState state;
    _win_main.get_state(state);
    tbx::BBox &bounds = state.visible_area().bounds();
    Log_info("vdu_info %d %d %d %d bounds %d %d %d %d pos %d %d", vdu_info[0], vdu_info[1], vdu_info[2], vdu_info[3], bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y, win_initial_position.x, win_initial_position.y);
    if (vdu_info[2] > 799) {
        bounds.min.y = (0 << vdu_info[1]);
//        if (vdu_info[3] > 767) {
//            bounds.min.x = (50 << vdu_info[1]);
////            bounds.max.x = bounds.min.x + (942 << vdu_info[0]);
////            bounds.max.y = bounds.min.y + (532 << vdu_info[1]);
//        } else {
//            bounds.min.x = (10 << vdu_info[1]);
//        }
        bounds.size(942 << vdu_info[0], 532 << vdu_info[1]);
        if (win_initial_position.x + bounds.width() > (vdu_info[2] << vdu_info[0])) {
            win_initial_position.x = 0;
            win_initial_position.y = 0;
        }
        if (bounds.bottom_left() != win_initial_position) {
            bounds.move_to(win_initial_position.x, win_initial_position.y);
        }
        win_initial_position.x += 50;
        win_initial_position.y -= 50;
    } else {
        bounds.min.x = 0;
        bounds.max.x = vdu_info[2] << vdu_info[0];
        bounds.min.y = (60 << vdu_info[1]);
        bounds.max.y = ((vdu_info[3] - 20) << vdu_info[1]);
    }

    if (parent) {
        if (min_size_no_toolbars) {
            _toolbars_visible = 0;
            _min_layout = true;
            bounds.min.x += CONTENT_MARGIN_LEFT;
            bounds.max.x -= 128;
            bounds.min.y += (CONTENT_MARGIN_BOTTOM + 28);
            bounds.max.y -= (CONTENT_MARGIN_TOP + 24);
        } else {
            _toolbars_visible = parent->_toolbars_visible;
        }
    } else {
        _toolbars_visible = g_app_config.OpenNewWindow;
    }
    Log_debug("CLDirectoryWindow::init g_app_config.OpenNewWindow %d new bounds %d %d %d %d", g_app_config.OpenNewWindow, bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
    tbx::BBox winextent = _win_main.extent();
    winextent.max.x = bounds.width();
    winextent.min.y = -(std::max(winextent.height(), bounds.height()));
    _win_main.extent(winextent);
    _win_main.open_window(state);

    split(canonical_dir.name(), parts, '.');
    std::string root_dir = parts[0];
    std::string roots_str;
    for(auto item: root_items) {
        if (!roots_str.empty()) {
            roots_str.append(",");
        }
        roots_str.append(item->name());
        Log_debug("Root =%s id:%d", item->name().c_str(), item->id());
    }

    auto item = g_app_data_model.get_or_refresh_path(canonical_dir.name());
    if (!item) {
        Logger::warn("CLDirectoryWindow::init dir item not found for [%s] will use root dir: %s", canonical_dir.name().c_str(), root_dir.c_str());
        item = g_app_data_model.get_root_item(root_dir);
    }

    //    switch (g_app_config.OpenNewWindow) {
//        case 1:
//            _toolbars_visible = TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW;
//            break;
//        case 2:
//            _toolbars_visible = TOOLBARS_VISIBLE_NONE;
//            break;
//        case 3:
//            _toolbars_visible = TOOLBARS_VISIBLE_TOP;
//            break;
//        case 4:
//            _toolbars_visible = TOOLBARS_VISIBLE_TOP_BOTTOM;
//            break;
//    }

    if (win_initial_position.y < 50) {
        win_initial_position.y = (vdu_info[3] << vdu_info[1]) - bounds.height();
    }
    if (win_initial_position.x >= (vdu_info[2] << vdu_info[0]) - bounds.width()) {
        win_initial_position.x = 0;
    }

    g_my_app->yield();

    tbx::StringSet roots_stringset = _win_toolbar.gadget(TOOLBAR_STRINGSET_ROOT);
    roots_stringset.available(roots_str);
    roots_stringset.selected(root_dir);

    setup_subwindows();
    change_root_dir(root_dir);
//    if (!change_root_dir(root_dir)) {
//        canonical_dir = g_app_data_model.get_root_items()[0]->path();
//        change_root_dir(canonical_dir.name());
//    }
    _win_toolbar.gadget(TOOLBAR_BTN_FAVORITE_ID).fade(g_app_data_model.get_favorites().empty());
    tbx::ext::TreeViewCurrentNode root_node = _treeview.current_node(_treeview_root_node_id);
    bool tree_root_has_childs = root_node.has_child();
    Log_debug("CLDirectoryWindow::init expand root node: %x -> %x has_child=%d", _treeview_root_node_id, root_node.node_id(), tree_root_has_childs);
    _treeview.notify_expansion(false);
    root_node.expand(true);
    _treeview.notify_expansion(true);

    if (item) {
        change_current_directory(item, true, true);

        _treeview.notify_expansion(false);
        root_node.expand(true);
        _treeview.notify_expansion(true);
    } else {
        Logger::crit("CLDirectoryWindow::init dir item not found and root item not found too for: %s!", canonical_dir.name().c_str());
    }
    _win_main.focus();
    _min_layout = false;
}

void CLDirectoryWindow::open_new(const tbx::Path& p, CLDirectoryWindow * parent) {
    auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
    instance->init(p, parent ? parent : CLDirectoryWindow::_latest_instance);
}


void CLDirectoryWindow::open_new_without_toolbars(const tbx::Path& p) {
    auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
    instance->init(p, this, true);
}


void CLDirectoryWindow::setup_top_toolbar(int scroll_x) {
    tbx::ShowSubWindowSpec toolbar_subwin_opts;
    tbx::BBox main_bounds = _win_main.bounds();

    bool _top_toolbar_visible = _toolbars_visible >= TOOLBARS_VISIBLE_TOP;

    int content_margin_top = (_top_toolbar_visible ? (_operation_mode == OPERATION_MODE_SEARCH ? CONTENT_MARGIN_TOP * 2 : CONTENT_MARGIN_TOP) : 0);

    Log_debug("CLDirectoryWindow::setup_top_toolbar scroll_x=%d", scroll_x);

    if (_top_toolbar_visible || _operation_mode == OPERATION_MODE_SEARCH) {
        toolbar_subwin_opts.wimp_parent = _win_main.window_handle();
        toolbar_subwin_opts.wimp_window = -1;
        toolbar_subwin_opts.visible_area.bounds() = tbx::BBox(main_bounds.min.x,
                                                              main_bounds.max.y - content_margin_top + 2,
                                                              main_bounds.max.x,
                                                              main_bounds.max.y);
        toolbar_subwin_opts.visible_area.scroll() = tbx::Point(scroll_x, 0);
        toolbar_subwin_opts.flags = 0
                                    //                                    | tbx::NEW_WINDOW_FLAGS
                                    | tbx::ALIGN_Y_SCROLL_VISIBLE_TOP
                                    | tbx::ALIGN_LEFT_VISIBLE_LEFT
                                    | tbx::ALIGN_BOTTOM_VISIBLE_TOP
                                    | tbx::ALIGN_RIGHT_VISIBLE_RIGHT
                                    | tbx::ALIGN_TOP_VISIBLE_TOP;

        _win_toolbar.show_as_subwindow(toolbar_subwin_opts);
    } else {
        _win_toolbar.hide();
    }
}

void CLDirectoryWindow::setup_subwindows() {
    tbx::ShowSubWindowSpec toolbar_subwin_opts;
    tbx::BBox main_bounds = _win_main.bounds();

    bool _top_toolbar_visible = _toolbars_visible >= TOOLBARS_VISIBLE_TOP;
    bool _bottom_toolbar_visible = _toolbars_visible >= TOOLBARS_VISIBLE_TOP_BOTTOM;
    bool _treeview_visible  = _toolbars_visible >= TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW;

    int content_margin_top = (_top_toolbar_visible ? (_operation_mode == OPERATION_MODE_SEARCH ? CONTENT_MARGIN_TOP * 2 : CONTENT_MARGIN_TOP) : 0);
    int content_margin_bottom = ((_bottom_toolbar_visible || _operation_mode == OPERATION_MODE_SEARCH) ? CONTENT_MARGIN_BOTTOM : 0);
    int content_margin_left = (_treeview_visible ? CONTENT_MARGIN_LEFT : 0);

    _margin = tbx::Margin(content_margin_left + 16, content_margin_top + 12, 16, content_margin_bottom);

    setup_top_toolbar(0);

    if (_bottom_toolbar_visible || _operation_mode == OPERATION_MODE_SEARCH) {
        toolbar_subwin_opts.wimp_parent = _win_main.window_handle();
        toolbar_subwin_opts.wimp_window = -1;
        toolbar_subwin_opts.visible_area.bounds() = tbx::BBox(main_bounds.min.x,
                                                              main_bounds.min.y,
                                                              main_bounds.max.x,
                                                              main_bounds.min.y + CONTENT_MARGIN_BOTTOM - 2);
        toolbar_subwin_opts.visible_area.scroll() = tbx::Point(0,0);
        toolbar_subwin_opts.flags = 0
                                    //                                    | tbx::NEW_WINDOW_FLAGS
                                    | tbx::ALIGN_Y_SCROLL_VISIBLE_TOP
                                    | tbx::ALIGN_LEFT_VISIBLE_LEFT
                                    | tbx::ALIGN_BOTTOM_VISIBLE_BOTTOM
                                    | tbx::ALIGN_RIGHT_VISIBLE_RIGHT
                                    | tbx::ALIGN_TOP_VISIBLE_BOTTOM;

//    Log_debug(("setup_subwindows1 %x %x %x (%d,%d) (%d,%d)", _win_main.handle(), _win_toolbar.handle(), _win_treeview.handle(),
//                  toolbar_subwin_opts.visible_area.bounds().min.x,
//                  toolbar_subwin_opts.visible_area.bounds().max.y,
//                  toolbar_subwin_opts.visible_area.bounds().max.x,
//                  toolbar_subwin_opts.visible_area.bounds().min.y);
    //_win_toolbar.extent(tbx::BBox(0, -content_margin_top, main_bounds.width(), 0));
        _win_statusbar.show_as_subwindow(toolbar_subwin_opts);
    } else {
        _win_statusbar.hide();
    }

    if (_treeview_visible) {
        tbx::ShowSubWindowSpec treeview_subwin_opts;
        treeview_subwin_opts.wimp_parent = _win_main.window_handle();
        treeview_subwin_opts.wimp_window = -1;
        treeview_subwin_opts.visible_area.bounds() = tbx::BBox(main_bounds.min.x,
                                                               main_bounds.min.y + content_margin_bottom,
                                                               main_bounds.min.x + CONTENT_MARGIN_LEFT,
                                                               main_bounds.max.y - content_margin_top);
        treeview_subwin_opts.visible_area.scroll() = tbx::Point(0,0);
        treeview_subwin_opts.flags = 0
//                                     | tbx::NEW_WINDOW_FLAGS
                                     | tbx::ALIGN_LEFT_VISIBLE_LEFT
                                     | tbx::ALIGN_BOTTOM_VISIBLE_BOTTOM
                                     | tbx::ALIGN_RIGHT_VISIBLE_LEFT
                                     | tbx::ALIGN_TOP_VISIBLE_TOP
//                                     | tbx::ALIGN_Y_SCROLL_VISIBLE_BOTTOM
                                     | tbx::ALIGN_Y_SCROLL_VISIBLE_TOP;

//        Log_debug(("setup_subwindows1 %x %x %x main (%d,%d) (%d,%d)", _win_main.handle(), _win_toolbar.window_handle(), _win_treeview.window_handle(),
//                      main_bounds.min.x,
//                      main_bounds.max.y,
//                      main_bounds.max.x,
//                      main_bounds.min.y);
//        Log_debug(("setup_subwindows2 %x %x %x (%d,%d) (%d,%d)", _win_main.handle(), _win_toolbar.window_handle(), _win_treeview.window_handle(),
//                  treeview_subwin_opts.visible_area.bounds().min.x,
//                  treeview_subwin_opts.visible_area.bounds().max.y,
//                  treeview_subwin_opts.visible_area.bounds().max.x,
//                  treeview_subwin_opts.visible_area.bounds().min.y);

        tbx::BBox tree_bounds = tbx::BBox(0,
                                          -(main_bounds.height() - content_margin_top - content_margin_bottom),
                                          CONTENT_MARGIN_LEFT,
                                          0);
        _win_treeview.extent(tbx::BBox(0, -99999, CONTENT_MARGIN_LEFT, 0));
        _treeview.bounds(tree_bounds);
        _win_treeview.show_as_subwindow(treeview_subwin_opts);

//        Log_debug(("setup_subwindows3 %x %x %x (%d,%d) (%d,%d) w:%d x h:%d", _win_main.window_handle(), _win_toolbar.window_handle(), _win_treeview.window_handle(),
//                      tree_bounds.min.x,
//                      tree_bounds.max.y,
//                      tree_bounds.max.x,
//                      tree_bounds.min.y,
//                      tree_bounds.width(),
//                      tree_bounds.height()
//        );
    } else {
        _win_treeview.hide();
    }

    _prev_win_size = main_bounds.size();

    if (_view) {
        _view->margin(_margin);
    }
}

void CLDirectoryWindow::update_treeview_bounds() {
    tbx::BBox main_bounds = _win_main.bounds();
    bool _top_toolbar_visible = _toolbars_visible >= TOOLBARS_VISIBLE_TOP;
    bool _bottom_toolbar_visible = _toolbars_visible >= TOOLBARS_VISIBLE_TOP_BOTTOM;
    int content_margin_top = (_top_toolbar_visible ? (_operation_mode == OPERATION_MODE_SEARCH ? CONTENT_MARGIN_TOP * 2 : CONTENT_MARGIN_TOP) : 0);
    int content_margin_bottom = ((_bottom_toolbar_visible || _operation_mode == OPERATION_MODE_SEARCH) ? CONTENT_MARGIN_BOTTOM : 0);
    tbx::BBox tree_bounds = tbx::BBox(0,
                                      -(main_bounds.height() - content_margin_top - content_margin_bottom),
                                      CONTENT_MARGIN_LEFT,
                                      0);
    _treeview.bounds(tree_bounds);
}

//tbx::WimpSprite CLDirectoryWindow::make_treeview_sprite(tbx::WimpSprite* wimp_sprite) {
//    tbx::Size sz, dst_sz;
//    int mode;
//    bool has_mask;
//    wimp_sprite->info(&sz, &mode, &has_mask);
//    dst_sz.height = sz.height / 2;
//    dst_sz.width = sz.width / 2;
//    tbx::WimpSprite wimp_spr = tbx::SpriteArea::create_wimp_sprite("spr", dst_sz.width, dst_sz.height, 0x301680B5, 0, 0);
//    Log_debug("user_spr created p=%p sz=%dx%d", user_spr.pointer(), user_spr.width(), user_spr.height());
//    tbx::ScaleFactors sf(1,1,2,2);
//    tbx::TranslationTable tt;
//    Log_debug("tbx::ScaleFactors created", 1);
//    if (wimp_sprite->mode() != 0x301680B5) {
//        tt.create(wimp_sprite);
//    }
//    Log_debug("Will capture sprite: %s", wimp_sprite->name().c_str());
//    if (wimp_sprite->has_palette()) {
//        tbx::SpriteCapture capture_sprite(&user_spr, true, false);
//        wimp_sprite->plot_scaled(0, 0, &sf, &tt, (1 << 4));
//        capture_sprite.release();
//    } else {
//        tbx::SpriteCapture capture_sprite(&user_spr, true, false);
//        wimp_sprite->plot_scaled(0, 0, &sf, 0, 0);
//        capture_sprite.release();
//    }
//    Log_debug("Captured sprite: %s", wimp_sprite->name().c_str());
////    Log_debug("Plot scaled '%s' has_mask:%d has_palette:%d sf xm:%d ym:%d xd:%d, yd:%d area:%p", user_spr.name().c_str(), user_spr.has_mask(), user_spr.has_palette(), sf.xmult(), sf.ymult(), sf.xdiv(), sf.ydiv(), area->pointer());
//    return area;
//}

void CLDirectoryWindow::assign_node_sprite(tbx::ext::TreeViewCurrentNode& node, CLDirectoryItemData *item) {
    CLBaseImage *spr = item->small_sprite_image();
//    Log_debug("CLDirectoryWindow::assign_node_sprite small spr:%p item:%s", spr, item->name().c_str());
    if (spr) {
        if (spr->get_image_type() == CL_IMAGE_TYPE_WIMP_SPRITE) {
            node.sprite(1, ((CLWimpSpriteImage*)spr)->name_str(), ((CLWimpSpriteImage*)spr)->name_str());
        } else {
            CLUserSpriteImage *user_spr = ((CLUserSpriteImage*)spr);
            std::string spr_name = user_spr->name();
            node.sprite((int)user_spr->get_area_pointer(), spr_name, spr_name);
        }
    } else {
        node.sprite(1, "small_dir", "small_diro");
    }
}

std::vector<std::string> CLDirectoryWindow::selected_item_names() {
    std::vector<std::string> files;
    if (!_selection->empty()) {
        for (tbx::view::Selection::Iterator it = _selection->begin(); it != _selection->end(); ++it) {
            files.push_back(_current_dir_items[*it]->name());
        }
    }
    return files;
}

std::vector<CLDirectoryItemData*> CLDirectoryWindow::selected_items() {
    std::vector<CLDirectoryItemData*> items;
    Log_debug("CLDirectoryWindow::selected_items selectio_count=%d items_count=%d", _selection->count(), _current_dir_items.size());
    if (!_selection->empty()) {
        for (tbx::view::Selection::Iterator it = _selection->begin(); it != _selection->end(); ++it) {
            Log_debug("CLDirectoryWindow::selected_items it=%d items_count=%d", *it, _current_dir_items.size());
            items.push_back(_current_dir_items[*it]);
        }
    }
    return items;
}

void CLDirectoryWindow::set_current_dir_items(const std::vector<CLDirectoryItemData*> &items) {
    _current_dir_items = items;

    _max_dir_item_size = 0;
    for(auto item : _current_dir_items) {
        if (item->dir_or_file_size() > _max_dir_item_size) {
            _max_dir_item_size = item->dir_or_file_size();
        }
    }

    bool desc = (_sort_order & 0xf) ? true : false;
    switch (_sort_order & 0xfff0) {
        case SORT_NAME:
            Log_debug("set_current_dir_items SORT_NAME", 1);
            std::sort(_current_dir_items.begin(), _current_dir_items.end(), [desc](CLDirectoryItemData *a, CLDirectoryItemData *b) {
                std::string aname = str_replace_all(a->name(), "_", "|");
                std::string bname = str_replace_all(b->name(), "_", "|");
                if (desc) {
                    return stricmp(aname.c_str(), bname.c_str()) > 0;
                } else {
                    return stricmp(aname.c_str(), bname.c_str()) < 0;
                }
            });
            break;
        case SORT_NAME_DIRS:
            Log_debug("set_current_dir_items SORT_NAME_DIRS", 1);
            std::sort(_current_dir_items.begin(), _current_dir_items.end(), [desc](CLDirectoryItemData *a, CLDirectoryItemData *b) {
                if (!a->is_file() && b->is_file()) {
                    return (desc ? false : true);
                } else if (a->is_file() && !b->is_file()) {
                    return (desc ? true : false);
                }
                std::string aname = str_replace_all(a->name(), "_", "|");
                std::string bname = str_replace_all(b->name(), "_", "|");
                if (desc) {
                    return stricmp(aname.c_str(), bname.c_str()) > 0;
                } else {
                    return stricmp(aname.c_str(), bname.c_str()) < 0;
                }
            });
            break;
        case SORT_DATE:
            Log_debug("set_current_dir_items SORT_DATE", 1);
            std::sort(_current_dir_items.begin(), _current_dir_items.end(), [desc](CLDirectoryItemData *a, CLDirectoryItemData *b) {
                if (desc) {
                    return a->mtime() > b->mtime();
                } else {
                    return a->mtime() < b->mtime();
                }
            });
            break;
        case SORT_SIZE:
            Log_debug("set_current_dir_items SORT_SIZE", 1);
            std::sort(_current_dir_items.begin(), _current_dir_items.end(), [desc](CLDirectoryItemData *a, CLDirectoryItemData *b) {
                if (desc) {
                    return (a->is_file() ? a->file_size() : a->dir_size()) > (b->is_file() ? b->file_size() : b->dir_size());
                } else {
                    return (a->is_file() ? a->file_size() : a->dir_size()) < (b->is_file() ? b->file_size() : b->dir_size());
                }
            });
            break;
        case SORT_FILETYPE:
            Log_debug("set_current_dir_items SORT_FILETYPE", 1);
            std::sort(_current_dir_items.begin(), _current_dir_items.end(), [desc](CLDirectoryItemData *a, CLDirectoryItemData *b) {
                if (desc) {
                    return stricmp(a->file_type_str().c_str(), b->file_type_str().c_str()) > 0;
                } else {
                    return stricmp(a->file_type_str().c_str(), b->file_type_str().c_str()) < 0;
                }
            });
            break;
    }
}

void CLDirectoryWindow::fast_refresh_current_view() {
    if (_operation_mode == OPERATION_MODE_DIR) {
        std::vector<CLDirectoryItemData *> sel_items = selected_items();

        _view->updates_enabled(false);
        _view->cleared();
        set_current_dir_items(_current_dir_item->childs());
        _view->inserted(0, _current_dir_items.size());
        _selection->clear();
        for(auto sel_item : sel_items) {
            for(int idx = 0; idx < _current_dir_items.size(); idx++) {
                if (_current_dir_items[idx] == sel_item) {
                    _selection->select(idx);
                }
            }
        }
        _view->updates_enabled(true);
        if (_view_mode == VIEW_MODE_BLOCKS) {
            ((CLBlocksView*)_view)->update_window_extent();
        }
    }
    _view->refresh();
}

void CLDirectoryWindow::refresh_current_view(bool keep_scroll, bool do_view_refresh) {
    int scroll_pos_y = 0;
    bool app_booted = false;
    if (keep_scroll) {
        scroll_pos_y = _win_main.scroll().y;
    }

    drag_stop(true);
    stop_rename_inplace();

    _menu_clicked_item_idx = -1;
//    _last_click_item_idx = -1;
//    _last_click_time = 0;
    _last_keypress_time = 0;

//    Log_debug("set_current_dir1 %s %p", path.c_str(), _view);
//    Log_debug("set_current_dir2 %s", path.c_str());
    _view->updates_enabled(false);

    _view->cleared();

    switch(_operation_mode) {
        case OPERATION_MODE_DIR:
            _current_dir_path.clear();
            if (_current_dir_item) {
                _current_dir_path = _current_dir_item->path().name();
                set_current_dir_items(_current_dir_item->childs());
                Log_debug("CLDirectoryWindow::refresh_current_view %s %d", _current_dir_path.c_str(), _current_dir_items.size());
            }
            _win_main.title(std::string("CLFiler: ") + _current_dir_path);
            break;
        case OPERATION_MODE_TIMELINE:
            switch(_timeline_filter) {
                case TIMELINE_FILTER_APPS:
                    _win_main.title("Timeline - Apps");
                    break;
                case TIMELINE_FILTER_FILES:
                    _win_main.title("Timeline - Files");
                    break;
                case TIMELINE_FILTER_FOLDERS:
                    _win_main.title("Timeline - Folders");
                    break;
            }
            break;
        case OPERATION_MODE_SEARCH:
            // title was set when mode changes and when text searched
//            _win_main.title("Search in: " + _current_dir_item->path().name());
            break;
    }

    if (!_current_dir_items.empty()) {
//        Log_debug("refresh_current_view0 %d", _current_dir_items.size());
        bool has_images = false;

        for(auto item : _current_dir_items) {
            if (item->is_application() && !item->app_booted()) {
                item->boot_app();
                app_booted = true;
            }
            if (CLImageFactory::can_load(item->file_type())) {
                has_images = true;
            }
        }

        if (_view_mode == VIEW_MODE_THUMBNAILS_MEDIUM || _view_mode == VIEW_MODE_THUMBNAILS_BIG) {
            tbx::view::ItemRenderer *renderer = ((CLTileView*)_view)->_item_renderer;
            if (has_images) {
                switch(_view_mode) {
                    case VIEW_MODE_THUMBNAILS_MEDIUM:
                        renderer = &_tile_thumbnail_medium_renderer;
                        break;
                    case VIEW_MODE_THUMBNAILS_BIG:
                        renderer = &_tile_thumbnail_big_renderer;
                        break;
                }
            } else {
                renderer = &_tile_thumbnail_small_renderer;
            }
            if (renderer != ((CLTileView*)_view)->_item_renderer) {
                ((CLTileView*)_view)->item_renderer(renderer);
            }
        }

        _view->inserted(0, _current_dir_items.size());

//        Log_debug("refresh_current_view0 inserted %d", _current_dir_items.size());
        if (main_view_mode() == VIEW_MODE_LIST) {
//            Log_debug("refresh_current_view size_column_to_width", 1);
            ((CLReportView *) _view)->size_column_to_width(0);
        } else if (main_view_mode() == VIEW_MODE_BLOCKS) {
            // pass
        } else {
//            Log_debug("refresh_current_view size_to_tiles", 1);
            ((tbx::view::TileView *)_view)->size_to_tiles();
        }
    }

    if (main_view_mode() == VIEW_MODE_THUMBNAILS) {
        if (!is_enough_memory((THUMBNAIL_IMAGE_WIDTH*THUMBNAIL_IMAGE_WIDTH*4 + sizeof(CLConvertedSpriteImage) + sizeof(CLDirectoryItemData) + 64) * _current_dir_items.size())) {
            CLImageCache::free_images_cache(500000);
        }
    } else {
        is_enough_memory((sizeof (CLDirectoryItemData) + sizeof(CLWimpSpriteImage) + 64) * _current_dir_items.size());
    }

//    Log_debug("refresh_current_view1", 1);
    _view->updates_enabled(true);
    _view->update_window_extent();

    if (do_view_refresh) {
        Log_debug("refresh_current_view _view->refresh this:%p", this);
        _view->refresh();
        _win_main.scroll(0, scroll_pos_y);
    }

    if (app_booted) {
        Log_debug("CLDirectoryWindow::refresh_current_view app_booted, rerefreshing", 1);
//        refresh_current_view_on_idle();
    }
    if (main_view_mode() == VIEW_MODE_THUMBNAILS && !_current_dir_items.empty()) {
//        Log_debug("Set thumbs size calculated",1);
        ((CLTileView *) _view)->thumbs_size_calculated();
    }
    Log_debug("refresh_current_view end", 1);
    //Log_debug("set_current_dir6 %s", path.c_str());
}

void CLDirectoryWindow::view_mode(int mode, bool save_and_refresh) {
    if (_view_mode != mode || _view_mode == VIEW_MODE_LIST) {
        Log_debug("CLDirectoryWindow::view_mode old:%d new:%d save_and_refresh:%d", _view_mode, mode, save_and_refresh);
        _view_mode = mode;
        delete _view;
        switch(_view_mode) {
            case VIEW_MODE_TILE_BIG: {
                //_view = new CLTileView(this, &_tile_big_icon_and_name_renderer);
                _view = new CLTileView(this,&_tile_big_icon_and_name_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Slarge");
                break;
            }
            case VIEW_MODE_TILE_SMALL: {
                _view = new CLTileView(this, &_tile_small_icon_and_name_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Ssmall");
                break;
            }
            case VIEW_MODE_LIST: {
                _view = new CLReportView(this);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Slist");
                break;
            }
            case VIEW_MODE_DIR_SIZES_BARS: {
                refresh_dir(false);
                _view = new CLReportView(this);
                sort_order(SORT_SIZE_DESC);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sbars");
                break;
            }
            case VIEW_MODE_BLOCKS: {
                refresh_dir(false);
                _view = new CLBlocksView(this);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sboxes");
                break;
            }
            case VIEW_MODE_THUMBNAILS_BIG: {
                _view = new CLTileView(this,&_tile_thumbnail_big_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sthn_large");
                break;
            }
            case VIEW_MODE_THUMBNAILS_MEDIUM: {
                _view = new CLTileView(this,&_tile_thumbnail_medium_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sthn_medium");
                break;
            }
            case VIEW_MODE_THUMBNAILS_SMALL: {
                _view = new CLTileView(this,&_tile_thumbnail_small_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sthn_small");
                break;
            }
            case VIEW_MODE_THUMBNAILS_TINY: {
                _view = new CLTileView(this,&_tile_thumbnail_tiny_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Sthn_tiny");
                break;
            }
            default:
                _view = new CLTileView(this,&_tile_big_icon_and_name_renderer);
                tbx::Button(_win_toolbar.gadget(0x14)).validation("R2;Slarge");
                break;
        }
        _selection->clear();
        _view->selection(_selection);
        _win_toolbar.force_redraw(_win_toolbar.extent());

        if (save_and_refresh) {
            if (_operation_mode == OPERATION_MODE_DIR) {
                g_app_state.view_mode = _view_mode;
                g_app_settings.set_value("Last", "view_mode", _view_mode);
            }
            if (_initialized) {
                Log_debug("view_mode refresh_current_view", 1);
                refresh_current_view(false, false);
                _win_main.scroll(0, 0);
                _view->refresh();
                Log_debug("view_mode refresh", 1);
            }
        }
    }
}

void CLDirectoryWindow::itemview_clicked(const tbx::view::ItemViewClickEvent &event) {
    int item_index = event.index();
    Log_debug("CLDirectoryWindow::itemview_clicked button:%x idx:%d _handling_click_event %d is_select:%d this:%p", event.click_event().button(), item_index, _handling_click_event, event.click_event().is_select(), this);
    if (_handling_click_event) {
        return;
    }
    _handling_click_event = true;
    _menu_clicked_item_idx = -1;
    os_t now = os_read_monotonic_time();

    drag_stop(true);
    stop_rename_inplace();

    if (item_index != event.NO_INDEX) {
        if (event.click_event().is_select_drag()) {
            if (!_selection->selected(item_index)) {
                _selection->set(item_index, item_index);
                _shift_selected_first_idx = item_index;
            }
            auto item = get_item(item_index);
            if (item) {
                drag_start(event.click_event(), item);
            }
        } else if (event.click_event().is_select_double() || event.click_event().is_adjust_double()) {
            auto item = get_item(item_index);
            int filetype = item->file_type();

            if (CLImageFactory::can_load(filetype) && event.click_event().is_select_double()) {
                new CLImageViewWindow(item, current_dir_items(), this);
            } else if ((g_app_config.OpenWindowClick == 1 && event.click_event().is_select_double())
                || (g_app_config.OpenWindowClick == 2 && event.click_event().is_adjust_double())) {
                if (item->is_file() || item->is_application()) {
                    fs_open_or_run(item_index);
                } else {
                    open_new(item->path(), this);
                }
            } else {
                fs_open_or_run(item_index);
            }
            if (_selection->selected(item_index)) {
                _selection->deselect(item_index);
            }
        } else if (event.click_event().is_menu()) {
            _menu_clicked_item_idx = item_index;
        } else if (event.click_event().is_adjust()) {
            if (is_shift_pressed() && _shift_selected_first_idx != -1) {
                for(int idx = std::min(_shift_selected_first_idx, item_index); idx <= std::max(_shift_selected_first_idx, item_index); idx++) {
                    _selection->select(idx);
                }
            }
            _shift_selected_first_idx = item_index;
        }
//    } else {
//        _last_click_time = 0;
//        _last_click_item_idx = event.NO_INDEX;
    }
    _handling_click_event = false;
    Log_debug("CLDirectoryWindow::itemview_clicked %d finished", item_index);
}

void CLDirectoryWindow::open_window(tbx::OpenWindowEvent &event) {
    tbx::BBox winb = _win_main.bounds();
//    Log_debug("CLDirectoryWindow::open_window %dx%d %d,%d %d,%d prev %dx%d win %d,%d %d,%d",
//                  event.visible_area().size().width, event.visible_area().size().height,
//                  event.visible_area().top_left().x, event.visible_area().top_left().y,
//                  event.visible_area().bottom_right().x, event.visible_area().bottom_right().y,
//                  _prev_win_size.width, _prev_win_size.height,
//                  winb.top_left().x, winb.top_left().y, winb.bottom_right().x, winb.bottom_right().y);
    if (!_handling_open_window_request && event.visible_area().size() != _prev_win_size) {
        _handling_open_window_request = true;
        tbx::Application::instance()->add_idle_command(&_open_window_request_listener);
        _idle_top_toolbar_scroller.reset_scroll();
    }
    if (!_caret_gained) {
        _win_main.focus();
    }
}

void CLDirectoryWindow::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
//    if (_view) {
//        g_my_app->yield();
//        _view->refresh();
//    }
}

void CLDirectoryWindow::key(tbx::KeyEvent &event) {
    int items_count = _view->count();
    CLDirectoryItemData *item;
    if (_selection->one()) {
        item = _current_dir_items[_selection->first()];
    } else {
        item = nullptr;
    }
    // read shift key state
    bool shift_pressed = is_shift_pressed();
    bool alt_pressed = is_alt_pressed();
    bool ctrl_pressed = is_ctrl_pressed();

    int key = event.key();
    Log_debug("CLDirectoryWindow::key key:%x modifier:%x shift:%d ctrl:%d alt:%d key:%d renaming idx:%d", event.key(), event.key_modifier(), shift_pressed, ctrl_pressed, alt_pressed, _renaming_item_idx);

    switch(key){
        case wimp_KEY_UP:
        case wimp_KEY_DOWN:
        case wimp_KEY_LEFT:
        case wimp_KEY_RIGHT: {
            if (items_count == 0) {
                return;
            }
            if (_selection->empty()) {
                _selection->select(0);
                _view->refresh();
            } else {
                unsigned int first_selected = _selection->first();
                switch(key) {
                    case wimp_KEY_UP:
                        if (first_selected > 0) {
                            if (main_view_mode() == VIEW_MODE_LIST) {
                                _selection->set(first_selected - 1, first_selected - 1);
                                scroll_to_first_selected(VIEW_SCROLL_TOP);
                                event.key_used();
                            } else {
                                int cols_per_row = ((CLTileView*)_view)->cols_per_row();
                                if (first_selected >= cols_per_row) {
                                    _selection->set(first_selected - cols_per_row, first_selected - cols_per_row);
                                    scroll_to_first_selected(VIEW_SCROLL_TOP);
                                    event.key_used();
                                }
                            }
                        }
                        break;
                    case wimp_KEY_DOWN:
                        if (first_selected < (items_count - 1)) {
                            if (main_view_mode() == VIEW_MODE_LIST) {
                                _selection->set(first_selected + 1, first_selected + 1);
                                scroll_to_first_selected(VIEW_SCROLL_BOTTOM);
                                event.key_used();
                            } else {
                                int cols_per_row = ((CLTileView*)_view)->cols_per_row();
                                if (first_selected + cols_per_row < items_count) {
                                    _selection->set(first_selected + cols_per_row, first_selected + cols_per_row);
                                    scroll_to_first_selected(VIEW_SCROLL_BOTTOM);
                                    event.key_used();
                                }
                            }
                        }
                        break;
                    case wimp_KEY_LEFT: {
                        if (main_view_mode() != VIEW_MODE_LIST) {
                            if (first_selected > 0) {
                                _selection->set(first_selected - 1, first_selected - 1);
                                scroll_to_first_selected(VIEW_SCROLL_TOP);
                                event.key_used();
                            }
                        }
                        break;
                    }
                    case wimp_KEY_RIGHT:
                        if (main_view_mode() != VIEW_MODE_LIST) {
                            if (first_selected < (items_count - 1)) {
                                _selection->set(first_selected + 1, first_selected + 1);
                                scroll_to_first_selected(VIEW_SCROLL_BOTTOM);
                                event.key_used();
                            }
                        }
                        break;
                }
            }
            break;
        }
        case wimp_KEY_RETURN:
            if (ctrl_pressed) {
                if (item) {
                    if (item->is_image() || item->is_directory() || item->is_application()) {
                        open_new_without_toolbars(item->path());
                    } else {
                        open_new_without_toolbars(_current_dir_path);
                    }
                } else {
                    open_new_without_toolbars(current_dir_path());
                }
                event.key_used();
            } else {
                if (item) {
                    fs_open_or_run(_selection->first());
                    event.key_used();
                }
            }
            break;

        case wimp_KEY_ESCAPE:
            stop_rename_inplace();
            break;

        case wimp_KEY_DELETE:
//            Log_debug("wimp_KEY_DELETE", 1);
            if (g_app_config.ClipboardKeys == 2) {
//                Log_debug("wimp_KEY_DELETE1", 1);
                set_action_items(selected_items());
                action_delete_items();
                event.key_used();
            }
            break;
        case ' ':
            if (_selection->any() && _selection->continuous() && _selection->last() + 1 < _current_dir_items.size()) {
                _selection->select(_selection->last()+1);
            }
            event.key_used();
            break;
        case 0x1: // Ctrl-A
            select_all();
            event.key_used();
            break;
        case 0x1a: // Ctrl-Z
            clear_selection();
            event.key_used();
            break;
        case 0x3: // Ctrl-C
            set_action_items(selected_items());
            action_copy_items();
            event.key_used();
            break;
        case 0x6: // Ctrl-F
            if (_operation_mode != OPERATION_MODE_SEARCH) {
                change_operation_mode(OPERATION_MODE_SEARCH);
            } else {
                change_operation_mode(OPERATION_MODE_DIR);
            }
            event.key_used();
            break;
        case 0x8: // Ctrl-H
            if (_operation_mode == OPERATION_MODE_DIR) {
                go_up_level();
                event.key_used();
            }
            break;
        case 0x9: // Ctrl-I
            if (item && CLImageFactory::can_load(item->file_type())) {
                new CLImageViewWindow(item, current_dir_items(), this);
                event.key_used();
            }
            break;
        case 0xb: // Ctrl-K
            if (g_app_config.ClipboardKeys == 1) {
                set_action_items(selected_items());
                action_delete_items();
                event.key_used();
            }
            break;
        case 0xe: // Ctrl-N
            if (item) {
                if (item->is_image() || item->is_directory() || item->is_application()) {
                    CLDirectoryWindow::open_new(item->path(), this);
                } else {
                    CLDirectoryWindow::open_new(_current_dir_path, this);
                }
            } else {
                CLDirectoryWindow::open_new(_current_dir_path, this);
            }
            event.key_used();
            break;
        case 0xf: // Ctrl-O
            g_app_data_model.fs_stamp(current_dir_path(), selected_item_names());
            break;
        case 0x12: // Ctrl-R
            if (_selection->count() == 1) {
                set_action_item(first_selected_item());
                start_rename_inplace();
                event.key_used();
            }
            break;
        case 0x13: { // Ctrl-S
            if (_selection->count() > 0) {
                set_action_items(selected_items());
                tbx::Window("WFileType").show_at_pointer(_win_main);
            }
            event.key_used();
            break;
        }
        case 0x14: // Ctrl-T
            toggle_toolbars();
            event.key_used();
            break;
        case 0x16: // Ctrl-V
            if (_operation_mode == OPERATION_MODE_DIR) {
                action_paste_items();
                event.key_used();
            }
            break;
        case 0x17: // Ctrl-W
            if (_operation_mode == OPERATION_MODE_DIR) {
                set_directory();
                event.key_used();
            }
            break;
        case 0x18: // Ctrl-X
            if (_operation_mode == OPERATION_MODE_DIR) {
                set_action_items(selected_items());
                action_cut_items();
                event.key_used();
            }
            break;
//        case 'T': // T
//        case 't': // t
//            if (shift_pressed) {
//                toggle_treeview();
//                event.key_used();
//            }
//            break;
        case 'C':
        case 'c':
            if (shift_pressed && first_selected_item()) {
                set_action_item(first_selected_item());
                tbx::Window("WCopyAs").show_at_pointer(_win_main);
                event.key_used();
            }
            break;
        case 'T':
        case 't':
            if (shift_pressed) {
                if (g_app_config.TreeviewItemsShown == 3) {
                    g_app_config.TreeviewItemsShown = 1;
                } else {
                    g_app_config.TreeviewItemsShown++;
                }
                treeview_full_update();
                event.key_used();
            }
            break;
        case 'S':
        case 's':
            if (shift_pressed) {
                for(auto &item : selected_items()) {
                    item->fs_set_filetype_by_ext();
                }
                event.key_used();
            }
            break;
        case 0x181: // F1
            if (can_run_file_type(0xadf)) {
                tbx::app()->os_cli("*Filer_Run <CLFiler$Dir>.!Help");
            } else {
                tbx::app()->os_cli("*Filer_Run <CLFiler$Dir>.!Help_text");
            }
            event.key_used();
            break;
        case 0x182: // F2
            if (_view_mode != VIEW_MODE_TILE_BIG) {
                view_mode(VIEW_MODE_TILE_BIG);
            }
            event.key_used();
            break;
        case 0x183: // F3
            if (_view_mode != VIEW_MODE_TILE_SMALL) {
                view_mode(VIEW_MODE_TILE_SMALL);
            }
            event.key_used();
            break;
        case 0x184: // F4
            if (_view_mode != VIEW_MODE_LIST) {
                view_mode(VIEW_MODE_LIST);
            }
            event.key_used();
            break;
        case 0x185: // F5
            if (_view_mode != VIEW_MODE_THUMBNAILS_BIG) {
                view_mode(VIEW_MODE_THUMBNAILS_BIG);
            }
            event.key_used();
            break;
        case 0x1ac: // Ctrl-Left
            go_backward();
            event.key_used();
            break;
        case 0x1ad: // Ctrl-Right
            go_forward();
            event.key_used();
            break;
        case 0x1af: // Ctrl-Up
            go_up_level();
            event.key_used();
            break;
        case 0x1cb: // F11
            refresh_dir(true);
            event.key_used();
            break;
    }
    if (!event.is_key_used() && !ctrl_pressed && !alt_pressed && key >= ' ' && key <= 126) {
        os_t _now  = os_read_monotonic_time();
        if ((_now - _last_keypress_time) > 100) {
            _search_by_prefix.clear();
            _last_keypress_time = _now;
            Log_debug("Key first", 1);
        }
        if ((_now - _last_keypress_time) < 70) {
            if (_search_by_prefix.empty() || _search_by_prefix[_search_by_prefix.size()-1] != ((char)key)) {
                _search_by_prefix.append((char*)&key);
            }
            _last_keypress_time = _now;
        }
        select_item_by_prefix();
    }
}

void CLDirectoryWindow::select_item_by_idx(int idx) {
    tbx::BBox item_bounds;
    tbx::WindowState state;
    _win_main.get_state(state);
    tbx::Point scrl = state.visible_area().scroll();
    _selection->set(idx,idx);
    _view->get_bounds(item_bounds, idx);
    if (item_bounds.max.y + item_bounds.height() + _margin.top > scrl.y) {
        scrl.y = item_bounds.max.y + item_bounds.height() +  _margin.top;
        _win_main.scroll(scrl);
    } else {
        int win_height = state.visible_area().bounds().height();
//        Log_debug("CLDirectoryWindow::select_item_by_idx max_y:%d h:%d winh:%d scy:%d calc:%d", item_bounds.max.y, item_bounds.height(), win_height, scrl.y, item_bounds.min.y + win_height + _margin.bottom);
        if (item_bounds.min.y + win_height - _margin.bottom < scrl.y) {
            scrl.y = item_bounds.min.y + win_height - _margin.bottom;
            _win_main.scroll(scrl);
        }
    }
}
void CLDirectoryWindow::select_item_by_prefix() {
    int prefix_len = _search_by_prefix.size();
    int i;
    const char *prefix_str = _search_by_prefix.c_str();
    if (!_selection->empty()) {
        for(i = _selection->first() + 1; i < _current_dir_items.size(); i++) {
            if (strnicmp(_current_dir_items[i]->name().c_str(), prefix_str, prefix_len) == 0) {
                select_item_by_idx(i);
                return;
            }
        }
    }
    for(i = 0 ; i < _current_dir_items.size(); i++) {
        if (strnicmp(_current_dir_items[i]->name().c_str(), prefix_str, prefix_len) == 0) {
            select_item_by_idx(i);
            return;
        }
    }
    _search_by_prefix_not_found = true;
    update_status_line();
}

void CLDirectoryWindow::set_action_item(CLDirectoryItemData* item) {
    _action_item_ids.clear();
    if (item) {
        _action_item_ids.push_back(item->id());
    }
}

void CLDirectoryWindow::set_action_items(const std::vector<CLDirectoryItemData*> &items) {
    _action_item_ids.clear();
    for(auto it: items) {
        _action_item_ids.push_back(it->id());
    }
}

std::vector<CLDirectoryItemData*> CLDirectoryWindow::get_action_items() {
    std::vector<CLDirectoryItemData*> items = g_app_data_model.find_items_by_ids(_action_item_ids);
    if (items.empty() && !_action_item_ids.empty()) {
        Log_error("CLDirectoryWindow::get_action_items items not found. Proably deleted, id[0]=", _action_item_ids[0]);
    }
    return items;
}

void CLDirectoryWindow::action_cut_items() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        g_app_state.selected_files = g_app_data_model.items_names(action_items);
        g_app_state.selected_dir = action_items[0]->parent()->path().name();
        g_app_state.selected_operation = SELECTED_OPERATION_CUT;
        std::string buf;
        if (action_items.size() > 1) {
            buf = g_my_app->messages().message("MovedManyIfPasteCommand", to_string((int) action_items.size()));
        } else {
            buf = g_my_app->messages().message("MovedIfPasteCommand", action_items[0]->display_name());
        }
        set_status_line(buf, true);
    }
}

void CLDirectoryWindow::action_copy_items() {
    std::string buf;
    auto action_items = get_action_items();
    if (action_items.empty()) {
        if (_current_dir_item && _current_dir_item->parent()) {
            g_app_state.selected_dir = _current_dir_item->parent()->path().name();
            g_app_state.selected_files = std::vector<std::string> {_current_dir_item->name()};
            g_app_state.selected_operation = SELECTED_OPERATION_COPY;
            buf = g_my_app->messages().message("CopyIfPasteCommand", _current_dir_item->display_name());
            set_status_line(buf, true);
        }
    } else {
        g_app_state.selected_files = g_app_data_model.items_names(action_items);
        g_app_state.selected_dir = action_items[0]->parent()->path().name();
        g_app_state.selected_operation = SELECTED_OPERATION_COPY;
        if (action_items.size() > 1) {
            buf = g_my_app->messages().message("CopyManyIfPasteCommand", to_string((int)action_items.size()));
        } else {
            buf = g_my_app->messages().message("CopyIfPasteCommand", action_items[0]->display_name());
        }
        set_status_line(buf, true);
    }
}

void CLDirectoryWindow::action_paste_items() {
    if (g_app_state.selected_operation != SELECTED_OPERATION_NONE && !g_app_state.selected_files.empty() && !_current_dir_path.empty()) {
        int flags = (g_app_state.selected_operation == SELECTED_OPERATION_COPY ? AppDataModel::FORCE_COPY : AppDataModel::FORCE_MOVE);
        g_app_data_model.fs_copy_or_move(g_app_state.selected_dir, g_app_state.selected_files, _current_dir_path, flags);
        if (flags == AppDataModel::FORCE_MOVE) {
            clear_selected_items();
        }
    }
}

void CLDirectoryWindow::action_delete_items() {
    std::string q;
    const std::vector<std::string> nofiles;
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        CLDirectoryItemData* first_item = action_items[0];
        if (action_items.size() == 1) {
//            char buf[200];
            if (first_item->file_type() < 0x1000) {
                q = g_my_app->messages().message("DeleteFileConfirm", first_item->name());
//                snprintf(buf, sizeof(buf) - 1, "Are you sure to delete file %s?", first_item->name().c_str());
            } else {
                q = g_my_app->messages().message("DeleteDirConfirm", first_item->name());
//                snprintf(buf, sizeof(buf) - 1, "Are you sure to delete directory %s?", first_item->name().c_str());
            }
            tbx::show_question(q, "Remove?", new FSRemoveConfirmCommand(first_item->id(), nofiles, _win_main.window_handle()), new SetFocusBackCommand(_win_main.window_handle()), true);
//            if (show_question(q.c_str(), g_my_app->messages().message("OKCancel").c_str()) == 3) {
//                first_item->fs_remove();
////                fs_remove_selected();
//            }
        } else {
            if (first_item->parent()) {
                char buf[200];
                snprintf(buf, sizeof(buf) - 1, "Are you sure to delete %ld selected items?", action_items.size());
                tbx::show_question(buf,"Remove?", new FSRemoveConfirmCommand(first_item->parent()->id(),  g_app_data_model.items_names(action_items), _win_main.window_handle()), new SetFocusBackCommand(_win_main.window_handle()), true);
//                if (show_question(buf, "OK,Cancel") == 3) {
//                    _current_dir_item->fs_remove_files(selected_item_names());
////                fs_remove_selected();
//                }
            }
        }
    } else {
        if (_current_dir_item && _current_dir_item->parent()) {
            q = g_my_app->messages().message("DeleteDirConfirm", _current_dir_item->name());
            tbx::show_question(q,"Remove?", new FSRemoveConfirmCommand(_current_dir_item->id(),  nofiles, _win_main.window_handle()), new SetFocusBackCommand(_win_main.window_handle()), true);
        }
    }
}

void CLDirectoryWindow::action_open_help() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        action_items[0]->fs_open_help();
    }
}

void CLDirectoryWindow::action_count() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        g_app_data_model.fs_count(action_items[0]->parent()->path().name(), g_app_data_model.items_names(action_items));
    }
}

void CLDirectoryWindow::action_info() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        new CLFileInfo(action_items[0]->path());
    }
}

void CLDirectoryWindow::action_stamp() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        g_app_data_model.fs_stamp(action_items[0]->parent()->path().name(), g_app_data_model.items_names(action_items));
    }
}

void CLDirectoryWindow::action_set_filetype_by_ext() {
    auto action_items = get_action_items();
    for(auto &item : action_items) {
        item->fs_set_filetype_by_ext();
    }
}

void CLDirectoryWindow::action_image_view() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        new CLImageViewWindow(action_items[0], action_items, this);
    }
}

void CLDirectoryWindow::action_set_dir() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        action_items[0]->fs_set_dir();
    }
}

void CLDirectoryWindow::action_open_new_window() {
    auto action_items = get_action_items();
    if (!action_items.empty()) {
        CLDirectoryWindow::open_new(action_items[0]->path(), this);
    }
}

void CLDirectoryWindow::clear_selected_items() {
    g_app_state.selected_files.clear();
    g_app_state.selected_operation = SELECTED_OPERATION_NONE;
    set_status_line("", true);
}

void CLDirectoryWindow::change_current_dir_without_history(CLDirectoryItemData* item) {
    if (_current_root != item->root()->name()) {
        change_root_dir(item->root()->name());
    }
    change_current_directory(item, true, true, false);
}

void CLDirectoryWindow::go_up_level() {
    if (_current_dir_item && _current_dir_item->parent()) {
        change_current_directory(_current_dir_item->parent(), true, true);
    }
}

void CLDirectoryWindow::go_backward() {
    if (_current_dir_item && !_backward_dir_items.empty()) {

        auto item_path = _backward_dir_items.back();
        _backward_dir_items.pop_back();
        CLDirectoryItemData *item = g_app_data_model.get_or_refresh_path(item_path);
        if (item) {
            _forward_dir_items.push_back(_current_dir_item->path().name());
            change_current_dir_without_history(item);
        }
    }
}

void CLDirectoryWindow::go_forward() {
    if (_current_dir_item && !_forward_dir_items.empty()) {
        auto item_path = _forward_dir_items.back();
        _forward_dir_items.pop_back();
        CLDirectoryItemData *item = g_app_data_model.get_or_refresh_path(item_path);
        if (item) {
            _backward_dir_items.push_back(_current_dir_item->path().name());
            change_current_dir_without_history(item);
        }
    }
}

void CLDirectoryWindow::set_status_line(const std::string& status, bool all_windows) {
    tbx::Button status_btn;
    if (all_windows) {
        for(auto dir_win : g_app_state.dir_windows) {
            dir_win->_status_message = status;
            dir_win->update_status_line();
        }
    } else {
        _status_message = status;
        update_status_line();
    }
}

void CLDirectoryWindow::scroll_to_first_selected(int scroll_top_or_bottom) {
    if (_selection->empty()) return;
    tbx::BBox item_bounds;
    tbx::WindowState state;
    _win_main.get_state(state);
    tbx::Point scrl = state.visible_area().scroll();

    _view->get_bounds(item_bounds, _selection->first());
    if (scroll_top_or_bottom == VIEW_SCROLL_TOP) {
//        Log_debug("CLDirectoryWindow::scroll_to_first_selected scy:%d itemb %d %d ", scrl.y, item_bounds.min.y, item_bounds.max.y);
        if (item_bounds.max.y + item_bounds.height() > scrl.y) {
            scrl.y = item_bounds.max.y + item_bounds.height();
            _win_main.scroll(scrl);
        }
    } else {
        int win_height = state.visible_area().bounds().height();
//        Log_debug("CLDirectoryWindow::scroll_to_first_selected scy:%d winh:%d itemb %d %d ", scrl.y, win_height, item_bounds.min.y, item_bounds.max.y);
        if (scrl.y - win_height > item_bounds.min.y) {
            scrl.y = item_bounds.min.y - item_bounds.height() + win_height;
            _win_main.scroll(scrl);
        }
    }
}

void CLDirectoryWindow::select_all() {
    if (_selection->type() != tbx::view::Selection::SINGLE) {
        _selection->clear();
        _selection->set(0, _current_dir_items.size() - 1);
    }
}

void CLDirectoryWindow::clear_selection() {
    _selection->clear();
}

void CLDirectoryWindow::load_and_set_current_directory_view_config() {
    int bgcolor = g_dir_settings.get_value(_current_dir_path, "bg_color", 0xffffffff);
    change_bg_color(bgcolor, false);

//    int vmode = g_dir_settings.get_value(_current_dir_path, "view_mode", _view_mode);
//    view_mode(vmode, false);

    unsigned int sorder = g_dir_settings.get_value(_current_dir_path, "sort_order", _sort_order);
    sort_order(sorder, false);
}

void CLDirectoryWindow::change_bg_color(unsigned int bg_color, bool save_and_refresh) {
    if (bg_color != _bg_color) {
        _bg_color = bg_color;
        if (_bg_color > 0 &&_bg_color <= 16) { // old legacy colors
            switch(_bg_color) {
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 10:
                case 11:
                case 13:
                    _fg_color = tbx::WimpColour::white;
                    break;
                default:
                    _fg_color = tbx::WimpColour::black;
                    break;
            }
        } else {
            tbx::Colour rgb = _bg_color;
            float luminance = 0.2126*rgb.red() + 0.7152*rgb.green() + 0.0722*rgb.blue();
            if (luminance < 140) {
                _fg_color = tbx::WimpColour::white;
            } else {
                _fg_color = tbx::WimpColour::black;
            }
        }

        Log_debug("CLDirectoryWindow::bg_color() path:%s bgcolor:%08x fgcolor:%d", _current_dir_path.c_str(), _bg_color, (int)_fg_color);
        if (save_and_refresh) {
            if (bg_color == 0xffffffff) {
                g_dir_settings.del_value(_current_dir_path, "bg_color");
                Log_debug("CLDirectoryWindow::change_bg_color(%d) path:%s (deleted) color_now:%d", bg_color, _current_dir_path.c_str(), _bg_color);
//        _toolbar_btn_select_color.background(0);
            } else {
                g_dir_settings.set_value(_current_dir_path, "bg_color", bg_color);
//        _toolbar_btn_select_color.background(bg_color);
                Log_debug("CLDirectoryWindow::change_bg_color(%08x) path:%s", bg_color, _current_dir_path.c_str());
            }
            Log_debug("change_bg_color _view->refresh", 1);
            _view->refresh();
        }
    }
}


class CLDirectoryWindowRunOnIdle : public tbx::Command {
    int _idx;
    CLDirectoryWindow* _me;
    bool need_refresh_after = false;
public:

    CLDirectoryWindowRunOnIdle(CLDirectoryWindow* me, int item_idx) : _me(me), _idx(item_idx) {};
    void execute() override {
        if (need_refresh_after) {
            Log_debug("CLDirectoryWindowRunOnIdle refresh", 1);
            _me->refresh_dir(false);
            need_refresh_after = false;
        } else {
            Log_debug("CLDirectoryWindowRunOnIdle start is_index_valid:%d idx:%d", _me->is_index_valid(_idx), _idx);
            auto item = _me->get_item(_idx);
            if (item) {
                Log_debug("CLDirectoryWindow::fs_open_or_run %x %s", item->file_type(), item->name().c_str());
                if (item->is_image() || item->is_directory()) {
                    _me->change_current_directory(item, true, true);
                } else {
                    if (is_shift_pressed() && item->is_application()) {
                        _me->change_current_directory(item, true, true);
                    } else {
                        item->fs_run();
                        g_timeline_model.append(item);
                        if (item->file_type() == 0xffc) { // util (need to refresh dir after run an util filetype)
                            need_refresh_after = true;
                        }
                    }
                }
            }
        }
        if (!need_refresh_after) {
            Log_debug("CLDirectoryWindowRunOnIdle remove idle command", 1);
            g_my_app->remove_idle_command(this);
            delete this;
        }
    }
};

void CLDirectoryWindow::fs_open_or_run(int idx) {
    g_my_app->add_idle_command(new CLDirectoryWindowRunOnIdle(this, idx));
}

void CLDirectoryWindow::fs_copy_move(const tbx::Path &src_dir, const std::vector<std::string> &src_files_vec, const tbx::Path &dst_dir) {
    const std::string &_src_dir = src_dir.name();
    const std::string &_dst_dir = dst_dir.name();
    int src_dot_pos = _src_dir.find('.');
    int dst_dot_pos = _dst_dir.find('.');

    if (g_app_config.CopyMoveConfirm == 1) {
        new CopyMoveConfirm(src_dir, src_files_vec, dst_dir);
    } else { // no confirm if shift or ctrl pressed
        bool shift_pressed = is_shift_pressed();
        bool ctrl_pressed = is_ctrl_pressed();
        if (shift_pressed || ctrl_pressed) {
            CLFilerAction *act = new CLFilerAction(_src_dir, src_files_vec);
            if (shift_pressed) {
                if (_dst_dir.substr(0, dst_dot_pos) == _src_dir.substr(0, src_dot_pos)) {
                    act->fs_rename(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                } else {
                    act->fs_move(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                }
            } else {
                act->fs_copy(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
            }
        } else {
            new CopyMoveConfirm(src_dir, src_files_vec, dst_dir);
        }
    }
}

void CLDirectoryWindow::start_rename_inplace() {
    auto action_items = get_action_items();
    if (action_items.empty()) {
        return;
    }
    int idx = find_item_index(action_items[0]);
    if (!is_index_valid(idx) || _renaming_item_idx == idx) {
        return;
    }
    stop_rename_inplace();
    _renaming_item_idx = idx;
    CLDirectoryItemData *item = _current_dir_items[idx];
    tbx::WindowState main_state;
    tbx::BBox item_bounds, inplace_screen_bounds;
    _win_main.get_state(main_state);
    _view->get_bounds(item_bounds, idx);
    switch(main_view_mode()) {
        case VIEW_MODE_TILE_BIG:
            item_bounds.max.y -= 82;
            item_bounds.min.x += 10;
            item_bounds.max.x -= 10;
            break;
        case VIEW_MODE_TILE_SMALL:
            item_bounds.min.x += 42;
            item_bounds.max.x -= 8;
            break;
        case VIEW_MODE_LIST:
            item_bounds.max.x = item_bounds.min.x + ((CLReportView*)_view)->column_width(0) - 10;
            item_bounds.min.x += 46;
            item_bounds.max.y -= 2;
            item_bounds.min.y += 2;
            break;
        case VIEW_MODE_THUMBNAILS:
            std::list<std::string> lines;
            switch(view_mode()) {
                case VIEW_MODE_THUMBNAILS_BIG:
                    item_bounds.max.y = item_bounds.min.y + ThumbnailRenderer::text_split_and_get_height(item, item_bounds.width() - THUMBNAIL_ITEM_CELL_PADDING*2, lines) + THUMBNAIL_ITEM_CELL_PADDING*2;
                    item_bounds.min.y += 4;
                    item_bounds.min.x += 10;
                    item_bounds.max.x -= 8;
                    break;
                case VIEW_MODE_THUMBNAILS_MEDIUM:
                    item_bounds.max.y = item_bounds.min.y + ThumbnailRenderer::text_split_and_get_height(item, item_bounds.width() - THUMBNAIL_ITEM_CELL_PADDING*4, lines) + THUMBNAIL_ITEM_CELL_PADDING*2;
                    item_bounds.min.y += 4;
                    item_bounds.min.x += 10;
                    item_bounds.max.x -= 8;
                    break;
                case VIEW_MODE_THUMBNAILS_SMALL:
                    item_bounds.max.y = item_bounds.min.y + ThumbnailRenderer::text_split_and_get_height(item, item_bounds.width() - THUMBNAIL_ITEM_CELL_PADDING*2, lines) + THUMBNAIL_ITEM_CELL_PADDING*2;
                    item_bounds.min.y += 4;
                    item_bounds.min.x += 10;
                    item_bounds.max.x -= 8;
                    break;
                case VIEW_MODE_THUMBNAILS_TINY:
                    item_bounds.max.y = item_bounds.min.y + ThumbnailRenderer::text_split_and_get_height(item, item_bounds.width() - THUMBNAIL_ITEM_CELL_PADDING*2, lines) + THUMBNAIL_ITEM_CELL_PADDING*2;
                    item_bounds.min.y += 2;
                    item_bounds.min.x += 10;
                    item_bounds.max.x -= 8;
                    break;
            }
            break;
    }
    inplace_screen_bounds = main_state.visible_area().screen(item_bounds);

    tbx::ShowSubWindowSpec inplace_subwin_opts;
    inplace_subwin_opts.wimp_parent = _win_main.window_handle();
    inplace_subwin_opts.wimp_window = -1;
    inplace_subwin_opts.visible_area.bounds() = inplace_screen_bounds;

    inplace_subwin_opts.flags = 0
//                                |tbx::ALIGN_LEFT_VISIBLE_LEFT
//                                | tbx::ALIGN_BOTTOM_VISIBLE_BOTTOM
//                                | tbx::ALIGN_RIGHT_VISIBLE_RIGHT
//                                | tbx::ALIGN_TOP_VISIBLE_TOP
                                ;
    _win_inplace.extent(tbx::BBox(0, -inplace_screen_bounds.size().height, inplace_screen_bounds.size().width, 0));
    tbx::BBox writable_bounds = tbx::BBox(0,-item_bounds.height(), item_bounds.width(), 0);
    tbx::WritableField fld = _win_inplace.gadget(0);
    fld.bounds(writable_bounds);
    fld.text(_current_dir_items[idx]->name());
    _win_inplace.show_as_subwindow(inplace_subwin_opts);
    g_my_app->yield();
    fld.focus();

    Log_debug("CLDirectoryWindow::start_rename_inplace idx:%d bounds:%d,%d,%d,%d %dx%d wb:%d,%d,%d,%d",
            _renaming_item_idx, item_bounds.min.x, item_bounds.max.y, item_bounds.max.x, item_bounds.min.y, item_bounds.size().width, item_bounds.size().height,
            writable_bounds.min.x, writable_bounds.max.y, writable_bounds.max.x, writable_bounds.min.y);
}

void CLDirectoryWindow::stop_rename_inplace() {
    if (_renaming_item_idx >= 0) {
        Log_debug("CLDirectoryWindow::stop_rename_inplace idx:%d", _renaming_item_idx);
        _win_inplace.hide();
        _win_main.focus();
        _renaming_item_idx = -1;
    }
}


bool CLDirectoryWindow::accept_file(tbx::LoadEvent &event) {
    CLDirectoryItemData *dst_item = get_drop_dest_item(event);
    if (!dst_item) {
        _loading_dropped_dest_path = "";
        _loading_dropped_file_name = "";
        Log_debug("CLDirectoryWindow::accept_file not accepted as dst_item==nullptrd", 1);
        return false;
    } else {
        Log_debug("CLDirectoryWindow::accept_file from_filer:%d %s", event.from_filer(), event.file_name().c_str());
        _loading_dropped_file_name = event.file_name();
        _loading_dropped_dest_path = dst_item->path().name();
        return true;
    }
}

std::string CLDirectoryWindow::destination_save_path(tbx::LoadEvent &event) {
    CLDirectoryItemData *dst_item = get_drop_dest_item(event);
    std::string dst;
    if (dst_item) {
        dst = dst_item->path().name() + "." + event.file_name();
    }
    Log_debug("CLDirectoryWindow::destination_save_path from_filer:%d %s dst:%s", event.from_filer(), event.file_name().c_str(), dst.c_str());
    return dst;
}

CLDirectoryItemData * CLDirectoryWindow::get_drop_dest_item(tbx::LoadEvent &event) {
    CLDirectoryItemData *dst_item = nullptr;
    if (event.destination_object() == _treeview_underlying_window) {
        tbx::ext::TreeNodeId node_id = _treeview.find_node(event.destination_point().x, event.destination_point().y);
        if (node_id) {
            dst_item = get_item_for_treeview_node_id(node_id);
        }
    } else {
        if (_operation_mode == OPERATION_MODE_DIR) {
            dst_item = _current_dir_item;
        }
    }
    return dst_item;
}

bool CLDirectoryWindow::load_file(tbx::LoadEvent &event) {
    Log_debug("CLDirectoryWindow::load_file from_filer:%d %s", event.from_filer(), event.file_name().c_str());

    CLDirectoryItemData *dst_path_item = g_app_data_model.find_item_by_path(_loading_dropped_dest_path);

    if (!dst_path_item) {
        Log_error("CLDirectoryWindow::load_file dst_path_item not found", 1);
        return true;
    }

    if (event.file_name().find("<Wimp$Scrap>") == std::string::npos) {
//        if (_loading_files.empty()) {
//            auto do_copy_action = [this]() {
//            };
//            Log_debug("CLDirectoryWindow::load_file g_idle_task.run_at_next_idle(do_copy_action)", 1);
//            g_idle_task.run_at_next_idle(do_copy_action);
//        }
        tbx::Path loaded_file_path = event.file_name();
        if (loaded_file_path.parent().name() == _loading_dropped_dest_path) {
            Log_debug("CLDirectoryWindow::load_file the loaded_file is already present %s in %s", event.file_name().c_str(), _loading_dropped_dest_path.c_str());
            g_app_data_model.refresh_on_idle(dst_path_item, 0, CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
            return true;
        }

        Log_debug("CLDirectoryWindow::load_file append %s", event.file_name().c_str());
        _loading_files.push_back(event.file_name());

        if (_loading_files.size() > 1) { // wait for all files come in next LoadEvents
            return true;
        }
        g_my_app->yield();

        Log_debug("CLDirectoryWindow::load_file after yield", 1);

        std::vector<std::string> files;
        tbx::Path current_fpath = tbx::Path(_loading_files[0]).parent();
        for (auto fname: _loading_files) {
            tbx::Path fpath = tbx::Path(fname);
            if (current_fpath.name() != fpath.parent().name()) {
                g_app_data_model.fs_copy(current_fpath, files, _loading_dropped_dest_path);
                Log_debug("CLDirectoryWindow::load_file do copy (current_fpath_dir changed) %s to %s",
                              files[0].c_str(), _loading_dropped_dest_path.c_str());
                files.clear();
                current_fpath = fpath.parent();
            }
            files.push_back(fpath.leaf_name());
        }
        Log_debug("CLDirectoryWindow::load_file do copy %s to %s", files[0].c_str(),
                  _loading_dropped_dest_path.c_str());
        _loading_files.clear();
        g_app_data_model.fs_copy(current_fpath, files, _loading_dropped_dest_path);

    } else {
        std::string dst = _loading_dropped_dest_path + "." + _loading_dropped_file_name;
        copy_file("<Wimp$Scrap>", dst.c_str());
        Log_debug("CLDirectoryWindow::load_file save %s to %s", event.file_name().c_str(),
                      dst.c_str());
        g_app_data_model.refresh_on_idle(dst_path_item, 0, CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
    }
    if (_loading_dropped_dest_path != _current_dir_path) {
        change_current_directory(dst_path_item, true, true);
    }
    return true;
}

void CLDirectoryWindow::lose_caret(tbx::CaretEvent &event) {
    _caret_gained = false;
}

class CLTreeViewSelectNodeCmd : public tbx::Command {
    tbx::ext::TreeNodeId _node_id;
    tbx::ext::TreeView _treeview;
public:
    CLTreeViewSelectNodeCmd(tbx::ext::TreeNodeId node_id, tbx::ext::TreeView treeview) : _node_id(node_id), _treeview(treeview) {};

    void execute() override {
        g_my_app->remove_idle_command(this);
        Log_debug("CLTreeViewSelectNodeCmd select node %x", _node_id);
        _treeview.current_node(_node_id).select(true, false);
        delete this;
    }
};

void CLDirectoryWindow::mouse_click(tbx::MouseClickEvent &event) {
    if (!_handling_click_event) {
        Log_debug("CLDirectoryWindow::mouse_click caret:%d", _caret_gained);
        if (!_caret_gained) {
            _win_main.focus();
            _caret_gained = true;
        }
    }
    if (event.id_block().self_object() == _treeview_underlying_window) {
        Log_info("TreeviewClickListener event btn:%x is drag: %d", event.button(), event.is_select_drag());
        CLDirectoryItemData* item = nullptr;

        if (event.is_select_drag() && _selected_node_id) {
            item = get_item_for_treeview_node_id(_selected_node_id);
            if (item) {
                drag_start(event, item);
            } else {
                Log_debug("TreeviewClickListener directory item not found for node_id = %d", _selected_node_id);
            }
//        } else if (event.is_select() && !_dragged_item_id && !is_shift_pressed()) {
//            tbx::ext::TreeNodeId node_id = _treeview.find_node(event.x(), event.y());
//            if (node_id) {
//                g_my_app->add_idle_command(new CLTreeViewSelectNodeCmd(node_id, _treeview));
//            } else {
//                Log_debug("TreeviewClickListener click_select but node not found at x:%d, y:%d", event.x(), event.y());
//            }
        }
    }
}

void CLDirectoryWindow::gain_caret(tbx::CaretEvent &event) {
    _caret_gained = true;
}

CLDirectoryWindow* CLDirectoryWindow::from_window(tbx::Window win) {
    if (win.null()) throw std::invalid_argument("Attempting to get CLDirectoryWindow from null toolbox handle");
    CLDirectoryWindow *mw = reinterpret_cast<CLDirectoryWindow *>(win.client_handle());
    if (mw == 0) throw std::invalid_argument("CLDirectoryWindow has 0 client handle");
    return mw;

}

CLDirectoryItemData* CLDirectoryWindow::first_selected_item() {
    if (!_selection->empty()) {
        return _current_dir_items[_selection->first()];
    }
    return nullptr;
}


void CLDirectoryWindow::treeview_node_dragged(const tbx::ext::TreeViewNodeDraggedEvent &event) {
    CLDirectoryItemData * drag_item = get_item_for_treeview_node_id(event.node());
    if (event.outside()) {
        _drag_src_win = _treeview_underlying_window;
        _dragged_item_id = drag_item->id();
        drag_finished(tbx::BBox{0,0,0,0});
    } else {

    }
}


void CLDirectoryWindow::drag_start(const tbx::MouseClickEvent& event, CLDirectoryItemData* item) {
    drag_stop(true);
    _drop_item_idx = -1;

    if (event.id_block().self_object() == _win_main) {
        auto idx = find_item_index(item);
        if (idx != -1) {
            _selection->select(idx);
        }
        _drag_src_win = _win_main;
    } else if (event.id_block().self_object() == _treeview_underlying_window) {
        _drag_src_win = _treeview_underlying_window;
    } else {
        Log_info("CLDirectoryWindow::drag_start but unknown drag source window", 1);
        return;
    }

    tbx::WimpSprite sprite = item->sprite_name();
    tbx::Size sprite_size = sprite.size();
    tbx::BBox start_box(event.point(), sprite_size);
    start_box.move(-sprite_size.width/2, -sprite_size.height/2);

    _dragged_item_id = item->id();
    _drag_src_win.drag_sprite(sprite, start_box, this, tbx::Window::DSFLAG_DROP_SHADOW);
    g_my_app->add_timer(5, this);

    Log_debug("CLDirectoryWindow::drag_start id=%d name=%s", _dragged_item_id, item->name().c_str());
}

void CLDirectoryWindow::drag_stop_cleanup() {
    Log_debug("CLDirectoryWindow::drag_stop_cleanup _drop_item_idx:%d", _drop_item_idx);
    if (_drop_item_idx > -1) {
        tbx::BBox box;
        _view->get_bounds(box, _drop_item_idx);
        _drop_item_idx = -1;
        _win_main.force_redraw(box);
    }
    if (_drop_tree_current_node_id) {
        _treeview.show_current_node(false);
        _drop_tree_current_node_id = 0;
    }
}

void CLDirectoryWindow::drag_stop(bool canceled) {
    if (_dragged_item_id) {
        Log_debug("CLDirectoryWindow::drag_stop %d", _dragged_item_id);
        g_my_app->remove_timer(this);
        _dragged_item_id = 0;
        if (canceled) {
            _drag_src_win.cancel_drag();
        }
        for(auto dirwin : g_app_state.dir_windows) {
            dirwin->drag_stop_cleanup();
        }
    }
}

void CLDirectoryWindow::drag_finished(const tbx::BBox &final) {
    if (!_dragged_item_id) {
        Log_debug("CLDirectoryWindow::drag_finished but _dragged_item_id == 0", 1);
        return;
    }
    std::vector<CLDirectoryItemData*> src_items;

    src_items.push_back(g_app_data_model.find_item_by_id(_dragged_item_id));
    if (src_items[0] == nullptr) {
        Log_error("CLDirectoryWindow::drag_finished but source item id=%d not found", _dragged_item_id);
        drag_stop();
        return;
    }
    drag_stop();
    Log_debug("CLDirectoryWindow::drag_finished dropped item:%s", src_items[0]->name().c_str());
    if (_drag_src_win == _win_main) {
        if (_selection->any()) {
            for(tbx::view::Selection::Iterator it = _selection->begin(); it != _selection->end(); ++it) {
                if (src_items[0] != _current_dir_items[*it]) {
                    src_items.push_back(_current_dir_items[*it]);
                }
            }
        }
    } else if (_drag_src_win == _treeview_underlying_window) {
        // pass
    } else {
        Log_info("CLDirectoryWindow::drag_finished but unknown drag source window", 1);
        return;
    }
    Log_debug("CLDirectoryWindow::drag_finished dropped items cnt:%d", src_items.size());


    tbx::PointerInfo where(true,false);

    auto where_window_handle = where.window_handle();
    std::string error;
    std::string current_dir_path = first_selected_item()->parent()->path().name();
    std::vector<std::string> src_files_vec;


    for(auto dirwin : g_app_state.dir_windows) { // copy/move to same/other windows
        Log_debug("CLDirectoryWindow::drag_finished h1:%x h2:%x this_h:%x dir_win_ptr:%p this_pts:%p wir_win_items_cnt:%d", where_window_handle, dirwin->_win_main.window_handle(), _win_main.window_handle(), dirwin, this, dirwin->_current_dir_items.size());
        if (where_window_handle == dirwin->_treeview_underlying_window.window_handle()) {
            tbx::ext::TreeNodeId node_id = dirwin->_treeview.find_node(where.mouse_x(), where.mouse_y());
            if (node_id) {
                CLDirectoryItemData* dst_item = dirwin->get_item_for_treeview_node_id(node_id);
                Log_debug("CLDirectoryWindow::drag_finished in the other win treeview node_id:%x", node_id);
                if (dst_item && dst_item->path().name() != src_items[0]->path().name()) {
                    Log_debug("CLDirectoryWindow::drag_finished in the other win treeview dst:%s", dst_item->path().name().c_str());
                    for(auto &src_item : src_items) {
                        if (!dst_item->is_file() && !src_item->is_file() && (dst_item->id() == src_item->id() ||
                                src_item->has_child(dst_item->id()))) {
                            tbx::show_message("A directory can not be moved or copied onto itself");
                            Logger::info("CLDirectoryWindow::drag_finished, can't save to self, return");
                            return;
                        }

                        src_files_vec.push_back(src_item->name());
                        Log_debug("Drop to other win treeview dir (copy/move) %s -> %s", src_item->name().c_str(), dst_item->path().name().c_str());
                    }
                    Log_debug("Drop to other win treeview dir (copy/move) src_cnt:%d -> %s", src_files_vec.size(), dst_item->path().name().c_str());
                    fs_copy_move(src_items[0]->path().parent(), src_files_vec, dst_item->path());
                }
            }
            return;
        }

        if (where_window_handle == dirwin->_win_main.window_handle()) {
            int drop_item_idx = dirwin->_view->screen_index(tbx::Point(where.mouse_x(), where.mouse_y()));
            Log_debug("CLDirectoryWindow::drag_finished in the other/same window drop_item_idx:%d", drop_item_idx);
            CLDirectoryItemData* dst_item = nullptr;
            if (drop_item_idx != -1) {
                dst_item = dirwin->_current_dir_items[drop_item_idx];
                if (dst_item->is_file()) {
                    dst_item = nullptr;
                }
            }
            if (dst_item) {
                Log_debug("Drop to window win:%p this:%p cnt:%d", dirwin, this, drop_item_idx, dirwin->_current_dir_items.size());
                Log_debug("Drop to window win:%p this:%p idx:%d id:%ld dst:%s", dirwin, this, drop_item_idx, dst_item->id(), dst_item->name().c_str());
//            std::string dst_name = _current_dir_items[drop_item_idx]->name();
                for(auto &src_item : src_items) {
                    if (!dst_item->is_file() && !src_item->is_file() && (dst_item->id() == src_item->id() ||
                            src_item->has_child(dst_item->id()))) {
                        tbx::show_message("A directory can not be moved or copied onto itself");
                        Logger::info("CLDirectoryWindow::drag_finished, can't save to self, return");
                        return;
                    }
                    src_files_vec.push_back(src_item->name());
                }
                if (dst_item->file_type() == tbx::FILE_TYPE_DIRECTORY || dst_item->file_type() == tbx::FILE_TYPE_APPLICATION) {
                    Log_debug("Drop to same window (copy/move) idx:%d -> %s", drop_item_idx, dst_item->path().name().c_str());
                    fs_copy_move(src_items[0]->path().parent(), src_files_vec, dst_item->path());
                    //fs_copy_or_move(_current_dir, src_files, dst, op_mode);
                } else {
                    Logger::error("Drop to same window (dst is file, copy to dst window) idx:%d id:%ld dst:%s", drop_item_idx, dst_item->id(), dst_item->path().name().c_str());
                    fs_copy_move(src_items[0]->path().parent(), src_files_vec, dirwin->_current_dir_path);
                }
                return;
            } else {
                CLDirectoryItemData* dst_item = dirwin->_current_dir_item;
                for(auto &src_item : src_items) {
                    if (src_item->parent() == dst_item) {
                        Logger::info("CLDirectoryWindow::drag_finished, drop to same directory, return");
                        return;
                    }
                    if (!dst_item->is_file() && !src_item->is_file() && (dst_item->id() == src_item->id() ||
                                                                         src_item->has_child(dst_item->id()))) {
                        tbx::show_message("A directory can not be moved or copied onto itself");
                        Logger::info("CLDirectoryWindow::drag_finished, can't save to self, return");
                        return;
                    }
                    src_files_vec.push_back(src_item->name());
                }
                fs_copy_move(src_items[0]->path().parent(), src_files_vec, dirwin->_current_dir_path);
                Log_debug("CLDirectoryWindow::drag_finished copy/move to own window %s->%s", src_items[0]->path().parent().name().c_str(), dirwin->_current_dir_path.c_str());
                return;
            }
        }
    }

    Log_debug("CLDirectoryWindow::drag_finished to other app", 1);
    for(auto &src_item : src_items) {
        char *task_name;
        std::string task_name_str;
        tbx::Saver saver;
        tbx::WimpMessage msg(0, 4);
        CLSaverSaveToFileHandler* handler;
        int task_handle;
        msg.your_ref(0);
        task_handle = msg.send(tbx::WimpMessage::Acknowledge, where.window_handle(), where.icon_handle());
        xtaskmanager_task_name_from_handle(reinterpret_cast<wimp_t>(task_handle), &task_name);
        task_name_str = task_name;
        for (auto & c: task_name_str) c = tolower(c);
        handler = new CLSaverSaveToFileHandler(this, src_item->path());
        saver.set_finished_handler(handler);
        if (task_name_str == "filer") {
            Log_debug("CLDirectoryWindow::drag_finished save to filer (%s) %s %x %d", task_name,
                          src_item->name().c_str(), src_item->file_type(), src_item->file_size());
            saver.set_save_to_file_handler(handler);
            saver.save(where, src_item->name(), src_item->file_type(), (src_item->is_file() ? src_item->file_size() : -1), false);
        } else if (task_name_str.find("pinboard") != std::string::npos) {
                Log_debug("CLDirectoryWindow::drag_finished save to Pinboard (%s) %s %x %d", task_name, src_item->name().c_str(), src_item->file_type(), src_item->file_size());

                tbx::WimpMessage filer_selection_msg(0x407, 60);
                filer_selection_msg[5] = where.mouse_x() - 94;
                filer_selection_msg[6] = where.mouse_y() - 62;
                filer_selection_msg[7] = where.mouse_x() + 94;
                filer_selection_msg[8] = where.mouse_y() + 62;
                filer_selection_msg[9] = 188;
                filer_selection_msg[10] = 124;
                filer_selection_msg[11] = 0;
                filer_selection_msg[12] = 2;
                filer_selection_msg[13] = 4;
                filer_selection_msg[14] = 2;
                filer_selection_msg[15] = 4;
                filer_selection_msg.your_ref(0);
                filer_selection_msg.send(tbx::WimpMessage::User, where.window_handle(), where.icon_handle());

                saver.save(where, src_item->path().name(), src_item->file_type(), (src_item->is_file() ? src_item->file_size() : -1), true);
        } else {
            Log_debug("CLDirectoryWindow::drag_finished save to app (%s) %s %x %d", task_name, src_item->name().c_str(), src_item->file_type(), src_item->file_size());
            saver.save(where, src_item->path().name(), src_item->file_type(), (src_item->is_file() ? src_item->file_size() : -1), true);
        }
        Log_debug("CLDirectoryWindow::drag_finished save %s", src_item->name().c_str());
    }
}

void CLDirectoryWindow::timer_handler(const tbx::PointerInfo& where) {
    tbx::BBox box;
    auto where_window_handle = where.window_handle();
    int old_drop_item_idx = _drop_item_idx;

    if (where_window_handle == _treeview_underlying_window.window_handle()) {
        tbx::ext::TreeNodeId node_id = _treeview.find_node(where.mouse_x(), where.mouse_y());
        if (_drop_tree_current_node_id != node_id) {
            if (node_id) {
                if (!_drop_tree_current_node_id) {
                    _treeview.show_current_node(true);
                }
                _treeview.current_node(node_id);
            } else {
                _treeview.show_current_node(false);
            }
            _drop_tree_current_node_id = node_id;
        }
        return;
    } else {
        if (_drop_tree_current_node_id) {
            _treeview.show_current_node(false);
            _drop_tree_current_node_id = 0;
        }
    }

    //Log_debug("timer win_handle %x tree handle %x node %x", where_window_handle, _treeview_underlying_window.window_handle(), _drop_tree_current_node_id);
    if (where_window_handle == _win_main.window_handle()) {
        tbx::Point pos = tbx::Point(where.mouse_x(), where.mouse_y());
        _drop_item_idx = _view->screen_index(pos);
        if (_drop_item_idx != -1 && is_index_valid(_drop_item_idx) && _current_dir_items[_drop_item_idx]->is_file()) {
            _drop_item_idx = -1;
        }
//        Log_debug("CLDirectoryWindow::timer redraw this:%p _drop_item_idx %d old_drop_item_idx %d %d,%d", this, _drop_item_idx, old_drop_item_idx, pos.x, pos.y);
    } else {
        _drop_item_idx = -1;
    }
    if (old_drop_item_idx != _drop_item_idx) {
        if (old_drop_item_idx >= 0) {
            Log_debug("CLDirectoryWindow::timer this:%p redraw old %d", this, old_drop_item_idx);
            _view->get_bounds(box, old_drop_item_idx);
            _win_main.force_redraw(box);
        }

        if (_drop_item_idx >= 0) {
            Log_debug("CLDirectoryWindow::timer this:%p redraw new %d", this, _drop_item_idx);
            _view->get_bounds(box, _drop_item_idx);
            _win_main.force_redraw(box);
        }
    }
}

void CLDirectoryWindow::timer(unsigned int elapsed) {
    tbx::PointerInfo where(true,false);

    for(auto dirwin : g_app_state.dir_windows) {
        dirwin->timer_handler(where);
    }
}

void CLSaverSaveToFileHandler::saver_save_to_file(tbx::Saver saver, std::string dst_file_name) {
    Log_debug("CLSaverSaveToFileHandler::saver_save_to_file %s -> %s", _src_file.name().c_str(), dst_file_name.c_str());
    tbx::Path dst = dst_file_name;
    if (_src_file.leaf_name() == dst.leaf_name()) {
        std::vector<std::string> f_vec;
        f_vec.push_back(_src_file.leaf_name());
        g_app_data_model.fs_copy(_src_file.parent(), f_vec, dst.parent());
        Log_debug("CLSaverSaveToFileHandler::saver_save_to_file via FilerAction src:%s dst:%s ", _src_file.name().c_str(),dst_file_name.c_str());
    } else {
        if (!_src_file.directory() && strcasecmp(dst_file_name.substr(0,12).c_str(), "<Wimp$Scrap>") == 0) {
            std::string scrap_path = tbx::Path::canonicalise("<Wimp$Scrap>");
            CLUtils::remove_recursive(scrap_path);
            copy_file(_src_file.name().c_str(), scrap_path.c_str());
            saver.file_save_completed(true, "<Wimp$Scrap>");
            Log_debug("CLSaverSaveToFileHandler::saver_save_to_file copy src:%s to Wimp$Scrap %s", _src_file.name().c_str(), scrap_path.c_str());
        }
//        if (!src.directory()) {
//            src.copy(dst_file_name, tbx::Path::COPY_RECURSE | tbx::Path::COPY_VERBOSE);
////            copy_file(_src_file_name.c_str(), dst_file_name.c_str());
//            if (dst_file_name == "<Wimp$Scrap>") {
//                saver.file_save_completed(true, dst_file_name + "." + src.leaf_name());
//            } else {
//                saver.file_save_completed(true, dst_file_name);
//            }
//            Log_debug("CLSaverSaveToFileHandler::saver_save_to_file via copy src:%s dst:%s ", _src_file_name.c_str(),dst_file_name.c_str());
//        } else {
//            Logger::error("CLSaverSaveToFileHandler::saver_save_to_file attempt to copy dirtectory to file (ignored) src:%s dst:%s ", _src_file_name.c_str(),dst_file_name.c_str());
//        }
    }
}

void CLSaverSaveToFileHandler::saver_finished(const tbx::SaverFinishedEvent &finished) {
    Log_debug("CLSaverSaveToFileHandler::saver_finished %s done:%d", _src_file.name().c_str(), finished.save_done());
    delete this;
}

CLBgColorMenu::CLBgColorMenu(tbx::Object obj) {
    tbx::ColourMenu mnu = obj;
    mnu.add_selection_listener(this);
    mnu.add_about_to_be_shown_listener(this);
}

void CLBgColorMenu::colourmenu_selection(tbx::ColourMenu colour_menu, tbx::WimpColour colour) {
    CLDirectoryWindow *me = CLDirectoryWindow::from_window(colour_menu.ancestor_object());
    me->change_bg_color(colour);
}

void CLBgColorMenu::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    CLDirectoryWindow *me = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    tbx::ColourMenu(event.id_block().self_object()).colour((int)me->bg_color());
}

CLBgColorDialog::CLBgColorDialog(tbx::Object obj) {
    dbox = obj;
    dbox.add_colour_selected_listener(this);
    dbox.add_about_to_be_shown_listener(this);
    Log_debug("CLBgColorDialog", 1);
}

void CLBgColorDialog::about_to_be_shown(tbx::AboutToBeShownEvent &event) {
    me = CLDirectoryWindow::from_window(event.id_block().ancestor_object());
    Log_debug("CLBgColorDialog::about_to_be_shown %08x", me->bg_color());
    dbox.colour(me->bg_color());
}

void CLBgColorDialog::colour_selected(const tbx::ColourSelectedEvent &event) {
    Log_debug("CLBgColorDialog::colour_selected %08x", ((event.red() << 24) | (event.green() << 16) | (event.blue() << 8)));
    if (event.none()) {
        me->change_bg_color(tbx::Colour::no_colour);
    } else {
        me->change_bg_color(((event.blue() << 24) | (event.green() << 16) | (event.red() << 8)));
    }
}

bool CLDirectoryWindow::change_root_dir(const std::string &new_root) {
    Log_debug("CLDirectoryWindow::change_root_dir Change root to: [%s]", new_root.c_str());
    if (_operation_mode != OPERATION_MODE_DIR) {
        change_operation_mode(OPERATION_MODE_DIR);
    }

    CLDirectoryItemData* root_item = g_app_data_model.get_existing_root_item(new_root);

    if (!root_item) {
        tbx::show_message("Drive "+new_root+" does not exist");
        return false;
    }

    tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node();
    Log_debug("current_node=%x root_node=%x", current_node.node_id(), _treeview_root_node_id);
    _treeview.auto_update(false);
    _treeview.notify_expansion(false);
    _treeview.notify_selection(false);
    if (_treeview_root_node_id) { // erase old
        _treeview.clear();
        Log_debug("CLDirectoryWindow::change_root_dir clear all", 1);
//        current_node.move_to(_treeview_root_node_id);
//        Log_debug("CLDirectoryWindow::change_root_dir move to", 1);
//        current_node.select(true, false);
//        current_node.expand(false, true);
//        current_node.erase();
//        Log_debug("CLDirectoryWindow::change_root_dir erased", 1);
    }
    tbx::StringSet(_win_toolbar.gadget(TOOLBAR_STRINGSET_ROOT)).selected(root_item->name());

    Log_debug("CLDirectoryWindow::change_root_dir got root item ptr:%p", root_item);
    _current_root = root_item->name();
    _treeview_root_node_id = current_node.add_child(root_item->name());
    current_node.private_word((void*)root_item->id());

    Log_debug("CLDirectoryWindow::change_root_dir root item id()=%p node_id=%x childs=%d", root_item->id(), _treeview_root_node_id, root_item->childs().size());

    if (!root_item->is_cache_loaded()) {
        g_app_data_model.load_diritem_cache(root_item, 0);
    }
    if (g_app_state.needs_refresh_root || !root_item->is_cache_loaded()) {
        Log_debug("CLDirectoryWindow::change_root_dir root do deep refresh",1);
        root_item->refresh_deep(4, CLREFRESH_INITIAL_REFRESH_ITEM | CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
        g_app_state.needs_refresh_root = false;
    } else {
        root_item->refresh(CLREFRESH_INITIAL_REFRESH_ITEM | CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
    }
    Log_debug("after refresh",1);

//    tbx::SpriteArea *area = make_treeview_sprite(root_item->sprite());
//    current_node.sprite((int)tbx::Application::instance()->sprite_area()->pointer(), "harddisc", "harddisc");
//    current_node.sprite(1, "harddisc", "harddisc");
    if (root_item->has_subdirectories()) {
        current_node.add_child("(Empty)");
        current_node.private_word(0);
    }
    _treeview.auto_update(true);
    _treeview.notify_expansion(true);
    _treeview.notify_selection(true);
    _treeview.update_display();

//    Log_debug("CLDirectoryWindow::change_root_dir canonical_root_dir=%s", new_root.c_str());
//    if (changedir) {
//        change_current_directory(root_item->id(), true, true);
//    }
//    current_node.move_to(_treeview_root_node_id);
    return true;
}

void CLDirectoryWindow::treeview_make_node(CLDirectoryItemData* dir_item, bool create_child) {
    tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node();
    if (create_child) {
        current_node.add_child(dir_item->name());
    } else {
        current_node.add_sibling(dir_item->name());
    }
    current_node.private_word((void*)dir_item->id());
    assign_node_sprite(current_node, dir_item);
//    Log_debug("CLDirectoryWindow::treeview_make_node new dir added %s node_id:%p priv_word:%p create_child:%d", dir_item->name().c_str(), current_node.node_id(), current_node.private_word(), create_child);
//
//        current_node.sprite(1, "small_dir", "small_diro");
    if (dir_item->has_subdirectories()) {
        auto prev_node_id = current_node.node_id();
        current_node.add_child("(Empty)");
        current_node.private_word(0);
        current_node.move_to(prev_node_id);
    }
}

void CLDirectoryWindow::treeview_remove_node(CLDirectoryItemData* dir_item) {
    tbx::ext::TreeNodeId node_id = treeview_set_current_node_for_item(dir_item, false, false);
    if (node_id) {
        _treeview.current_node().erase();
    }
}

void CLDirectoryWindow::treeview_update_node_contents(tbx::ext::TreeNodeId node_id) {
    tbx::ext::TreeNodeId initial_node_id = node_id;
    tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node(node_id);
    CLDirectoryItemData* current_item = get_item_for_treeview_node_id(node_id);
    bool treeview_changed = false;
    if (!current_item) {
        Logger::error("CLDirectoryWindow::treeview_update_node_contents CLDirectoryItemData* not found for node_id:%p item_id:%p (node deleted)", node_id, current_node.private_word());
        current_node.erase();
        return;
    }

    std::set<unsigned int> exists_in_treeview;
    _treeview.auto_update(false);
    // update childs (add child of child) with new found directories and delete treeview entries if directory not found on fs (was deleted on fs)
    std::vector<tbx::ext::TreeNodeId> nodes_to_delete;
    if (current_node.move_child()) {
//        Log_debug("CLDirectoryWindow::treeview_update_node_contents current_node.move_child() %s %x", current_node.text().c_str(), current_node.node_id());
        do {
            CLDirectoryItemData* child = current_item->find_child_by_id((unsigned int)current_node.private_word());
            if (child) { // dir found in fs (update and delete from map)
                if ((child->file_type() == tbx::FILE_TYPE_DIRECTORY
                   || g_app_config.TreeviewItemsShown == 2 && child->is_image())
                   || (g_app_config.TreeviewItemsShown == 3 && (child->is_image() || child->file_type() == tbx::FILE_TYPE_APPLICATION))) {
                    if (child->has_subdirectories() && !current_node.has_child()) { // dir on three view without child but on fs with child
                        auto prev_node_id = current_node.node_id();
                        current_node.add_child("(Empty)");
                        current_node.private_word(0);
                        current_node.move_to(prev_node_id);
                        treeview_changed = true;
                        Log_debug("CLDirectoryWindow::treeview_update_node_contents new subdir '..' added as child %s", child->name().c_str());
                    }
                    exists_in_treeview.insert(child->id());
                } else {
                    nodes_to_delete.push_back(current_node.node_id());
                }
//                Log_debug("CLDirectoryWindow::treeview_update_node_contents current_node dir found on fs and erased from map %s", current_node.text().c_str());
            } else {
                nodes_to_delete.push_back(current_node.node_id());
                Log_debug("CLDirectoryWindow::treeview_update_node_contents current_node dir not found on fs and erased from treeview %s", current_node.text().c_str());
            }
        } while(current_node.move_next());
    }

    if (!nodes_to_delete.empty()) {
        for(auto node_id: nodes_to_delete) {
            current_node.move_to(node_id);
            Log_debug("CLDirectoryWindow::treeview_update_node_contents node erased %s", current_node.text().c_str());
            current_node.erase();
        }
        treeview_changed = true;
    }
    current_node.move_to(initial_node_id);

    // add all missing new directories from directories_on_fs map
    bool create_child = true;
    for(auto child : current_item->childs()) {
        if (exists_in_treeview.find(child->id()) != exists_in_treeview.end()) {
            continue;
        }
//        if (child->file_type() != tbx::FILE_TYPE_DIRECTORY) {
//            continue;
//        }
        if (child->file_type() == tbx::FILE_TYPE_DIRECTORY
            || (g_app_config.TreeviewItemsShown == 2 && child->is_image())
            || (g_app_config.TreeviewItemsShown == 3 && (child->is_image() || child->file_type() == tbx::FILE_TYPE_APPLICATION))) {
            treeview_make_node(child, create_child);

            if (create_child) {
                create_child = false;
            }
            treeview_changed = true;
        }
    }
    _treeview.auto_update(true);

    if (treeview_changed) {
        _treeview.update_display();
    }
}

void CLDirectoryWindow::treeview_delete_empty_child() {
    tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node();
    tbx::ext::TreeNodeId initial_node_id = current_node.node_id();
    if (current_node.move_child()) {
//        Log_debug("CLDirectoryWindow::treeview_update_node_contents current_node.move_child() %s %x", current_node.text().c_str(), current_node.node_id());
        do {
            if ((unsigned int)current_node.private_word() == 0) {
                current_node.erase();
                break;
            }
        } while(current_node.move_next());
    }
    current_node.move_to(initial_node_id);
}

CLDirectoryItemData* CLDirectoryWindow::get_item_for_treeview_node_id(tbx::ext::TreeNodeId node_id) {
    tbx::ext::TreeViewCurrentNode node = _treeview.current_node(node_id);
    return g_app_data_model.find_item_by_id((unsigned int)node.private_word());
}

tbx::ext::TreeNodeId CLDirectoryWindow::treeview_set_current_node_for_item(CLDirectoryItemData* dir_item, bool select_node, bool expand_node) {
    if (!_treeview_root_node_id) {
        return 0;
    }
    int ids_path_depth = 0, current_depth = 0;
    std::list<unsigned int> ids_path;
    CLDirectoryItemData *item = dir_item;
    while(item) {
        ids_path.push_front(item->id());
        item = item->parent();
        ids_path_depth++;
    }
    tbx::ext::TreeViewCurrentNode node = _treeview.current_node();
    tbx::ext::TreeNodeId prev_node_id = node.node_id();
    node.move_to(_treeview_root_node_id);
    if (node.private_word() != (void*)ids_path.front()) {
        Logger::error("CLDirectoryWindow::get_treeview_node_id_for_item root node not found %ld != %ld [%s]", node.private_word(), (void*)ids_path.front(), dir_item->path().name().c_str());
        return 0;
    }
    for(auto id : ids_path) {
        bool found = false;
        current_depth++;
        do {
            if ((void*)id == node.private_word()) {
                if (current_depth == ids_path_depth) {
                    if (select_node) {
                        _treeview.notify_selection(false);
                        if (!node.expanded()) {
                            _treeview.notify_expansion(false);
                            tbx::ext::TreeNodeId found_node_id = node.node_id();
                            while (node.node_id() != _treeview_root_node_id) {
                                node.move_parent();
                                if (node.expanded()) {
                                    break;
                                } else {
                                    node.expand(true, false);
                                }
                            }
                            node.move_to(found_node_id);
                            _treeview.notify_expansion(true);
                        }
                        node.select(true, false);
                        _treeview.notify_selection(true);
                        if (expand_node) {
                            node.expand(true, false);
                        }
                    }
//                    Log_debug("CLDirectoryWindow::treeview_set_current_node_for_item %s found:%x %s", dir_item->name().c_str(), node.node_id(), node.text().c_str());
                    return node.node_id();
                } else {
                    node.move_child();
                    found = true;
                    break;
                }
            }
        } while(node.move_next());
        if (!found) {
//            Logger::warn("CLDirectoryWindow::get_treeview_node_id_for_item node not found for id %ld [%s]", id, dir_item->name().c_str());
            goto node_not_found;
        }
    }
node_not_found:
    node.move_to(prev_node_id);
    return 0;
}

void CLDirectoryWindow::change_current_directory(unsigned int current_dir_item_id, bool select_treeview_node, bool do_view_refresh) {
    CLDirectoryItemData *item = g_app_data_model.find_item_by_id(current_dir_item_id);
    if (item) {
        change_current_directory(item, select_treeview_node, do_view_refresh);
    } else {
        Logger::info("Change current dir to root [%s] as item id: %ld is removed", _current_root.c_str(), current_dir_item_id);
        change_root_dir(_current_root);
    }
}

void CLDirectoryWindow::change_current_directory(std::string &dpath) {
    CLDirectoryItemData *item = g_app_data_model.get_or_refresh_path(dpath);
    if (item) {
        change_current_directory(item, true, true, true);
    } else {
        std::vector<std::string> parts;
        split(dpath, parts, '.');
        std::string root_dir = parts[0];
        CLDirectoryItemData *item = g_app_data_model.find_item_by_path(root_dir);
        if (item) {
            Logger::error("change_current_directory path:%s not found will change current dir to root: %s", dpath.c_str(), root_dir.c_str());
            change_current_directory(item, true, true, true);
        } else {
            Logger::crit("change_current_directory path:%s not found and root:%s not found also, do  nothing", dpath.c_str(), root_dir.c_str());
        }
    }
}

int CLDirectoryWindow::find_item_index(CLDirectoryItemData *item) {
    for(int i =  0; i < _current_dir_items.size(); i++) {
        if (_current_dir_items[i] == item) {
            return i;
        }
    }
    return -1;
}

void CLDirectoryWindow::change_current_directory(CLDirectoryItemData *dir_item, bool select_treeview_node, bool do_view_refresh, bool with_history) {
    _shift_selected_first_idx = -1;
    tbx::Path existing_dir = g_app_data_model.find_real_existing_dir_or_parent(dir_item->path());
    if (existing_dir.name() != dir_item->path().name()) {
        dir_item = g_app_data_model.find_item_by_path(existing_dir.name());
        Logger::error("CLDirectoryWindow::change_current_directory dir:%s not exists, use dir:%s instead dir_item:%p", dir_item->path().name().c_str(), existing_dir.name().c_str(), dir_item);
    }

    if (dir_item) {
        if (main_view_mode() == VIEW_MODE_THUMBNAILS) {
//            Log_debug("init_thumbs_size",1);
            ((CLTileView *) _view)->init_thumbs_size();
        }

        _refreshing_at_change_dir = true;

        if (!dir_item->is_cache_loaded()) {
            g_app_data_model.load_diritem_cache(dir_item, 0);
        }
        bool is_root = (dir_item->parent() == nullptr);
        tbx::ext::TreeViewCurrentNode node;
        if (with_history && _current_dir_item && _operation_mode == OPERATION_MODE_DIR) {
            _backward_dir_items.push_back(_current_dir_item->path().name());
            if (_backward_dir_items.size() > 10) {
                _backward_dir_items.erase(_backward_dir_items.begin());
            }
            std::string back_hist_str;
            bool first_item = true;
            for(auto _item_path : _backward_dir_items) {
                CLDirectoryItemData *_it = g_app_data_model.find_item_by_path(_item_path);
                if (_it) {
                    if (first_item) {
                        first_item = false;
                    } else {
                        back_hist_str.append(",");
                    }
                    back_hist_str.append(_it->path().name());
                }
            }
            g_app_settings.set_value("Last","Backward", back_hist_str);
//            g_app_settings.set_value("Last", "Dir", dir_item->path().name());
            _forward_dir_items.clear();
        }

        _current_dir_item = dir_item;
        _current_dir_path = dir_item->path().name();

        if (_operation_mode != OPERATION_MODE_DIR) {
            change_operation_mode(OPERATION_MODE_DIR);
            return;
        }

        _win_toolbar.gadget(TOOLBAR_BTN_FORW_ID).fade(_forward_dir_items.empty());
        _win_toolbar.gadget(TOOLBAR_BTN_BACK_ID).fade(_backward_dir_items.empty());
        _win_toolbar.gadget(TOOLBAR_BTN_UP_ID).fade(is_root);

        Log_debug("CLDirectoryWindow::change_current_directory: %s items=%d", _current_dir_path.c_str(), _current_dir_item->childs().size());

        dir_item->refresh(0);

//        for(auto child : dir_item->childs()) {
//            if (child->is_application() && !child->app_booted()) {
//                child->boot_app();
//            }
//        }
        Log_debug("CLDirectoryWindow::change_current_directory refresh_current_view %s childs=%d", dir_item->name().c_str(), dir_item->childs().size());

        load_and_set_current_directory_view_config();

//        if (_view_mode == VIEW_MODE_BLOCKS) {
//            _current_dir_item->calc_fake_dir_size();
//        }

        refresh_current_view(false, do_view_refresh);

        tbx::ext::TreeNodeId current_node_id = treeview_set_current_node_for_item(dir_item, select_treeview_node, false);
        if (current_node_id) {
            treeview_update_node_contents(current_node_id);
            node = _treeview.current_node(current_node_id);
            if (!dir_item->parent() && dir_item->has_subdirectories()) { // expand root dir
                _treeview.notify_expansion(false);
                node.expand(true, false);
                _treeview.notify_expansion(true);
            }
            if (dir_item->parent()) {
                node.make_visible();
            }
            Log_debug("CLDirectoryWindow::change_current_directory node found for: %s items=%d", dir_item->name().c_str(), dir_item->childs().size());
        } else {
            auto path_item = dir_item;
            std::vector<CLDirectoryItemData*> items_path;
            tbx::ext::TreeNodeId found_node_id = 0;
            _treeview.notify_selection(false);
            _treeview.notify_expansion(false);
            while(path_item) {
                if (found_node_id) {
                    break;
                }
                items_path.push_back(path_item);
                path_item = path_item->parent();
                found_node_id = treeview_set_current_node_for_item(path_item, false, false);
            }
            if (found_node_id) {
                tbx::ext::TreeNodeId  prev_found_node_id = found_node_id;
                Log_debug("CLDirectoryWindow::change_current_directory found node_id for: %s", path_item->name().c_str());
                treeview_update_node_contents(found_node_id);
                for(int i = items_path.size() - 1; i >= 0; i--) {
                    found_node_id = treeview_set_current_node_for_item(items_path[i], false, true);
                    if (!found_node_id) {
                        _treeview.current_node(prev_found_node_id);
//                        treeview_delete_empty_child();
                        treeview_make_node(items_path[i], true);
                        node = _treeview.current_node();
                        found_node_id = node.node_id();
                        node.move_parent();
//                        node.expand(true, false);
                    }
                    treeview_update_node_contents(found_node_id);
                    prev_found_node_id = found_node_id;
                }

                node = _treeview.current_node(found_node_id);
                while(node.node_id() != _treeview_root_node_id) {
                    node.expand(true, false);
                    if (!node.move_parent()) {
                        break;
                    }
                }
                node.expand(true, false);
                node = _treeview.current_node(found_node_id);
                node.select(select_treeview_node, false);
                node.make_visible();

            } else {
                Logger::warn("CLDirectoryWindow::change_current_directory root node not found for %s", dir_item->name().c_str());
            }
        }
        _treeview.notify_selection(true);
        _treeview.notify_expansion(true);

        // Stupid RISCOS have bug that app icon is not painted correctly immediately after app !boot so do double refresh
//        if (!dir_item->was_displayed()) {
//            dir_item->was_displayed(true);
//            tbx::Application::instance()->yield();
////            _view->refresh();
//        }

        refresh_dir(false);

        g_timeline_model.append(_current_dir_item);

        on_favorites_changed();

        _status_message = "";
        _search_by_prefix_not_found = false;
        update_status_line();

        _refreshing_at_change_dir = false;

//        if (_thumb_sizes_changed) {
//            save_cached_thumbnails_sizes();
//        }
    } else {
        Logger::error("CLDirectoryWindow::change_current_directory dir dir_item not found");
    }
    Log_debug("CLDirectoryWindow::change_current_directory exit childs=%d", _current_dir_item->childs().size());
}

void CLDirectoryWindow::treeview_node_selected(const tbx::ext::TreeViewNodeSelectedEvent &event) {
    if (event.selected()) {
        tbx::ext::TreeNodeId node_id = event.node();
        tbx::ext::TreeViewCurrentNode node = _treeview.current_node(node_id);
        _selected_node_id = node_id;
        Log_debug("CLDirectoryWindow::treeview_node_selected set _selected_node_id=%d", node_id);
        if (node.private_word() != 0) {
            CLDirectoryItemData* dir_item = get_item_for_treeview_node_id(node_id);
            if (dir_item) {
                if (!node.expanded()) {
                    _treeview.notify_expansion(false);
                    node.expand(true, false);
                    _treeview.notify_expansion(true);
                }
                change_current_directory(dir_item, false, true);
            } else {
                Log_error("CLDirectoryWindow::treeview_node_selected dir_item not found for node: %s", node.text().c_str());
            }
        } else {
            Log_error("CLDirectoryWindow::treeview_node_selected private_word = 0 text:%s",
                          node.text().c_str());
        }
    }
}

void CLDirectoryWindow::treeview_node_expanded(const tbx::ext::TreeViewNodeExpandedEvent &event) {
    if (event.expanded()) {
        Log_debug("CLDirectoryWindow::treeview_node_expanded %x", event.node());
        CLDirectoryItemData* dir_item = get_item_for_treeview_node_id(event.node());
        if (!dir_item->is_cache_loaded()) {
            g_app_data_model.load_diritem_cache(dir_item);
        }
        dir_item->refresh(0);
        treeview_update_node_contents(event.node());
        //g_app_data_model.refresh_on_idle(dir_item, 0, CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
    } else {
        // collapse recursive
        tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node(event.node());
        _treeview.notify_expansion(false);
        current_node.expand(false, true);
        _treeview.notify_expansion(true);
    }
}

void CLDirectoryWindow::treeview_full_update() {
    if (_current_dir_item) {
        change_root_dir(_current_dir_item->root()->name());
        change_current_directory(_current_dir_item, true, true);
    }
}

void CLDirectoryWindow::on_dir_item_added(CLDirectoryItemData *item) {
    if (_operation_mode == OPERATION_MODE_DIR) {

////        if (!item->is_file()) {

        if (item->file_type() == tbx::FILE_TYPE_DIRECTORY
            || (g_app_config.TreeviewItemsShown == 2 && item->is_image())
            || (g_app_config.TreeviewItemsShown == 3 && (item->is_image() || item->file_type() == tbx::FILE_TYPE_APPLICATION))) {
            tbx::ext::TreeNodeId parent_node_id = treeview_set_current_node_for_item(item->parent(), false, false);
            auto cur_node = _treeview.current_node();
            bool parent_expanded = cur_node.expanded();
            if (parent_node_id && (parent_expanded || cur_node.selected() || !cur_node.has_child())) {
                treeview_make_node(item, true);
                Log_debug("CLDirectoryWindow::on_dir_item_added treeview_make_node item=%s",
                              item->name().c_str());
            }
            if (parent_node_id == _treeview_root_node_id && !parent_expanded) {
                _treeview.notify_expansion(false);
                cur_node = _treeview.current_node(parent_node_id);
                cur_node.expand(true);
                _treeview.notify_expansion(true);
            }
        }

        if (item->parent() == _current_dir_item && !_refreshing_at_change_dir) {
            refresh_current_view_on_idle();
        }
    }
}

void CLDirectoryWindow::on_dir_item_removed(CLDirectoryItemData *item) {
    if (_operation_mode == OPERATION_MODE_DIR) {
        if (item->file_type() == tbx::FILE_TYPE_DIRECTORY
            || (g_app_config.TreeviewItemsShown == 2 && item->is_image())
            || (g_app_config.TreeviewItemsShown == 3 && (item->is_image() || item->file_type() == tbx::FILE_TYPE_APPLICATION))) {
            treeview_remove_node(item);
        }
        if (item == _current_dir_item) {
            Logger::info("CLDirectoryWindow::on_dir_item_removed current dir %s (changed to root)",
                         _current_dir_path.c_str());
            change_current_directory(item->root());
        } else if (item->parent() == _current_dir_item) {
            Logger::info("CLDirectoryWindow::on_dir_item_removed. remove item, clear selection, refresh_current_view_on_idle");
            if (!_refreshing_at_change_dir) {
                int item_idx = find_item_index(item);
                if (item_idx >= 0) {
                    _view->updates_enabled(false);
                    _view->removed(item_idx, 1);
                    _current_dir_items.erase(_current_dir_items.begin() + item_idx);
                }
                if (!_selection->empty()) {
                    for(tbx::view::Selection::Iterator it = _selection->begin(); it != _selection->end(); ++it) {
                        if (_current_dir_items[*it] == item) {
                            _selection->clear();
                            break;
                        }
                    }
                }
                if (is_idle_refresh_pending()) {
                    return;
                }
                refresh_current_view_on_idle();
            }
        }
        if (!item->is_file()) {
            treeview_remove_node(item);
        }
    } else {
        if (!_current_dir_items.empty()) {
            for(int i =  _current_dir_items.size() - 1; i >= 0; i--) {
                if (_current_dir_items[i] == item) {
                    _view->removing(i, 1);
                    _current_dir_items.erase(_current_dir_items.begin() + i);
                    _view->removed(i, 1);
//                    refresh_current_view_on_idle();
                    break;
                }
            }
            if (!item->is_file()) {
                _search_model->remove_search_item(item);
            }
        }
    }
}

void CLDirectoryWindow::on_dir_item_updated(CLDirectoryItemData *item, int what) {
    if (item) {
//        Log_debug("CLDirectoryWindow::on_dir_item_updated %s what:%d dirwin:%p", item->name().c_str(), what, this);
//        if (item == _current_dir_item || (_selection->count() == 1 && item == first_selected_item())) {
//            update_status_line();
//        }
        if (what == CLUPDATED_NAME) {
            tbx::ext::TreeViewCurrentNode current_node = _treeview.current_node();
            tbx::ext::TreeNodeId node_id = treeview_set_current_node_for_item(item, false, false);
            if (node_id) {
                if (current_node.text() != item->name()) {
                    current_node.text(item->name());
                }
            }
        }
        if (!_refreshing_at_change_dir) {
            if (what >= CLUPDATED_TYPE || _view_mode == VIEW_MODE_LIST || (what == CLUPDATED_THUMBNAIL && main_view_mode() == VIEW_MODE_THUMBNAILS)) {
                if (item->parent() == _current_dir_item) {
                    int item_idx = find_item_index(item);
                    if (item_idx >= 0) {
                        if (main_view_mode() == VIEW_MODE_THUMBNAILS) {
                            tbx::BBox bounds;
                            ((CLTileView*)_view)->get_bounds(bounds, item_idx);
                            _win_main.force_redraw(bounds);
                        } else {
                            _view->changed(item_idx, 1);
                        }
//                    Log_debug("Update changed item idx:%d %s", item_idx, item->name().c_str());
                    }
                }
            } else {
                if (what == CLUPDATED_SIZE && item == _current_dir_item && (_view_mode == VIEW_MODE_DIR_SIZES_BARS || _view_mode == VIEW_MODE_BLOCKS || _view_mode == VIEW_MODE_LIST)) {
                    refresh_current_view_on_idle(300, true);
                }
            }
            if (what != CLUPDATED_THUMBNAIL && _selection->one() && _current_dir_items[_selection->first()] == item) {
                update_status_line();
            }
        }
    }
}

void CLDirectoryWindow::refresh_roots_widget() {
    auto root_items = g_app_data_model.get_root_items();
    std::string roots_str;
    for(auto item: root_items) {
        if (!roots_str.empty()) {
            roots_str.append(",");
        }
        roots_str.append(item->name());
    }
    tbx::StringSet roots_stringset = _win_toolbar.gadget(TOOLBAR_STRINGSET_ROOT);
    roots_stringset.available(roots_str);
    if (!_current_root.empty()) {
        roots_stringset.selected(_current_root);
    }
}

void CLDirectoryWindow::on_root_item_added(CLDirectoryItemData* item) {
    refresh_roots_widget();
}

void CLDirectoryWindow::on_root_item_removed(CLDirectoryItemData* item) {
    CLDirectoryItemData *current_root_item = get_item_for_treeview_node_id(_treeview_root_node_id);
    if (current_root_item == item) {
        auto root_items = g_app_data_model.get_root_items();
        Logger::info("Change current root from %s to %s as previous root is removed", item->name().c_str(), root_items[0]->name().c_str());
        change_root_dir(root_items[0]->name());
    }
    refresh_roots_widget();
}

void CLDirectoryWindow::toggle_toolbars(int value) {
    if (value >= 0) {
        _toolbars_visible = value;
    } else {
        _toolbars_visible -= 1;
    }
    if (_toolbars_visible < 0) {
        _toolbars_visible = TOOLBARS_VISIBLE_TOP_BOTTOM_TREEVIEW;
    }
    setup_subwindows();
    g_app_choices.set_OpenNewWindow(_toolbars_visible);
}

//void CLDirectoryWindow::toggle_top_toolbar() {
//    if (_top_toolbar_visible) {
//        _top_toolbar_visible = false;
//        setup_subwindows();
//    } else {
//        _top_toolbar_visible = true;
//        setup_subwindows();
//    }
//    g_app_choices.set_OpenNewWindow(_top_toolbar_visible, _treeview_visible);
//}

void CLDirectoryWindow::on_search_state_callback(unsigned int state, CLDirectoryItemData* item) {
//    Log_debug("CLDirectoryWindow::on_search_state_callback state=%d", state);
    switch(state) {
        case SearchState::FOUND: {
            _current_dir_items.push_back(item);
            if (_current_dir_items.size() == 1) {
                Log_debug("on_search_state_callback refresh_current_view", 1);
                refresh_current_view(false, true);
            } else {
                _view->inserted(_current_dir_items.size() - 1, 1);
            }
            break;
        }
        case SearchState::SEARCHING: {
            update_status_line();
            break;
        }
        case SearchState::STOPPED:
            tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(true);
            Log_debug("on_search_state_callback refresh_current_view", 1);
            refresh_current_view(false, true);
            update_status_line();
            break;
        case SearchState::FINISHED: {
            tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(true);
            Log_debug("on_search_state_callback refresh_current_view", 1);
            refresh_current_view(false, true);
            update_status_line();
            break;
        }
        default:
            break;
    }
}

void CLDirectoryWindow::change_operation_mode(int mode) {
    tbx::view::Selection *selection;
    int move_right_offset = 200/*, timeline_move_top_offset = 130*/, i;
    int move_right_gadgets_ids[] = {0x7, 0x8, 0xf, 0x10, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21};
    int timeline_move_top_gadgets_ids[] = {0x23, 0x24, 0x25};
    int timeline_disable_ids[] = {0x7, 0xf, 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c};
    if (mode == _operation_mode) {
        return;
    }

    if (_operation_mode == OPERATION_MODE_TIMELINE) {
//        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).validation("R1");
//        for(i = 0; i < (sizeof(move_right_gadgets_ids) / sizeof(int)); i++) {
//            _win_toolbar.gadget(move_right_gadgets_ids[i]).move_by(-move_right_offset, 0);
//        }
//        for(i = 0; i < (sizeof(timeline_move_top_gadgets_ids) / sizeof(int)); i++) {
//            _win_toolbar.gadget(timeline_move_top_gadgets_ids[i]).move_by(0, -timeline_move_top_offset);
//        }
        for(i = 0; i < (sizeof(timeline_disable_ids) / sizeof(int)); i++) {
            _win_toolbar.gadget(timeline_disable_ids[i]).fade(false);
        }
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).validation("R1;Stimeline");
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R1;Stl_files");
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R1;Stl_folder");
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R1;Stl_app");
    }
    if (_operation_mode == OPERATION_MODE_SEARCH) {
        _search_model->stop();
        _toolbar_search_text_or_option_changed_listener.search_text.clear();
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).fade(false);
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).fade(false);
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).fade(false);
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).fade(false);
    }

//    Log_debug("change_operation_mode1", 1);
    _operation_mode = mode;
    switch(mode) {
        case OPERATION_MODE_SEARCH:
            selection = new tbx::view::SingleSelection();
            _bg_color = tbx::Colour::no_colour;
            _fg_color = tbx::WimpColour::black;
            _view->selection(selection);
            _selection->remove_listener(this);
            delete _selection;
            _selection = selection;
            _selection->add_listener(this);
            _saved_view_mode = _view_mode;
            _current_dir_items.clear();
            _win_main.title("Search in: " + _current_dir_item->path().name());
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_SEARCH_ID)).validation("R2;Ssearch");
            tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(true);
            tbx::WritableField(_win_toolbar.gadget(TOOLBAR_SEARCH_WRITEABLE_ID)).text("");

            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).fade(true);
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).fade(true);
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).fade(true);
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).fade(true);
            setup_subwindows();
            view_mode(VIEW_MODE_LIST);
            break;
        case OPERATION_MODE_TIMELINE:
            selection = new tbx::view::SingleSelection();
            _view->selection(selection);
            _bg_color = tbx::Colour::no_colour;
            _fg_color = tbx::WimpColour::black;
            _selection->remove_listener(this);
            delete _selection;
            _selection = selection;
            _selection->add_listener(this);
            _saved_view_mode = _view_mode;
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_TOGGLE)).validation("R2;Stimeline");
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R1;Stl_folder");
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R1;Stl_files");
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R1;Stl_app");
            switch(_timeline_filter) {
                case TIMELINE_FILTER_FOLDERS:
                    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R2;Stl_folder");
                    break;
                case TIMELINE_FILTER_FILES:
                    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R2;Stl_files");
                    break;
                case TIMELINE_FILTER_APPS:
                    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R2;Stl_app");
                    break;
            }
//            for(i = 0; i < (sizeof(move_right_gadgets_ids) / sizeof(int)); i++) {
//                _win_toolbar.gadget(move_right_gadgets_ids[i]).move_by(move_right_offset, 0);
//            }
//            for(i = 0; i < (sizeof(timeline_move_top_gadgets_ids) / sizeof(int)); i++) {
//                _win_toolbar.gadget(timeline_move_top_gadgets_ids[i]).move_by(0, timeline_move_top_offset);
//            }
            for(i = 0; i < (sizeof(timeline_disable_ids) / sizeof(int)); i++) {
                _win_toolbar.gadget(timeline_disable_ids[i]).fade(true);
            }
            _current_dir_items = g_timeline_model.filtered_items(_timeline_filter);
            setup_subwindows();
            if (_view_mode == VIEW_MODE_LIST) {
                ((CLReportView*) _view)->setup_columns();
                refresh_current_view(false, true);
            } else {
                view_mode(VIEW_MODE_LIST);
            }

            Log_debug("CLDirectoryWindow::change_operation_mode OPERATION_MODE_TIMELINE items size:%d", _current_dir_items.size());
            break;
        case OPERATION_MODE_DIR:
//            Log_debug("change_operation_mode2", 1);
            selection = new tbx::view::MultiSelection();
            _view->selection(selection);
            _selection->remove_listener(this);
            delete _selection;
            _selection = selection;
//            Log_debug("change_operation_mode4", 1);
            _selection->add_listener(this);
//            Log_debug("change_operation_mode5", 1);
            tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_SEARCH_ID)).validation("R1;Ssearch");
            setup_subwindows();
//            Log_debug("change_operation_mode6", 1);
            view_mode(_saved_view_mode);
            if (_saved_view_mode == VIEW_MODE_LIST) {
                ((CLReportView *) _view)->setup_columns();
            }
            change_current_directory(_current_dir_item, true, true);
//            Log_debug("change_operation_mode refresh_current_view", 1);
            refresh_current_view(false, true);
//            Log_debug("change_operation_mode10", 1);
            break;
    }
}

void CLDirectoryWindow::start_stop_search() {
    if (_operation_mode == OPERATION_MODE_SEARCH) {
        std::string txt = tbx::WritableField(_win_toolbar.gadget(TOOLBAR_SEARCH_WRITEABLE_ID)).text();
        if (txt.empty()) {
            _search_model->stop();
            tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(true);
        } else {
            if (_current_dir_item) {
                unsigned int options = 0;
                options |= tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_APPS_ID)).on() ? CLSEARCH_FLAG_SEARCH_IN_APPS : 0;
                options |= tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_IMAGES_ID)).on() ? CLSEARCH_FLAG_SEARCH_IN_IMAGES : 0;
                options |= tbx::OptionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_OPTION_CASE_ID)).on() ? CLSEARCH_FLAG_SEARCH_CASE_SENSITIVE : 0;
                _search_model->start(_current_dir_item, txt, options);
                _current_dir_items.clear();
                _view->cleared();
                _win_main.title("Searching for: "+ txt + " in: " + _current_dir_item->path().name());
                tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).text("Pause");
                tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(false);
                _search_paused = false;
                Log_debug("CLDirectoryWindow::start_stop_search start searching in %s", _current_dir_item->path().name().c_str());
            } else {
                Log_debug("CLDirectoryWindow::start_stop_search _current_dir_item==null search impossible", 1);
            }
        }
    }
}

void CLDirectoryWindow::toggle_pause_search() {
    if (_search_paused) {
        _search_paused = false;
        _search_model->pause(false);
        tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(false);
        tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).text("Pause");
    } else {
        _search_paused = true;
        _search_model->pause(true);
        tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).fade(false);
        tbx::ActionButton(_win_toolbar.gadget(TOOLBAR_SEARCH_PAUSE_ID)).text("Continue");
        Log_debug("toggle_pause_search refresh_current_view", 1);
        refresh_current_view(false, true);
    }
}

//CLDirectoryItemData *CLDirectoryWindow::tree_menu_clicked_item() {
//    return _tree_menu->menu_clicked_item;
//}

void CLDirectoryWindow::refresh_roots() {
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_REFRESH_ID)).validation("R2;Srefresh");
    _win_toolbar.force_redraw(_win_toolbar.extent());
    set_status_line("Detecting drives...", true);
    g_my_app->yield();
    g_app_data_model.refresh_roots();
    refresh_dir(true);
    set_status_line("", true);
}

void CLDirectoryWindow::set_directory() {
    if (_operation_mode == OPERATION_MODE_DIR) {
        xosfscontrol_dir(_current_dir_path.c_str());
    }
}

void CLDirectoryWindow::sort_order(unsigned int sort, bool save_and_refresh) {
    if (sort != _sort_order) {
        _sort_order = sort;
        if (_sort_order & 0xf) {
            tbx::Button(_win_toolbar.gadget(0x17)).validation("Sdown");
        } else {
            tbx::Button(_win_toolbar.gadget(0x17)).validation("Sup");
        }
        _win_toolbar.force_redraw(_win_toolbar.extent());
        switch (_sort_order & 0xfff0) {
            case SORT_NAME:
                tbx::StringSet(_win_toolbar.gadget(0x16)).selected_index(0);
                break;
            case SORT_NAME_DIRS:
                tbx::StringSet(_win_toolbar.gadget(0x16)).selected_index(1);
                break;
            case SORT_DATE:
                tbx::StringSet(_win_toolbar.gadget(0x16)).selected_index(2);
                break;
            case SORT_SIZE:
                tbx::StringSet(_win_toolbar.gadget(0x16)).selected_index(3);
                break;
            case SORT_FILETYPE:
                tbx::StringSet(_win_toolbar.gadget(0x16)).selected_index(4);
                break;
        }
        if (save_and_refresh) {
            g_dir_settings.set_value(_current_dir_path, "sort_order", _sort_order);
            refresh_current_view(true, true);
        }
    }
}

//void CLDirectoryWindow::refresh_directory_sizes(bool force) {
//    if (!_current_dir_item) {
//        return;
//    }
//    if (_operation_mode != OPERATION_MODE_DIR) {
//        change_operation_mode(OPERATION_MODE_DIR);
//    }
//    Log_debug("CLDirectoryWindow::refresh_directory_sizes %s force:%d dir_size:%lld is_fullscanned:%d", _current_dir_item->name().c_str(), force, _current_dir_item->dir_size(), _current_dir_item->is_full_scanned());
//
//    unsigned int options = CLREFRESH_WITH_YIELD | CLREFRESH_RUN_ITEM_CALLBACKS;
//    if (force || !_current_dir_item->is_full_scanned()) {
//        options |= (CLREFRESH_FORCE_CALC_SIZE | CLREFRESH_CALC_SIZE);
//        g_app_data_model.refresh_on_idle(item, 1000, options);
//    } else {
//        g_app_data_model.refresh_on_idle(item, 0, options);
//    }
////    if (force || !CLDirSizeScanWindow::running()) {
////        CLDirSizeScanWindow::start(_current_dir_item, force);
////    }
//}

void CLDirectoryWindow::refresh_dir(bool force) {
    if (!_current_dir_item) {
        Log_info("CLDirectoryWindow::refresh_dir but no _current_dir_item, return",1);
        return;
    }
    if (_operation_mode != OPERATION_MODE_DIR) {
        Log_info("CLDirectoryWindow::refresh_dir _operation_mode:%d is not OPERATION_MODE_DIR, return", _operation_mode);
        return;
    }
    Log_debug("CLDirectoryWindow::refresh_dir %s force:%d", _current_dir_item->name().c_str(), force);
//    if (force && !_current_dir_path.empty()) {
//        clear_cached_thumbnails_sizes();
//        CLImageCache::invalidate_all_thumbnals_in_directory(_current_dir_path);
//    }
    unsigned int options = CLREFRESH_WITH_YIELD | CLREFRESH_RUN_ITEM_CALLBACKS;
    if (_refreshing_at_change_dir) {
        options |= CLREFRESH_ONLY_DEEP_NO_CURRENT;
    }
    if ((_view_mode == VIEW_MODE_DIR_SIZES_BARS || _view_mode == VIEW_MODE_BLOCKS)
     && (force || !_current_dir_item->is_full_scanned())) {
        options |= CLREFRESH_CALC_SIZE;
        if (force) {
            Log_debug("CLDirectoryWindow::refresh_dir %s CLREFRESH_FORCE_CALC_SIZE", _current_dir_item->name().c_str());
            options |= CLREFRESH_FORCE_CALC_SIZE;
            g_app_data_model.refresh_on_idle(_current_dir_item, 1000, options);
        } else {
            Log_debug("CLDirectoryWindow::refresh_dir %s CLREFRESH_CALC_SIZE", _current_dir_item->name().c_str());
            g_app_data_model.refresh_on_idle(_current_dir_item, 3, options);
        }
    } else {
        Log_debug("CLDirectoryWindow::refresh_dir %s", _current_dir_item->name().c_str());
        g_app_data_model.refresh_on_idle(_current_dir_item, 0, options);
    }
}

void CLDirectoryWindow::on_info_changed() {
    update_status_line();
}

void CLDirectoryWindow::on_refresh_started() {
    Log_debug("CLDirectoryWindow::on_refresh_started", 1);
    if (!(_operation_mode == OPERATION_MODE_DIR && _view_mode != VIEW_MODE_BLOCKS && _view_mode != VIEW_MODE_DIR_SIZES_BARS)) {
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_REFRESH_ID)).validation("R1;Sstop");
        _win_toolbar.force_redraw(_win_toolbar.extent());
    }
    update_status_line();
}

void CLDirectoryWindow::on_refresh_finished(bool aborted, bool any_item_changed) {
    Log_debug("CLDirectoryWindow::on_refresh_finished aborted:%d", aborted);
//    if (!(_operation_mode == OPERATION_MODE_DIR && _view_mode != VIEW_MODE_BLOCKS && _view_mode != VIEW_MODE_DIR_SIZES_BARS)) {
    tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_REFRESH_ID)).validation("R1;Srefresh");
    _win_toolbar.force_redraw(_win_toolbar.extent());
//    }

    if (any_item_changed && (_view_mode == VIEW_MODE_DIR_SIZES_BARS || _view_mode == VIEW_MODE_BLOCKS)) {
        Log_debug("CLDirectoryWindow::on_refresh_finished refresh_current_view_on_idle", 1);
        refresh_current_view_on_idle();
    }
    update_status_line();
}


/*
void CLDirectoryWindow::load_cached_thumbnail_sizes() {
    _thumb_sizes.clear();
    std::string thumbs_sizes_file = CLImageCache::get_thumbs_cache_path(_current_dir_path + "./thumbs-info");
    FILE *f = fopen(thumbs_sizes_file.c_str(), "rb");
    if (f) {
//        Log_debug("CLDirectoryWindow::load_cached_thumbnail_sizes reading:%s", thumbs_sizes_file.c_str());
        while(!feof(f)) {
            char fname[255];
            int img_w, img_h;
            unsigned long size;
            if (fscanf(f,"%s\t%d\t%d\t%lu\n", fname, &img_w, &img_h, &size) != 4) {
                break;
            }
            _thumb_sizes[fname] = {img_w, img_h, size};
//            Log_debug("CLDirectoryWindow::load_cached_thumbnail_sizes loaded %s w:%d h:%d size:%lu", fname, img_w, img_h, size);
        }
        fclose(f);
    } else {
        Logger::warn("CLDirectoryWindow::load_cached_thumbnail_sizes can't read thumbs-info (%s), error:%s", thumbs_sizes_file.c_str(), strerror(errno));
    }
}

bool CLDirectoryWindow::is_cached_thumbnail_size(CLDirectoryItemData* item) {
    if (item) {
        auto found = _thumb_sizes.find(item->name());
        if (found != _thumb_sizes.end()) {
            if (found->second.size == item->file_size()) {
                return true;
            }
        }
    }
    return false;
}

bool CLDirectoryWindow::get_cached_thumbnail_size(CLDirectoryItemData* item, int &w, int &h) {
    if (item) {
        auto found = _thumb_sizes.find(item->name());
        if (found != _thumb_sizes.end()) {
//            Log_debug("CLDirectoryWindow::get_cached_thumbnail_size %s found:%p size:%lu item size:%lu", item->name().c_str(), found, found->second.size, item->file_size());
            if (found->second.size == item->file_size()) {
                w = found->second.width;
                h = found->second.height;
//                Log_debug("CLDirectoryWindow::get_cached_thumbnail_size %s found w:%d h:%d", item->name().c_str(), w, h);
                return true;
            }
        }
    }
    return false;
}

void CLDirectoryWindow::set_cached_thumbnail_size(CLDirectoryItemData* item, int w, int h) {
//    Log_debug("CLDirectoryWindow::set_cached_thumbnail_size idx:%d w:%d h:%d _thumb_sizes_changed:%d items:%d", idx, w, h, _thumb_sizes_changed, _thumb_sizes.size());
    if (item) {
        _thumb_sizes[item->name()] = {w, h, item->file_size()};
        if (!_thumb_sizes_changed) {
            _thumb_sizes_changed = true;
            save_cached_thumbnails_sizes();
        }
    }
}

class CLSaveCachedThumbnailsSizesCmd : public tbx::Command {
private:
    CLDirectoryWindow *_me;
public:
    CLSaveCachedThumbnailsSizesCmd(CLDirectoryWindow *me) : _me(me) {};

    void execute() override {
        g_my_app->remove_idle_command(this);
        if (!g_app_state.is_dir_window_exists(_me)) {
            Log_error("CLSaveCachedThumbnailsSizesCmd CLDirectoryWindow is closed %p",_me);
            return;
        }
        _me->_save_cached_thumbnails_pending = false;
        if (!_me->_thumb_sizes.empty()) {
            std::string filename = CLImageCache::get_thumbs_cache_path(_me->_current_dir_path + "./thumbs-info");
            Log_debug("CLDirectoryWindow::save_cached_thumbnails_sizes saving dir:%s path:%s items:%d", _me->_current_dir_path.c_str(), filename.c_str(), _me->_thumb_sizes.size());
            FILE *f = fopen(filename.c_str(),"w");
            if (!f) {
                CLUtils::create_directories_for_file(filename);
            }
            f = fopen(filename.c_str(),"w");
            if (f) {
                for(auto &ths : _me->_thumb_sizes) {
                    fprintf(f, "%s\t%d\t%d\t%lu\n", ths.first.c_str(), ths.second.width, ths.second.height, ths.second.size);
                }
                fclose(f);
            } else {
                Logger::warn("CLDirectoryWindow::save_cached_thumbnail_size can't save thumbs-info (%s), error:%s", filename.c_str(), strerror(errno));
            }
        }
        _me->_thumb_sizes_changed = false;
        delete this;
    }
};

void CLDirectoryWindow::save_cached_thumbnails_sizes() {
    Log_debug("CLDirectoryWindow::save_cached_thumbnails_sizes, save pending:%d", _save_cached_thumbnails_pending);
    if (!_save_cached_thumbnails_pending) {
        _save_cached_thumbnails_pending = true;
        g_my_app->add_idle_command(new CLSaveCachedThumbnailsSizesCmd(this));
    }
}

void CLDirectoryWindow::clear_cached_thumbnails_sizes() {
    _thumb_sizes_changed = false;
    _thumb_sizes.clear();
    tbx::Path(CLImageCache::get_thumbs_cache_path(_current_dir_path + "./thumbs-info")).remove();
}
*/

void CLDirectoryWindow::on_favorites_changed() {
//    Log_debug("CLDirectoryWindow::on_favorites_changed empty:%d", g_app_data_model.get_favorites().empty());
    _win_toolbar.gadget(TOOLBAR_BTN_FAVORITE_ID).fade(g_app_data_model.get_favorites().empty());
    if (g_app_data_model.is_favorite(_current_dir_path)) {
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_FAVORITE_ID)).validation("R1;Sfavoritess");
    } else {
        tbx::Button(_win_toolbar.gadget(TOOLBAR_BTN_FAVORITE_ID)).validation("R1;Sfavorite");
    }
}

void CLDirectoryWindow::FavoriteClickListener::mouse_click(tbx::MouseClickEvent &event) {
    auto mnu = tbx::Menu("MFavorite");
    new CLFavoriteMenu(mnu, _me);
    mnu.show_at_pointer();
}

void CLDirectoryWindow::ToolbarButtonNavClickListener::mouse_click(tbx::MouseClickEvent &event) {
    switch (event.id_block().self_component().id()) {
        case TOOLBAR_BTN_UP_ID:
            _me->go_up_level();
            break;
        case TOOLBAR_BTN_BACK_ID:
            _me->go_backward();
            break;
        case TOOLBAR_BTN_FORW_ID:
            _me->go_forward();
            break;
    }
}

void CLDirectoryWindow::SearchPanelToggleListener::mouse_click(tbx::MouseClickEvent &event) {
#if DONATE == 1
    CLAskDonate::open();
#else
    if (_me->_operation_mode == OPERATION_MODE_DIR) {
        _me->change_operation_mode(OPERATION_MODE_SEARCH);
    } else {
        _me->change_operation_mode(OPERATION_MODE_DIR);
    }
#endif
}


void CLDirectoryWindow::TimelineToggleClickListener::mouse_click(tbx::MouseClickEvent &event) {
    if (_me->_operation_mode == OPERATION_MODE_TIMELINE) {
        _me->change_operation_mode(OPERATION_MODE_DIR);
    } else {
        _me->change_operation_mode(OPERATION_MODE_TIMELINE);
    }
}

void CLDirectoryWindow::TimelineFoldersClickListener::mouse_click(tbx::MouseClickEvent &event) {
    Log_debug("CLDirectoryWindow::TimelineFoldersClickListener::mouse_click", 1);
    if (_me->_timeline_filter == TIMELINE_FILTER_FOLDERS && _me->_operation_mode == OPERATION_MODE_TIMELINE) {
        _me->change_operation_mode(OPERATION_MODE_DIR);
    } else {
        _me->_timeline_filter = TIMELINE_FILTER_FOLDERS;
        if (_me->_operation_mode != OPERATION_MODE_TIMELINE) {
            _me->change_operation_mode(OPERATION_MODE_TIMELINE);
        } else {
            ((CLReportView*)_me->_view)->setup_columns();
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R2;Stl_folder");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R1;Stl_files");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R1;Stl_app");
            _me->_current_dir_items = g_timeline_model.filtered_items(_me->_timeline_filter);
            _me->refresh_current_view(false, true);
        }
    }
}

void CLDirectoryWindow::TimelineFilesClickListener::mouse_click(tbx::MouseClickEvent &event) {
    Log_debug("CLDirectoryWindow::TimelineFilesClickListener::mouse_click", 1);
    if (_me->_timeline_filter == TIMELINE_FILTER_FILES && _me->_operation_mode == OPERATION_MODE_TIMELINE) {
        _me->change_operation_mode(OPERATION_MODE_DIR);
    } else {
        _me->_timeline_filter = TIMELINE_FILTER_FILES;
        if (_me->_operation_mode != OPERATION_MODE_TIMELINE) {
            _me->change_operation_mode(OPERATION_MODE_TIMELINE);
        } else {
            ((CLReportView*)_me->_view)->setup_columns();
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R1;Stl_folder");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R2;Stl_files");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R1;Stl_app");
            _me->_current_dir_items = g_timeline_model.filtered_items(_me->_timeline_filter);
            _me->refresh_current_view(false, true);
        }
    }
}

void CLDirectoryWindow::TimelineAppsClickListener::mouse_click(tbx::MouseClickEvent &event) {
    Log_debug("CLDirectoryWindow::TimelineAppsClickListener::mouse_click", 1);
    if (_me->_timeline_filter == TIMELINE_FILTER_APPS && _me->_operation_mode == OPERATION_MODE_TIMELINE) {
        _me->change_operation_mode(OPERATION_MODE_DIR);
    } else {
        _me->_timeline_filter = TIMELINE_FILTER_APPS;
        if (_me->_operation_mode != OPERATION_MODE_TIMELINE) {
            _me->change_operation_mode(OPERATION_MODE_TIMELINE);
        } else {
            ((CLReportView*)_me->_view)->setup_columns();
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FOLDERS)).validation("R1;Stl_folder");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_FILES)).validation("R1;Stl_files");
            tbx::Button(_me->_win_toolbar.gadget(TOOLBAR_BTN_TIMELINE_APPS)).validation("R2;Stl_app");
            _me->_current_dir_items = g_timeline_model.filtered_items(_me->_timeline_filter);
            _me->refresh_current_view(false, true);
        }
    }
}

void CLDirectoryWindow::update_status_line() {
    std::ostrstream out_stream;
    std::string out_str;
    CLDirectoryItemData *cur_dir_item = current_dir_item();
    CLDirectoryItemData *selected_item = nullptr;
    if (_status_message.empty()) {
        if (g_thumbnailer.is_creating()) {
            out_stream << "Creating thumbnail..." << std::ends;
        } else {
            CLDirectoryItemData *refreshing_item = g_app_data_model.get_refreshing_item();
            SearchState srch_state = _search_model->get_search_state();
            if (refreshing_item && srch_state == SearchState::STOPPED && (main_view_mode() == VIEW_MODE_BLOCKS || main_view_mode() == VIEW_MODE_DIR_SIZES_BARS)) {
                out_stream << "Refreshing: " << refreshing_item->path().name() << std::ends;
            } else {
                bool is_saving_cache = g_app_data_model.is_saving_cache();
                if (is_saving_cache && srch_state == SearchState::STOPPED) {
                    out_stream << "Saving cache..." << std::ends;
                } else {
                    if (_search_by_prefix_not_found) {
                        out_stream << "No file name or folder found with first letter \"" << _search_by_prefix << "\"" << std::ends;
                    } else {
                        auto sel_items = selected_items();
                        if (sel_items.empty()) {
                            if (srch_state != SearchState::STOPPED) {
                                if (srch_state == SearchState::SEARCHING) {
                                    out_stream << "Searching... ";
                                }
                                if (_current_dir_items.empty()) {
                                    out_stream << "No matches found";
                                } else {
                                    out_stream << _current_dir_items.size() << " items found";
                                }
                                if (srch_state == SearchState::SEARCHING) {
                                    out_stream << ", scanning: " << _search_model->get_searching_in_item()->path().name();
                                }
                                out_stream << std::ends;
                            } else {
                                if (!cur_dir_item || cur_dir_item->parent() == nullptr) {
                                    int64_t free_space = 0, total_space = 0;
                                    int max_obj_size = 0;
                                    os_error *err;
                                    err = xosfscontrol_free_space64(_current_root.c_str(), (bits *) &free_space,
                                                                    ((int *) &free_space + 1), &max_obj_size,
                                                                    (bits *) &total_space, ((int *) &total_space + 1));
                                    Log_debug("xosfscontrol_free_space64 drive:%s free:%llx l:%lx h:%lx total:%llx l:%lx h:%lx",
                                              _current_root.c_str(), free_space, free_space, *((int *) &free_space + 1),
                                              total_space, total_space, *((int *) &total_space + 1));
                                    if (err) {
                                        Logger::error("xosfscontrol_free_space64 failed, error: (%d) %s", err->errnum,
                                                      err->errmess);
                                        free_space = 0;
                                        total_space = 0;
                                        err = xosfscontrol_free_space(_current_root.c_str(), (int *) &free_space, &max_obj_size,
                                                                      (int *) &total_space);
                                        if (err) {
                                            Logger::error("xosfscontrol_free_space failed, error: (%d) %s", err->errnum,
                                                          err->errmess);
                                            free_space = -1;
                                            total_space = -1;
                                        }
                                    }
                                    out_stream << "Total:" << file_size_to_displayed_string(total_space) << " | Free:"
                                               << file_size_to_displayed_string(free_space) << std::ends;
                                } else {
                                    selected_item = cur_dir_item;
                                }
                            }
                        } else {
                            if (sel_items.size() == 1) {
                                selected_item = sel_items[0];
                            } else {
                                int64_t sel_size = 0;
                                for (auto item: sel_items) {
                                    sel_size += item->dir_or_file_size();
                                }
                                out_stream << sel_items.size() << " selected items " << file_size_to_displayed_string(sel_size)
                                           << std::ends;
                            }
                        }
                        if (selected_item) {
                            out_stream << std::setiosflags(std::ios::left) << std::setw(64) << selected_item->name() \
 << " " << file_size_to_displayed_string(selected_item->dir_or_file_size());
                            if (selected_item->is_directory()) {
                                if (selected_item->childs().empty()) {
                                    out_stream << " | Dir         ";
                                } else {
                                    out_stream << " | Dir (" << selected_item->childs().size() << " items)";
                                }
                            } else {
                                out_stream << " | " << std::setw(12) << selected_item->file_type_str() << " (" << std::hex << std::setfill('0') << std::setw(3) << std::right << selected_item->file_type() << ")";
                            }
                            out_stream << " | " << selected_item->mtime_str() \
 << " | " << selected_item->attrs_str()
                                       << std::ends;
                        }
                    }
                }
            }
        }
    } else {
        out_stream << _status_message << std::ends;
    }
    out_str = out_stream.str();

    if (tbx::Button(_win_statusbar.gadget(1)).value() != out_str) {
        tbx::Button(_win_statusbar.gadget(1)).value(out_str);
    }
}

void CLDirectoryWindow::selection_changed(const tbx::view::SelectionChangedEvent &event) {
    _search_by_prefix_not_found = false;
    update_status_line();
}

void CLDirectoryWindow::IdleTopToolbarScroller::pointer_leaving(const tbx::EventInfo &ev) {
//    if (ev.id_block().self_object() == _win_toolbar) {
//        Log_debug("pointer_leaving toolbar", 1);
    if (_scrolling) {
        _scrolling = false;
//        _scroll_x = 0;
        tbx::Application::instance()->remove_timer(this);
        _me->_win_toolbar.scroll(_scroll_x, 0);
    }
//    }
}

void CLDirectoryWindow::IdleTopToolbarScroller::pointer_entering(const tbx::EventInfo &ev) {
//    if (ev.id_block().self_object() == _me->_win_toolbar) {
//        Log_debug("pointer_entering toolbar", 1);
    if (!_scrolling) {
        tbx::WindowState st;
        _me->_win_main.get_state(st);
        int right_x = _me->_win_toolbar.gadget(0x10).bounds().max.x + 8;
        if ((right_x - st.visible_area().bounds().width()) > 0) {
            _scrolling = true;
//            _scroll_x = 0;
            tbx::Application::instance()->add_timer(1, this);
        }
    }
//    }
}


void CLDirectoryWindow::IdleTopToolbarScroller::timer(unsigned int elapsed) {
    if (_scrolling) {
        tbx::PointerInfo pi = tbx::PointerInfo(true, false);
        tbx::WindowState st;
        _me->_win_main.get_state(st);
        int right_x = _me->_win_toolbar.gadget(0x10).bounds().max.x + 8;
        int max_x = st.visible_area().bounds().max.x;
        int min_x = st.visible_area().bounds().min.x;
        int scroll_max_x = right_x - (max_x - min_x);
//        Log_debug("pi.mouse_x() %d _win.max_x=%d _win.min_x=%d right_x=%d scroll_max_x=%d", pi.mouse_x(), max_x, min_x, right_x, scroll_max_x);
        if (scroll_max_x > 0) {
            if (pi.mouse_x() > max_x - 80 && _scroll_x < scroll_max_x) {
                _scroll_x += 4;
                _me->_win_toolbar.scroll(_scroll_x, 0);
                return;
            }
            if (pi.mouse_x() < min_x + 80 && _scroll_x > 0) {
                _scroll_x -= 4;
                _me->_win_toolbar.scroll(_scroll_x, 0);
                return;
            }
        }
    }
}

void CLDirectoryWindow::IdleTopToolbarScroller::reset_scroll() {
    if (_scroll_x > 0) {
        _scroll_x = 0;
        _me->_win_toolbar.scroll(_scroll_x, 0);
        if (_scrolling) {
            _scrolling = false;
            tbx::Application::instance()->remove_timer(this);
        }
    }
}

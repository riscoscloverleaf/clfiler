//
// Created by slenz on 01.07.2022.
//
#include <tbx/bbox.h>
#include <tbx/offsetgraphics.h>
#include <tbx/font.h>
#include <cloverleaf/CLUtf8.h>
#include "CLBlocksView.h"
#include "CLDirectoryWindow.h"

std::map<int, tbx::Colour> _filetype_colors = {
        {0x1000, tbx::Colour(0x79,0xDB,0xFF)}, // dir
        {0x2000, tbx::Colour(0xC0,0xFF,0xC0)}, // app

        {0xa91, tbx::Colour(0xFE,0xF7,0xC0)}, // zip
        {0xc46, tbx::Colour(0xFE,0xF7,0xC0)}, // tar

        {0xA64, tbx::Colour(0xff,0xa4,0x05)}, // mp4
        {0xBF8, tbx::Colour(0xff,0xa4,0x05)}, // mpg

        {0x1AD, tbx::Colour(0xff,0x5d,0x05)}, // mp3
        {0xFD4, tbx::Colour(0xff,0x5d,0x05)}, // midi
        {0xB78, tbx::Colour(0xff,0x5d,0x05)}, // midi
        {0x770, tbx::Colour(0xff,0x5d,0x05)}, // midi
        {0x1B0, tbx::Colour(0xff,0x5d,0x05)}, // stm
        {0xCB6, tbx::Colour(0xff,0x5d,0x05)}, // mod
        {0xC02, tbx::Colour(0xff,0x5d,0x05)}, // stm
        {0xAF1, tbx::Colour(0xff,0x5d,0x05)}, // !Maestro

        {0xFF0, tbx::Colour(0xff,0xea,0x05)}, // dxf
        {0xFF9, tbx::Colour(0xff,0xea,0x05)}, // sprite
        {0xAFF, tbx::Colour(0xff,0xea,0x05)}, // draw
        {0xD94, tbx::Colour(0xff,0xea,0x05)}, // artworks

        {0xFF0, tbx::Colour(0xff,0xea,0x05)}, // tiff
        {0x695, tbx::Colour(0xff,0xea,0x05)}, // gif
        {0x69C, tbx::Colour(0xff,0xea,0x05)}, // bmp
        {0xC85, tbx::Colour(0xff,0xea,0x05)}, // jpg
        {0xB60, tbx::Colour(0xff,0xea,0x05)}, // bmp

        {0xfff, tbx::Colour(0xff,0xff,0xff)}, // text
        {0xadf, tbx::Colour(0xff,0xff,0xff)}, // pdf
        {0xfeb, tbx::Colour(0xff,0xff,0xff)}, // obey
        {0, tbx::Colour(0xee,0xee,0xee)} // text
};

float clamp( const float v, const float lo, const float hi )
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

std::vector<std::pair<Rectangle,CLDirectoryItemData*>> TravelLayout(Layout *current_layout, size_t count) {
    std::vector<std::pair<Rectangle,CLDirectoryItemData*>> result;
    result.reserve(count);
    while (current_layout != nullptr)
    {
        auto current = current_layout->get_layout_rectangles();
        result.insert(result.end(), current.begin(), current.end());
        current_layout = current_layout->next_layout.get();
    }
    return result;
}

std::vector<std::pair<Rectangle,CLDirectoryItemData*>> SolveSquarifiedTreemap(const std::vector<CLDirectoryItemData*> &dir_items, const Rectangle &container)
{
    int64_t total_size = 0;
    for(auto item : dir_items) {
        total_size += item->dir_or_file_size();
    }
    std::unique_ptr<Layout> original_layout = make_unique<Layout>();
    original_layout->container = container;
    original_layout->total_size = total_size;
    Layout *current_layout = original_layout.get();
    for (auto item : dir_items)
    {
        Layout *next = current_layout->add_item(item);
        if (next != nullptr)
        {
            current_layout = next;
        }
    }

    return TravelLayout(original_layout.get(), dir_items.size());
}

CLBlocksView::CLBlocksView(CLDirectoryWindow *dir_win) :
    tbx::view::ItemView(dir_win->_win_main),
    _dir_win(dir_win)
{
    tbx::Font desktop_font;
    if (desktop_font.desktop_font()) {
        std::string font_id = desktop_font.identifier();
        std::string font_name;
        int end_pos = font_id.find('.');
        if (end_pos != std::string::npos) {
            font_name = font_id.substr(2, end_pos - 2);
        } else {
            end_pos = font_id.find("\\E");
            if (end_pos != std::string::npos) {
                font_name = font_id.substr(2, end_pos - 2);
            } else {
                font_name = font_id.substr(2, 255);
            }
        }
        Log_debug("desktop font: %s %s", font_id.c_str(), font_name.c_str());
        _name_style = new CLFontStyle(font_name.c_str(), rufl_WEIGHT_400, 28);
        _size_style = new CLFontStyle(font_name.c_str(), rufl_WEIGHT_400, 24);
//        rufl_dump_state();
    }

    margin(dir_win->_margin);
    _window.add_open_window_listener(this);
    add_click_listener(dir_win);
//    _window.extent(tbx::BBox(0,-10000,10000,0));
}

CLBlocksView::~CLBlocksView() {
    _window.remove_open_window_listener(this);
    delete _name_style;
    delete _size_style;
}

#define X_PADDING 8
#define Y_PADDING 8
#define MIN_BLOCK_WIDTH 10
#define MIN_BLOCK_TEXT_WIDTH 70
#define MIN_BLOCK_TEXT_HEIGHT 52
#define MIN_BLOCK_TEXT_AND_ICON_HEIGHT 102

void CLBlocksView::redraw(const tbx::RedrawEvent &event) {
    int y_offset_text, y_offset_icon;
    CLGraphics g(event.visible_area());
    g.offset_x(_margin.left + g.offset_x());
    g.offset_y(g.offset_y() -_margin.top);
    int i = 0;

    g.foreground(tbx::Colour(0xAA, 0xAA, 0xAA));
    g.fill_rectangle(0, 0, event.visible_area().bounds().width(), -event.visible_area().bounds().height());

    if (squarified_data.empty()) {
        int y = - (event.visible_area().bounds().height() - (_margin.top + _margin.bottom)) / 2;
        g.draw_text_centered(0, y, event.visible_area().bounds().width() - (_margin.left + _margin.right),
            std::string("Directory is empty"),
            *_name_style, tbx::Colour::black, tbx::Colour::white);
    } else {
        for(auto &sqitem : squarified_data) {
            Rectangle rect = sqitem.first;
            if (rect.width < MIN_BLOCK_WIDTH) {
                continue;
            }
            auto item = sqitem.second;
            auto color_it = _filetype_colors.find(item->file_type());
            auto color = tbx::Colour(0xCC, 0xCC, 0xCC);
            if (color_it != _filetype_colors.end()) {
                color = color_it->second;
            }
            g.foreground(color);
            g.fill_rectangle(rect.x, -(rect.y + rect.height), rect.x + rect.width, -rect.y);
            g.foreground(tbx::Colour::black);
            g.rectangle(rect.x, -(rect.y + rect.height), rect.x + rect.width, -rect.y);
            if (rect.width > MIN_BLOCK_TEXT_WIDTH) {
                //Log_debug("CLBlocksView::redraw h:%d %s", rect.height, item->name().c_str());
                if (rect.height > MIN_BLOCK_TEXT_AND_ICON_HEIGHT) {
                    y_offset_icon = -(rect.y + ((rect.height - Y_PADDING*2) / 2));
                    tbx::BBox img_box = tbx::BBox(rect.x, y_offset_icon, rect.x + rect.width, y_offset_icon+30);
                    //Log_debug("CLBlocksView::redraw draw icon img:%s", item->name().c_str());
                    if (item->small_sprite_image()) {
                        g.draw_image_scaled_centered(*item->small_sprite_image(), 100, img_box);
//                    Log_debug("CLBlocksView::redraw draw use small sprite icon img:%s", item->name().c_str());
                    } else {
                        g.draw_image_scaled_centered(*item->sprite_image(), 50, img_box);
                    }
                    y_offset_text = y_offset_icon - 30;
                } else {
                    y_offset_text = -(rect.y + ((rect.height - Y_PADDING*2) / 2));
                }
                if (rect.height > MIN_BLOCK_TEXT_HEIGHT) {
//            y_offset_text = -rect.y;
                    std::string name = riscos_local_to_utf8(item->display_name());
                    g.draw_text_centered(rect.x + X_PADDING, y_offset_text, rect.width - X_PADDING*2, name, *_name_style, tbx::Colour::black, tbx::Colour::white);
//            Log_debug("g.draw_text_centered x=%d y=%d w=%d %s", rect.x + X_PADDING, y_offset_text, rect.width - X_PADDING*2, item->display_name().c_str());
                    y_offset_text -= 24;
                    g.draw_text_centered(rect.x + X_PADDING, y_offset_text, rect.width - X_PADDING*2, item->dir_or_file_size_str_with_unit(), *_size_style, tbx::Colour::black, tbx::Colour::white);
                }
            }
            if (rect.item_idx == _dir_win->_drop_item_idx) {
//            Log_debug("CLBlocksView::redraw idx:%d", rect.item_idx);
                g.foreground(_dir_win->_fg_color.colour());
                g.rectangle(rect.x + 2, -(rect.y + rect.height) + 2, rect.x + rect.width - 2, -rect.y - 2);
            }
//        Log_debug("CLBlocksView::redraw x=%d y=%d w=%d h=%d s=%lld n=%s", rect.x, -(rect.y + rect.height), rect.width, rect.height, item->dir_or_file_size(), item->name().c_str());
        }
    }
}

void CLBlocksView::update_window_extent() {
    if (!updates_enabled()) return;

    tbx::WindowState state;
    _window.get_state(state);
    tbx::BBox extent(0, -state.visible_area().bounds().height(), state.visible_area().bounds().width(), 0);
//    _window.extent(extent);
    recalc_layout(extent);
}

void CLBlocksView::open_window(tbx::OpenWindowEvent &event) {
    if (updates_enabled()) {
        tbx::WindowState state;
        _window.get_state(state);
        if (state.visible_area().bounds().width() != event.visible_area().width()
            || state.visible_area().bounds().height() != event.visible_area().height()) {
            recalc_layout(event.visible_area());
            refresh();
        }
    }
}

bool CLBlocksView::recalc_layout(const tbx::BBox &work_area) {
    std::vector<CLDirectoryItemData*> items = _dir_win->current_dir_items();
    std::sort(items.begin(), items.end(), [](CLDirectoryItemData *a, CLDirectoryItemData *b) {
        return (a->is_file() ? a->file_size() : a->dir_size()) > (b->is_file() ? b->file_size() : b->dir_size());
    });
    Rectangle container;
    container.width = work_area.width() - (_margin.left + _margin.right);
    container.height = work_area.height() - (_margin.top + _margin.bottom);
    squarified_data = SolveSquarifiedTreemap(items, container);
    int i = 0;
    for(auto item : _dir_win->current_dir_items()) {
        for(auto &sqitem : squarified_data) {
            if (sqitem.second == item) {
                if (sqitem.first.width >= MIN_BLOCK_WIDTH) {
                    sqitem.first.item_idx = i;
                }
                break;
            }
        }
        i++;
    }
//    Log_debug("CLBlocksView::recalc_layout %dx%d", work_area.width(), work_area.height());
    _window.extent(tbx::BBox(0, -work_area.height(), work_area.width(), 0));
    return true;
}

void CLBlocksView::refresh() {
    if (updates_enabled()) _window.force_redraw(_window.extent());
}

void CLBlocksView::inserted(unsigned int where, unsigned int how_many)
{
    _count += how_many;
    update_window_extent();
    refresh();
}

void CLBlocksView::removed(unsigned int where, unsigned int how_many)
{
    update_window_extent();
    refresh();
}

void CLBlocksView::changed(unsigned int where, unsigned int how_many)
{
    update_window_extent();
    refresh();
}

void CLBlocksView::cleared()
{
    if (_count)
    {
        _count = 0;
        update_window_extent();
        refresh();
    }
}

void CLBlocksView::get_bounds(tbx::BBox &bounds, unsigned int index) const
{
    bounds.min.x = 0;
    bounds.max.x = 0;
    bounds.max.y = 0;
    bounds.min.y = 0;
    for(auto &sqitem : squarified_data) {
        Rectangle rect = sqitem.first;
        if (rect.item_idx == index) {
            bounds.min.x = _margin.left + rect.x;
            bounds.max.x = bounds.min.x + rect.width;
            bounds.max.y = -_margin.top - rect.y;
            bounds.min.y = bounds.max.y - rect.height;
            break;
        }
    }
}

void CLBlocksView::get_bounds(tbx::BBox &bounds, unsigned int first, unsigned int last) const
{
    bounds.min.x = 0;
    bounds.max.x = 0;
    bounds.max.y = 0;
    bounds.min.y = 0;
}

unsigned int CLBlocksView::insert_index(const tbx::Point &scr_pt) const {
    return 0;
}

unsigned int CLBlocksView::screen_index(const tbx::Point &scr_pt) const {
    return hit_test(scr_pt);
}

unsigned int CLBlocksView::hit_test(const tbx::Point &scr_pt) const {
    tbx::WindowState state;
    _window.get_state(state);

    tbx::Point work = state.visible_area().work(scr_pt);
    work.x -= _margin.left;
    work.y += _margin.top;
    for(auto &sqitem : squarified_data) {
        Rectangle rect = sqitem.first;
        if (rect.width < MIN_BLOCK_WIDTH) {
            continue;
        }
        tbx::BBox bounds = tbx::BBox(rect.x, -(rect.y + rect.height), rect.x + rect.width, -rect.y);
        if (bounds.contains(work)) {
            Log_debug("CLBlocksView::hit_test x=%d y=%d w=%d h=%d s=%lld n=%s idx=%d", rect.x, rect.y, rect.width, rect.height, sqitem.second->dir_or_file_size(), sqitem.second->name().c_str(), rect.item_idx);
            return rect.item_idx;
        }
    }
    return NO_INDEX;
}

void CLBlocksView::auto_size(bool on) {
}

void CLBlocksView::margin(const tbx::Margin &margin) {
    tbx::Margin m = margin;
    m.top -= 6;
    m.left -= 16;
    m.right -= 16;
    m.bottom -= 2;
    ItemView::margin(m);
}


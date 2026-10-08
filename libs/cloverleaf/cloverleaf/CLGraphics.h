//
// Created by lenz on 3/9/20.
//

#ifndef CLOVERLEAF_CLGRAPHICS_H
#define CLOVERLEAF_CLGRAPHICS_H
#include <map>
#include <list>
#include <rufl.h>
#include <tbx/offsetgraphics.h>
#include <tbx/drawpath.h>
#include <tbx/command.h>
#include <tbx/colour.h>
#include <tbx/font.h>
#include "CLImage.h"

class CLFontStyle {
private:
    int _font_height;
public:
    char* fontname;
    rufl_style style;
    int size;

    CLFontStyle(const char *fontname, rufl_style style, int size);
    ~CLFontStyle();

    int get_font_height();
};

class CLDesktopFontStyle : public CLFontStyle {
    static std::string _desktop_font_name;
public:
    CLDesktopFontStyle(rufl_style style, int size) :
        CLFontStyle(_desktop_font_name.c_str(), style, size) {};
    static void find_desktop_font();
};

struct CLTextStyle {
    tbx::Colour fg_color;
    tbx::Colour bg_color;
    CLFontStyle font_style;
};

struct CLTextColor {
    tbx::Colour text_color;
    tbx::Colour bg_color;
};

struct CLSpan {
    size_t start;
    int len;
    int x;
    int width;
    bool newline;
};

class CLGraphics : public tbx::OffsetGraphics {
public:
    CLGraphics(const tbx::VisibleArea &area) : OffsetGraphics(area) {};
    CLGraphics(int offsetX, int offsetY) : OffsetGraphics(offsetX, offsetY) {};

    void draw_image(CLBaseImage& img, int left_x, int bottom_y,
                    tbx::Colour background_colour = 0);

    void draw_image(const std::string& filename, int left_x, int bottom_y,
                    tbx::Colour background_colour = 0);

    void draw_cached_image(const std::string& filename, int left_x, int bottom_y,
                    tbx::Colour background_colour = 0);

    void draw_image_scaled(CLBaseImage& img, int left_x, int bottom_y,
                           int req_width, int req_height,
                           tbx::Colour background_colour = 0);

    void draw_image_scaled(const std::string& filename, int left_x, int bottom_y,
                    int req_width, int req_height,
                    tbx::Colour background_colour = 0);

    void draw_cached_image_scaled(const std::string& filename, int left_x, int bottom_y,
                           int req_width, int req_height,
                           tbx::Colour background_colour = 0);

    void draw_image_scaled_centered(CLBaseImage& img, int scale, const tbx::BBox& box, tbx::Colour background_colour = 0);
    void draw_image_fit_box(CLBaseImage& img, const tbx::BBox& box, tbx::Colour background_colour = 0, int max_upscale_percent = 100);

    void draw_image_circled(int left_x, int bottom_y, int radius, const std::string& filename, tbx::Colour bgcolor);
    void draw_circle(int left_x, int bottom_y, int radius, int line_thickness, tbx::Colour fill_color, tbx::Colour border_color);

    void draw_rounded_box(int xmin, int ymin, int xmax, int ymax, int border_radius, int line_thickness, tbx::Colour fill_color, tbx::Colour border_color);

    void set_font_colour(tbx::Colour fgcolor, tbx::Colour bgcolor);
    void draw_text(int x, int y, const std::string& text, const CLFontStyle& font_style, tbx::Colour fcolor, tbx::Colour bcolor, bool background_rectangle = false);
    void draw_text_cut_to_width(int x, int y, int width, const std::string& text, const CLFontStyle& font_style, tbx::Colour fcolor, tbx::Colour bcolor, bool append_suffix = true);
    void draw_text_centered(int x, int y, int width, const std::string& text, const CLFontStyle& font_style, tbx::Colour fcolor, tbx::Colour bcolor, bool append_suffix = true, bool background_rectangle = false);

    static std::string cut_string_to_with(const std::string& text, int width, const CLFontStyle& font_style, bool append_suffix = false);
    static int get_text_width(const std::string& text, const CLFontStyle& font_style);
    static int split_text_to_width(const std::string& text, int width, const CLFontStyle& font_style, std::list<std::string>& lines);
    static std::list<CLSpan> split_text_to_spans(const std::string& text, int text_start, int text_len, int x, int box_width, const CLFontStyle& font_style);
};

#endif //CLOVERLEAF_CLGRAPHICS_H

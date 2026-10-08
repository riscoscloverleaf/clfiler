//
// Created by lenz on 3/22/20.
//

#ifndef CLOVERLEAF_CLIMAGE_H
#define CLOVERLEAF_CLIMAGE_H

#include <oslib/osspriteop.h>
#include <cloverleaf/tinct.h>
#include <tbx/colour.h>
#include <tbx/sprite.h>
#include <tbx/modeinfo.h>
#include <tbx/drawfile.h>
#include <tbx/path.h>
#include <string>
#include <cloverleaf/Logger.h>
#include <tbx/jpeg.h>
#include "awrender.h"

#define FILE_TYPE_PNG 0xB60
#define FILE_TYPE_JPEG 0xC85
#define FILE_TYPE_ICO 0x132
#define FILE_TYPE_GIF 0x695
#define FILE_TYPE_SPRITE 0xFF9
#define FILE_TYPE_DRAW 0xAFF
#define FILE_TYPE_WEBP 0xA66
#define FILE_TYPE_PSD 0xF98
#define FILE_TYPE_TIFF 0xFF0
#define FILE_TYPE_SVG 0xAAD
#define FILE_TYPE_PDF 0xADF
#define FILE_TYPE_ARTWORKS 0xD94

#define FILE_TYPE_MOVIEFS 0xFB2
#define FILE_TYPE_MP4 0xA64
#define FILE_TYPE_MPG 0xBF8
#define FILE_TYPE_MPEG 0x0F8
#define FILE_TYPE_AVI 0x071
#define FILE_TYPE_VOB 0xA8D

#define SPREXT_TINCT_LIKE_MODE (os_mode)0x78608051

class CLImageMagickLoader;
class CLImageJPGLoader;
class CLImagePNGLoader;

enum {
    CL_IMAGE_TYPE_SPRITE,
    CL_IMAGE_TYPE_WIMP_SPRITE,
    CL_IMAGE_TYPE_CONVERTED_SPRITE,
    CL_IMAGE_TYPE_DRAW,
    CL_IMAGE_TYPE_ARTWORKS,
    CL_IMAGE_TYPE_MOCK,
    CL_IMAGE_TYPE_PNG,
    CL_IMAGE_TYPE_JPEG
};

class CLBaseImage {
protected:
    int _width_px = 0, _height_px = 0, _width = 0, _height = 0;
    bool _has_alpha = false;
    static awrender_vdu_block _vdu_block;
public:
    CLBaseImage();

    static void read_vdu_info();
    static awrender_vdu_block& get_vdu_info() { return _vdu_block; }

    virtual ~CLBaseImage();

    inline int width_px() const { return _width_px; }
    inline int height_px() const { return _height_px; }
    inline int width() const { return _width; }
    inline int height() const { return _height; }
    inline bool has_alpha() { return _has_alpha; };
    virtual int get_image_type() = 0;
    virtual int size() = 0;
    virtual bool is_valid() = 0;
    virtual bool load(const std::string& filename) = 0;
    virtual bool save(const std::string& filename) { return false; }
    virtual bool load(osspriteop_header* spr) { return false; }
    virtual void plot(int left_x, int bottom_y, tbx::Colour background_colour) = 0;
    virtual void plot_scaled(int left_x, int bottom_y,
                     int req_width, int req_height,
                     tbx::Colour background_colour) = 0;

    int get_percent_scale_to_fit_in_box(const tbx::Size& sz, int max_upscale_percent = 100);
    tbx::BBox get_scaled_and_centered_box(const tbx::BBox& fit_in_box);
    tbx::BBox get_scaled_and_centered_box(const tbx::BBox& fit_in_box, int scale_percent);

};

class CLMockImage : public CLBaseImage {
public:
    CLMockImage(int width, int height) {
        _height = height;
        _width = width;
    }

    int get_image_type() override { return CL_IMAGE_TYPE_MOCK; }

    bool is_valid() override {
        return false;
    }

    bool load(const std::string &filename) override {
        return false;
    }

    int size() override { return 0; }

    void plot(int left_x, int bottom_y, tbx::Colour background_colour) override {
    }

    void plot_scaled(int left_x, int bottom_y, int req_width, int req_height, tbx::Colour background_colour) override {
    }
};


class CLUserSpriteImage : public CLBaseImage {
    friend CLImageMagickLoader;
    friend CLImageJPGLoader;
    friend CLImagePNGLoader;
protected:
    osspriteop_area *_sprite_area = nullptr;
    static unsigned int initial_tinct_options;
    static unsigned int _color_translation_table[256];
    static bool ro5extsprs;
    int _prepare_color_translation_table();
    void _get_wimp_scale(int* factor) const;
    void _spriteop_plot_scaled(int left_x, int bottom_y, int* sf);

public:
    CLUserSpriteImage();
    CLUserSpriteImage(const CLUserSpriteImage& src);
    CLUserSpriteImage(int width_px, int height_px, bool has_alpha, const char* spr_name = nullptr);
    CLUserSpriteImage(const std::string& file_name);
    virtual ~CLUserSpriteImage();

    int get_image_type() override { return CL_IMAGE_TYPE_SPRITE; }
    static osspriteop_area* create_sprite_area(int width_px, int height_px, const char* spr_name = nullptr);
    static inline unsigned char * get_sprite_pixels_pointer(unsigned char* sprite_area) { return sprite_area + 16 + 44; }

    static void detect_mode();

    virtual bool load(const std::string& filename);
    virtual bool save(const std::string &filename);
    virtual int size();
    bool load_spritearea(const std::string& filename);

    inline bool has_palette() const {return get_sprite_pointer()->image > 44;};
    inline int num_colors() const { return tbx::ModeInfo((int)get_sprite_pointer()->mode).colours(); }
    inline osspriteop_area *get_area_pointer() const { return _sprite_area; }
    inline osspriteop_header *get_sprite_pointer() const { return _sprite_area ? reinterpret_cast<osspriteop_header *>(((char *)_sprite_area) + _sprite_area->first) : nullptr; }
    inline unsigned char * get_sprite_pixels_pointer() { return _sprite_area ? ((unsigned char*)_sprite_area) + _sprite_area->first + 44 : nullptr; }
    inline tbx::UserSprite get_user_sprite() { tbx::SpriteArea((tbx::OsSpriteAreaPtr)_sprite_area, false).get_sprite(
                reinterpret_cast<tbx::OsSpritePtr> ( get_sprite_pointer())); }
    std::string name() const;
    virtual bool is_valid() { return (_sprite_area != nullptr); }

    // x,y,req_width,req_height in OS Units
    virtual void plot(int left_x, int bottom_y,
                      tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);

};

class CLWimpSpriteImage : public CLBaseImage {
protected:
    tbx::WimpSprite *_wimp_sprite = nullptr;
    void _get_wimp_scale(int* factor) const;
    void _spriteop_plot_scaled(int left_x, int bottom_y, int* sf);

public:
    CLWimpSpriteImage(tbx::WimpSprite *spr);
    virtual ~CLWimpSpriteImage();

    int get_image_type() override { return CL_IMAGE_TYPE_WIMP_SPRITE; }

    virtual bool load(const std::string& filename) { return false; };
    virtual bool save(const std::string &filename) { return false; };
    virtual int size() { return sizeof(CLWimpSpriteImage) + 16; };

    inline tbx::WimpSprite* wimp_sprite() { return _wimp_sprite; }
    inline const char* name() { return  _wimp_sprite->name().c_str(); }
    inline const std::string name_str() { return  _wimp_sprite->name(); }
    virtual bool is_valid() { return _wimp_sprite != nullptr; }

    virtual void plot(int left_x, int bottom_y,
                      tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);
};


class CLDrawImage : public CLBaseImage {
protected:
    tbx::DrawFile _draw_file;
public:
    CLDrawImage() {};
    virtual ~CLDrawImage() {};
    int get_image_type() override { return CL_IMAGE_TYPE_DRAW; }
    virtual bool load(const std::string& filename);
    virtual int size() { return _draw_file.size(); }
    virtual void plot(int left_x, int bottom_y, tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);
    virtual bool is_valid() { return _draw_file.is_valid(); }
};

class CLJPEGImage : public CLBaseImage {
protected:
    tbx::JPEG _jpeg_file;
public:
    CLJPEGImage() {};
    CLJPEGImage(unsigned char* jpegbuf, unsigned long jpegSize);
    virtual ~CLJPEGImage() {};
    int get_image_type() override { return CL_IMAGE_TYPE_JPEG; }
    virtual bool load(const std::string& filename);
    virtual bool save(const std::string& filename);
    virtual int size() { return _jpeg_file.size(); }
//    virtual bool assign_jpeg(unsigned char* jpegbuf, unsigned long jpegSize);
    virtual void plot(int left_x, int bottom_y, tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);
    virtual bool is_valid() { return _jpeg_file.is_valid(); }
};


class CLArtWorksImage : public CLBaseImage {
protected:
    int _size = 0;
    static bool _mudules_loaded;
    std::string _aw_filename;
    void *_aw_image = nullptr;
    void *_aw_workspace = nullptr;
    artworks_handle _aw_handle;
    tbx::BBox _bounds;

public:
    CLArtWorksImage() { Logger:: debug("CLArtWorksImage %p", this); };
    virtual ~CLArtWorksImage();

    int get_image_type() override { return CL_IMAGE_TYPE_ARTWORKS; }
    static bool have_renderer() { return _mudules_loaded; }
    static bool load_artworks_modules();

    virtual bool load(const std::string& filename);
    virtual int size() { return _size; }
    virtual void plot(int left_x, int bottom_y, tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);
    virtual bool is_valid() { return _size > 0; }
};


class CLConvertedSpriteImage : public CLUserSpriteImage {
    friend CLImageMagickLoader;
    friend CLImagePNGLoader;
    friend CLImageJPGLoader;
public:
    int get_image_type() override { return CL_IMAGE_TYPE_CONVERTED_SPRITE; }

    CLConvertedSpriteImage() : CLUserSpriteImage() {};
    CLConvertedSpriteImage(const std::string& file_name) : CLUserSpriteImage(file_name) {};
    virtual bool load(const std::string& filename);
};


class CLPNGImage : public CLBaseImage {
private:
    unsigned char* _dataBuf = nullptr;
    unsigned long _dataSize = 0;
    CLUserSpriteImage *_spr = nullptr;
public:
    int get_image_type() override { return CL_IMAGE_TYPE_PNG; }

    CLPNGImage()  {};
    CLPNGImage(unsigned char* dataBuf, unsigned long dataSize, int width_px, int height_px);
    CLPNGImage(const std::string& file_name);
    virtual ~CLPNGImage();
    virtual bool load(const std::string& filename);
    virtual bool save(const std::string& filename);

    virtual bool is_valid() { return _dataBuf != nullptr; }
    virtual int size();

    virtual void plot(int left_x, int bottom_y, tbx::Colour background_colour = 0);
    virtual void plot_scaled(int left_x, int bottom_y,
                             int req_width, int req_height,
                             tbx::Colour background_colour = 0);
};


class CLImageFactory {
public:
    static std::vector<std::pair<int,std::string>> supported_save_formats;
    static std::vector<std::pair<int,std::string>> supported_load_formats;
    static CLUserSpriteImage* resize_image(int thumb_width_px, int thumb_height_px, CLBaseImage* src, bool centered_in_box = false, int offset_x = 0);
    static CLBaseImage* make_thumbnail(int thumb_width_px, int thumb_height_px, CLBaseImage* src);
    static CLBaseImage *load_image(const tbx::Path& image_p);
    static bool save_sprite_image(CLUserSpriteImage* src, const std::string& output_file, int filetype, const std::string& options);
    static bool save_image(CLBaseImage* src, const std::string& output_file, int filetype, const std::string& options, int twidth=0, int theight=0);
    static bool can_load(int filetype);
    static bool can_save(int filetype);
    static std::string get_image_type_str(int file_type);
};

#endif //CLOVERLEAF_CLIMAGE_H

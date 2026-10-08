//
// Created by lenz on 3/22/20.
//
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <kernel.h>
#include <swis.h>
#include <tbx/colour.h>
#include <tbx/path.h>
#include <tbx/application.h>
#include <tbx/osgraphics.h>
#include <tbx/oserror.h>
#include <oslib/osspriteop.h>
#include <oslib/osbyte.h>
#include <oslib/wimp.h>
#include "tinct.h"
#include "awrender.h"
#include "CLImage.h"
#include "CLImageJPGLoader.h"
#include "CLImagePNGLoader.h"
#include "CLImageSpriteLoader.h"
#include "CLImageMagickLoader.h"
#include "CLFFmpegLoader.h"
#include "CLUtils.h"
#include "Logger.h"

#define DATA_FORMAT_RGB ((os_mode_flags) 0x4u)
#define DATA_FORMAT BGR ((os_mode_flags) 0x0u)

unsigned int CLUserSpriteImage::initial_tinct_options = 0;
unsigned int CLUserSpriteImage::_color_translation_table[256];
bool CLUserSpriteImage::ro5extsprs = false;

#define osversion_200     (0xA1)
#define osversion_201     (0xA2)
#define osversion_300     (0xA3)
#define osversion_31x     (0xA4)
#define osversion_350     (0xA5)
#define osversion_360     (0xA6)
#define osversion_37x     (0xA7)
#define osversion_380_40X (0xA8)
#define osversion_4XX     (0xA9) /* also RISC OS "SIX" */
#define osversion_5       (0xAA)

#define os_version() \
    osbyte1(osbyte_IN_KEY, 0, 0xFF)

awrender_vdu_block CLBaseImage::_vdu_block;

CLBaseImage::CLBaseImage() { /*Log_debug("CLBaseImage::CLBaseImage %p", this); */}
CLBaseImage::~CLBaseImage() { /*Log_debug("CLBaseImage::~CLBaseImage w:%d, h:%d %p", _width, _height, this);*/ }

void CLBaseImage::read_vdu_info()
{
    static const os_VDU_VAR_LIST(4) var_list =
            {{
                     os_MODEVAR_XEIG_FACTOR,
                     os_MODEVAR_YEIG_FACTOR,
                     os_MODEVAR_LOG2_BPP,
                     -1,
             }};

    os_read_vdu_variables((const os_vdu_var_list *) &var_list,
                          (int *) &_vdu_block);
    wimp_read_true_palette(_vdu_block.palette);
}


int CLBaseImage::get_percent_scale_to_fit_in_box(const tbx::Size& sz, int max_upscale_percent) {
    int scale = 100;
    if (width() > height()) {
        if (max_upscale_percent > 100 || width() > sz.width) {
            scale = std::min(100 * sz.width / width(), max_upscale_percent);
        }
        if (height() * scale / 100 > sz.height) {
            scale = std::min(100 * sz.height / height(), max_upscale_percent);
        }
    } else {
        if (max_upscale_percent > 100 || height() > sz.height) {
            scale = std::min(100 * sz.height / height(), max_upscale_percent);
        }
        if (width() * scale / 100 > sz.width) {
            scale = std::min(100 * sz.width / width(), max_upscale_percent);
        }
    }
//    Log_debug("get_percent_scale_to_fit_in_box szw=%d szh=%d w=%d h=%d scale=%d", sz.width, sz.height, width(), height(), scale);
    return scale;
}

tbx::BBox CLBaseImage::get_scaled_and_centered_box(const tbx::BBox &fit_in_box, int scale_percent) {
    int req_width = width(),
        req_height = height(),
        box_width = fit_in_box.width(),
        box_height = fit_in_box.height(),
        x = 0, y = 0;

    if (scale_percent != 100) {
        req_height = height() * scale_percent / 100;
        req_width = width() * scale_percent / 100;
    }
    y = (box_height - req_height) / 2;
    x = (box_width - req_width) / 2;
    tbx::BBox box = tbx::BBox(fit_in_box.min.x + x, fit_in_box.min.y + y, fit_in_box.max.x - x, fit_in_box.max.y - y);
//    Log_debug("fit_in_box scale:%d %x %y  w:%d h:%d bw:%d bh:%d", scale_percent, x, y, fit_in_box.width(), fit_in_box.height(), box.width(), box.height());
    return tbx::BBox(fit_in_box.min.x + x, fit_in_box.min.y + y, fit_in_box.max.x - x, fit_in_box.max.y - y);
}

tbx::BBox CLBaseImage::get_scaled_and_centered_box(const tbx::BBox &fit_in_box) {
    int scale = get_percent_scale_to_fit_in_box(fit_in_box.size(), 100);
//    Log_debug("scale=%d", scale);
    return get_scaled_and_centered_box(fit_in_box, scale);
}

bool CLDrawImage::load(const std::string &filename) {
    if (!_draw_file.load(filename)) {
        Logger::warn("Failed to load DRAW image %s", filename.c_str());
        return false;
    }
    Log_debug("CLDrawImage::load DRAW image %s", filename.c_str());
    tbx::DrawTransform tr;
    tbx::BBox bounds;
    _draw_file.bounds(bounds, &tr);
    _width = bounds.width() / 256;
    _height = bounds.height() / 256;
    _width_px = _width  / 2;
    _height_px = _height / 2;
    _has_alpha = true;
//    Log_debug("CLDrawImage::load bbox %d,%d,%d,%d w:%d h:%d ww:%d hh:%d", bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y, bounds.width(), bounds.height(), _width_px, _height_px);
    return true;
}

void CLDrawImage::plot_scaled(int left_x, int bottom_y, int req_width, int req_height, tbx::Colour background_colour) {
    if (!is_valid()) {
        return;
    }
    int scale = get_percent_scale_to_fit_in_box(tbx::Size(req_width, req_height), 1000);
    tbx::DrawTransform dt;
    dt.scale(tbx::Fixed16(scale) / tbx::Fixed16(100));

    tbx::BBox bounds;
    _draw_file.bounds(bounds, &dt);

    int x = left_x - (bounds.min.x / 256),
        y = bottom_y - (bounds.min.y / 256);

//    if (scale != 100) {
//        x = x * scale / 100;
//        y = y * scale / 100;
//    }
    dt.translate_os(x,y);
    //dt.scale_os();
//    Log_debug("CLDrawImage::plot_scaled x=%d y=%d scale=%d bbox %d,%d,%d,%d %d,%d,%d,%d", x, y, scale, bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y, bounds.min.x / 256, bounds.min.y / 256, bounds.max.x / 256, bounds.max.y / 256);
    _draw_file.render(&dt, 0, -1);
}

void CLDrawImage::plot(int left_x, int bottom_y, tbx::Colour background_colour) {
    if (!is_valid()) {
        return;
    }
    tbx::DrawTransform dt;
    tbx::BBox bounds;
    _draw_file.bounds(bounds, &dt);
    int x = left_x - (bounds.min.x / 256),
            y = bottom_y - (bounds.min.y / 256);

    dt.translate_os(x,y);
//    Log_debug("CLDrawImage::plot_scaled x=%d y=%d scale=%d bbox %d,%d,%d,%d %d,%d,%d,%d", x, y, scale, bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y, bounds.min.x / 256, bounds.min.y / 256, bounds.max.x / 256, bounds.max.y / 256);
    _draw_file.render(&dt, 0, 2);
}


CLUserSpriteImage::CLUserSpriteImage() :
    _sprite_area(nullptr)
{
//    Log_debug("CLSpriteImage::CLSpriteImage() %p", this);
}

CLUserSpriteImage::CLUserSpriteImage(const CLUserSpriteImage& src)
{
    _sprite_area = (osspriteop_area*)malloc(src._sprite_area->size);
    if (!_sprite_area) {
        Logger::error("CLUserSpriteImage(const CLUserSpriteImage& src) Not enough memory");
        throw std::runtime_error("CLUserSpriteImage(const CLUserSpriteImage& src) Not enough memory");
    }
    memcpy(_sprite_area, src._sprite_area, src._sprite_area->size);
}

CLUserSpriteImage::CLUserSpriteImage(const std::string &file_name) : CLUserSpriteImage() {
    load(file_name);
//    Log_debug("CLSpriteImage::CLSpriteImage(filename=%s w=%d,h=%d,alpha=%d) %p", file_name.c_str(), _width_px, _height_px,_has_alpha, this);
}

CLUserSpriteImage::CLUserSpriteImage(int width_px, int height_px, bool has_alpha, const char *spr_name) {
    _sprite_area = create_sprite_area(width_px, height_px, spr_name);
    _has_alpha = has_alpha;
    _width_px = width_px;
    _height_px = height_px;
    _width = width_px * 2;
    _height = height_px * 2;
//    Log_debug("CLSpriteImage::CLSpriteImage(w=%d,h=%d,alpha=%d) %p", width_px, height_px,has_alpha, this);
}

CLUserSpriteImage::~CLUserSpriteImage() {
//    Log_debug("CLSpriteImage::CLSpriteImageeImage %p", this);
    if (_sprite_area) {
//        Log_debug("CLSpriteImage::CLSpriteImageeImage free area:%p", _sprite_area);
        free(_sprite_area);
    }
}

osspriteop_area* CLUserSpriteImage::create_sprite_area(int width_px, int height_px, const char *spr_name) {
    int area_size = 16 + 44 + width_px * height_px * 4;
    osspriteop_header *sprite;
    osspriteop_area *sprite_area = (osspriteop_area*)malloc(area_size+4);
    if (!sprite_area) {
        Logger::error("CLUserSpriteImage::create_sprite_area. Not enough memory");
        return nullptr;
    }

    memset(sprite_area, 0, area_size);

    /* area control block */
    sprite_area->size = area_size;
    sprite_area->sprite_count = 1;
    sprite_area->first = 16;
    sprite_area->used = area_size;

    /* sprite control block */
    sprite = (osspriteop_header *) (sprite_area + 1);
    sprite->size = area_size - 16;
    if (spr_name == nullptr) {
        strncpy(sprite->name, "bitmap", 12);
    } else {
        strncpy(sprite->name, spr_name, 12);
    }
    sprite->width = width_px - 1;
    sprite->height = height_px - 1;
    sprite->left_bit = 0;
    sprite->right_bit = 31;
    sprite->image = sprite->mask = 44;
    if (ro5extsprs) {
        sprite->mode = SPREXT_TINCT_LIKE_MODE;
    } else {
        sprite->mode = tinct_SPRITE_MODE;
    }

//    Log_debug("CLUserSpriteImage::CLUserSpriteImage create_sprite_area area:%p spr:%p w:%d, h:%d len:%d", sprite_area, sprite, width_px, height_px, area_size);
    return sprite_area;
}

void CLUserSpriteImage::detect_mode() {
    int mode_flags = 0;
    xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_MODE_FLAGS, &mode_flags,NULL);
    int data_format = (mode_flags & os_MODE_FLAG_DATA_FORMAT) >> os_MODE_FLAG_DATA_FORMAT_SHIFT;
    if (data_format == DATA_FORMAT_RGB) {
        Logger::info("RGB format detected. Set tinct_USE_OS_SPRITE_OP");
        initial_tinct_options = tinct_USE_OS_SPRITE_OP;
    } else {
        Logger::info("BGR format detected. Set tinct_ERROR_DIFFUSE");
        initial_tinct_options = tinct_ERROR_DIFFUSE;
    }
    int os     = os_version();
    int sprext = CLUtils::get_module_version("SpriteExtend");

    /* Detect RISC OS 5 and SpriteExtend 1.52 which is the first version that
     * knows about extended sprite words. */
    ro5extsprs = (os >= osversion_5) && (sprext >= 0x00015200);
    Log_debug("has RO5 and SpriteExtend module = %d", ro5extsprs);
}


bool CLUserSpriteImage::load_spritearea(const std::string &filename) {
    if (_sprite_area) {
        free(_sprite_area);
        _sprite_area = nullptr;
    }

    if (!tbx::Path(filename).exists()) {
        Logger::error("CLUserSpriteImage::load not exists file:%s", filename.c_str());
        return false;
    }

    _kernel_swi_regs regs;
    _kernel_oserror* err;
    // get size of file
    regs.r[0] = 5;
    regs.r[1] = (int) filename.c_str();
    if (_kernel_swi(OS_File, &regs, &regs) == NULL) {
        unsigned int fileSize = regs.r[4];
        fileSize += 4;
        unsigned int area_size = (fileSize >> 2);
        if (fileSize & 3) ++area_size; // Round up to full number of words
        area_size = (area_size << 2);
        if (area_size < 16) area_size = 16;
        _sprite_area = (osspriteop_area *) malloc(area_size + 4);
        if (!_sprite_area) {
            Logger::error("CLUserSpriteImage::load not enough memory, file:%s", filename.c_str());
            return false;
        }
        //Log_debug("allocated %d file size %d", (area_size + 4), fileSize);
        _sprite_area->size = area_size;
        _sprite_area->first = 16;
        // init area
        regs.r[0] = 9 + 256;
        regs.r[1] = (int) _sprite_area;
        err = _kernel_swi(OS_SpriteOp, &regs, &regs);
        if (err == NULL) {
            //Log_debug("CLSpriteImage::load_spritearea init %s size:%d", filename.c_str(), fileSize);
            // load spritearea
            regs.r[0] = 10 + 512;
            regs.r[1] = (int) _sprite_area;
            regs.r[2] = (int) filename.c_str();
            err = _kernel_swi(OS_SpriteOp, &regs, &regs);
            if (err == NULL) {
                osspriteop_header *spr = (osspriteop_header *) (((char*)_sprite_area) + _sprite_area->first);
                if (spr->mode == SPREXT_TINCT_LIKE_MODE && !ro5extsprs) {
                    Log_debug("replace sprite %s mode %x -> %x", spr->name, spr->mode, tinct_SPRITE_MODE);
                    spr->mode == tinct_SPRITE_MODE;
                }
                if (spr->mode == tinct_SPRITE_MODE || spr->mode == SPREXT_TINCT_LIKE_MODE) {
                    _width_px = spr->width + 1;
                    _height_px = spr->height + 1;
                    _width = _width_px * 2;
                    _height = _height_px * 2;
                    _has_alpha = true;
                    return true;
                } else {
                    _kernel_swi_regs in, out;
                    in.r[0] = 40 + 512;
                    in.r[1] = (int)(_sprite_area);
                    in.r[2] = (int)spr;
                    err = _kernel_swi(OS_SpriteOp, &in, &out);
                    if (err == NULL) {
                        _width_px = out.r[3];
                        _height_px = out.r[4];
                        _has_alpha = (out.r[5] != 0);
                        spr->mode = reinterpret_cast<os_mode>(out.r[6]);

                        tbx::ModeInfo mi((int)spr->mode);
                        tbx::Point eig = mi.eig();
                        _width = _width_px << eig.x;
                        _height = _height_px << eig.y;
                        Log_debug("CLUserSpriteImage::load loaded %s first:%d %p %p spr:%s w:%d (%d) h:%d (%d)", filename.c_str(), _sprite_area->first, _sprite_area, spr, spr->name, _width_px, _width, _height_px, _height);
                        return true;
                    } else {
                        Logger::error("CLUserSpriteImage::load failed to get sprite info, sprite:%s (%s), error:(%d) %s", filename.c_str(), spr->name, err->errnum,
                                      err->errmess);
                    }
                }
            } else {
                Logger::error("CLUserSpriteImage::load failed to load sprite area, sprite:%s, error:(%d) %s", filename.c_str(), err->errnum,
                              err->errmess);
            }
        } else {
            Logger::error("CLUserSpriteImage::load failed to init sprite area, sprite:%s, error:(%d) %s", filename.c_str(), err->errnum, err->errmess);
        }
    }

    if (_sprite_area) {
        Log_debug("CLUserSpriteImage::load failed to load %s", filename.c_str());
        free(_sprite_area);
        _sprite_area = 0;
    }
    return false;
}

bool CLUserSpriteImage::load(const std::string &filename) {
    return load_spritearea(filename);
}

bool CLUserSpriteImage::save(const std::string &filename) {
    if (_sprite_area) {
        try {
            tbx::SpriteArea area((tbx::OsSpriteAreaPtr)_sprite_area, false);
            area.save(filename);
            return true;
        } catch (tbx::OsError &err) {
            Logger::error("CLUserSpriteImage::save can't save spritearea to:%s error: (%d) %s", filename.c_str(), err.number(), err.what());
            return false;
        }
    }
}

int CLUserSpriteImage::size() {
    if (_sprite_area) {
        return _sprite_area->size + sizeof(CLUserSpriteImage);
    }
    return 0;
}


int CLUserSpriteImage::_prepare_color_translation_table() {
    _kernel_oserror *error;
    _kernel_swi_regs regs;
    int num_clrs = num_colors();

//    Log_debug("CLSpriteImage::_prepare_color_translation_table palette:%d colors:%d area:%p spr:%p (%s)", has_palette(), num_clrs, get_area_pointer(), get_sprite_pointer(), get_sprite_pointer()->name);

    if (num_clrs > 256) {
        Logger::warn("CLUserSpriteImage::_prepare_color_translation_table num_colors=%d", num_clrs);
        return 0;
    }
//    Log_debug("CLSpriteImage::_prepare_color_translation_table haspal:%d colors:%d mode:%x", has_palette(), num_clrs, get_sprite_pointer()->mode);
    if (has_palette() || num_clrs > 16)
    {
//        Log_debug("ColourTrans_SelectTable", 1);
        regs.r[0] = (int)get_area_pointer();
        regs.r[1] = (int)get_sprite_pointer();
        regs.r[2] = -1;
        regs.r[3] = -1;
        regs.r[4] = (int)_color_translation_table;
        // The following parameter is new to RiscOS 3.1
        // bit 0 - set if pointer to sprite directly, clear if pointer to name
        // bit 1 - set to use current palette for mode, clear for default palette
        regs.r[5] = 3;
        error = _kernel_swi(ColourTrans_SelectTable, &regs, &regs);
//        error = _kernel_swi(ColourTrans_GenerateTable, &regs, &regs);
        if (error) {
            Logger::error("ColourTrans_SelectTable error:(%d) '%s'", error->errnum, error->errmess);
            return 0;
        }
    } else {
//        Log_debug("Wimp_ReadPixTrans", 1);
        regs.r[0] = 512;
        regs.r[1] = (int)get_area_pointer();
        regs.r[2] = (int)get_sprite_pointer();
        regs.r[6] = 0;
        regs.r[7] = (int)_color_translation_table;
        error = _kernel_swi(Wimp_ReadPixTrans, &regs, &regs);
        if (error) {
            Logger::error("Wimp_ReadPixTrans error:(%d) '%s'", error->errnum, error->errmess);
            return 0;
        }
    }
    return (int)_color_translation_table;
}

void CLUserSpriteImage::_get_wimp_scale(int* factor) const
{
    _kernel_swi_regs regs;
    regs.r[0] = 512; // Sprite area specified
    regs.r[1] = (int)get_area_pointer();
    regs.r[2] = (int)get_sprite_pointer();
    regs.r[6] = (int)(factor);
    regs.r[7] = 0;
    _kernel_swi(Wimp_ReadPixTrans, &regs, &regs);
}

void CLUserSpriteImage::_spriteop_plot_scaled(int left_x, int bottom_y, int* sf) {
    _kernel_swi_regs regs;
    _kernel_oserror *error = nullptr;
    int color_trans_table = 0;
    unsigned int opts = osspriteop_USE_MASK | osspriteop_USE_PALETTE;
    tbx::ModeInfo mi((int)get_sprite_pointer()->mode);
//        Log_debug("_spriteop_plot_scaled spr %s clrs:%d", get_sprite_pointer()->name, mi.colours());
    if (get_sprite_pointer()->mode != tinct_SPRITE_MODE) {
        color_trans_table = _prepare_color_translation_table();
    }
    regs.r[0] = 52 + 512;
    regs.r[1] = (int)(get_area_pointer());
    regs.r[2] = (int)get_sprite_pointer();
    regs.r[3] = left_x;
    regs.r[4] = bottom_y;
    regs.r[5] = opts;
    regs.r[6] = (int)sf;
    regs.r[7] = color_trans_table;
    error = _kernel_swi(OS_SpriteOp, &regs, &regs);

    if (error) {
        Logger::error("CLUserSpriteImage::_spriteop_plot_scaled error:(%d) %s area:%p sf:%p [%d,%d,%d,%d]", error->errnum, error->errmess, get_area_pointer(), sf, sf[0], sf[1], sf[2],sf[3]);
    }
}

void CLUserSpriteImage::plot(int left_x, int bottom_y,
                         tbx::Colour background_colour)
{
    unsigned int tinct_options = 0;
    if (_sprite_area == nullptr) {
        return;
    }

    _kernel_oserror *error = nullptr;

    /*      Set up our flagword
    */
    if (get_sprite_pointer()->mode == tinct_SPRITE_MODE && !ro5extsprs) {
        tinct_options |= (initial_tinct_options | (background_colour & 0xffffff00));

        if (has_alpha()) {
//            Logger::info("Tinct_PlotAlpha %s", get_sprite_pointer()->name);
            error = _swix(Tinct_PlotAlpha, _INR(2,4) | _IN(7),
                          ( ((unsigned char *)_sprite_area) + _sprite_area->first),
                          left_x, bottom_y,
                          tinct_options);
        } else {
//            Logger::info("Tinct_Plot %s", get_sprite_pointer()->name);
            error = _swix(Tinct_Plot, _INR(2, 4) | _IN(7),
                          (((unsigned char *) _sprite_area) + _sprite_area->first),
                          left_x, bottom_y,
                          tinct_options);
        }
    } else {
        int sf[4];
        _get_wimp_scale(sf);
        _spriteop_plot_scaled(left_x, bottom_y, sf);
    }

    if (error) {
        Logger::error("CLUserSpriteImage::plot %s: 0x%x: %s",
              (has_alpha() ? "alpha" : ""), error->errnum, error->errmess);
    }
}


void CLUserSpriteImage::plot_scaled(int left_x, int bottom_y,
                                int req_width, int req_height,
                                tbx::Colour background_colour)
{
    unsigned int tinct_options = 0;
    if (_sprite_area == nullptr) {
        return;
    }

    _kernel_oserror *error = nullptr;

    /*      Set up our flagword
    */
    if (get_sprite_pointer()->mode == tinct_SPRITE_MODE && !ro5extsprs) {
        tinct_options |= (initial_tinct_options | (background_colour & 0xffffff00));

        if (has_alpha()) {
//            Logger::info("Tinct_PlotScaledAlpha %d %d orig %dx%d req %dx%d", left_x, bottom_y, width(), height(), req_width, req_height);
            error = _swix(Tinct_PlotScaledAlpha, _INR(2,7),
                          ( ((unsigned char *)_sprite_area) + _sprite_area->first),
                          left_x, bottom_y,
                          req_width, req_height,
                          tinct_options);
        } else {
//            Logger::info("Tinct_PlotScaled %d %d orig %dx%d req %dx%d", left_x, bottom_y, width(), height(), req_width, req_height);
            error = _swix(Tinct_PlotScaled, _INR(2,7),
                          ( ((unsigned char *)_sprite_area) + _sprite_area->first),
                          left_x, bottom_y,
                          req_width, req_height,
                          tinct_options);
        }
    } else {
        int sf[4];
        _get_wimp_scale(sf);
        sf[0] *= req_width;
        sf[1] *= req_height;
        sf[2] *= width();
        sf[3] *= height();
        _spriteop_plot_scaled(left_x, bottom_y, sf);
    }

    if (error) {
        Logger::error("CLUserSpriteImage::plot_scaled %s: 0x%x: %s",
                     (has_alpha() ? "alpha" : ""), error->errnum, error->errmess);
    }
}

std::string CLUserSpriteImage::name() const
{
    char name[13];
    strncpy(name, get_sprite_pointer()->name, 12);
    name[12] = 0;
    return std::string(name);
}


CLWimpSpriteImage::CLWimpSpriteImage(tbx::WimpSprite *spr)  {
    if (spr->exist()) {
        tbx::Size sz(0, 0);
        int mode;
        if (spr->info(&sz, &mode)) {
            tbx::ModeInfo mi(mode);
            _width = sz.width << mi.eig().x;
            _height = sz.height << mi.eig().y;
            _width_px = sz.width;
            _height_px = sz.height;
            _wimp_sprite = new tbx::WimpSprite(spr->name());
//            Log_debug(("CLSpriteImage::CLSpriteImage(wimpsprite=%s w=%d,h=%d, pxw=%d, pxh=%d alpha=%d) %p", spr->name().c_str(), width(), height(), _width_px,
//                          _height_px, _has_alpha, this);
        }
    } else {
        Logger::error("CLWimpSpriteImage::CLWimpSpriteImage(wimpsprite=%s) not exists!", spr->name().c_str());
    }
}

CLWimpSpriteImage::~CLWimpSpriteImage() {
    delete _wimp_sprite;
}

void CLWimpSpriteImage::_get_wimp_scale(int* factor) const
{
    _kernel_swi_regs regs;
    regs.r[0] = 256; // Sprite area specified
    regs.r[1] = (int)1;
    regs.r[2] = (int)_wimp_sprite->name().c_str();
    regs.r[6] = (int)(factor);
    regs.r[7] = 0;
    _kernel_swi(Wimp_ReadPixTrans, &regs, &regs);
}

static tbx::TranslationTable wimp_spr_tt;
void CLWimpSpriteImage::_spriteop_plot_scaled(int left_x, int bottom_y, int* sf) {
//    Log_debug("_spriteop_plot_scaled wimp_spr %s", _wimp_sprite->name().c_str());
    tbx::ScaleFactors scf(sf[0], sf[1], sf[2], sf[3]);
    wimp_spr_tt.create(_wimp_sprite);
    _wimp_sprite->plot_scaled(left_x, bottom_y, &scf, &wimp_spr_tt);
}

void CLWimpSpriteImage::plot(int left_x, int bottom_y,
                         tbx::Colour background_colour)
{
    if (_wimp_sprite) {
        int sf[4];
        _get_wimp_scale(sf);
        _spriteop_plot_scaled(left_x, bottom_y, sf);
    }
}


void CLWimpSpriteImage::plot_scaled(int left_x, int bottom_y,
                                int req_width, int req_height,
                                tbx::Colour background_colour)
{
    if (_wimp_sprite) {

        int sf[4];
        _get_wimp_scale(sf);
        sf[0] *= req_width;
        sf[1] *= req_height;
        sf[2] *= width();
        sf[3] *= height();
        _spriteop_plot_scaled(left_x, bottom_y, sf);
    }
}


bool CLConvertedSpriteImage::load(const std::string &filename) {
    tbx::Path file_path(filename);
    int filetype = file_path.file_type();
    if (_sprite_area) {
        free(_sprite_area);
        _sprite_area = nullptr;
    }
    switch (filetype) {
        case FILE_TYPE_PNG:
            CLImagePNGLoader::load(filename, this);
            break;
        case FILE_TYPE_JPEG:
            CLImageJPGLoader::load(filename, this);
            break;
        case FILE_TYPE_GIF:
        case FILE_TYPE_ICO:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_PSD:
        case FILE_TYPE_SVG:
        case FILE_TYPE_WEBP:
            if (CLImageMagickLoader::is_imagemagick_installed()) {
                CLImageMagickLoader::load(file_path, this, filetype);
            }
            break;
        case FILE_TYPE_MOVIEFS:
        case FILE_TYPE_MP4:
        case FILE_TYPE_MPG:
        case FILE_TYPE_MPEG:
        case FILE_TYPE_AVI:
        case FILE_TYPE_VOB:
            if (CLFFmpegLoader::is_ffmpeg_installed()) {
                CLFFmpegLoader::load(file_path, this, filetype);
            }
            break;
        default:
            Logger::error("CLConvertedSpriteImage::load %s unknown filetype %d", filename.c_str(), filetype);
            break;
    }
//    Log_debug("CLSpriteImage::load %s area:%p this:%p", filename.c_str(), _sprite_area, this);
    return (_sprite_area != nullptr);
}


CLUserSpriteImage* CLImageFactory::resize_image(int thumb_width_px, int thumb_height_px, CLBaseImage *src, bool centered_in_box, int offset_x) {
    if (!src->is_valid()) {
        return nullptr;
    }
    int save_size, i;
    char *save_area = nullptr;
    unsigned int save_regs[4];
    unsigned int *pixels_ptr;
    osspriteop_header *spr;
    _kernel_oserror *err;
    tbx::UserSprite checkb_spr = tbx::Application::instance()->sprite_area()->get_sprite("checkb");
    CLUserSpriteImage* thumb_sprite;
    // fit image in box
    if (centered_in_box) {
        thumb_sprite = new CLUserSpriteImage(thumb_width_px + offset_x, thumb_height_px, true);
    } else {
        if (src->width_px() > src->height_px()) {
            thumb_height_px =src->height_px() * thumb_width_px / src->width_px();
        } else {
            thumb_width_px = src->width_px() * thumb_height_px / src->height_px();
        }
        thumb_sprite = new CLUserSpriteImage(thumb_width_px, thumb_height_px, true);
    }

    // hack for artworks images (it unable to draw on new SPREXT sprites)
    if (src->get_image_type() == CL_IMAGE_TYPE_ARTWORKS) {
        thumb_sprite->get_sprite_pointer()->mode = tinct_SPRITE_MODE;
        pixels_ptr = (unsigned int*)thumb_sprite->get_sprite_pixels_pointer();
        for(i = 0; i < thumb_width_px * thumb_height_px; i++) {
            *pixels_ptr = 0xffffffff;
            pixels_ptr++;
        }
    }
    // read save area size
    err = _swix(OS_SpriteOp, _INR(0,2)|_OUT(3), 62+512,
          thumb_sprite->get_area_pointer(),
          thumb_sprite->get_sprite_pointer(),
          &save_size);

    if (err) {
        Logger::error("CLImageFactory::resize_image OS_SpriteOp (Read save area size) error: (%d) %s", err->errnum, err->errmess);
        goto error;
    }

    Log_debug("CLImageFactory::resize_image Save_size %d", save_size);
    save_area = new char[save_size];
    *((int *)save_area) = 0;

    // switch output to sprite
    save_regs[0] = 60|512;
    err = _swix(OS_SpriteOp, _INR(0,3)|_OUTR(0,3),
          save_regs[0],
          thumb_sprite->get_area_pointer(),
          thumb_sprite->get_sprite_pointer(),
          save_area,
          &save_regs[0],
          &save_regs[1],
          &save_regs[2],
          &save_regs[3]
    );
    if (err) {
        Logger::error("CLImageFactory::resize_image OS_SpriteOp (Switch output to sprite) error: (%d) %s", err->errnum, err->errmess);
        goto error;
    }

    Log_debug("CLImageFactory::resize_image start capture area:%p spr:%p w=%d h=%d orig w=%d orig h=%d", thumb_sprite->get_area_pointer(), thumb_sprite->get_sprite_pointer(), thumb_width_px, thumb_height_px, src->width_px(), src->height_px());

//    _swix(Tinct_Plot, _INR(2, 4) | _IN(7),
//          ((unsigned char *) checkb_spr.pointer()),
//          0, 0,
//          tinct_FILL_HORIZONTALLY | tinct_FILL_VERTICALLY);
    if (centered_in_box) {
        int new_thumb_height_px, new_thumb_width_px, x, y;
        if (src->width_px() > src->height_px()) {
            new_thumb_height_px = src->height_px() * thumb_width_px / src->width_px();
            new_thumb_width_px = thumb_width_px;
        } else {
            new_thumb_width_px = src->width_px() * thumb_height_px / src->height_px();
            new_thumb_height_px = thumb_height_px;
        }
        x = (thumb_width_px * 2 - new_thumb_width_px * 2) / 2 + offset_x * 2;
        y = (thumb_height_px * 2 - new_thumb_height_px * 2) / 2;
//        Log_debug("CLImageFactory::resize_image start capture x:%d y:%d neww:%d newh:%d", x, y, new_thumb_width_px, new_thumb_height_px);
        src->plot_scaled(x, y, new_thumb_width_px*2, new_thumb_height_px*2, tbx::Colour::white);
    } else {
        src->plot_scaled(0, 0, thumb_width_px*2, thumb_height_px*2, tbx::Colour::white);
    }

//    thumb_sprite->get_sprite_pointer()->mode = SPREXT_TINCT_LIKE_MODE;
    _swix(OS_SpriteOp, _INR(0,3),
                        save_regs[0], save_regs[1], save_regs[2], save_regs[3]
    );

    // hack for artworks images (revert transparency pixels)
    if (src->get_image_type() == CL_IMAGE_TYPE_ARTWORKS) {
        pixels_ptr = (unsigned int*)thumb_sprite->get_sprite_pixels_pointer();
        for(i = 0; i < thumb_width_px * thumb_height_px; i++) {
            if (*pixels_ptr & 0xff000000) {
                *pixels_ptr &= 0x00ffffff;
            } else {
                *pixels_ptr |= 0xff000000;
            }
            pixels_ptr++;
        }
        thumb_sprite->get_sprite_pointer()->mode = SPREXT_TINCT_LIKE_MODE;
    }

    goto exit;


error:
    delete thumb_sprite;
    thumb_sprite = nullptr;
exit:
    delete save_area;
    Log_debug("CLImageFactory::resize_image return thumb area:%p spr:%p w=%d h=%d", thumb_sprite->get_area_pointer(), thumb_sprite->get_sprite_pointer(), thumb_sprite->width(), thumb_sprite->height());
    return thumb_sprite;
}


CLBaseImage* CLImageFactory::make_thumbnail(int thumb_width_px, int thumb_height_px, CLBaseImage* src) {
    if (thumb_height_px > src->height_px() && thumb_width_px > src->width_px()) {
        thumb_height_px = src->height_px();
        thumb_width_px = src->width_px();
    }
    CLUserSpriteImage* thumb_sprite = CLImageFactory::resize_image(thumb_width_px, thumb_height_px, src);
    CLBaseImage *thumb_img = nullptr;
    if (thumb_sprite) {
//        Log_debug("thumb resized %p pixels=%p", thumb_sprite->get_sprite_pointer(), thumb_sprite->get_sprite_pixels_pointer());
        if (src->has_alpha()) {
            thumb_img = CLImagePNGLoader::compress(thumb_sprite->get_sprite_pointer(), 6);
        } else {
            thumb_img = CLImageJPGLoader::compress(thumb_sprite->get_sprite_pointer(), 60);
        }
        delete thumb_sprite;
        if (thumb_img) {
            return thumb_img;
        } else {
            Logger::error("Can't convert or load thumbnail sprite to JPEG or PNG");
            return nullptr;
        }
    } else {
        Logger::error("Can't resize sprite src:%p w:%d h:%d", src, src->width_px(), src->height_px());
        return nullptr;
    }
}

CLBaseImage* CLImageFactory::load_image(const tbx::Path &image_p) {
    int ft = image_p.file_type();
    CLBaseImage *img = nullptr;
    switch (ft) {
        case FILE_TYPE_DRAW:
            img = new CLDrawImage();
            break;
        case FILE_TYPE_JPEG:
            img = new CLJPEGImage();
            break;
        case FILE_TYPE_PNG:
            img = new CLPNGImage();
            break;
        case FILE_TYPE_ARTWORKS:
            img = new CLArtWorksImage();
            break;
        case FILE_TYPE_SPRITE:
            img = new CLUserSpriteImage();
            break;
        default:
            img = new CLConvertedSpriteImage();
            break;
    }
    if (img) {
//        Log_debug("img ft:%x %p", image_p.file_type(), img);
        if (!img->load(image_p.name())) {
//            Log_debug("img deleted %p", img);
            delete img;
            img = nullptr;
        }
    }
//    Log_debug("img ret %p", img);
    return img;
}

bool CLImageFactory::can_load(int filetype) {
    switch (filetype) {
        case FILE_TYPE_JPEG:
        case FILE_TYPE_PNG:
        case FILE_TYPE_DRAW:
        case FILE_TYPE_SPRITE:
            return true;
        case FILE_TYPE_ARTWORKS:
            return CLArtWorksImage::have_renderer();
            return true;
        case FILE_TYPE_GIF:
        case FILE_TYPE_ICO:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_SVG:
        case FILE_TYPE_WEBP:
            return CLImageMagickLoader::is_imagemagick_installed();
        case FILE_TYPE_MOVIEFS:
        case FILE_TYPE_MP4:
        case FILE_TYPE_MPG:
        case FILE_TYPE_MPEG:
        case FILE_TYPE_AVI:
        case FILE_TYPE_VOB:
            return CLFFmpegLoader::is_ffmpeg_installed();
        default:
            return false;
    }
}

std::vector<std::pair<int,std::string>> CLImageFactory::supported_save_formats = {
        {FILE_TYPE_JPEG, "JPEG"},
        {FILE_TYPE_PNG, "PNG"},
        {FILE_TYPE_TIFF, "TIFF"},
        {FILE_TYPE_GIF, "GIF"},
        {FILE_TYPE_WEBP, "WEBP"},
        {FILE_TYPE_SPRITE, "Sprite"}
};

std::vector<std::pair<int,std::string>> CLImageFactory::supported_load_formats = {
        {FILE_TYPE_PNG, "PNG"},
        {FILE_TYPE_JPEG, "JPEG"},
        {FILE_TYPE_ICO, "ICO"},
        {FILE_TYPE_GIF, "GIF"},
        {FILE_TYPE_SPRITE, "Sprite"},
        {FILE_TYPE_DRAW, "DRAW"},
        {FILE_TYPE_WEBP, "WEBP"},
        {FILE_TYPE_PSD, "PSD"},
        {FILE_TYPE_TIFF, "TIFF"},
        {FILE_TYPE_SVG, "SVG"},
        {FILE_TYPE_PDF, "PDF"},
        {FILE_TYPE_ARTWORKS, "ArtWorks"},

        {FILE_TYPE_MOVIEFS, "MOVIEFS"},
        {FILE_TYPE_MP4, "MP4"},
        {FILE_TYPE_MPG, "MPG"},
        {FILE_TYPE_MPEG, "MPEG"},
        {FILE_TYPE_AVI, "AVI"},
        {FILE_TYPE_VOB, "VOB"}
};

bool CLImageFactory::save_sprite_image(CLUserSpriteImage *src, const std::string &output, int filetype, const std::string& options) {
    return CLImageMagickLoader::save(src, output, filetype, options);
}

bool CLImageFactory::save_image(CLBaseImage *src, const std::string &output, int filetype, const std::string& options, int twidth, int theight) {
    bool result = false;
    if (!twidth) {
        twidth = src->width_px();
    }
    if (!theight) {
        theight = src->height_px();
    }
    CLUserSpriteImage *img = CLImageFactory::resize_image(twidth, theight, src);
    if (!img) {
        Logger::error("CLImageFactory::save_image can't make sprite from source");
    }
    result = CLImageFactory::save_sprite_image(img, output, filetype, options);
    delete img;
    return result;
}

std::string CLImageFactory::get_image_type_str(int file_type) {
    for(auto file_format : supported_load_formats) {
        if (file_format.first == file_type) {
            return file_format.second;
        }
    }
    return std::string("Unknown");
}


CLJPEGImage::CLJPEGImage(unsigned char *jpegbuf, unsigned long jpegSize) {
//    Log_debug("_jpeg_file.loading buf %p %p s=%d", this, jpegbuf, jpegSize);
    if (_jpeg_file.load(jpegbuf, jpegSize)) {
        Log_debug("_jpeg_file.load buf %p %p s=%d", this, jpegbuf, jpegSize);
        _width_px = _jpeg_file.width();
        _height_px = _jpeg_file.height();
        _width = _width_px * 2;
        _height = _height_px * 2;
    } else {
        _width_px = 0;
        _height_px = 0;
        _width = 0;
        _height = 0;
    }
}

bool CLJPEGImage::load(const std::string &filename) {
//    Log_debug("CLJPEGImage::load %s", filename.c_str());
    bool result = _jpeg_file.load(filename);
    if (result) {
        _width_px = _jpeg_file.width();
        _height_px = _jpeg_file.height();
        _width = _width_px * 2;
        _height = _height_px * 2;
    }
    return result;
}

void CLJPEGImage::plot(int left_x, int bottom_y, tbx::Colour background_colour) {
    if (is_valid()) {
        _jpeg_file.plot(left_x, bottom_y);
    }
}

void CLJPEGImage::plot_scaled(int left_x, int bottom_y, int req_width, int req_height, tbx::Colour background_colour) {
    if (is_valid()) {
        tbx::ScaleFactors sf(req_width, req_height, width(), height());
        _jpeg_file.plot(left_x, bottom_y, sf);
    }
}

bool CLJPEGImage::save(const std::string &filename) {
    Log_debug("_jpeg_file.save buf %p %s", this, filename.c_str());
    _jpeg_file.save(filename);
}


CLPNGImage::CLPNGImage(unsigned char* dataBuf, unsigned long dataSize, int width_px, int height_px) {
    _dataBuf = static_cast<unsigned char *>(malloc(dataSize));
    if (_dataBuf) {
        memcpy(_dataBuf, dataBuf, dataSize);
        _dataSize = dataSize;
        _width_px = width_px;
        _width = _width_px * 2;
        _height_px = height_px;
        _height = _height_px * 2;
        _has_alpha = true;
    } else {
        Logger::error("CLPNGImage::CLPNGImage can't allocate %d bytes of memory", dataSize);
    }
}

CLPNGImage::~CLPNGImage() {
    if (_dataBuf) {
        free(_dataBuf);
        _dataBuf = nullptr;
    }
    delete _spr;
}

CLPNGImage::CLPNGImage(const std::string& filename) {
    load(filename);
}

bool CLPNGImage::load(const std::string& filename) {
    if (_dataBuf) {
        free(_dataBuf);
        _dataBuf = nullptr;
        _dataSize = 0;
    }
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) {
        Logger::error("CLPNGImage::load can't open file %s to read", filename.c_str());
        return false;
    }
    fseek(file, 0, SEEK_END);
    unsigned long size = ftell(file);
    if (size > 0) {
        fseek(file, 0, SEEK_SET);
        _dataBuf = static_cast<unsigned char *>(malloc(size));
        if (_dataBuf) {
            int readed = fread(_dataBuf, size, 1, file);
//            Log_debug("Read to memory size %d readed cnt:%d", size, readed);
            if (readed != 1) {
                free(_dataBuf);
                _dataBuf = nullptr;
                Logger::error("CLPNGImage::load can't read file %s to memory", filename.c_str());
            } else {
                _dataSize = size;
                fclose(file);

                _spr = CLImagePNGLoader::decompess(_dataBuf, _dataSize);
                if (_spr) {
                    _height = _spr->height();
                    _width = _spr->width();
                    _height_px = _spr->height_px();
                    _width_px = _spr->width_px();
                    _has_alpha = true;
                    return true;
                } else {
                    Logger::error("CLPNGImage::load can't decode PNG file %s", filename.c_str());
                    free(_dataBuf);
                    _dataBuf = nullptr;
                    _dataSize = 0;
                    return false;
                }
            }
        } else {
            Logger::error("CLPNGImage::load can't allocate %d bytes of memory", size);
        }
    } else {
        Logger::error("CLPNGImage::load can't read file %s to memory, file size = 0", filename.c_str());
    }
    fclose(file);
    return false;
}

bool CLPNGImage::save(const std::string& filename) {
    if (_dataBuf) {
        FILE *file = fopen(filename.c_str(), "wb");
        if (!file) {
            Logger::error("CLPNGImage::save can't open file %s to write", filename.c_str());
            return false;
        }
        if (fwrite(_dataBuf, _dataSize, 1, file) != 1) {
            Logger::error("CLPNGImage::save can't save file %s", filename.c_str());
            return false;
        }
        fclose(file);
        tbx::Path(filename).file_type(0xb60);
    }
    return true;
}

int CLPNGImage::size() {
    if (_spr) {
        return _dataSize + _spr->size() + sizeof(CLPNGImage);
    } else {
        return _dataSize + sizeof(CLPNGImage);
    }
}

void CLPNGImage::plot(int left_x, int bottom_y, tbx::Colour background_colour) {
    if (!_spr && is_valid()) {
        _spr = CLImagePNGLoader::decompess(_dataBuf, _dataSize);
    }
    if (_spr) {
        _spr->plot(left_x, bottom_y, background_colour);
        delete _spr;
        _spr = nullptr;
    }
}

void CLPNGImage::plot_scaled(int left_x, int bottom_y, int req_width, int req_height, tbx::Colour background_colour) {
    if (!_spr && is_valid()) {
        _spr = CLImagePNGLoader::decompess(_dataBuf, _dataSize);
    }
    if (_spr) {
        tbx::ScaleFactors sf(req_width, req_height, width(), height());
        _spr->plot_scaled(left_x, bottom_y, req_width, req_height, background_colour);
        delete _spr;
        _spr = nullptr;
    }
}

//_kernel_oserror *artworks_callback(int                     new_size,
//                                   awrender_callback_regs *regs,
//                                   artworks_handle        *handle)
//{
//    regs->resizable_size = flex_size(handle->resizable_block);
//
//    if (new_size > regs->resizable_size)
//    {
//        /* small error block to save space - yuck */
//        static char no_mem[] = "\0\0\0\1NoMem";
//
//        if (flex_extend(handle->resizable_block, new_size) == 0)
//            return (_kernel_oserror *) &no_mem;
//
//        regs->resizable_size = new_size;
//    }
//
//    regs->resizable_block = *handle->resizable_block;
//
//    if ((int) handle->fixed_block == -1)
//    {
//        regs->fixed_block = (void *) -1;
//        regs->fixed_size  = regs->resizable_size;
//    }
//    else
//    {
//        regs->fixed_block = *handle->fixed_block;
//        regs->fixed_size  = flex_size(handle->fixed_block);
//    }
//
//    return NULL;
//}

bool CLArtWorksImage::_mudules_loaded = false;

_kernel_oserror *artworks_callback(int                     new_size,
                                   awrender_callback_regs *regs,
                                   artworks_handle        *handle)
{
    regs->resizable_size = flex_size(handle->resizable_block);

    if (new_size > regs->resizable_size)
    {
        /* small error block to save space - yuck */
        static char no_mem[] = "\0\0\0\1NoMem";

        if (flex_extend(handle->resizable_block, new_size) == 0)
            return (_kernel_oserror *) &no_mem;

        regs->resizable_size = new_size;
    }

    regs->resizable_block = *handle->resizable_block;

    if ((int) handle->fixed_block == -1)
    {
        regs->fixed_block = (void *) -1;
        regs->fixed_size  = regs->resizable_size;
    }
    else
    {
        regs->fixed_block = *handle->fixed_block;
        regs->fixed_size  = flex_size(handle->fixed_block);
    }

    return NULL;
}

CLArtWorksImage::~CLArtWorksImage() {
    Log_debug("CLArtWorksImage::~CLArtWorksImage %p", this);
    if (_aw_image) {
        flex_free(&_aw_image);
        _aw_image = nullptr;
    }
    if (_aw_workspace) {
        flex_free(&_aw_workspace);
        _aw_workspace = nullptr;
    }
}

bool CLArtWorksImage::load_artworks_modules() {
    if (!_mudules_loaded)   {
        /* Check the ArtWorks renderer is present. */
        if (xos_cli("LoadArtWorksModules")) {/* no _kernel_system in GCC */
            Logger::error("LoadArtWorksModules failed");
            return false;
        } else {
            _mudules_loaded = true;
        }
    }
    return true;
}

bool CLArtWorksImage::load(const std::string &filename) {
    _aw_filename = filename;
    os_error       *e;
    int             file_size;

    tbx::Path aw_file_path(filename);
    tbx::PathInfo aw_file_info;
    _size = 0;
    aw_file_path.path_info(aw_file_info);

    file_size = aw_file_info.length();
    if (flex_alloc(&_aw_image, file_size) == 0) {
        Logger::error("Out of memory at load of ArtWorks image %s", filename.c_str());
        return false;
    }
    if (flex_alloc(&_aw_workspace,
                   awrender_DefaultWorkSpace) == 0)
    {
        flex_free(&_aw_image);
        Logger::error("Out of memory at load of ArtWorks image %s", filename.c_str());
        return false;
    }
    std::FILE *fp = std::fopen(filename.c_str(), "rb");
    if (fp) {
        std::fread(_aw_image, 1, file_size, fp);
        std::fclose(fp);
    } else {
        Logger::error("Can't read ArtWorks file %s", filename.c_str());
        flex_free(&_aw_image);
        flex_free(&_aw_workspace);
        return false;
    }
    _aw_handle.resizable_block = &_aw_image;
    _aw_handle.fixed_block = (void **) -1;

    e = (os_error *) awrender_file_init(_aw_image,
                                        (awrender_callback_handler) artworks_callback,
                                        file_size,
                                        &_aw_handle);
    if (e) {
        flex_free(&_aw_workspace);
        flex_free(&_aw_image);
        Logger::error("awrender_file_init failed, error: %s (%d)", e->errmess, e->errnum);
        return false;
    }

    awrender_doc_bounds((awrender_doc) _aw_image, (os_box*)&_bounds);

    _width = _bounds.width() / 256;
    _height = _bounds.height() / 256;
    _width_px = _width / 2;
    _height_px = _height / 2;
    _size = file_size + awrender_DefaultWorkSpace;
    _has_alpha = true;
    Log_debug("CLArtWorksImage::load loaded: %s w:%d h:%d bounds min:%d,%d max:%d,%d 256 min:%d,%d max:%d,%d", filename.c_str(), _width, _height, _bounds.min.x, _bounds.min.y, _bounds.max.x, _bounds.max.y, _bounds.min.x / 256, _bounds.min.y / 256, _bounds.max.x / 256, _bounds.max.y / 256);
    return true;
}

//tbx::UserSprite CLArtWorksImage::get_scaled(int w, int h) {
//    bool load_scaled;
//    tbx::SpriteArea *spr_area;
//    tbx::UserSprite spr;
//    if (w != 0 && h != 0) {
//        if (scaled_h == h && scaled_w == w && _scaled_spr_area.is_valid()) {
//            return _scaled_spr_area.get_sprite((int*)0x4);
//        }
//        load_scaled = true;
//    } else {
//        load_scaled = false;
//    }
//
///*
//    strcpy(tmp_spr_name, tmpnam(nullptr));
//    snprintf(convert_cmd, sizeof(convert_cmd), "<CLFiler$Dir>.draw2spr %s %s %s",
//            scale_opts, _aw_filename.c_str(), tmp_spr_name);
//    Log_debug("CLArtWorksImage::load_scaled draw2spr cmd:%s", convert_cmd);
//    tbx::Application::instance()->start_wimp_task(convert_cmd);
//
//    if (CLUtils::is_file_exist(tmp_spr_name)) {
//        if (load_scaled) {
//            spr_area = &_scaled_spr_area;
//
//        } else {
//            spr_area = &_full_spr_area;
//        }
//        spr_area->load(tmp_spr_name);
//        if (spr_area->is_valid()) {
//            spr = spr_area->get_sprite((int*)0x4);
//            Log_debug("loaded area ptr=%p sprptr=%p", spr_area->pointer(), spr.pointer());
//            tbx::Path(tmp_spr_name).remove();
//            if (load_scaled) {
//                scaled_w = w;
//                scaled_h = h;
//            }
//        } else {
//            Logger::error("CLArtWorksImage::load_scaled can't load spritearea %s", tmp_spr_name);
//        }
//    } else {
//        Logger::error("CLArtWorksImage::load_scaled can't convert to sprite %s->%s", _aw_filename.c_str(), tmp_spr_name);
//    }
//    */
//    return spr;
//}
//
void CLArtWorksImage::plot(int left_x, int bottom_y, tbx::Colour background_colour) {
    if (is_valid()) {
        plot_scaled(left_x, bottom_y, _width, _height, background_colour);
    }
}

void CLArtWorksImage::plot_scaled(int left_x, int bottom_y, int req_width, int req_height, tbx::Colour background_colour) {
    if (is_valid()) {
        int scale = get_percent_scale_to_fit_in_box(tbx::Size(req_width, req_height), 1000);

        tbx::DrawTransform dt;
        dt.scale(tbx::Fixed16(scale) / tbx::Fixed16(100));

        int x = left_x - ((_bounds.min.x / 256) * scale / 100),
                y = bottom_y - ((_bounds.min.y / 256) * scale / 100);

        dt.translate_os(x, y);

        awrender_info_block info_block;
        info_block.dither_x = 0;
        info_block.dither_y = 0;
        info_block.clip_rect.x0 = _bounds.min.x;
        info_block.clip_rect.y0 = _bounds.min.y;
        info_block.clip_rect.x1 = _bounds.max.x;
        info_block.clip_rect.y1 = _bounds.max.y;

        artworks_handle     handle;
        handle.resizable_block = &_aw_workspace;
        handle.fixed_block     = &_aw_image;

        Log_debug("CLArtWorksImage::plot_scaled  left_x:%d bottom_y:%d x:%d y:%d w:%d h:%d bounds min:%d,%d max:%d,%d 256 min:%d,%d max:%d,%d", left_x, bottom_y, x,y, _width, _height, _bounds.min.x, _bounds.min.y, _bounds.max.x, _bounds.max.y, _bounds.min.x / 256, _bounds.min.y / 256, _bounds.max.x / 256, _bounds.max.y / 256);
        /* Shouldn't report errors in redraw loops. */
        (void)    awrender_render(_aw_image,
                                  &info_block,
                                  (const os_trfm*)&dt,
                                  &_vdu_block,
                                  _aw_workspace,
                                  (awrender_callback_handler) artworks_callback,
                                  110, // max quality
                                  awrender_OutputToVDU,
                                  &handle);
    }
}

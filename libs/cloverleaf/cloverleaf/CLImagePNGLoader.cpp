//
// Created by lenz on 3/22/20.
//

#include <cstdlib>
#include <png.h>
#include <memory.h>
#include <cloverleaf/Logger.h>
#include "CLImagePNGLoader.h"
#include "CLImage.h"

/**
 * cl_png_warning -- callback for libpng warnings
 */
static void cl_png_warning(png_structp png_ptr, png_const_charp warning_message)
{
    Logger::info(warning_message);
}

/**
 * cl_png_error -- callback for libpng errors
 */
static void cl_png_error(png_structp png_ptr, png_const_charp error_message)
{
    Logger::error(error_message);
    longjmp(png_jmpbuf(png_ptr), 1);
}

static void cl_png_setup_transforms(png_structp png_ptr, png_infop info_ptr)
{
    int bit_depth, color_type, intent;
    double gamma;

    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    /* Set up our transformations */
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    if (!(color_type & PNG_COLOR_MASK_ALPHA)) {
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
    }

    /* gamma correction - we use 2.2 as our screen gamma
     * this appears to be correct (at least in respect to !Browse)
     * see http://www.w3.org/Graphics/PNG/all_seven.html for a test case
     */
    if (png_get_sRGB(png_ptr, info_ptr, &intent)) {
        png_set_gamma(png_ptr, 2.2, 0.45455);
    } else {
        if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
            png_set_gamma(png_ptr, 2.2, gamma);
        } else {
            png_set_gamma(png_ptr, 2.2, 0.45455);
        }
    }

    png_read_update_info(png_ptr, info_ptr);
}

/* structure to store PNG image bytes */
struct mem_encode
{
    unsigned char *buffer;
    size_t size;
};

struct mem_decode
{
    unsigned char *buffer;
    size_t size;
    size_t offset;
};

static void cl_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    /* with libpng15 next line causes pointer deference error; use libpng12 */
    struct mem_encode* p=(struct mem_encode*)png_get_io_ptr(png_ptr); /* was png_ptr->io_ptr */
    size_t nsize = p->size + length;

    /* allocate or grow buffer */
    if(p->buffer)
        p->buffer = (unsigned char*)realloc(p->buffer, nsize);
    else
        p->buffer = (unsigned char*)malloc(nsize);

    if(!p->buffer) {
        png_error(png_ptr, "Write Error");
    }

    /* copy new bytes to end of buffer */
    memcpy(p->buffer + p->size, data, length);
    p->size += length;
}

/* This is optional but included to show how png_set_write_fn() is called */
static void cl_png_flush(png_structp png_ptr)
{
}

void cl_png_read_data(png_structp _pngptr, png_bytep _data, png_size_t _len)
{
    mem_decode *input;

    /* Get input */
    input = (mem_decode*)png_get_io_ptr(_pngptr);

    /* Copy data from input */
    memcpy(_data, input->buffer + input->offset, _len);
    input->offset += _len;
}

static png_bytep *calc_row_pointers(unsigned char *buffer, int width, int height)
{
    size_t rowstride = width * 4;
    png_bytep *row_ptrs;
    int hloop;

    row_ptrs = static_cast<png_bytep *>(malloc(sizeof(png_bytep) * height));

    if (row_ptrs != nullptr) {
        for (hloop = 0; hloop < height; hloop++) {
            row_ptrs[hloop] = buffer + (rowstride * hloop);
        }
    }

    return row_ptrs;
}

bool CLImagePNGLoader::load(const std::string& filename, CLConvertedSpriteImage* img)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info_ptr;
    png_uint_32 width, height;
    osspriteop_area* sprite_area = nullptr;
    volatile png_bytep * volatile row_pointers = nullptr;
    FILE *f = nullptr;

    img->_sprite_area = nullptr;
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                     cl_png_error, cl_png_warning);
    if (png_ptr == nullptr) {
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    end_info_ptr = png_create_info_struct(png_ptr);
    if (end_info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return false;
    }

    /* setup error exit path */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* cleanup and bail */
        goto png_cache_convert_error;
    }

    /* read from file */
    f = fopen(filename.c_str(), "rb");

    png_init_io(png_ptr, f);

    /* ensure the png info structure is populated */
    png_read_info(png_ptr, info_ptr);

    /* setup output transforms */
    cl_png_setup_transforms(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    /* Claim the required memory for the converted PNG */;
    sprite_area = CLUserSpriteImage::create_sprite_area(width, height);
    if (sprite_area == nullptr) {
        /* cleanup and bail */
        goto png_cache_convert_error;
    }

    row_pointers = calc_row_pointers(CLUserSpriteImage::get_sprite_pixels_pointer((unsigned char*)sprite_area), width, height);

    if (row_pointers != nullptr) {
        png_read_image(png_ptr, (png_bytep *) row_pointers);
        img->_width_px = width;
        img->_width = width * 2;
        img->_height_px = height;
        img->_height = height * 2;
        img->_has_alpha = true;
        img->_sprite_area = sprite_area;
        goto png_cache_convert_exit;
    }

png_cache_convert_error:
    if (sprite_area) {
        free(sprite_area);
    }

png_cache_convert_exit:
    /* cleanup png read */
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);

    if (row_pointers != nullptr) {
        free((png_bytep *) row_pointers);
    }

    if (f) {
        fclose(f);
    }

    return (img->_sprite_area != nullptr);
}


CLUserSpriteImage* CLImagePNGLoader::decompess(byte *data, const size_t size)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    osspriteop_area* sprite_area = nullptr;
    mem_decode input = {.buffer = data, .size = size, .offset = 0};
    volatile png_bytep * volatile row_pointers = nullptr;

    CLUserSpriteImage* spr = nullptr;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                     cl_png_error, cl_png_warning);
    if (png_ptr == nullptr) {
        return nullptr;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return nullptr;
    }


    /* setup error exit path */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* cleanup and bail */
        goto png_cache_convert_error;
    }

    /* read from file */
    png_set_read_fn(png_ptr, (void*)&input, cl_png_read_data);

//    png_init_io(png_ptr, f);

    /* ensure the png info structure is populated */
    png_read_info(png_ptr, info_ptr);

    /* setup output transforms */
    cl_png_setup_transforms(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    /* Claim the required memory for the converted PNG */;
    spr = new CLUserSpriteImage(width, height, true);
    sprite_area = spr->_sprite_area;
    if (sprite_area == nullptr) {
        /* cleanup and bail */
        goto png_cache_convert_error;
    }
//    Log_debug("CLImagePNGLoader::decompess 1", 1);
    row_pointers = calc_row_pointers(spr->get_sprite_pixels_pointer(), width, height);
//    Log_debug("CLImagePNGLoader::decompess 2", 1);
    if (row_pointers != nullptr) {
        png_read_image(png_ptr, (png_bytep *) row_pointers);
        png_read_end(png_ptr, info_ptr);
        goto png_cache_convert_exit;
    }
png_cache_convert_error:
//    Log_debug("CLImagePNGLoader::decompess err", 1);
    delete spr;
    spr = nullptr;

png_cache_convert_exit:
    /* cleanup png read */
//    Log_debug("CLImagePNGLoader::decompess 3", 1);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    if (row_pointers != nullptr) {
        free((png_bytep *) row_pointers);
    }

    return spr;
}

CLPNGImage* CLImagePNGLoader::compress(struct osspriteop_header* sprite, int quality) {
    int out_idx;
    volatile png_bytepp row_pointers = nullptr;
    int spr_w = sprite->width + 1;
    int spr_h = sprite->height + 1;
    uint32_t *input_row;
    uint8_t out_row[spr_w * 4];
    uint32_t pixel;
    uint32_t *sprite_image = (uint32_t *)((uint8_t *)sprite + sprite->image);
    struct mem_encode state;

/* initialise - put this before png_write_png() call */
    state.buffer = NULL;
    state.size = 0;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        Logger::error("CLImagePNGLoader::save Can't create PNG write struct, spr: %p.", sprite);
        return nullptr;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        Logger::error("CLImagePNGLoader::save Can't create PNG info struct, spr: %p", sprite);
        goto png_error;
    }

    if (setjmp(png_jmpbuf(png))) {
        Logger::error("CLImagePNGLoader::save Fatal error in converting, spr: %p", sprite);
        goto png_error;
    }

    png_set_write_fn(png, &state, cl_png_write_data, cl_png_flush);
    //png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
            png,
            info,
            spr_w, spr_h,
            8,
            PNG_COLOR_TYPE_RGBA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
    );
    png_set_compression_level(png, 6);
    png_write_info(png, info);

    for(int h = 0; h < spr_h; h++) {
        input_row = sprite_image + h * spr_w;
        out_idx = 0;
        for(int w = 0; w < spr_w; w++) {
            pixel = input_row[w];
            out_row[out_idx++] = (pixel) & 0xff;
            out_row[out_idx++] = (pixel >> 8) & 0xff;
            out_row[out_idx++] = (pixel >> 16) & 0xff;
            out_row[out_idx++] = (pixel >> 24) & 0xff;
        }
        png_write_row(png, out_row);
    }

    png_write_end(png, NULL);


    png_destroy_write_struct(&png, &info);
    return new CLPNGImage(state.buffer, state.size, spr_w, spr_h);

png_error:
    png_destroy_write_struct(&png, &info);
    return nullptr;
}

bool CLImagePNGLoader::save(struct osspriteop_header* sprite, const std::string &filename, int quality) {
    bool result = false;
    CLPNGImage *img = compress(sprite, quality);
    if (img) {
        result = img->save(filename);
        delete img;
    }
    return result;
}

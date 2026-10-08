//
// Created by lenz on 3/23/20.
//

#include <cstdlib>
#include <cstdio>
#include <csetjmp>
#include "CLImageJPGLoader.h"
#include "CLImage.h"
#include "rosprite.h"

#define JPEG_INTERNAL_OPTIONS

#include "CLImageJPGLoader.h"
#include "jpeglib.h"
#include "Logger.h"

/** absolute minimum size of a jpeg below which it is not even worth
 * trying to read header data
 */
#define MIN_JPEG_SIZE 20

static char cl_jpeg_error_buffer[JMSG_LENGTH_MAX];

static unsigned char cl_jpeg_eoi[] = { 0xff, JPEG_EOI };

/**
 * JPEG data source manager: initialize source.
 */
static void cl_jpeg_init_source(j_decompress_ptr cinfo)
{
}


/**
 * JPEG data source manager: fill the input buffer.
 *
 * This can only occur if the JPEG data was truncated or corrupted. Insert a
 * fake EOI marker to allow the decompressor to output as much as possible.
 */
static boolean cl_jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
    cinfo->src->next_input_byte = cl_jpeg_eoi;
    cinfo->src->bytes_in_buffer = 2;
    return TRUE;
}


/**
 * JPEG data source manager: skip num_bytes worth of data.
 */

static void cl_jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    if ((long) cinfo->src->bytes_in_buffer < num_bytes) {
        cinfo->src->next_input_byte = 0;
        cinfo->src->bytes_in_buffer = 0;
    } else {
        cinfo->src->next_input_byte += num_bytes;
        cinfo->src->bytes_in_buffer -= num_bytes;
    }
}


/**
 * JPEG data source manager: terminate source.
 */
static void cl_jpeg_term_source(j_decompress_ptr cinfo)
{
}


/**
 * Error output handler for JPEG library.
 *
 * This logs to NetSurf log instead of stderr.
 * Warnings only - fatal errors are trapped by cl_jpeg_error_exit
 *                 and do not call the output handler.
 */
static void cl_jpeg_error_log(j_common_ptr cinfo)
{
    cinfo->err->format_message(cinfo, cl_jpeg_error_buffer);
    Logger::error("CLImageJpeg: %s", cl_jpeg_error_buffer);
}


/**
 * Fatal error handler for JPEG library.
 *
 * This prevents jpeglib calling exit() on a fatal error.
 */
static void cl_jpeg_error_exit(j_common_ptr cinfo)
{
    jmp_buf *setjmp_buffer = (jmp_buf *) cinfo->client_data;

    cinfo->err->format_message(cinfo, cl_jpeg_error_buffer);
    Logger::error("CLImageJpeg: %s", cl_jpeg_error_buffer);

    longjmp(*setjmp_buffer, 1);
}

bool CLImageJPGLoader::load(const std::string &filename, CLConvertedSpriteImage *img) {
    const uint8_t *source_data = nullptr; /* Jpeg source data */
    size_t source_size; /* length of Jpeg source data */
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jmp_buf setjmp_buffer;
    int height;
    int width;
    osspriteop_area* sprite_area = nullptr;
    uint8_t * volatile pixels = nullptr;
    size_t rowstride;
    struct jpeg_source_mgr source_mgr = {
            0,
            0,
            cl_jpeg_init_source,
            cl_jpeg_fill_input_buffer,
            cl_jpeg_skip_input_data,
            jpeg_resync_to_restart,
            cl_jpeg_term_source };


    /* obtain jpeg source data and perfom minimal sanity checks */
    FILE *f = fopen(filename.c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    source_size = ftell(f);
    if (source_size < MIN_JPEG_SIZE) {
        fclose(f);
        return false;
    }

    source_data = static_cast<const uint8_t *>(malloc(source_size));
    if (!source_data) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);
    fread((void *) source_data, sizeof(uint8_t), source_size, f);
    fclose(f);


    /* setup a JPEG library error handler */
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = cl_jpeg_error_exit;
    jerr.output_message = cl_jpeg_error_log;

    /* handler for fatal errors during decompression */
    if (setjmp(setjmp_buffer)) {
        free((void*)source_data);
        free(sprite_area);
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    cinfo.client_data = &setjmp_buffer;
    jpeg_create_decompress(&cinfo);

    /* setup data source */
    source_mgr.next_input_byte = source_data;
    source_mgr.bytes_in_buffer = source_size;
    cinfo.src = &source_mgr;

    /* read JPEG header information */
    jpeg_read_header(&cinfo, TRUE);

    /* set output processing parameters */
    if (cinfo.jpeg_color_space == JCS_CMYK ||
        cinfo.jpeg_color_space == JCS_YCCK) {
        cinfo.out_color_space = JCS_CMYK;
    } else {
        cinfo.out_color_space = JCS_RGB;
    }
    cinfo.dct_method = JDCT_ISLOW;

    /* commence the decompression, output parameters now valid */
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;

    /* create opaque bitmap (jpegs cannot be transparent) */
    sprite_area = CLUserSpriteImage::create_sprite_area(width, height);
    if (sprite_area == nullptr) {
        /* empty bitmap could not be created */
        free((void*)source_data);
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    pixels = CLUserSpriteImage::get_sprite_pixels_pointer((unsigned char*)sprite_area);

    /* Convert scanlines from jpeg into bitmap */
    rowstride = width * 4;
    do {
        JSAMPROW scanlines[1];

        scanlines[0] = (JSAMPROW) (pixels +
                                   rowstride * cinfo.output_scanline);
        jpeg_read_scanlines(&cinfo, scanlines, 1);

        if (cinfo.out_color_space == JCS_CMYK) {
            int i;
            for (i = width - 1; 0 <= i; i--) {
                /* Trivial inverse CMYK -> RGBA */
                const int c = scanlines[0][i * 4 + 0];
                const int m = scanlines[0][i * 4 + 1];
                const int y = scanlines[0][i * 4 + 2];
                const int k = scanlines[0][i * 4 + 3];

                const int ck = c * k;
                const int mk = m * k;
                const int yk = y * k;

#define DIV255(x) ((x) + 1 + ((x) >> 8)) >> 8
                scanlines[0][i * 4 + 0] = DIV255(ck);
                scanlines[0][i * 4 + 1] = DIV255(mk);
                scanlines[0][i * 4 + 2] = DIV255(yk);
                scanlines[0][i * 4 + 3] = 0xff;
#undef DIV255
            }
        } else {
#if RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || RGB_PIXELSIZE != 4
            /* Missmatch between configured libjpeg pixel format and
             * NetSurf pixel format.  Convert to RGBA */
            int i;
            for (i = width - 1; 0 <= i; i--) {
                int r = scanlines[0][i * RGB_PIXELSIZE + RGB_RED];
                int g = scanlines[0][i * RGB_PIXELSIZE + RGB_GREEN];
                int b = scanlines[0][i * RGB_PIXELSIZE + RGB_BLUE];
                scanlines[0][i * 4 + 0] = r;
                scanlines[0][i * 4 + 1] = g;
                scanlines[0][i * 4 + 2] = b;
                scanlines[0][i * 4 + 3] = 0xff;
            }
#endif
        }
    } while (cinfo.output_scanline != cinfo.output_height);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free((void*)source_data);

    img->_width_px = width;
    img->_width = width * 2;
    img->_height_px = height;
    img->_height = height * 2;
    img->_sprite_area = sprite_area;
    img->_has_alpha = false;
    return true;
}


//bool CLImageJPGLoader::save(struct rosprite* sprite, const std::string &filename, int quality) {
//    int w, h, out_idx;
//    struct jpeg_compress_struct cinfo;
//    struct jpeg_error_mgr jerr;
//    jmp_buf setjmp_buffer;
//    JSAMPROW row_pointer[1];
//    uint32_t *input_row;
//    uint8_t out_row[sprite->width * 3];
//    uint32_t pixel;
//
//    /* obtain jpeg source data and perfom minimal sanity checks */
//    FILE *f = fopen(filename.c_str(), "wb");
//    if (!f) {
//        return false;
//    }
//
//    /* setup a JPEG library error handler */
//    cinfo.err = jpeg_std_error(&jerr);
//    jerr.error_exit = cl_jpeg_error_exit;
//    jerr.output_message = cl_jpeg_error_log;
//
//    /* handler for fatal errors during decompression */
//    if (setjmp(setjmp_buffer)) {
//        Logger::error("CLImageJPGLoader::save fatal error");
//        jpeg_destroy_compress(&cinfo);
//        fclose(f);
//        return false;
//    }
//    cinfo.client_data = &setjmp_buffer;
//    jpeg_create_compress(&cinfo);
//
//    /* setup output */
//    jpeg_stdio_dest(&cinfo, f);
//
//    cinfo.image_width=sprite->width;
//    cinfo.image_height=sprite->height;
//    cinfo.input_components=3;
//    cinfo.in_color_space=JCS_RGB;
//    jpeg_set_defaults(&cinfo);
//
//    jpeg_set_quality(&cinfo, quality, true);
//
//    /* compression */
//    jpeg_start_compress(&cinfo, true);
//
//    row_pointer[0] = out_row;
//
//    for(h = 0; h < sprite->height; h++) {
//        input_row = sprite->image + h * sprite->width;
//        out_idx = 0;
//        //Log_debug("pixel %d %x %x %x %x", h, input_row[0], input_row[1], input_row[2], input_row[3]);
//        for(w = 0; w < sprite->width; w++) {
//            // convert from RRGGBBAA (uint32) -> RR GG BB (uint8)
//            pixel = input_row[w];
//            out_row[out_idx++] = (pixel >> 24) & 0xff;
//            out_row[out_idx++] = (pixel >> 16) & 0xff;
//            out_row[out_idx++] = (pixel >> 8) & 0xff;
//        }
//        jpeg_write_scanlines(&cinfo, row_pointer, 1);
////        Log_debug("row %d green: %x %x", h, out_row[1], out_row[4]);
////        compressjpeg_write_line(jimgtag, out_row);
//    }
//
//    jpeg_finish_compress(&cinfo);
//    jpeg_destroy_compress(&cinfo);
//    fclose(f);
//    return true;
//}


bool CLImageJPGLoader::save(struct osspriteop_header* sprite, const std::string &filename, int quality) {
    bool result = false;
    CLJPEGImage *img = compress(sprite, quality);
    if (img) {
        result = img->save(filename);
        delete img;
    }
    return result;
}

CLJPEGImage* CLImageJPGLoader::compress(struct osspriteop_header* sprite, int quality) {
    int w, h, out_idx;
    unsigned long jpegSize = 0;
    unsigned char* jpegBuf = nullptr;
    CLJPEGImage* img = nullptr;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int spr_w = sprite->width + 1;
    int spr_h = sprite->height + 1;
    jmp_buf setjmp_buffer;
    JSAMPROW row_pointer[1];
    uint32_t *input_row;
    uint8_t out_row[spr_w * 3];
    uint32_t pixel;

    /* setup a JPEG library error handler */
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = cl_jpeg_error_exit;
    jerr.output_message = cl_jpeg_error_log;

    /* handler for fatal errors during decompression */
    if (setjmp(setjmp_buffer)) {
        Logger::error("CLImageJPGLoader::save fatal error");
        jpeg_destroy_compress(&cinfo);
        return nullptr;
    }
    cinfo.client_data = &setjmp_buffer;
    jpeg_create_compress(&cinfo);

    /* setup output */
    jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);
//    jpeg_stdio_dest(&cinfo, f);

    cinfo.image_width=spr_w;
    cinfo.image_height=spr_h;
    cinfo.input_components=3;
    cinfo.in_color_space=JCS_RGB;
    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, true);

//    Log_debug("jpg start compress w=%d h=%d", cinfo.image_width, cinfo.image_height);
    /* compression */
    jpeg_start_compress(&cinfo, true);

    row_pointer[0] = out_row;
    uint32_t *sprite_image = (uint32_t *)((uint8_t *)sprite + sprite->image);
//    Log_debug("jpg start compress w=%d h=%d %p", cinfo.image_width, cinfo.image_height, sprite_image);
    for(h = 0; h < spr_h; h++) {

        input_row = sprite_image + h * spr_w;
        out_idx = 0;
//        Log_debug("pixel %d %x %x %x %x", h, input_row[0], input_row[1], input_row[2], input_row[3]);
        for(w = 0; w < spr_w; w++) {
            // convert from AABBGGRR (uint32) -> RR GG BB (uint8)
            pixel = input_row[w];
            out_row[out_idx++] = (pixel) & 0xff;
            out_row[out_idx++] = (pixel >> 8) & 0xff;
            out_row[out_idx++] = (pixel >> 16) & 0xff;
        }
//        Log_debug("1row %d green: %x %x  buf:%p size:%d", h, out_row[1], out_row[4], jpegBuf, jpegSize);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
//        Log_debug("row %d green: %x %x  buf:%p size:%d", h, out_row[1], out_row[4], jpegBuf, jpegSize);
//        compressjpeg_write_line(jimgtag, out_row);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

//    Log_debug("CLImageJPGLoader::compress buf=%p s=%d", jpegBuf, jpegSize);
    img = new CLJPEGImage(jpegBuf, jpegSize);
    free(jpegBuf);
//    Log_debug("CLImageJPGLoader::compress return %p s=%d", img, jpegSize);
    if (img->is_valid()) {
        return img;
    } else {
        delete img;
        Log_debug("CLImageJPGLoader::compress failed spr:%p w:%d h:%d", sprite, spr_w, spr_h);
        return nullptr;
    }
}

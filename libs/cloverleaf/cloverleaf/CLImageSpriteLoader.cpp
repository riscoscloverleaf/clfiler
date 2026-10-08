//
// Created by slenz on 14.12.2022.
//

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <tbx/sprite.h>
#include <cloverleaf/Logger.h>
#include "CLImageSpriteLoader.h"

osspriteop_area* CLImageSpriteLoader::load(const std::string& filename, int* width_ptr, int* height_ptr) {
//    tbx::SpriteArea *area = new
    FILE *f = nullptr;
    char *sprite_area = 0;
    int file_length;
    osspriteop_header* spr;

    *height_ptr = 0;
    *width_ptr = 0;
    /* read from file */
    f = fopen(filename.c_str(), "rb");
    if (!f) {
        goto error;
    }
    fseek(f, 0, SEEK_END);
    file_length = ftell(f);
    fseek(f, 0, SEEK_SET);
    sprite_area = (char*)malloc(file_length);
    if (!fread(sprite_area, file_length, 1, f)) {
        goto error;
    }
    spr = ((osspriteop_header*)(sprite_area + 16));
    *width_ptr = spr->width + 1;
    *height_ptr = spr->height + 1;
exit:
    if (f) {
        fclose(f);
    }
    return (osspriteop_area*)sprite_area;
error:
    Logger::error("CLImageSpriteLoader::load can't load sprite from:%s error: (%d) %s", filename.c_str(), errno, strerror(errno));
    if (sprite_area) {
        free(sprite_area);
        sprite_area = nullptr;
    }
    goto exit;
}
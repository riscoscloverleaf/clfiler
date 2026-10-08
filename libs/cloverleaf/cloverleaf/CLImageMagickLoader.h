//
// Created by lenz on 3/23/20.
//

#ifndef CLOVERLEAF_CLIMAGEMAGICKLOADER_H
#define CLOVERLEAF_CLIMAGEMAGICKLOADER_H

#include <string>
#include <oslib/osspriteop.h>
#include <tbx/path.h>

class CLConvertedSpriteImage;
class CLUserSpriteImage;
class CLImageMagickLoader {
    static bool _is_imagemagick_installed;
public:
    static bool is_imagemagick_installed() { return _is_imagemagick_installed; }
    static bool check_imagemagick_installed();
    static bool load(tbx::Path& filepath, CLConvertedSpriteImage *img, int filetype);
    static bool can_save(int filetype);
    static bool can_direct_convert_from(int filetype);
    static bool save(CLUserSpriteImage *img, const std::string& output_file, int filetype, const std::string& options);
    static bool convert(tbx::Path& input_file, const std::string& output_file, int filetype, const std::string& options);
    static std::string info(tbx::Path& input_file);
};


#endif //CLOVERLEAF_CLIMAGEJPGLOADER_H

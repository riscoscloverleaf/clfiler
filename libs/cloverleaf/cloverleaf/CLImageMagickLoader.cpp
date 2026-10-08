//
// Created by lenz on 3/23/20.
//

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <unixlib/local.h>
#include <tbx/application.h>
#include <tbx/messagewindow.h>
#include "rosprite.h"
#include "CLImageMagickLoader.h"
#include "CLImage.h"
#include "CLUtils.h"
#include "Logger.h"

bool CLImageMagickLoader::_is_imagemagick_installed;

bool CLImageMagickLoader::load(tbx::Path &filepath, CLConvertedSpriteImage *img, int filetype) {
    char cmd[500];
    bool result;
    std::string tmp_out_filename = tmpnam(nullptr);
    tmp_out_filename.append("/SPR");
    switch(filetype) {
        case FILE_TYPE_GIF:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_PSD:
        case FILE_TYPE_SVG:
        case FILE_TYPE_WEBP:
        case FILE_TYPE_JPEG:
            break;
        default:
            Logger::error("CLImageMagickLoader::load file format not recognized, file: %s", filepath.name().c_str());
            return nullptr;
    }
//    snprintf(cmd, sizeof(cmd), "convert --help 1><CLFiler$ChoicesDir>.magick-log 2><CLFiler$ChoicesDir>.magick-errors");
    snprintf(cmd, sizeof(cmd), "convert \"%s:%s\" \"SPR:%s\" 1><CLFiler$ChoicesDir>.magick-log 2><CLFiler$ChoicesDir>.magick-errors",
            CLUtils::filetype2ext(filepath.file_type()), CLUtils::unixify(filepath.name()).c_str(),
             CLUtils::unixify(tmp_out_filename).c_str());

    Log_debug("CLImageMagickLoader::load convert to sprite, cmd: %s", cmd);
    tbx::Application::instance()->start_wimp_task(cmd);
//    tbx::Path(tmp_out_filename).remove();
    result = img->load_spritearea(tmp_out_filename);
    Log_debug("CLImageMagickLoader::load convert to sprite, riscos sprite: %s, loaded:%d", tmp_out_filename.c_str(), result);

    tbx::Path(tmp_out_filename).remove();

    return result;
}

bool CLImageMagickLoader::check_imagemagick_installed() {
    char *imagemagick_convert;
    imagemagick_convert = getenv("Alias$convert");
    Log_debug("imagick convert: %s", imagemagick_convert);
    _is_imagemagick_installed = (imagemagick_convert != nullptr);
    return _is_imagemagick_installed;
}

bool CLImageMagickLoader::save(CLUserSpriteImage *img, const std::string &output_file, int filetype, const std::string& options) {
    if (!check_imagemagick_installed()) {
        tbx::show_message("You need to have Imagemagick installed and activated (loaded) to use save function");
        return false;
    }
    char spr_filename[256];
    char png_filename[256];
    char cmd[500];
    std::string spr2png_cmd;
    std::string output_tmp = tmpnam(nullptr);

    switch(filetype) {
        case FILE_TYPE_GIF:
            output_tmp.append("/gif");
            break;
        case FILE_TYPE_TIFF:
            output_tmp.append("/tif");
            break;
        case FILE_TYPE_WEBP:
            output_tmp.append("/webp");
            break;
        case FILE_TYPE_JPEG:
            output_tmp.append("/jpg");
            break;
        case FILE_TYPE_PNG:
            output_tmp.append("/png");
            break;
        case FILE_TYPE_SPRITE:
            output_tmp.append("/spr");
            break;
        default:
            Logger::error("CLImageMagickLoader::save format not supported, file: %s filetype=d%", output_file.c_str(), filetype);
            return false;
    }
    snprintf(spr_filename, sizeof(spr_filename)-1, "%s/SPR", std::tmpnam(nullptr));
    snprintf(png_filename, sizeof(png_filename)-1, "%s/PNG", std::tmpnam(nullptr));
    os_mode old_mode = img->get_sprite_pointer()->mode;
    img->get_sprite_pointer()->mode = tinct_SPRITE_MODE;
    img->save(spr_filename);
    img->get_sprite_pointer()->mode = old_mode;
    spr2png_cmd = "<CLFiler$Dir>.spr2png -a " + std::string(spr_filename) + " " + std::string(png_filename)+" 1><CLFiler$ChoicesDir>.spr2png-log 2><CLFiler$ChoicesDir>.spr2png-errors";
    tbx::Application::instance()->start_wimp_task(spr2png_cmd);
    if (!tbx::Path(png_filename).exists()) {
        Logger::error("Can't convert sprite to PNG, cmd: %s", spr2png_cmd.c_str());
        return false;
    }

    snprintf(cmd, sizeof(cmd)-1,
            "convert \"png:%s\" %s \"%s\" 1><CLFiler$ChoicesDir>.magick-log 2><CLFiler$ChoicesDir>.magick-errors",
             CLUtils::unixify(png_filename).c_str(), options.c_str(), CLUtils::unixify(output_tmp).c_str());

    Log_debug("CLImageMagickLoader::save convert sprite to other format, cmd: %s", cmd);
    tbx::Application::instance()->start_wimp_task(cmd);
//    Log_debug("CLImageMagickLoader::saving complete", 1);

    if (CLUtils::is_file_exist(output_tmp)) {
        tbx::Path(spr_filename).remove();
        tbx::Path(png_filename).remove();
        tbx::Path(output_tmp).file_type(filetype);
        tbx::Path(output_tmp).move(output_file, tbx::Path::COPY_FORCE);
        return true;
    }
    Logger::error("CLImageMagickLoader::save failed to convert sprite(png) to other format, cmd: %s", cmd);
    return false;
}

bool CLImageMagickLoader::convert(tbx::Path& input_file, const std::string& output_file, int filetype, const std::string& options) {
    char tmp_filename[256];
    char cmd[500];
    std::string format;
    std::string input_tmp = tmpnam(nullptr);
    input_tmp.append("/");
    input_tmp.append(CLUtils::filetype2ext(input_file.file_type()));
    input_file.copy(input_tmp);
    Log_debug("copy %s to %s", input_file.name().c_str(), input_tmp.c_str());
    tbx::Path inp = tbx::Path(input_tmp);

    if (!inp.exists()) {
        Logger::error("Can't copy input file %s to %s", input_file.name().c_str(), input_tmp.c_str());
        return false;
    }

    strcpy(tmp_filename, tmpnam(nullptr));
    switch(filetype) {
        case FILE_TYPE_GIF:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_WEBP:
        case FILE_TYPE_JPEG:
        case FILE_TYPE_PNG:
        case FILE_TYPE_SPRITE:
            strcat(tmp_filename, "/");
            strcat(tmp_filename, CLUtils::filetype2ext(filetype));
            break;
        default:
            Logger::error("CLImageMagickLoader::convert format not supported, file: %s filetype=d%", output_file.c_str(), filetype);
            return false;
    }
    snprintf(cmd, sizeof(cmd)-1,
             "convert \"%s\" %s \"%s\" 1><CLFiler$ChoicesDir>.magick-log 2><CLFiler$ChoicesDir>.magick-errors",
             CLUtils::unixify(input_tmp).c_str(),
             options.c_str(),
             CLUtils::unixify(tmp_filename).c_str());

    Log_debug("CLImageMagickLoader::convert convert file to other format, cmd: %s", cmd);
    tbx::Application::instance()->start_wimp_task(cmd);

    if (CLUtils::is_file_exist(tmp_filename)) {
//        inp.remove();
        tbx::Path(tmp_filename).file_type(filetype);
        tbx::Path(tmp_filename).move(output_file, tbx::Path::COPY_FORCE);
        return true;
    }
    Logger::error("CLImageMagickLoader::convert failed to convert to other format, cmd: %s", cmd);
    return false;
}

bool CLImageMagickLoader::can_save(int filetype) {
    if (!CLImageMagickLoader::is_imagemagick_installed()) {
        return false;
    }

    switch (filetype) {
        case FILE_TYPE_JPEG:
        case FILE_TYPE_PNG:
        case FILE_TYPE_SPRITE:
        case FILE_TYPE_GIF:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_WEBP:
            return true;
        default:
            return false;
    }
}

bool CLImageMagickLoader::can_direct_convert_from(int filetype) {
    if (!CLImageMagickLoader::is_imagemagick_installed()) {
        return false;
    }

    switch (filetype) {
        case FILE_TYPE_SVG:
        case FILE_TYPE_PSD:
        case FILE_TYPE_ICO:
        case FILE_TYPE_JPEG:
        case FILE_TYPE_PNG:
        case FILE_TYPE_GIF:
        case FILE_TYPE_TIFF:
        case FILE_TYPE_WEBP:
            return true;
        default:
            return false;
    }
}

std::string CLImageMagickLoader::info(tbx::Path& input_file) {
    char cmd[500];
    snprintf(cmd, sizeof(cmd)-1,
             "identify -verbose \"%s:%s\" 1><CLFiler$ChoicesDir>.magick-verbose-log 2><CLFiler$ChoicesDir>.magick-errors",
             CLUtils::filetype2ext(input_file.file_type()), CLUtils::unixify(input_file.name()).c_str());

    Log_debug("CLImageMagickLoader::info, cmd: %s", cmd);
    tbx::Application::instance()->start_wimp_task(cmd);

    return CLUtils::get_file_contents("<CLFiler$ChoicesDir>.magick-verbose-log");
}

//
// Created by slenz on 11.01.2023.
//

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <unixlib/local.h>
#include <tbx/application.h>
#include "rosprite.h"
#include "CLFFmpegLoader.h"
#include "CLImage.h"
#include "CLUtils.h"
#include "Logger.h"

bool CLFFmpegLoader::_is_ffmpeg_installed;
bool CLFFmpegLoader::load(const tbx::Path &filepath, CLConvertedSpriteImage *img, int filetype) {
    char tmp_riscos_filename[256];
    char tmp_unix_filename[256];
    bool result = false;
    char cmd[2000];
    __unixify(std::tmpnam(nullptr), 0, tmp_unix_filename, sizeof(tmp_unix_filename) - 1, 0);
    snprintf(cmd, sizeof(cmd), "ffmpeg -ss 2 -i '%s' -vframes 1 -f image2 -y '%s' 1><CLFiler$ChoicesDir>.ffmpeg-log 2><CLFiler$ChoicesDir>.ffmpeg-errors",
             CLUtils::str_replace_all(filepath.leaf_name(), "/", ".").c_str(),
             tmp_unix_filename);
//    snprintf(cmd, sizeof(cmd), "ffmpeg -loglevel quiet -hide_banner -nostdin -ss 0 -i '%s' -frames:v 1 -f image2 -y '%s' 1><CLFiler$ChoicesDir>.ffmpeg-log 2><CLFiler$ChoicesDir>.ffmpeg-errors",
//             CLUtils::str_replace_all(filepath.leaf_name(), "/", ".").c_str(),
//             tmp_unix_filename);

    tbx::Path current_app_dir = tbx::Application::instance()->directory();
    filepath.parent().set_current_directory();

    Log_debug("CLFFmpegLoader::load convert to JPG, cmd: %s", cmd);
    tbx::Application::instance()->start_wimp_task("<CLFiler$Dir>.AEoff");
    tbx::Application::instance()->start_wimp_task(cmd);
    tbx::Application::instance()->start_wimp_task("<CLFiler$Dir>.AEon");
    __riscosify(tmp_unix_filename, 0, 0, tmp_riscos_filename, sizeof(tmp_riscos_filename) - 1, &filetype);

    tbx::Path p(tmp_riscos_filename);
    if (!p.exists()) {
        snprintf(cmd, sizeof(cmd), "ffmpeg -ss 0 -i '%s' -vframes 1 -f image2 -y '%s' 1><CLFiler$ChoicesDir>.ffmpeg-log 2><CLFiler$ChoicesDir>.ffmpeg-errors",
                 CLUtils::str_replace_all(filepath.leaf_name(), "/", ".").c_str(),
                 tmp_unix_filename);

        Log_debug("CLFFmpegLoader::load convert to JPG (second attempt), cmd: %s", cmd);
        tbx::Application::instance()->start_wimp_task("<CLFiler$Dir>.AEoff");
        tbx::Application::instance()->start_wimp_task(cmd);
        tbx::Application::instance()->start_wimp_task("<CLFiler$Dir>.AEon");
    }
    if (p.exists()) {
        p.file_type(FILE_TYPE_JPEG);
        result = img->load(p.name());
        Log_debug("CLFFmpegLoader::load JPG %s, loaded:%d", tmp_riscos_filename, result);
        p.remove();
    } else {

        Logger::error("CLFFmpegLoader::load error in video file  %s, result:%d", filepath.name().c_str(), result);
        result = false;
    }

    current_app_dir.set_current_directory();

    return result;
}

bool CLFFmpegLoader::check_ffmpeg_installed() {
    char *ffmpeg_alias;
    ffmpeg_alias = getenv("Alias$ffmpeg");
    Log_debug("ffmpeg: %s", ffmpeg_alias);
    _is_ffmpeg_installed = (ffmpeg_alias != nullptr);
    return _is_ffmpeg_installed;
}

//
// Created by slenz on 11.01.2023.
//

#ifndef CLFILER_CLFFMPEGLOADER_H
#define CLFILER_CLFFMPEGLOADER_H

#include <string>
#include <oslib/osspriteop.h>
#include <tbx/path.h>

class CLConvertedSpriteImage;
class CLFFmpegLoader {
    static bool _is_ffmpeg_installed;
public:
    static bool is_ffmpeg_installed() { return _is_ffmpeg_installed; }
    static bool check_ffmpeg_installed();
    static bool load(const tbx::Path& filepath, CLConvertedSpriteImage *img, int filetype);
};


#endif //CLFILER_CLFFMPEGLOADER_H

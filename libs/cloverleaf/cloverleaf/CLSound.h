//
// Created by lenz on 6/14/20.
//

#ifndef CLOVERLEAF_CLSOUND_H
#define CLOVERLEAF_CLSOUND_H

#include <string>

class CLSound {
public:
    static void play_mp3_file(const char *file, int volume = -1);
};


#endif //CLOVERLEAF_CLSOUND_H

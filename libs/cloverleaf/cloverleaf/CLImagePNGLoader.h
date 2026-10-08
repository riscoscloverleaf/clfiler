//
// Created by lenz on 3/22/20.
//

#ifndef CLOVERLEAF_CLIMAGEPNGLOADER_H
#define CLOVERLEAF_CLIMAGEPNGLOADER_H

#include <string>
#include <oslib/osspriteop.h>

class CLConvertedSpriteImage;
class CLUserSpriteImage;
class CLPNGImage;

class CLImagePNGLoader {
public:
    static bool load(const std::string& filename, CLConvertedSpriteImage* img);
    static bool save(struct osspriteop_header* sprite, const std::string &filename, int quality);
    static CLPNGImage* compress(struct osspriteop_header* sprite, int quality);
    static CLUserSpriteImage* decompess(byte *data, const size_t size);
};


#endif //CLOVERLEAF_CLIMAGEPNGLOADER_H

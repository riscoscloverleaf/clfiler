//
// Created by lenz on 3/23/20.
//

#ifndef CLOVERLEAF_CLIMAGEJPGLOADER_H
#define CLOVERLEAF_CLIMAGEJPGLOADER_H

#include <string>
#include <oslib/osspriteop.h>
#include "CLImage.h"

class CLImageJPGLoader {
public:
    //static osspriteop_area* load(const std::string& filename, int* width_ptr, int* height_ptr);
    static bool load(const std::string &filename, CLConvertedSpriteImage *img);
//    static bool save(struct rosprite* sprite, const std::string &filename, int quality);
    static bool save(struct osspriteop_header* sprite, const std::string &filename, int quality);
    static CLJPEGImage* compress(struct osspriteop_header* sprite, int quality);
};


#endif //CLOVERLEAF_CLIMAGEJPGLOADER_H

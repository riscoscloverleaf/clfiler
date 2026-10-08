//
// Created by slenz on 14.12.2022.
//

#ifndef CLFILER_CLIMAGESPRITELOADER_H
#define CLFILER_CLIMAGESPRITELOADER_H

#include <string>
#include <oslib/osspriteop.h>

class CLImageSpriteLoader {
public:
    static osspriteop_area* load(const std::string& filename, int* width_ptr, int* height_ptr);
};


#endif //CLFILER_CLIMAGESPRITELOADER_H

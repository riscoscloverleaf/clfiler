//
// Created by lenz on 3/23/20.
//

#ifndef CLOVERLEAF_CLIMAGECACHE_H
#define CLOVERLEAF_CLIMAGECACHE_H

#include <string>
#include <map>
#include "CLImage.h"

class LRUImagesCache;
class CLImageCache {
    static LRUImagesCache *lru_cache;
    static std::string thumb_cache_path;

    static void delete_cached_thumbnails(const tbx::Path& file_p);
public:
    static void init_images_cache(unsigned int capacity, const std::string& _thumb_cache_path); // capacity is number if images stored
    static void free_images_cache(unsigned int keep_size); // free cache but keep some images
    static CLBaseImage* get_or_create_cached_thumbnail(const tbx::Path& file_p, int maxw, int maxh);
    static CLBaseImage* get_cached_thumbnail(const tbx::Path& file_p, const std::string& key);
    static CLBaseImage* create_cached_thumbnail(const tbx::Path& file_p, const std::string& key, int maxw, int maxh);
    static CLBaseImage* load_cached_image(const tbx::Path& file_p);
    static void rename_thumbnail(const std::string key_old, const std::string key_new);
    static void invalidate_thumbnal(const std::string& filename);
    static std::string make_thumbnail_key(const tbx::Path& file_p);
    static void invalidate_all_thumbnals_in_directory(const std::string& path);
    static tbx::Path get_thumbs_cache_path(const tbx::Path& src_dir_p);
};

#endif //CLOVERLEAF_CLIMAGECACHE_H

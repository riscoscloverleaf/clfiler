//
// Created by lenz on 3/23/20.
//
#include <strstream>
#include <memory>
#include <cstdio>
#include <tbx/path.h>
#include <tbx/drawfile.h>
#include <tbx/hourglass.h>
#include <tbx/application.h>
#include <tbx/swixcheck.h>
#include "CLImageCache.h"
#include "CLImageJPGLoader.h"
#include "Logger.h"
#include "CLUtils.h"

#define APPROX_EMPTY_NODE_SIZE 64

class CLImageNode {
public:
    std::string key;
    CLBaseImage *value;

    CLImageNode *prev, *next;

    CLImageNode(const std::string& k, CLBaseImage *v): key(k), value(v), prev(NULL), next(NULL) {}
    ~CLImageNode() { delete value; }
};

class CLImageNodeDoublyLinkedList {
    CLImageNode *front, *rear;

    bool isEmpty() {
        return rear == NULL;
    }

public:
    CLImageNodeDoublyLinkedList(): front(NULL), rear(NULL) {}

    CLImageNode* add_node_to_head(const std::string& key, CLBaseImage* value) {
        CLImageNode *node = new CLImageNode(key, value);
        if(!front && !rear) {
            front = rear = node;
        }
        else {
            node->next = front;
            front->prev = node;
            front = node;
        }
        return node;
    }

    void move_node_to_head(CLImageNode *node) {
        if(node==front) {
            return;
        }
        if(node == rear) {
            rear = rear->prev;
            rear->next = NULL;
        }
        else {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }

        node->next = front;
        node->prev = NULL;
        front->prev = node;
        front = node;
    }

    void remove_rear_node() {
        if(isEmpty()) {
            return;
        }
        if(front == rear) {
            delete rear;
            front = rear = NULL;
        }
        else {
            CLImageNode *temp = rear;
            rear = rear->prev;
            rear->next = NULL;
            delete temp;
        }
    }

    void remove_node(CLImageNode* node) {
        if (node == rear) {
            rear = node->prev;
        }
        if (node == front) {
            front = node->next;
        }
        if (node->prev) {
            Log_debug("node prev change prev", 1);
            node->prev->next = node->next;
        }
        if (node->next) {
            Log_debug("node -> next change next", 1);
            node->next->prev = node->prev;
        }

        Log_debug("delete node", 1);
        delete node;
    }

    CLImageNode* get_rear_node() {
        return rear;
    }
};

class LRUImagesCache{
    unsigned int capacity, size;
    CLImageNodeDoublyLinkedList *images_list;
    std::map<std::string, CLImageNode*> images_map;

public:
    LRUImagesCache(unsigned int capacity) {
        this->capacity = capacity;
        size = 0;
        images_list = new CLImageNodeDoublyLinkedList();
        images_map = std::map<std::string, CLImageNode*>();
    }

    CLBaseImage* get(const std::string& key) {
        auto found = images_map.find(key);
        if(found==images_map.end()) {
            return nullptr;
        }
        CLBaseImage* val = found->second->value;

        // move the node to front
        images_list->move_node_to_head(found->second);
        return val;
    }

    void put(const std::string& key, CLBaseImage* value) {
        if(images_map.find(key)!=images_map.end()) {
            // if key already present, update value and move node to head
            images_map[key]->value = value;
            images_list->move_node_to_head(images_map[key]);
            return;
        }

        free_images_cache(capacity);

        // add new node to head to Queue
        CLImageNode *node = images_list->add_node_to_head(key, value);
        size += APPROX_EMPTY_NODE_SIZE;
        size += value->size();
        images_map[key] = node;
//        Log_debug("put image to cache item size:%d cache size:%d", value->get_area_pointer()->size, size);
    }

    ~LRUImagesCache() {
        std::map<std::string, CLImageNode*>::iterator i1;
        for(i1=images_map.begin();i1!=images_map.end();i1++) {
            delete i1->second;
        }
        delete images_list;
    }

    void free_images_cache(unsigned int keep_size) {
        unsigned int sprite_size;
        CLImageNode *rear_node;
        while (size >= keep_size) {
            rear_node = images_list->get_rear_node();
            if (!rear_node) {
                break;
            }
            sprite_size = APPROX_EMPTY_NODE_SIZE;
            sprite_size += rear_node->value->size();
//            Log_debug("free_images_cache keep_size:%d delete item:%s size:%d", keep_size, rear_node->key.c_str(), sprite_size);
            //delete rear_node->value;
            images_map.erase(rear_node->key);
            images_list->remove_rear_node();
            size -= sprite_size;
        }
//        Log_debug("free_images_cache cache size:%d", size);
    }

    void free_item(const std::string& key) {
        int erased = images_map.erase(key);
//        Log_debug("free_item %s erased=%d", key.c_str(), erased);
        if (erased) {
            CLImageNode *found_node = nullptr;
            CLImageNode *node = images_list->get_rear_node();
//            Log_debug("rear_node=%p", node);
            while (node) {
//                Log_debug("node=%p nkey=%s key=%s", node, node->key.c_str(), key.c_str());
                if (node->key == key) {
                    found_node = node;
                    break;
                }
                node = node->prev;
            }
            if (found_node) {
//                Log_debug("free_item delete node %p %s", found_node, key.c_str());
//                delete found_node->value;
                images_list->remove_node(found_node);
            }
        } else {
            Logger::info("LRUImagesCache::free_item item not found: %s", key.c_str());
        }
    }
};

LRUImagesCache* CLImageCache::lru_cache = nullptr;
std::string CLImageCache::thumb_cache_path;

void CLImageCache::init_images_cache(unsigned int capacity, const std::string& _thumb_cache_path) {
    delete lru_cache;
    lru_cache = new LRUImagesCache(capacity);
    thumb_cache_path = _thumb_cache_path;
}

void CLImageCache::free_images_cache(unsigned int keep_size) {
    lru_cache->free_images_cache(0);
}

CLBaseImage* CLImageCache::load_cached_image(const tbx::Path &file_p) {
    CLBaseImage* img = lru_cache->get(file_p.name());
    if (img) {
        return img;
    }
    img = CLImageFactory::load_image(file_p.name());
    if (img) {
        lru_cache->put(file_p.name(), img);
        return img;
    } else {
        Logger::error("CLImageCache::load_cached_image image %s not valid (deleted)", file_p.name().c_str());
        return nullptr;
    }
}

std::string CLImageCache::make_thumbnail_key(const tbx::Path &file_p) {
    std::string result = CLUtils::str_replace_all(CLUtils::str_replace_all(file_p.name(), "::", "-"),".$","");
    tbx::PathInfo info;
    info.read(file_p);

    if (info.file()) {
        result.append("~");
        result.append(CLUtils::to_string(info.length()));
        result.append(CLUtils::to_string(info.load_address()));
        result.append(CLUtils::to_string(info.exec_address()));
    }
    return result;
}

tbx::Path CLImageCache::get_thumbs_cache_path(const tbx::Path& src_p) {
    return tbx::Path(CLImageCache::thumb_cache_path + "." + make_thumbnail_key(src_p));
}

void CLImageCache::rename_thumbnail(const std::string key_old, const std::string key_new) {
    tbx::Path thumb_cache_p_old = CLImageCache::thumb_cache_path + "." + key_old;
    Log_debug("CLImageCache::rename_thumbnail %s -> %s old thumb %s exists: %d", key_old.c_str(), key_new.c_str(), thumb_cache_p_old.name().c_str(), thumb_cache_p_old.exists());
    lru_cache->free_item(key_old);
    if (thumb_cache_p_old.exists()) {
        tbx::Path thumb_cache_p_new = CLImageCache::thumb_cache_path + "." + key_new;
        thumb_cache_p_new.remove();
        try {
            thumb_cache_p_old.rename(thumb_cache_p_new);
        } catch (tbx::OsError &err)  {
            Log_error("CLImageCache::rename_thumbnail rename %s -> %s, error: %s", thumb_cache_p_old.name().c_str(), thumb_cache_p_new.name().c_str(), err.what());
        }
        Log_debug("CLImageCache::rename_thumbnail %s -> %s", key_old.c_str(), key_new.c_str());
    }
}

void CLImageCache::invalidate_thumbnal(const std::string& filename) {
    std::string key = make_thumbnail_key(filename);
    if (!key.empty()) {
        tbx::Path thumb_cache_p = CLImageCache::thumb_cache_path + "." + key;
        thumb_cache_p.remove();
        lru_cache->free_item(key);
    }
}

void CLImageCache::invalidate_all_thumbnals_in_directory(const std::string& path) {
    std::string key = make_thumbnail_key(path), k;
    if (!key.empty()) {
        tbx::Path thumb_cache_p = CLImageCache::thumb_cache_path + "." + key, p;
        Log_debug("CLImageCache::invalidate_all_thumbnals_in_directory p=%s", thumb_cache_p.name().c_str());
        for(tbx::PathInfo::Iterator it = tbx::PathInfo::begin(thumb_cache_p); it != tbx::PathInfo::end(); ++it) {
            if (it->file()) {
                p = thumb_cache_p.child(it->name());
                k = key + "." + it->name();
                lru_cache->free_item(k);
                p.remove();
            }
        }
    }
}

CLBaseImage* CLImageCache::get_or_create_cached_thumbnail(const tbx::Path &file_p, int maxw, int maxh) {
    std::string key = make_thumbnail_key(file_p);
    if (!key.empty()) {
        CLBaseImage *thumb = get_cached_thumbnail(file_p, key);
        if (!thumb) {
            thumb  = create_cached_thumbnail(file_p, key, maxw, maxh);
        }
        return thumb;
    } else {
        return nullptr;
    }
}

CLBaseImage* CLImageCache::get_cached_thumbnail(const tbx::Path &file_p, const std::string& cache_key) {
    CLBaseImage *thumb = lru_cache->get(cache_key);
    if (thumb) {
//        Log_debug("CLImageCache::get_cached_thumbnail return from mem cache %s", file_p.name().c_str());
        return thumb;
    }
    std::string thumb_cache_path_name = CLImageCache::thumb_cache_path + "." + cache_key;
    tbx::Path thumb_p = thumb_cache_path_name;

    if (thumb_p.exists()) {
        int ft = thumb_p.file_type();
        if (ft < 0x1000) {
//        Log_debug("CLImageCache::get_cached_thumbnail attempt to load: %s ft:%x", file_p.name().c_str(), ft);
            switch (ft) {
                case FILE_TYPE_JPEG:
//                    Log_debug("CLImageCache::get_cached_thumbnail JPEG", 1);
                    thumb = new CLJPEGImage();
                    break;
                case FILE_TYPE_PNG:
//                    Log_debug("CLImageCache::get_cached_thumbnail PNG", 1);
                    thumb = new CLPNGImage();
                    break;
                case FILE_TYPE_SPRITE:
//                    Log_debug("CLImageCache::get_cached_thumbnail SPR", 1);
                    thumb = new CLUserSpriteImage();
                    break;
                default:
                    break;
            }
            if (!thumb) {
                Logger::error("CLImageCache::get_cached_thumbnail unknown thumbnail filetype, name:%s type:%x", thumb_p.name().c_str(), thumb_p.file_type());
                return nullptr;
            }

            if (thumb->load(thumb_cache_path_name)) {
//                Log_debug("get_cached_thumbnail loaded:%s", thumb_cache_path_name.c_str());
                lru_cache->put(cache_key, thumb);
            } else {
                delete thumb;
                thumb = nullptr;
            }
        }
    }
//    Log_debug("get_cached_thumbnail load:%s return:%p", thumb_cache_path_name.c_str(), thumb);
    return thumb;
}

void CLImageCache::delete_cached_thumbnails(const tbx::Path& file_p) {
    std::string filename = file_p.leaf_name();
    std::string prefix = filename.substr(0, filename.find("~"));
    tbx::Path dir_p = file_p.parent();
    int prefix_len = prefix.length();
//    Log_debug("CLImageCache::delete_cached_thumbnails for %s prefix:%s", file_p.name().c_str(), prefix.c_str());
    for(tbx::PathInfo::Iterator it = tbx::PathInfo::begin(dir_p); it != tbx::PathInfo::end(); ++it) {
        if (it->name().substr(0, prefix_len) == prefix) {
//            Log_debug("CLImageCache::delete_cached_thumbnails delete %s", dir_p.child(it->name()).name().c_str());
            dir_p.child(it->name()).remove();
        }
    }
}

CLBaseImage* CLImageCache::create_cached_thumbnail(const tbx::Path &file_p, const std::string& cache_key, int maxw, int maxh) {
    CLBaseImage *img = nullptr;
    CLBaseImage *thumb = nullptr;
    std::string thumb_cache_path_name = CLImageCache::thumb_cache_path + "." + cache_key;
    tbx::Path thumb_p = thumb_cache_path_name;

    tbx::Hourglass hg;
    hg.on();

    delete_cached_thumbnails(thumb_cache_path_name);
//    Log_debug("CLImageCache::create_cached_thumbnail %s src: %s", thumb_p.name().c_str(), file_p.name().c_str());
    img = CLImageFactory::load_image(file_p);
//    Log_debug("CLImageCache::create_cached_thumbnail loading src:%s thumb:[%s] img:%p", file_p.name().c_str(), thumb_cache_path_name.c_str(), img);
    if (img) {
        bool skip_thumbnail = (img->width_px() <= maxw && img->height_px() <= maxh && (img->get_image_type() == CL_IMAGE_TYPE_JPEG || img->get_image_type() == CL_IMAGE_TYPE_SPRITE || img->get_image_type() == CL_IMAGE_TYPE_PNG));
        if (skip_thumbnail) {
//            Log_debug("CLImageCache::create_cached_thumbnail use orig image as it already fits src:%s", file_p.name().c_str());
            thumb = img;
        } else {
            Log_debug("CLImageCache::create_cached_thumbnail making thumbnail %s for %s", thumb_cache_path_name.c_str(), file_p.name().c_str());
            thumb = CLImageFactory::make_thumbnail(maxw, maxh, img);
        }
    }
    if (thumb) {
        if (thumb != img) {
            Log_debug("CLImageCache::create_cached_thumbnail saving to %s w:%d h:%d sprw:%d sprh:%d", thumb_cache_path_name.c_str(), thumb->width(), thumb->height(), thumb->width_px(), thumb->height_px());
            if (CLUtils::create_directories_for_file(thumb_cache_path_name)) {
                thumb->save(thumb_cache_path_name);
            }
        }
        lru_cache->put(cache_key, thumb);
    }
    hg.off();

//    Log_debug("CLImageCache::create_cached_thumbnail finish img=%p thumb=%p", img, thumb);
    if (img && img != thumb) {
//        Log_debug("delete img %p", img);
        delete img;
    }
    return thumb;
}

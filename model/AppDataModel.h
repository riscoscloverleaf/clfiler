//
// Created by lenz on 8/31/21.
//

#ifndef CLFILER_APPDATAMODEL_H
#define CLFILER_APPDATAMODEL_H

#include <map>
#include <list>
#include <functional>
#include <memory>
#include <tbx/application.h>
#include <tbx/sprite.h>
#include <tbx/path.h>
#include <tbx/command.h>
#include <tbx/ext/treeview.h>
#include <tbx/prepolllistener.h>
#include <tbx/postpolllistener.h>
#undef NONE
#include <tbx/fileraction.h>
#include "cloverleaf/Logger.h"
#include "AppTypes.h"

#define CLDIRITEM_FLAG_REALLY_EXSITS                  (unsigned int) 1
#define CLDIRITEM_FLAG_IS_FILE                        (unsigned int) 2
#define CLDIRITEM_FLAG_IS_IMAGE                       (unsigned int) 4
#define CLDIRITEM_FLAG_HAS_SUBDIRS                    (unsigned int) 8
#define CLDIRITEM_FLAG_APP_BOOTED                     (unsigned int) 16
#define CLDIRITEM_FLAG_HAS_POSSIBLE_BIG_IMAGES        (unsigned int) 32
//#define CLDIRITEM_FLAG_WAS_DISPLAYED                  (unsigned int) 64
//#define CLDIRITEM_FLAG_SMALL_SPRITE_LOADED            (unsigned int) 128
#define CLDIRITEM_FLAG_HAS_FILES                      (unsigned int) 256
#define CLDIRITEM_FLAG_CACHE_LOADED                   (unsigned int) 512
#define CLDIRITEM_FLAG_NEED_FULL_SCAN                 (unsigned int) 1024
#define CLDIRITEM_FLAG_FULL_SCANNED                   (unsigned int) 2048
#define CLDIRITEM_FLAG_SPRITE_RELOADED                (unsigned int) 4096
#define CLDIRITEM_FLAG_SPRITE_APP_DEFAULT             (unsigned int) 8192

#define CLSEARCH_FLAG_SEARCH_IN_IMAGES         (unsigned int) 1
#define CLSEARCH_FLAG_SEARCH_IN_APPS           (unsigned int) 2
#define CLSEARCH_FLAG_SEARCH_CASE_SENSITIVE    (unsigned int) 4

#define CLREFRESH_WITH_YIELD                (unsigned int) 1
#define CLREFRESH_RUN_ITEM_CALLBACKS        (unsigned int) 2
#define CLREFRESH_CALC_SIZE                 (unsigned int) 4
#define CLREFRESH_FORCE_CALC_SIZE           (unsigned int) 8
#define CLREFRESH_INITIAL_REFRESH_ITEM      (unsigned int) 16
#define CLREFRESH_ONLY_DEEP_NO_CURRENT      (unsigned int) 32

#define CLUPDATED_NONE      0
#define CLUPDATED_ATTRS     1
#define CLUPDATED_MTIME     2
#define CLUPDATED_SIZE      3
#define CLUPDATED_THUMBNAIL 4
#define CLUPDATED_TYPE      5
#define CLUPDATED_NAME      6

#define TIMELINE_FILTER_FOLDERS 1
#define TIMELINE_FILTER_FILES 2
#define TIMELINE_FILTER_APPS 3

#define THUMBNAIL_IMAGE_WIDTH 180
#define THUMBNAIL_IMAGE_HEIGHT 180

class CLUserSpriteImage;
class CLWimpSpriteImage;
class CLBaseImage;

struct CLDirectoryItemDataSerialized {
    unsigned int num_childs = 0;
    int attrs = 0;
    int file_type = 0;
    unsigned int flags = 0;
    long long mtime = 0;
    int64_t size = 0;
    char name[256];

    void serialize(CLDirectoryItemData *item);
    bool save(FILE* out);
    bool load(FILE* in);
};


class CLDirectoryItemData {
friend AppDataModel;
friend CLDirectoryItemDataSerialized;

protected:
    std::string _name;
    std::string _display_name;
    std::string _mtime_str;
    std::string _file_type_str;
    std::string _dir_or_file_size_str;
    std::string _dir_or_file_size_unit;
    std::string _attrs_str;
    CLWimpSpriteImage *_sprite_image = nullptr;
    CLBaseImage *_small_sprite_image = nullptr;
    long long _mtime = 0;
    size_t _file_size = 0;
    int64_t _dir_size = 0;
    unsigned long _id = 0;
    unsigned int _flags = 0;
    int _file_type = 0;
    int _attrs = 0;
    CLDirectoryItemData *_parent = nullptr;
    std::vector<CLDirectoryItemData*> _childs;

public:
    CLDirectoryItemData() {};
    CLDirectoryItemData(tbx::PathInfo& p, CLDirectoryItemData *parent);
    CLDirectoryItemData(const std::string &root_path);
    CLDirectoryItemData(CLDirectoryItemData *parent, CLDirectoryItemDataSerialized& data);
    CLDirectoryItemData(CLDirectoryItemData *parent, CLDirectoryItemData& src);

    ~CLDirectoryItemData();

    void name(const std::string& newname);

    int serialize(CLDirectoryItemDataSerialized& data);

    static int get_file_type(tbx::PathInfo& info);

    tbx::WimpSprite* sprite();
    CLBaseImage* small_sprite_image();
    CLWimpSpriteImage* sprite_image();
    inline std::string sprite_name() { return sprite()->name(); }
    inline const std::string& name() const { return _name; }
    inline unsigned int id() { return _id; }
    const std::string &display_name();
    const std::string &file_type_str();
//    const std::string &file_size_str();
    const std::string &attrs_str();
    const std::string &mtime_str();
    std::string mtime_short_str();
    std::string dir_or_file_size_str();
    std::string dir_or_file_size_unit();
    std::string dir_or_file_size_str_with_unit();
    inline int file_type() { return _file_type; }
    inline int attrs() { return _attrs; }
    inline size_t file_size() { return _file_size; }
    inline int64_t  dir_size() { return _dir_size; }
    inline long long mtime() { return _mtime; }
    inline int64_t dir_or_file_size() { return is_file() ? _file_size : _dir_size; };
    inline bool is_image() { return _flags & CLDIRITEM_FLAG_IS_IMAGE; }
    inline bool is_file() { return _flags & CLDIRITEM_FLAG_IS_FILE; }
    inline bool is_full_scanned() { return _flags & CLDIRITEM_FLAG_FULL_SCANNED; }
    inline bool is_directory() { return _file_type == tbx::FILE_TYPE_DIRECTORY; }
    inline bool is_application() { return _file_type == tbx::FILE_TYPE_APPLICATION; }

    inline bool is_protected() { return (!(_attrs & tbx::PathInfo::OWNER_WRITE) && (_attrs & tbx::PathInfo::OWNER_LOCKED)); }
    inline bool is_unprotected() { return ((_attrs & tbx::PathInfo::OWNER_WRITE) && !(_attrs & tbx::PathInfo::OWNER_LOCKED)); }
    inline bool is_public() { return ((_attrs & tbx::PathInfo::OTHER_READ)); }
    inline bool is_private() { return (!(_attrs & tbx::PathInfo::OTHER_READ)); }

    inline bool has_subdirectories() { return _flags & CLDIRITEM_FLAG_HAS_SUBDIRS; };
    inline bool has_files() { return _flags & CLDIRITEM_FLAG_HAS_FILES; };
    inline bool app_booted() { return _flags & CLDIRITEM_FLAG_APP_BOOTED; };
    inline bool is_cache_loaded() { return _flags & CLDIRITEM_FLAG_CACHE_LOADED; };
//    inline bool was_displayed() { return _flags & CLDIRITEM_FLAG_WAS_DISPLAYED; };
//    inline bool was_displayed(bool val) { if (val) _flags |= CLDIRITEM_FLAG_WAS_DISPLAYED; else _flags &= ~CLDIRITEM_FLAG_WAS_DISPLAYED; };
    inline void app_booted(bool val) { if (val) _flags |= CLDIRITEM_FLAG_APP_BOOTED; else _flags &= ~CLDIRITEM_FLAG_APP_BOOTED; }
    inline bool exists() { return !_name.empty(); }
    inline bool really_exists() { return path().exists(); }
    inline const std::vector<CLDirectoryItemData*>& childs() { return _childs; };
    inline CLDirectoryItemData* parent() { return _parent; };
    bool has_help();
    CLDirectoryItemData* root();
    bool has_child(unsigned int id);
    bool is_modified();

    CLDirectoryItemData* find_child_by_name(const std::string& name);
    CLDirectoryItemData* find_child_by_id(unsigned int id);
    tbx::Path path();
    void boot_app();
    int update(tbx::PathInfo& info);
    void flush_app_icons();
    void update_dir_size(int64_t dir_size, int options);
    void fs_rename(const std::string& newname);
    void fs_remove_files(const std::vector<std::string>& files);
    void fs_remove();
    void fs_create_directory(const std::string &newname);
    void fs_set_filetype(int filetype);
    void fs_set_filetype_by_ext();
    void fs_run();
    void fs_open_help();
    void fs_stamp();
    void fs_copy_local(const std::string& newname);
    void fs_set_dir();
    bool refresh(unsigned int options = CLREFRESH_WITH_YIELD);
    bool refresh_deep(int refresh_depth, unsigned int options = CLREFRESH_WITH_YIELD);
    void calc_fake_dir_size();
    void copy_item_info(CLDirectoryItemData* src);
    void move_item_info(CLDirectoryItemData* src);
    bool can_set_filetype_by_ext();
};

class CLTimelineItemData : public CLDirectoryItemData {
public:
    explicit CLTimelineItemData(std::string& full_name, tbx::PathInfo& info);
};

class CLTimelineModel {
private:
    std::vector<unsigned long> _item_ids;
public:
    CLTimelineModel() {};
    void append(CLDirectoryItemData *item);
    std::vector<CLDirectoryItemData*> filtered_items(int timeline_filter);
    bool save();
    bool load();
};

extern CLTimelineModel g_timeline_model;

//strcut CLDirectoryItemCacheStruct {
//    std::string _name;
//    long long _mtime = 0;
//    size_t _file_size = 0;
//    int64_t _dir_size = 0;
//    int _file_type = 0;
//    int _attrs = 0;
//};

class CLFilerAction : public tbx::FilerAction, public tbx::FilerActionFinishedListener {
public:
    enum CLFilerActionOperation {
        CL_OP_NONE,
        CL_OP_COPY,
        CL_OP_MOVE,
    };
private:
    std::string _refresh_src;
    std::string _refresh_dst;
    std::vector<std::string> _src_objects;
    CLFilerActionOperation _operation;
    bool _refresh_childs;
    bool _dst_exsist_will_merged;
public:
    CLFilerAction(const std::string& src_dir);
    CLFilerAction(const std::string& src_dir, const std::vector<std::string> &src_files);
    void fs_add_object(const std::string& src_obj);
    void send_selected_objects();

    void fs_copy(const std::string& dst_dir, int options = 0);
    void fs_rename(const std::string& dst_dir, int options = 0);
    void fs_move(const std::string& dst_dir, int options = 0);
    void fs_remove(int options = 0);
    void fs_stamp(int options = 0);
    void fs_set_access(unsigned int set_bits, unsigned int leave_bits, int options = 0);
    void set_refresh_src(const std::string& src, bool refresh_childs = false) { _refresh_src = src; _refresh_childs = refresh_childs; }
    void fileraction_finished() override;
};


class CLSearchModelListener {
public:
    virtual void on_search_state_callback(unsigned int, CLDirectoryItemData*) = 0;
};

enum SearchState {
    SEARCHING,
    FOUND,
    FINISHED,
    STOPPED
};

class CLSearchModel : public tbx::Command {
private:
    unsigned int _search_options;
    std::string _search_substring;
    CLSearchModelListener *_listener;
    CLDirectoryItemData *_searching_in_item = nullptr;
    SearchState _search_state = SearchState::STOPPED;
//    std::vector<CLDirectoryItemData> _found_items;
    std::vector<CLDirectoryItemData*> _search_items;

    void search_in_item(CLDirectoryItemData* item);
public:
    CLSearchModel(CLSearchModelListener *listener) : _listener(listener) {  _search_state = SearchState::STOPPED; };
    virtual ~CLSearchModel();

    void start(CLDirectoryItemData* item, const std::string& substring, unsigned int options);
    void stop();
    void pause(bool paused = true);
    void remove_search_item(CLDirectoryItemData* item);
    void add_search_item(CLDirectoryItemData* item);
    CLDirectoryItemData* get_searching_in_item() { return _searching_in_item; }
    SearchState get_search_state() { return _search_state; }
    const std::string& search_substring() { return _search_substring; }
//    std::vector<CLDirectoryItemData>& found() { return _found_items; }

    void execute() override;
};


class CLDirItemsIdleRefresher : public tbx::Command {
    struct RefreshItem {
        CLDirectoryItemData *item = 0;
        int refresh_depth = 0;
        unsigned int options = 0;
    };
private:
//    int tick = 1;
    bool executing = false;
    bool refreshing = false;
    bool aborted = false;
    bool any_item_changed = false;
//    RefreshItem last_refresh_item;
    std::vector<RefreshItem> _items_to_refresh;
public:
    void add_refreshed_item(CLDirectoryItemData *item, int refresh_depth, unsigned int options, bool first=false);
    void remove_refresh_item(CLDirectoryItemData *item);
    void execute() override;
    void abort();
    inline bool is_refreshing() { return refreshing; }
    bool is_refreshing_item(CLDirectoryItemData* item);
};

class CLIdleCacheSaver : public tbx::Command {
private:
    bool saving = false;
    std::vector<unsigned long> _items_to_save;
public:
    void save_cache_on_idle(CLDirectoryItemData *item);
    void cancel_save(CLDirectoryItemData *item);
    void execute() override;
    inline bool is_saving() { return saving; }
};


struct CLThumbSize {
    int width;
    int height;
    unsigned long size;
};

class CLThumbSizes {
public:
    std::map<std::string, CLThumbSize> thumb_sizes;
    std::string path;
    bool needs_to_be_save = false;

    CLThumbSizes(const std::string& p) : path(p) {};

    static CLThumbSizes* load(const std::string& path);

    void save();
    bool get_thumb_size(CLDirectoryItemData* item, int &w, int &h);
    bool set_thumb_size(CLDirectoryItemData* item, int w, int h);
    int size() { return thumb_sizes.size(); };
};

class CLThumbnailer {
private:
    std::vector<CLThumbSizes*> _thumbnail_sizes;
    CLThumbSizes* get_thumb_sizes_for_item(CLDirectoryItemData* item);

public:
    class CLThumbSizesIdleSaver : public tbx::Command {
        public:
            bool saving = false;
            CLThumbnailer *_thumbnailer;
            explicit CLThumbSizesIdleSaver(CLThumbnailer *thumbnailer) : _thumbnailer(thumbnailer) {};
            void execute() override {
                saving = false;
                tbx::Application::instance()->remove_idle_command(this);
//                Log_debug("_thumbnail_sizes %d", _thumbnailer->_thumbnail_sizes.size());
                for(auto tsz : _thumbnailer->_thumbnail_sizes) {
                    if (tsz->needs_to_be_save) {
//                        Log_debug("_thumbnail_sizes saving %s", tsz->path.c_str());
                        tsz->save();
                    }
                }
            };
            void save_on_idle() {
                if (!saving) {
                    saving = true;
                    tbx::Application::instance()->add_idle_command(this);
//                    Log_debug("thumb_sizes save_on_idle, will save",1);
//                } else {
//                    Log_debug("thumb_sizes save_on_idle, already saving",1);
                }
            }
            inline bool is_saving() { return saving; }
    } _thumb_sizes_saver;

    class CLThumbIdleCreator : public tbx::Command {
    public:
        bool _creating = false;
        CLThumbnailer *_thumbnailer;
        std::vector<unsigned long> item_ids;

        explicit CLThumbIdleCreator(CLThumbnailer *thumbnailer) : _thumbnailer(thumbnailer) {};

        void execute() override;
        void create_on_idle(CLDirectoryItemData* item);
        inline bool is_creating() { return _creating; }
    } _thumb_creator;

    CLThumbnailer() :
        _thumb_sizes_saver(this),
        _thumb_creator(this)
    {};

    bool is_creating() { return _thumb_creator.is_creating(); };
    bool get_cached_thumbnail_size(CLDirectoryItemData* item, int &w, int &h);
    bool set_cached_thumbnail_size(CLDirectoryItemData* item, int w, int h);
    CLBaseImage* get_thumbnail(CLDirectoryItemData* item);
    CLBaseImage* create_thumbnail(CLDirectoryItemData* item);
};

extern CLThumbnailer g_thumbnailer;



class AppDataModelChangedListener {
public:
    virtual void on_dir_item_added(CLDirectoryItemData*) = 0;
    virtual void on_dir_item_removed(CLDirectoryItemData*) = 0;
    virtual void on_dir_item_updated(CLDirectoryItemData*, int what) = 0;
    virtual void on_root_item_added(CLDirectoryItemData*) = 0;
    virtual void on_root_item_removed(CLDirectoryItemData*) = 0;
    virtual void on_favorites_changed() = 0;
};

class AppDataModelRefreshListener {
public:
    virtual void on_refresh_finished(bool aborted, bool any_item_changed) {};
    virtual void on_refresh_started() {};
    virtual void on_info_changed() {};
};

class AppDataModel {
friend CLDirectoryItemData;
friend CLDirItemsIdleRefresher;
private:
    unsigned long _id_sequence;
    std::vector<CLDirectoryItemData*> _root_items;
    std::map<unsigned long, CLDirectoryItemData*> _id_2_item;
    std::vector<AppDataModelChangedListener*> _change_listeners;
    std::vector<AppDataModelRefreshListener*> _refresh_listeners;
    CLDirItemsIdleRefresher _idle_refresher;
    CLIdleCacheSaver _idle_cache_saver;
    std::vector<std::string> _favorites;
    std::string running_state;
    bool _favorites_loaded = false;
    bool _new_app_booted = false;
    void add_root_item(const std::string& root);
    CLDirectoryItemData* _root_refreshing_item = nullptr;
    CLDirectoryItemData* _refreshing_item = nullptr;
    int _operations_counter;
//    bool save_childs_state(CLDirectoryItemData *parent_item, FILE *out);
//    bool load_childs_state(CLDirectoryItemData *parent_item, size_t num_childs, FILE *in);
    static char vector_buffer[12+1]; // buffer for 1 vector

public:
//    class CLPrePollListener : public tbx::PrePollListener {
//        bool pre_poll() override;
//    } pre_poll_listener;
//
//    class CLPostPollListener : public tbx::PostPollListener {
//        void post_poll(int reason_code, tbx::PollBlock &poll_block, tbx::IdBlock &id_block, int reply_to) override;
//    } post_poll_listener;

//    AppDataModel();
//    ~AppDataModel();

    enum CopyOrMoveOptions {
        DEFAULT,
        FORCE_COPY,
        FORCE_MOVE
    };
    unsigned long register_item(CLDirectoryItemData* item);
    void deregister_item(CLDirectoryItemData* item);
    const std::vector<CLDirectoryItemData*>& get_root_items() { return _root_items; };
    CLDirectoryItemData* get_root_item(const std::string& root_path);
    CLDirectoryItemData* get_existing_root_item(const std::string& root_path);
    CLDirectoryItemData* find_item_by_path(const std::string& path);
    CLDirectoryItemData* find_item_by_id(unsigned long id);
    std::vector<CLDirectoryItemData*> find_items_by_ids(const std::vector<unsigned long> &ids);
    std::vector<std::string> items_names(const std::vector<CLDirectoryItemData*> &items);
    std::vector<unsigned long> items_ids(const std::vector<CLDirectoryItemData*> &items);
    std::vector<std::string> items_names_by_ids(const std::vector<unsigned long> &items);

    CLDirectoryItemData* get_or_refresh_path(std::string dpath);
    std::string get_csd();
    void refresh_roots(bool removable_only = false);
    void refresh_on_idle(CLDirectoryItemData* item, int refresh_depth, unsigned int options);
    void refresh_on_idle_first(CLDirectoryItemData* item, int refresh_depth, unsigned int options);
    tbx::Path find_real_existing_dir_or_parent(tbx::Path dir);
    void on_refresh_started(CLDirectoryItemData *item);
    void on_refresh_finished(bool aborted, bool any_item_changed);
    void on_info_change();
    void abort_refresh();
    inline bool is_refreshing() { return _idle_refresher.is_refreshing(); }
    inline bool is_saving_cache() { return _idle_cache_saver.is_saving(); }

    CLDirectoryItemData* get_root_refreshing_item() { return _root_refreshing_item; }
    CLDirectoryItemData* get_refreshing_item() { return _refreshing_item; }
    void on_dir_item_added(CLDirectoryItemData*);
    void on_dir_item_removed(CLDirectoryItemData*);
    void on_dir_item_updated(CLDirectoryItemData*, int what);
    void on_root_item_added(CLDirectoryItemData*);
    void on_root_item_removed(CLDirectoryItemData*);
    void on_favorites_changed();
    void fs_copy(const std::string &src_dir, const std::vector<std::string> &src_files, const std::string &dst_dir);
    void fs_copy_or_move(const std::string &src_dir, const std::vector<std::string> &src_files, const std::string &dst_dir, int flags);
    void fs_stamp(const std::string &src_dir, const std::vector<std::string> &src_files);
    void fs_count(const std::string &src_dir, const std::vector<std::string> &src_files);
    void fs_access(const std::string &src_dir, const std::vector<std::string> &src_files, unsigned int changed_bit, unsigned int protected_mask_bits, bool recursive);
    void add_change_listener(AppDataModelChangedListener* listener);
    void remove_change_listener(AppDataModelChangedListener* listener);
    void add_refresh_listener(AppDataModelRefreshListener* listener);
    void remove_refresh_listener(AppDataModelRefreshListener* listener);

    void new_app_booted(bool val) { _new_app_booted = val; }
    bool new_app_booted() { return _new_app_booted; }

    std::vector<std::string> get_favorites();
    const std::vector<std::string>& add_favorite(const std::string& fav);
    const std::vector<std::string>& remove_favorite(const std::string& fav);
    bool is_favorite(const std::string& fav);

    bool save_state();
    bool save_state(CLDirectoryItemData* root_item);
    bool load_state();

    bool load_diritem_cache(CLDirectoryItemData *item, int depth = 0);
    bool save_diritem_cache(CLDirectoryItemData *item);
    bool remove_diritem_cache(CLDirectoryItemData *item);
    void save_diritem_cache_on_idle(CLDirectoryItemData *item);
    void rename_diritem_cache(const tbx::Path& oldpath, const tbx::Path& newpath);
    std::string get_diritem_cache_filename(CLDirectoryItemData *item);
    std::string get_diritem_cache_filename(const tbx::Path &p);

    void load_autofiletypes();
};

extern AppDataModel g_app_data_model;
#endif //CLFILER_APPDATAMODEL_H

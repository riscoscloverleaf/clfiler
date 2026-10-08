//
//
// Created by lenz on 8/31/21.
//

#include <fstream>
#include <oslib/osfscontrol.h>
#include <oslib/sharefs.h>
#include <oslib/os.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <tbx/path.h>
#include <tbx/application.h>
#include <cloverleaf/Logger.h>
#include <cloverleaf/CLImage.h>
#include <cloverleaf/CLImageCache.h>
#include <cloverleaf/CLUtf8.h>
#include <cloverleaf/CLUtils.h>
#include <tbx/swixcheck.h>
#include <tbx/messagewindow.h>
#include <tbx/stringutils.h>
#include "../utils.h"
#include "../global.h"
#include "AppDataModel.h"
#include "DrivesDetect.h"

struct AutoFiletypeExt {
    int file_type;
    std::vector<std::string> extensions;
};
static std::vector<AutoFiletypeExt> auto_filetype_extensions;

//static char *fsnames_with_drives[] = { "ADFS", "ADFS4", "SCSIFS", "SCSI", "CDFS", "IDEFS", "IDE", "ATAFS", "ZIDEFS", "MASSFS", "SDFS", "SATAFS", "SATA", "HDFFS","DOSFS", "DOS", "FAT32FS", "FAT32", "RAM", nullptr };
static char *fsnames_with_drives_removable[] = { "SDFS", "FAT32FS", "FAT32", nullptr };

AppDataModel g_app_data_model = AppDataModel();
CLTimelineModel g_timeline_model = CLTimelineModel();
CLThumbnailer g_thumbnailer = CLThumbnailer();
char AppDataModel::vector_buffer[12+1];

CLDirectoryItemData::CLDirectoryItemData(tbx::PathInfo& info, CLDirectoryItemData *parent) {
    _id = g_app_data_model.register_item(this);
    _parent = parent;
    update(info);

    //    Log_debug("CLDirectoryItemData::CLDirectoryItemData %p %s", this, info.name().c_str());
}

CLDirectoryItemData::CLDirectoryItemData(const std::string& root_dir) :
    _file_type(0x1000),
    _attrs(0),
    _mtime(0),
    _file_size(-1),
    _flags(CLDIRITEM_FLAG_REALLY_EXSITS)
{
    _id = g_app_data_model.register_item(this);
    name(root_dir);
    tbx::WimpSprite spr = tbx::WimpSprite("harddisc");
    _sprite_image = new CLWimpSpriteImage(&spr);
//    Log_debug("CLDirectoryItemData::CLDirectoryItemData root %p %s", this, root_dir.c_str());
}

CLDirectoryItemData::CLDirectoryItemData(CLDirectoryItemData *parent, CLDirectoryItemDataSerialized& data)
{
    _id = g_app_data_model.register_item(this);
    _name = data.name;
    _flags = data.flags;
    _file_type = data.file_type;
    _mtime = data.mtime;
    _attrs = data.attrs;
    if (_flags & CLDIRITEM_FLAG_IS_FILE) {
        _file_size = data.size;
    } else {
        _dir_size = data.size;
    }
    _parent = parent;
    _parent->_childs.push_back(this);
}

CLDirectoryItemData::CLDirectoryItemData(CLDirectoryItemData *parent, CLDirectoryItemData& src)
{
    _id = g_app_data_model.register_item(this);
    _name = src._name;
    _flags = src._flags;
    _file_type = src._file_type;
    _mtime = src._mtime;
    _attrs = src._attrs;
    _file_size = src._file_size;
    _dir_size = src._dir_size;
    if (src._small_sprite_image) {
        if (src._small_sprite_image->get_image_type() == CL_IMAGE_TYPE_WIMP_SPRITE) {
            _small_sprite_image = new CLWimpSpriteImage(((CLWimpSpriteImage*)src._small_sprite_image)->wimp_sprite());
        } else {
            _small_sprite_image = new CLUserSpriteImage(*(CLUserSpriteImage*)src._small_sprite_image);
        }
    }
    if (src._sprite_image) {
        _sprite_image = new CLWimpSpriteImage(src._sprite_image->wimp_sprite());
    }
    _parent = parent;
    _parent->_childs.push_back(this);
}

CLDirectoryItemData::~CLDirectoryItemData() {
    Log_debug("CLDirectoryItemData::~CLDirectoryItemData %p %s", this, _name.c_str());
    //g_app_data_model.on_dir_item_removed(this);
    g_app_data_model.deregister_item(this);
    delete _sprite_image;
    delete _small_sprite_image;
    if (!_childs.empty()) {
        for(auto child: _childs) {
            delete child;
        }
    }
    _parent = nullptr;
}

void CLDirectoryItemData::flush_app_icons() {
    if (is_application()) {
        delete _sprite_image;
        delete _small_sprite_image;

        _sprite_image = nullptr;
        _small_sprite_image = nullptr;

        for(auto ch : _childs) {
            if (ch->is_application()) {
                ch->flush_app_icons();
            }
        }
    }
}

int CLDirectoryItemData::update(tbx::PathInfo& info) {
    int updated = 0, should_boot = false;
    int ft;

    if (!info.exists()) {
        return CLUPDATED_NONE;
    }

    if (_name != info.name()) {
//        Log_debug("CLDirectoryItemData::update name != info.name %s != %s", _name.c_str(), info.name().c_str());
        _name = info.name();
        _display_name.clear();
        delete _sprite_image;
        delete _small_sprite_image;
        _sprite_image = nullptr;
        _small_sprite_image = nullptr;
        updated = CLUPDATED_NAME;
        app_booted(false);
    }

    _flags &= (~CLDIRITEM_FLAG_IS_IMAGE & ~CLDIRITEM_FLAG_IS_FILE);
    ft = get_file_type(info);
    if (ft < 0x1000) {
        if (info.image_file()) {
            _flags |= (CLDIRITEM_FLAG_IS_FILE | CLDIRITEM_FLAG_IS_IMAGE);
        } else {
            _flags |= CLDIRITEM_FLAG_IS_FILE;
        }
    }

    if (ft != _file_type) {
//        Log_debug("CLDirectoryItemData::update ft != _file_type [%x != %x]", ft, _file_type);
        _file_type = ft;
        _file_type_str.clear();
        delete _sprite_image;
        delete _small_sprite_image;
        _sprite_image = nullptr;
        _small_sprite_image = nullptr;
        updated = std::max(CLUPDATED_TYPE, updated);
    }

    if (_flags & CLDIRITEM_FLAG_IS_FILE) {
        int fsize = info.length();
        if (fsize != _file_size) {
            _file_size = fsize;
            _dir_or_file_size_str.clear();
            updated = std::max(CLUPDATED_SIZE, updated);
        }
    }

    if (_attrs != info.attributes()) {
        _attrs = info.attributes();
        _attrs_str.clear();
        updated = std::max(CLUPDATED_ATTRS, updated);
    }

    long long mt = info.modified_time().centiseconds();
    if (_mtime != mt) {
        _mtime = mt;
        _mtime_str.clear();
//        _flags |= CLDIRITEM_FLAG_MTIME_CHANGED;
        updated = std::max(CLUPDATED_MTIME, updated);
    }

//    Log_debug("name %s ft:%x raw_ft:%x img:%d", info.name().c_str(), _file_type, info.raw_file_type(), info.image_file());
//    if (is_application() && !app_booted()) {
//        boot_app();
//    }
    return updated;
}

int CLDirectoryItemData::get_file_type(tbx::PathInfo& info) {
    int ft = info.file_type();
    if (ft != tbx::FILE_TYPE_APPLICATION) {
        if (info.directory()) {
            return tbx::FILE_TYPE_DIRECTORY;
        } else if (info.image_file()) {
            return info.raw_file_type();
        } else {
            return ft;
        }
    }
    return ft;
}

void CLDirectoryItemData::name(const std::string &newname) {
    _name = newname;
    _display_name.clear();
//    _path.clear();
}

const std::string& CLDirectoryItemData::display_name() {
    if (_display_name.empty()) {
        if (_name.length() > 28) {
            _display_name = _name.substr(0, 28) + "...";
        } else {
            _display_name = _name;
        }
    }
    return _display_name;
}

//const std::string& CLDirectoryItemData::name_utf8() {
//    if (_name_utf8.empty()) {
//        _name_utf8 = riscos_local_to_utf8(_name);
//    }
//    return _name_utf8;
//}

const std::string& CLDirectoryItemData::file_type_str() {
    if (_file_type_str.empty()) {
        switch(_file_type) {
            case tbx::FILE_TYPE_APPLICATION:
                _file_type_str = "Application";
                break;
            case tbx::FILE_TYPE_DIRECTORY:
                _file_type_str = "Directory";
                break;
            default:
                _file_type_str = get_file_type_text(_file_type);
        }
    }
    return _file_type_str;
}

const std::string& CLDirectoryItemData::attrs_str() {
//    Log_debug("CLDirectoryItemData::attrs_str id:%d this:%p", _id, this);
    if (_attrs_str.empty()) {
        _attrs_str = "";
        if ((_attrs & tbx::PathInfo::OWNER_LOCKED)) {
            _attrs_str.append("L");
        }
        if ((_attrs & tbx::PathInfo::OWNER_WRITE)) {
            _attrs_str.append("W");
        }
        if ((_attrs & tbx::PathInfo::OWNER_READ)) {
            _attrs_str.append("R");
        }
        _attrs_str.append("/");
        if ((_attrs & tbx::PathInfo::OTHER_LOCKED)) {
            _attrs_str.append("l");
        }
        if ((_attrs & tbx::PathInfo::OTHER_WRITE)) {
            _attrs_str.append("w");
        }
        if ((_attrs & tbx::PathInfo::OTHER_READ)) {
            _attrs_str.append("r");
        }
    }
    return _attrs_str;
}

const std::string& CLDirectoryItemData::mtime_str() {
    if (_mtime_str.empty()) {
        _mtime_str = tbx::UTCTime(_mtime).text();
    }
    return _mtime_str;
}

std::string CLDirectoryItemData::mtime_short_str() {
    char buf[20];
    tbx::UTCTime(_mtime).text("%DY.%MN.%CE%YR %24:%MI", buf, sizeof(buf));
    return std::string(buf);
}

tbx::WimpSprite *CLDirectoryItemData::sprite() {
    return sprite_image()->wimp_sprite();
}

CLWimpSpriteImage *CLDirectoryItemData::sprite_image() {
    if (!_sprite_image) {
//        if (_file_type == tbx::FILE_TYPE_APPLICATION && !app_booted()) {
//            boot_app();
//        }
        tbx::WimpSprite spr = tbx::WimpSprite(_file_type, _name);
        _sprite_image = new CLWimpSpriteImage(&spr);

        if (is_application() && strcmp(_sprite_image->name(),"application") == 0) {
            _flags |= CLDIRITEM_FLAG_SPRITE_APP_DEFAULT;
        }
//        _sprite_image = new tbx::WimpSprite(_file_type, _name);
        Log_debug("CLDirectoryItemData::sprite spr:%s name:%s", _sprite_image->name(), _name.c_str());
    } else {
        if (is_application() && (_flags & CLDIRITEM_FLAG_SPRITE_APP_DEFAULT) && !(_flags & CLDIRITEM_FLAG_SPRITE_RELOADED)) {
            Log_debug("CLDirectoryItemData::sprite_image reload sprite %s", name().c_str());
            if (_sprite_image) {
                delete _sprite_image;
            }
            tbx::WimpSprite spr = tbx::WimpSprite(_file_type, _name);
            if (strcmp(spr.name().c_str(), "application") == 0) {
                Log_debug("CLDirectoryItemData::sprite_image reloaded sprite but it still default %s", name().c_str());
                if (!app_booted()) {
                    Log_debug("CLDirectoryItemData::sprite_image reload sprite boot app %s", name().c_str());
                    boot_app();
                    tbx::WimpSprite spr2 = tbx::WimpSprite(_file_type, _name);
                    _sprite_image = new CLWimpSpriteImage(&spr2);
                } else {
                    _sprite_image = new CLWimpSpriteImage(&spr);
                }
            } else {
                Log_debug("CLDirectoryItemData::sprite_image reloaded sprite was changed to %s app %s", spr.name().c_str(), name().c_str());
                _flags &= ~CLDIRITEM_FLAG_SPRITE_APP_DEFAULT;
                _sprite_image = new CLWimpSpriteImage(&spr);
            }
            _flags |= CLDIRITEM_FLAG_SPRITE_RELOADED;
        }
        Log_debug("CLDirectoryItemData::sprite loaded spr:%s w:%d h:%d name:%s", _sprite_image->name(), _sprite_image->width(), _sprite_image->height(), _name.c_str());
    }
    return _sprite_image;
}

CLBaseImage *CLDirectoryItemData::small_sprite_image() {
//    if (_file_type == tbx::FILE_TYPE_APPLICATION && !app_booted()) {
//        boot_app();
//    }
    std::string small_name;
    char buf[32];
    CLWimpSpriteImage *big_spr = sprite_image();
//    Log_debug("CLDirectoryItemData::small_sprite item:%s", _name.c_str());
    if (_small_sprite_image == nullptr) {

        if (_file_type == tbx::FILE_TYPE_APPLICATION) {
            if (big_spr->name_str() == "application") {
                small_name = "small_app";
            } else {
                small_name = "sm" + big_spr->name_str();
                std::transform(small_name.begin(), small_name.end(), small_name.begin(),
                               [](unsigned char c){ return std::tolower(c); });
            }
        } else if (_file_type == tbx::FILE_TYPE_DIRECTORY) {
            small_name = "small_dir";
        } else {
            if (big_spr->name_str() == "file_xxx") {
                small_name = "small_xxx";
            } else {
                snprintf(buf, sizeof(buf)-1, "small_%03x", _file_type);
                small_name = std::string(buf);
            }
        }
//        Log_debug("CLDirectoryItemData::small_sprite sprname:%s", small_name.c_str());
        tbx::WimpSprite spr = tbx::WimpSprite(small_name);
        if (spr.exist()) {
            _small_sprite_image = new CLWimpSpriteImage(&spr);
        } else {
            if (small_name.substr(0, 3) == "sm!") {
                int height = 18;
                int width = 20;
                Log_debug("wimp_spr try to create nm=%s sz=%dx%d", small_name.c_str(), width, height);
                _small_sprite_image = CLImageFactory::resize_image(width, height, big_spr, true, 8);
            }
        }
//        Log_debug("Check small sprite name '%s' exists:%d size:%dx%d spr_ptr:%p", small_name.c_str(), small_sprite->exist(), small_sprite->width(), small_sprite->height(), _small_sprite);
    }
//    if (_small_sprite) {
//        Log_debug("CLDirectoryItemData::small_sprite item:%s return: %p spr:%s", _name.c_str(), _small_sprite, _small_sprite->name().c_str());
//    } else {
//        Log_debug("CLDirectoryItemData::small_sprite item:%s return: %p", _name.c_str(), _small_sprite);
//    }
    return _small_sprite_image;
}

CLDirectoryItemData* CLDirectoryItemData::find_child_by_name(const std::string& name) {
    for(auto item : _childs) {
        if (item->_name == name) {
            return item;
        }
    }
    return nullptr;
}

CLDirectoryItemData* CLDirectoryItemData::find_child_by_id(unsigned int id) {
    if (id == 0) {
        return nullptr;
    }
    for(auto item : _childs) {
        if (item->_id == id) {
            return item;
        }
    }
    return nullptr;
}

tbx::Path CLDirectoryItemData::path() {
    CLDirectoryItemData *item = this;
    std::string _path = "";
//    if (_path.empty()) {
        while(item) {
            if (item->_parent == nullptr) {
                if (item->_name[item->_name.size() - 1] != '$') {
                    _path = item->_name + ".$" + _path;
                } else {
                    _path = item->_name + _path;
                }
                break;
            } else {
                _path = "." + item->_name + _path;
            }
            item = item->_parent;
        }
//    }
    return tbx::Path(_path);
}

void CLDirectoryItemData::boot_app() {
    std::string bootapp_cmd = "Filer_Boot "+path().name();
    Log_debug("%s", bootapp_cmd.c_str());
    tbx::Application::instance()->os_cli(bootapp_cmd);
    _flags |= CLDIRITEM_FLAG_APP_BOOTED;
}

void CLDirectoryItemData::fs_rename(const std::string& newname_) {
    std::string newname = newname_;
    trim(newname,"\t\r\n ");
    newname = str_replace_all(newname, " ", "\xA0");
    if (!newname.empty()) {
        tbx::Path newname_path = _parent->path().child(newname);
        std::string old_key = CLImageCache::make_thumbnail_key(this->path());
        try {
            path().rename(newname_path.name());
        } catch (const tbx::OsError& err) {
            tbx::show_message(err.what(), "CLFiler error", "error");
            return;
        }
        CLImageCache::rename_thumbnail(old_key, CLImageCache::make_thumbnail_key(newname_path));
        g_app_data_model.rename_diritem_cache(path(), newname_path);
        tbx::PathInfo info;
        newname_path.raw_path_info(info, true);
        g_app_data_model.on_dir_item_updated(this, update(info));
    }
}

void CLDirectoryItemData::fs_run() {
    Log_debug("CLDirectoryItemData::fs_run %x", _file_type);
    // read shift key state
    std::string cmd("*Filer_Run ");
    if (g_app_state.filer_ver >= 240) {
        cmd.append("-Shift ");
    }
    cmd += path().name();
    Log_debug("Run app %s", cmd.c_str());
    try {
        tbx::app()->os_cli(cmd);
    } catch (tbx::OsError &err)  {
        tbx::show_message("Unable to run the application");
    }
}

void CLDirectoryItemData::fs_remove_files(const std::vector<std::string>& files) {
    if (!files.empty()) {
        CLFilerAction *act = new CLFilerAction(path().name(), files);
        act->fs_remove(tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
        Log_debug("%s", "CLDirectoryItemData::fs_remove_selected", 1);
    }
}

void CLDirectoryItemData::fs_remove() {
    CLFilerAction *act = new CLFilerAction(_parent->path().name());
    act->fs_add_object(_name);
    act->fs_remove(tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
    Log_debug("CLDirectoryItemData::fs_remove [%s]", _name.c_str());
}

void CLDirectoryItemData::fs_set_filetype(int filetype) {
    tbx::Path p = path();
    p.file_type(filetype);
    tbx::PathInfo info;
    p.raw_path_info(info, true);
    g_app_data_model.on_dir_item_updated(this, update(info));
    g_app_data_model.save_diritem_cache_on_idle(this->parent());
}

void CLDirectoryItemData::fs_create_directory(const std::string &newname) {
    std::string space_replaced = str_replace_all(newname, " ","\xA0");
    Log_debug("CLDirectoryItemData::fs_create_directory [%s] in [%s]", space_replaced.c_str(), path().name().c_str());
    path().child(space_replaced).create_directory();
//    mkdir(path().child(newname).name().c_str(),0777);
    g_app_data_model.refresh_on_idle_first(this, 0, CLREFRESH_RUN_ITEM_CALLBACKS);
}

void CLDirectoryItemData::fs_open_help() {
    tbx::Path hlp = path().child("!Help");
    if (hlp.exists()) {
        std::string cmd = "Filer_Run ";
        cmd.append(hlp.name());
        _kernel_oscli(cmd.c_str());
    }
}

void CLDirectoryItemData::fs_stamp() {
    CLFilerAction *act = new CLFilerAction(_parent->path().name());
    act->add_objects(_name);
    act->set_refresh_src(path().name(), true);
    act->stamp(tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
    Log_debug("CLDirectoryItemData::fs_stamp [%s]", _name.c_str());
}

void CLDirectoryItemData::fs_copy_local(const std::string &newname) {
    CLFilerAction *act = new CLFilerAction(_parent->path().name());
    act->add_objects(_name);
    act->copy_local(newname, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
    _parent->update_dir_size(_parent->dir_size() + dir_or_file_size(), 0);
    Log_debug("CLDirectoryItemData::fs_copy_local [%s]", _name.c_str());
}

bool CLDirectoryItemData::refresh_deep(int refresh_depth, unsigned int options) {
    bool result = false;
    if (!(options & CLREFRESH_ONLY_DEEP_NO_CURRENT)) {
        result = refresh(options);
    }
    Log_debug("%s refresh_deep deeps=%d has_files=%d dir_size=%d", name().c_str(), refresh_depth, has_files(), dir_size());
    if ((options & CLREFRESH_INITIAL_REFRESH_ITEM) || refresh_depth > 0) {
        unsigned int child_opt = options & ~CLREFRESH_INITIAL_REFRESH_ITEM;
        for(auto child : _childs) {
            if (child->is_application() || child->is_directory()) {
                bool cache_loaded = child->is_cache_loaded();
                if (!cache_loaded) {
                    cache_loaded = g_app_data_model.load_diritem_cache(child);
                }
                if (refresh_depth > 0) {
                    if (options & CLREFRESH_FORCE_CALC_SIZE) {
                        Log_debug("add_refreshed_item CLREFRESH_FORCE_CALC_SIZE child=%s", child->name().c_str());
                        g_app_data_model._idle_refresher.add_refreshed_item(child, refresh_depth - 1, child_opt);
                        continue;
                    }
                    if (options & CLREFRESH_CALC_SIZE && !child->is_full_scanned()) {
                        Log_debug("add_refreshed_item CLREFRESH_CALC_SIZE child=%s", child->name().c_str());
                        g_app_data_model._idle_refresher.add_refreshed_item(child, refresh_depth - 1, child_opt);
                        continue;
                    }
                    if (child->_dir_size == 0 && !child->is_full_scanned()) {
                        Log_debug("dir %s size = 0 no_files", child->name().c_str());
                        g_app_data_model._idle_refresher.add_refreshed_item(child, std::min(refresh_depth - 1, 2), child_opt);
                        continue;
                    }
                } else if ((options & CLREFRESH_INITIAL_REFRESH_ITEM) && child->_dir_size == 0) {
                    Log_debug("CLREFRESH_INITIAL_REFRESH_ITEM dir %s size = 0 no_files", child->name().c_str());
                    g_app_data_model._idle_refresher.add_refreshed_item(child, 2, child_opt);
                    continue;
                }

//                if (refresh_depth < 2 && child->_dir_size == 0 && (options & CLREFRESH_INITIAL_REFRESH_ITEM)) {
//                    Log_debug("dir %s size = 0 no_files", child->name().c_str());
//                    g_app_data_model._idle_refresher.add_refreshed_item(child, 2, child_opt);
//                } else {
//                }
            }
        }
    }
    return result;
}

void CLDirectoryItemData::update_dir_size(int64_t new_size, int options) {
    Log_debug("CLDirectoryItemData::update_dir_size item_changed this dir size changed %lld!=%lld %s", new_size, _dir_size, name().c_str());

    int64_t diff_size = new_size - _dir_size;
    _dir_size = new_size;
    _dir_or_file_size_str.clear();
    _dir_or_file_size_unit.clear();
    if (options & CLREFRESH_RUN_ITEM_CALLBACKS) {
        g_app_data_model.on_dir_item_updated(this, CLUPDATED_SIZE);
    }
    g_app_data_model.save_diritem_cache_on_idle(this);
//        Log_debug("update parent dirs diff:%lld", diff_size);
    auto parent = _parent;
    CLDirectoryItemData *current_item = this;
    while (parent) {
//            Log_debug("update parent dir [%s] size was:%lld now:%lld", parent->name().c_str(), parent->_dir_size, parent->_dir_size + diff_size);
        parent->_dir_size += diff_size;
        if (parent->_dir_size < 0) {
            parent->_dir_size = 0;
        }
        parent->_dir_or_file_size_str.clear();
        parent->_dir_or_file_size_unit.clear();
        if (current_item->is_full_scanned()) {
            parent->_flags |= CLDIRITEM_FLAG_FULL_SCANNED;
            for(auto ch : parent->childs()) {
                if ((ch->is_directory() || ch->is_application()) && !ch->is_full_scanned()) {
                    parent->_flags &= ~CLDIRITEM_FLAG_FULL_SCANNED;
                    Log_debug("update parent dir [%s] set no fullscanned because child: %s is not fullscanned", parent->name().c_str(), ch->name().c_str());
                    break;
                }
            }
        } else {
            parent->_flags &= ~CLDIRITEM_FLAG_FULL_SCANNED;
        }
        Log_debug("update parent dir [%s] set fullscanned:%d, current [%s] fullscanned:%d", parent->name().c_str(), parent->is_full_scanned(), current_item->name().c_str(), current_item->is_full_scanned());
        if (options & CLREFRESH_RUN_ITEM_CALLBACKS) {
            g_app_data_model.on_dir_item_updated(parent, CLUPDATED_SIZE);
        }
        g_app_data_model.save_diritem_cache_on_idle(parent);
        current_item = parent;
        parent = parent->_parent;
    }
}

bool CLDirectoryItemData::refresh(unsigned int options) {
    tbx::Path p = path();
    bool child_found;
    bool item_changed = false;
    auto flags_prev = _flags;
    int updated_what;
    CLDirectoryItemData *child;

    g_app_data_model._refreshing_item = this;

    std::vector<CLDirectoryItemData*> new_added_childs;
    int64_t current_size_of_dir = 1024;

    if (_parent) {
        tbx::PathInfo info;
        p.raw_path_info(info, true);
        updated_what = update(info);
        if (updated_what) {
            if (updated_what && (options & CLREFRESH_RUN_ITEM_CALLBACKS)) {
                g_app_data_model.on_dir_item_updated(this, updated_what);
            }
            item_changed = true;
            Log_debug("CLDirectoryItemData::refresh this dir changed %s", name().c_str());
        }
    }

    for(auto child : _childs) {
        child->_flags &= (~CLDIRITEM_FLAG_REALLY_EXSITS);
    }

    Log_debug("CLDirectoryItemData::refresh path=%s", p.name().c_str());
    int i = 0, ft;
    for(tbx::PathInfo::Iterator it = tbx::PathInfo::begin(p); it != tbx::PathInfo::end(); ++it) {
        child_found = false;
        ft = CLDirectoryItemData::get_file_type(*it);
        for(auto child : _childs) {
            if (child->_name == it->name() && child->file_type() == ft) {
                child_found = true;
                child->_flags |= CLDIRITEM_FLAG_REALLY_EXSITS;
                updated_what = child->update(*it);
                if (updated_what && (options & CLREFRESH_RUN_ITEM_CALLBACKS)) {
                    g_app_data_model.on_dir_item_updated(child, updated_what);
                    item_changed = true;
                    Log_debug("CLDirectoryItemData::refresh item_changed child changed %s", child->name().c_str());
                }
                if (child->is_directory() || child->is_application()) {
                    current_size_of_dir += child->_dir_size;
                } else {
                    current_size_of_dir += (child->_file_size + (512 - (child->_file_size % 512)));
                }
                break;
            }
        }
        if (!child_found) {
            CLDirectoryItemData* item = new CLDirectoryItemData(*it, this);

            new_added_childs.push_back(item);
            if (item->is_directory() || item->is_application()) {
                current_size_of_dir += item->_dir_size;
            } else {
                current_size_of_dir += (item->_file_size + (512 - (item->_file_size % 512)));
            }
            //Log_debug("CLDirectoryItemData::refresh new found %s", item->name().c_str());
        }
        if ((options & CLREFRESH_WITH_YIELD) && (i++ % 30) == 0) {
            tbx::Application::instance()->yield();
        }
//            Log_debug("CLDirectoryItemData::refresh found=%d len=%d this=%s", child_found, _childs.size(), _name.c_str());
    }
    _flags &= ~CLDIRITEM_FLAG_HAS_SUBDIRS;
    _flags &= ~CLDIRITEM_FLAG_HAS_FILES;
    _flags &= ~CLDIRITEM_FLAG_HAS_POSSIBLE_BIG_IMAGES;
    _flags |= CLDIRITEM_FLAG_FULL_SCANNED;
//    Log_debug("CLDirectoryItemData::refresh3", 1);

    if (!_childs.empty()) {
        for(int i = _childs.size() - 1; i >= 0; i--) {
            child = _childs[i];
            if (!(child->_flags & CLDIRITEM_FLAG_REALLY_EXSITS)) {
                Log_debug("CLDirectoryItemData::refresh item_changed item erased name=%s parent=%s i=%d", child->name().c_str(), this->name().c_str(), i);
                item_changed = true;
                Log_debug("CLDirectoryItemData::refresh child erased %s", child->name().c_str());
                _childs.erase(_childs.begin() + i);
                g_app_data_model.on_dir_item_removed(child);
                delete child;
            } else {
                if (child->is_directory()) {
                    _flags |= CLDIRITEM_FLAG_HAS_SUBDIRS;
                } else if (child->is_file()) {
                    _flags |= CLDIRITEM_FLAG_HAS_FILES;
                    if (CLImageFactory::can_load(child->file_type()) && child->file_type() != FILE_TYPE_SPRITE) {
                        _flags |= CLDIRITEM_FLAG_HAS_POSSIBLE_BIG_IMAGES;
                    }
                }
                if (!child->is_full_scanned() && (child->is_directory() || child->is_application())) {
                    _flags &= ~CLDIRITEM_FLAG_FULL_SCANNED;
                }
            }
        }
    }

    if (!new_added_childs.empty()) {
        for(auto item : new_added_childs) {
            _childs.push_back(item);
            if (item->is_directory() || item->is_application()) {
                _flags &= ~CLDIRITEM_FLAG_FULL_SCANNED;
            }
            if (item->is_directory()) {
                _flags |= CLDIRITEM_FLAG_HAS_SUBDIRS;
            } else if (item->is_file()) {
                _flags |= CLDIRITEM_FLAG_HAS_FILES;
            }
            if (options & CLREFRESH_RUN_ITEM_CALLBACKS) {
                g_app_data_model.on_dir_item_added(item);
            }
        }
    }

    // update parents dir size
    if (_parent && ((current_size_of_dir != _dir_size) ||
                   (flags_prev & CLDIRITEM_FLAG_FULL_SCANNED) != (_flags & CLDIRITEM_FLAG_FULL_SCANNED) ||
                   (!is_full_scanned() && _parent->is_full_scanned()))) {
        item_changed = true;
        update_dir_size(current_size_of_dir, options);
    }

    if (item_changed) {
        g_app_data_model.save_diritem_cache_on_idle(this);
    }

//    Log_debug("CLDirectoryItemData::refresh [%s] [%s] id:%d end", name().c_str(), path().name().c_str(), id());
    g_app_data_model.on_info_change();
    g_app_data_model._refreshing_item = nullptr;
    return item_changed;
}

void CLDirectoryItemData::calc_fake_dir_size() {
    _dir_size = 0;
    for(auto child : _childs) {
        if (child->is_file()) {
            _dir_size += (child->_file_size + (512 - (child->_file_size % 512)));
        } else {
            child->_dir_size = random()/2 + 100000;
            _dir_size += child->_dir_size;
            Log_debug("set fake size for %s: %lld", child->name().c_str(), child->dir_or_file_size());
        }
    }
}

CLDirectoryItemData *CLDirectoryItemData::root() {
    auto item = this;
    while(item) {
        if (!item->_parent) {
            return item;
        }
        item = item->_parent;
    }
    return nullptr;
}

bool CLDirectoryItemData::has_child(unsigned int id) {
    for(auto child: _childs) {
        if (child->_id == id) {
            return true;
        }
        if (!child->_childs.empty()) {
            if (child->has_child(id)) {
                return true;
            }
        }
    }
    return false;
}

void CLDirectoryItemData::move_item_info(CLDirectoryItemData* src) {
    if (!src->_parent->_childs.empty()) {
        for(int i = src->_parent->_childs.size() - 1; i >= 0; i--) {
            if (src->_parent->_childs[i] == src) {
                g_app_data_model.on_dir_item_removed(src);
                src->_parent->_childs.erase(src->_parent->_childs.begin() + i);
                Log_debug("src_parent_childs.erase src:%s", src->name().c_str());
                break;
            }
        }
    }
    src->_parent = this;
    _childs.push_back(src);
    g_app_data_model.on_dir_item_added(src);
    g_app_data_model.save_diritem_cache_on_idle(src);
}

void CLDirectoryItemData::copy_item_info(CLDirectoryItemData* src) {
    CLDirectoryItemData* new_child = new CLDirectoryItemData(this, *src);
    if (!src->_childs.empty()) {
        for(auto ch : src->_childs) {
            new_child->copy_item_info(ch);
        }
    }
    g_app_data_model.on_dir_item_added(new_child);
    g_app_data_model.save_diritem_cache_on_idle(src);
}


bool CLDirectoryItemData::is_modified() {
    return path().modified_time().centiseconds() != _mtime;
}

bool CLDirectoryItemData::has_help() {
    return (is_application() && path().child("!Help").exists());
}

std::string CLDirectoryItemData::dir_or_file_size_unit() {
    if (_dir_or_file_size_unit.empty()) {
        if (is_file()) {
            file_size_to_unit_size_string(_file_size, _dir_or_file_size_unit, _dir_or_file_size_str);
        } else {
            if (_dir_size > 0) {
                file_size_to_unit_size_string(_dir_size, _dir_or_file_size_unit, _dir_or_file_size_str);
            }
        }
    }
    return _dir_or_file_size_unit;
}

std::string CLDirectoryItemData::dir_or_file_size_str() {
    if (_dir_or_file_size_str.empty()) {
        if (is_file()) {
            file_size_to_unit_size_string(_file_size, _dir_or_file_size_unit, _dir_or_file_size_str);
        } else {
            if (_dir_size > 0) {
                file_size_to_unit_size_string(_dir_size, _dir_or_file_size_unit, _dir_or_file_size_str);
            }
        }
    }
    return _dir_or_file_size_str;
}

std::string CLDirectoryItemData::dir_or_file_size_str_with_unit() {
    if (_dir_or_file_size_str.empty()) {
        if (is_file()) {
            file_size_to_unit_size_string(_file_size, _dir_or_file_size_unit, _dir_or_file_size_str);
        } else {
            if (_dir_size > 0) {
                file_size_to_unit_size_string(_dir_size, _dir_or_file_size_unit, _dir_or_file_size_str);
            }
        }
    }
    return _dir_or_file_size_str + ' ' + _dir_or_file_size_unit;
}

void CLDirectoryItemData::fs_set_dir() {
    xosfscontrol_dir(path().name().c_str());
}

bool CLDirectoryItemData::can_set_filetype_by_ext() {
    if (_name.find('/') != std::string::npos) {
        for(auto &autoext: auto_filetype_extensions) {
            for(auto &e : autoext.extensions) {
                if (_name.size() > e.size() && stricmp(_name.c_str() + _name.size() - e.size(), e.c_str()) == 0) {
                    return true;
                }
            }
        }
    }
    if (_name.find(',') != std::string::npos) {
        std::vector<std::string> parts;
        int filetype;
        split(_name, parts, ',');
        if (parts.size() == 2 && parts[1].size() == 3 && sscanf(parts[1].c_str(), "%3x", &filetype) == 1) {
            return true;
        }
    }
    return false;
}

void CLDirectoryItemData::fs_set_filetype_by_ext() {
    if (_name.find('/') != std::string::npos) {
        for(auto &autoext: auto_filetype_extensions) {
            for(auto &e : autoext.extensions) {
                if (_name.size() > e.size() && stricmp(_name.c_str() + _name.size() - e.size(), e.c_str()) == 0) {
                    fs_set_filetype(autoext.file_type);
                    return;
                }
            }
        }
    }
    if (_name.find(',') != std::string::npos) {
        std::vector<std::string> parts;
        split(_name, parts, ',');
        int filetype = 0;
        if (parts.size() == 2 && parts[1].size() == 3 && sscanf(parts[1].c_str(), "%3x", &filetype) == 1 && filetype) {
            tbx::Path p = path();
            p.file_type(filetype);
            fs_rename(parts[0]);
        }
    }
}


void CLDirItemsIdleRefresher::add_refreshed_item(CLDirectoryItemData *item, int refresh_depth, unsigned int options, bool first) {
    int i = 0;
    for(auto &refresh_item : _items_to_refresh) {
        if (refresh_item.item == item && (!first || i < 2)) {
            Logger::warn("IdleRefresher::add_refreshed_item [%s] already refreshing idx=%d", item->name().c_str(), i);
            return;
        }
        i++;
    }

    Log_debug("CLDirItemsIdleRefresher::add_refreshed_item item=%s dirs=%lld childs=%d calc_dir_sz=%d parent=%x", item->name().c_str(), item->dir_size(), item->childs().size(), options & CLREFRESH_CALC_SIZE, item->parent());
//    if (options & CLREFRESH_CALC_SIZE && !(options & CLREFRESH_FORCE_CALC_SIZE)) {
//        if (item->parent() == nullptr && !item->is_full_scanned()) { // root
//            bool root_childs_scanned = true;
//            for (auto ch: item->childs()) {
//                if (ch->is_directory() || ch->is_application()) {
//                    if (!ch->is_cache_loaded() || !ch->is_full_scanned()) {
//                        root_childs_scanned = false;
//                    }
//                }
//            }
//            if (root_childs_scanned) {
//                Logger::warn("IdleRefresher::add_refreshed_item root seems scanned, exit", 1);
//                return;
//            }
//        }

//        } else {
//            if (!item->childs().empty()) {
//                bool childs_scanned = true;
//                for(auto ch : item->childs()) {
//                    if (ch->is_directory() || ch->is_application()) {
//                        if (!ch->is_full_scanned()) {
//                            childs_scanned = false;
//                            refresh_depth = std::min(refresh_depth, 4);
//                            goto childs_exit;
//                        } else {
//                            for(auto gch :  ch->childs()) {
//                                if (gch->is_directory() || gch->is_application()) {
//                                    if (!gch->is_full_scanned()) {
//                                        childs_scanned = false;
//                                        refresh_depth = std::min(refresh_depth, 4);
//                                        goto childs_exit;
//                                    }
//                                }
//                            }
//                        }
//                    }
//                }
//                childs_exit:
//                if (childs_scanned) {
//                    Logger::warn("IdleRefresher::add_refreshed_item [%s] already refreshed, size calculated = %lld", item->name().c_str(), item->dir_size());
//                    return;
//                }
//            }
//        }
//    }
    RefreshItem ri;
    ri.item = item;
    ri.refresh_depth = refresh_depth;
    ri.options = options;
    if (first) {
        _items_to_refresh.insert(_items_to_refresh.begin(), ri);
    } else {
        _items_to_refresh.push_back(ri);
    }

    if (!refreshing) {
        Log_debug("IdleRefresher::add_refreshed_item [%s] added idle command", item->name().c_str());
        tbx::Application::instance()->add_idle_command(this);
        g_app_data_model.on_refresh_started(item);
        refreshing = true;
        any_item_changed = false;
        aborted = false;
    }
}

void CLDirItemsIdleRefresher::remove_refresh_item(CLDirectoryItemData *item) {
    int i;
    bool found;

    if (!_items_to_refresh.empty()) {
        for(i = _items_to_refresh.size() - 1; i >=0;  i--) {
            if (_items_to_refresh[i].item == item) {
                _items_to_refresh.erase(_items_to_refresh.begin() + i);
                Log_debug("IdleRefresher::remove_refresh_item_id removed [%s]", item->name().c_str());
                break;
            }
        }
        do {
            found = false;
            for(i = _items_to_refresh.size() - 1; i >=0;  i--) {
                CLDirectoryItemData *parent = _items_to_refresh[i].item->parent();
                while(parent != nullptr) {
                    if (parent == item) {
                        Log_debug("IdleRefresher::remove_refresh_item_id removed [%s] parent for [%s]", _items_to_refresh[i].item->name().c_str(), item->path().name().c_str());
                        _items_to_refresh.erase(_items_to_refresh.begin() + i);
                        found = true;
                        break;
                    }
                    parent = parent->parent();
                }
                if (found) {
                    break;
                }
            }
        } while(found);
    }
}

void CLDirItemsIdleRefresher::execute() {
//    Log_debug("CLDirItemsIdleRefresher::execute tick:%d", tick);
//    tick++;

    if (!refreshing) {
        Logger::warn("CLDirItemsIdleRefresher::execute but not refreshing, ignored");
        tbx::Application::instance()->remove_idle_command(this);
        return;
    }
    if (executing) {
//        Logger::warn("CLDirItemsIdleRefresher::execute previous refresh not finished, ignored");
        return;
    }

    // stupid test if we still have enough memory
    if (!is_enough_memory(500000)) {
        aborted = true;
        _items_to_refresh.clear();
        tbx::Application::instance()->remove_idle_command(this);
        Logger::crit("Not enough memory in the IdleRefresher", 1);
    }

    executing = true;
    if (!_items_to_refresh.empty() && !aborted) {
        RefreshItem refresh_item = _items_to_refresh[0];
//        last_refresh_item = refresh_item;
        if (refresh_item.item->path().exists()) {
            Log_debug("IdleRefresher::execute [%s] refreshing", refresh_item.item->name().c_str());
            if (refresh_item.item->refresh_deep(refresh_item.refresh_depth, refresh_item.options)) {
                any_item_changed = true;
            }
        } else {
            CLDirectoryItemData *parent = refresh_item.item->parent();
            Logger::info("IdleRefresher::execute [%s] not exists try parent: [%s]", refresh_item.item->name().c_str(), parent->name().c_str());
            while(parent) {
                if (parent->path().exists()) {
                    break;
                }
                parent = parent->parent();
            }
            if (parent) {
                Logger::info("IdleRefresher::execute [%s] found existing parent: [%s] refreshing", refresh_item.item->name().c_str(), parent->name().c_str());
                if (parent->refresh(refresh_item.options & ~CLREFRESH_INITIAL_REFRESH_ITEM)) {
                    any_item_changed = true;
                }
            } else {
                Logger::warn("IdleRefresher::execute [%s] parent not found, skip refresh");
            }
        }
        for(auto it = _items_to_refresh.begin(); it < _items_to_refresh.end(); it++) {
            if (it->item == refresh_item.item) {
                _items_to_refresh.erase(it);
                break;
            }
        }
    } else {
        Log_debug("IdleRefresher::execute items_to_refresh is empty, remove idle command and finish refreshing, any_item_changed=%d", any_item_changed);
        tbx::Application::instance()->remove_idle_command(this);
//        if (any_item_changed && !aborted) {
//            g_app_data_model.save_state();
//        }
        g_app_data_model.on_refresh_finished(aborted, any_item_changed);
        any_item_changed = false;
        refreshing = false;
        aborted = false;
        _items_to_refresh.clear();
    }
    executing = false;
    Log_debug("IdleRefresher::execute refresh end (items cnt:%d)", _items_to_refresh.size());
}

void CLDirItemsIdleRefresher::abort() {
    aborted = true;
    _items_to_refresh.clear();
    Log_debug("IdleRefresher::abort()",1);
}


void CLIdleCacheSaver::save_cache_on_idle(CLDirectoryItemData *item) {
    for(auto &it : _items_to_save) {
        if (it == item->id()) {
            Log_debug("CLIdleCacheSaver::save_cache_on_idle item [%s] already in save queue (count:%d)", item->name().c_str(), _items_to_save.size());
            return;
        }
    }
    _items_to_save.push_back(item->id());
    //Log_debug("CLIdleCacheSaver::save_cache_on_idle item [%s] added to save queue (count:%d)", item->name().c_str(), _items_to_save.size());
    if (!saving) {
        tbx::Application::instance()->add_idle_command(this);
        saving = true;
        g_app_data_model.on_info_change();
    }
}

void CLIdleCacheSaver::cancel_save(CLDirectoryItemData *item) {
    if (!_items_to_save.empty()) {
        for(auto i = _items_to_save.begin(); i < _items_to_save.end(); i++) {
            if (*i == item->id()) {
                Log_debug("CLIdleCacheSaver::cancel_save item [%s] save canceled", item->name().c_str());
                _items_to_save.erase(i);
                break;
            }
        }
        bool found;
        do {
            found = false;
            for(int i = _items_to_save.size() - 1; i >=0;  i--) {
                CLDirectoryItemData *saving_item = g_app_data_model.find_item_by_id(_items_to_save[i]);
                if (saving_item) {
                    CLDirectoryItemData *parent = saving_item->parent();
                    while(parent != nullptr) {
                        if (parent == item) {
                            Log_debug("CLIdleCacheSaver::cancel_save save canceled for [%s] parent for [%s]", saving_item->name().c_str(), item->path().name().c_str());
                            _items_to_save.erase(_items_to_save.begin() + i);
                            found = true;
                            break;
                        }
                        parent = parent->parent();
                    }
                    if (found) {
                        break;
                    }
                }
            }
        } while(found);
    }
}

void CLIdleCacheSaver::execute() {
    if (g_app_data_model.is_refreshing()) {
        Log_debug("CLIdleCacheSaver::execute is_refreshing, postpone saving",1);
        return;
    }
    if (!_items_to_save.empty()) {
        unsigned long _id = _items_to_save[0];
        CLDirectoryItemData *item = g_app_data_model.find_item_by_id(_id);
        _items_to_save.erase(_items_to_save.begin());
        if (item) {
            Log_debug("CLIdleCacheSaver::execute saving cache: %s", item->name().c_str());
            g_app_data_model.save_diritem_cache(item);
        } else {
            Log_error("CLIdleCacheSaver::execute saving item not found, id:%lld", _id);
        }
    }

    if (_items_to_save.empty()) {
        saving = false;
        tbx::Application::instance()->remove_idle_command(this);
        Log_debug("CLIdleCacheSaver::execute queue empty, stop saving",1);
        g_app_data_model.on_info_change();
    }
}



CLFilerAction::CLFilerAction(const std::string &src_dir) :
    _refresh_childs(false),
    _refresh_src(src_dir)
{
    _dst_exsist_will_merged = false;
    directory(src_dir);
    add_finished_listener(this);
}

CLFilerAction::CLFilerAction(const std::string &src_dir, const std::vector<std::string> &src_files) :
        _refresh_childs(false),
        _refresh_src(src_dir)
{
    _dst_exsist_will_merged = false;
    _src_objects = src_files;
    directory(src_dir);
    add_finished_listener(this);
}

void CLFilerAction::send_selected_objects() {
    std::string str_obj;
    for(auto& src : _src_objects) {
        if (!str_obj.empty()) {
            str_obj.append(" ");
        }
        str_obj.append(src);
        if (str_obj.length() > MAX_SELECTED_FILES_LENGTH) {
            add_objects(str_obj);
            str_obj.clear();
        }
    }
    if (!str_obj.empty()) {
        add_objects(str_obj);
    }
}

void CLFilerAction::fs_add_object(const std::string& obj) {
    _src_objects.push_back(obj);
}

void CLFilerAction::fs_rename(const std::string &dst_dir, int options) {
    _refresh_dst = dst_dir;
    _refresh_childs = true;
    _operation = CLFilerActionOperation::CL_OP_MOVE;
    send_selected_objects();
    Log_info("CLFilerAction::fs_rename src:%s first_obj:%s dst:%s:", _refresh_src.c_str(), _src_objects[0].c_str(), dst_dir.c_str());
    tbx::FilerAction::rename(dst_dir, options);
}

void CLFilerAction::fs_copy(const std::string &dst_dir, int options) {
    _refresh_childs = true;
    _refresh_dst = dst_dir;
    _operation = CLFilerActionOperation::CL_OP_COPY;
    tbx::Path dstpath = dst_dir;
    for(auto &src : _src_objects) {
        if (dstpath.child(src).exists()) {
            _dst_exsist_will_merged = true;
            break;
        }
    }
    send_selected_objects();
    Log_info("CLFilerAction::fs_copy src:%s first_obj:%s dst:%s:", _refresh_src.c_str(), _src_objects[0].c_str(), dst_dir.c_str());
    tbx::FilerAction::copy(dst_dir, options);
}

void CLFilerAction::fs_move(const std::string &dst_dir, int options) {
    _refresh_childs = true;
    _refresh_dst = dst_dir;
    _operation = CLFilerActionOperation::CL_OP_MOVE;
    tbx::Path dstpath = dst_dir;
    for(auto &src : _src_objects) {
        if (dstpath.child(src).exists()) {
            _dst_exsist_will_merged = true;
            break;
        }
    }
    send_selected_objects();
    Log_info("CLFilerAction::fs_move src:%s first_obj:%s dst:%s:", _refresh_src.c_str(), _src_objects[0].c_str(), dst_dir.c_str());
    tbx::FilerAction::move(dst_dir, options);
}

void CLFilerAction::fs_remove(int options) {
    send_selected_objects();
    Log_info("CLFilerAction::fs_remove src:%s first_obj:%s", _refresh_src.c_str(), _src_objects[0].c_str());
    tbx::FilerAction::remove(options);
}

void CLFilerAction::fs_stamp(int options) {
    send_selected_objects();
    Log_info("CLFilerAction::fs_stamp src:%s first_obj:%s", _refresh_src.c_str(), _src_objects[0].c_str());
    tbx::FilerAction::stamp(options);
}

void CLFilerAction::fs_set_access(unsigned int set_bits, unsigned int leave_bits, int options) {
    _refresh_childs = (options & tbx::FilerAction::RECURSE);
    send_selected_objects();
    Log_info("CLFilerAction::fs_set_access src:%s first_obj:%s", _refresh_src.c_str(), _src_objects[0].c_str());
    tbx::FilerAction::set_access(set_bits, leave_bits, options);
}

void CLFilerAction::fileraction_finished() {
    int opts = CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD;
    CLDirectoryItemData* src = nullptr;
    CLDirectoryItemData* dst = nullptr;
    if (!_refresh_src.empty()) {
        src = g_app_data_model.find_item_by_path(_refresh_src);
    }
    if (!_refresh_dst.empty()) {
        dst = g_app_data_model.find_item_by_path(_refresh_dst);
    }

    if (src && dst && (_operation == CLFilerActionOperation::CL_OP_COPY || _operation == CLFilerActionOperation::CL_OP_MOVE)) {
        CLDirectoryItemData* src_child = nullptr;
        if (!_dst_exsist_will_merged) {
            for(auto &chname : _src_objects) {
                src_child = src->find_child_by_name(chname);
                if (src_child) {
                    auto dst_item_path = tbx::Path(_refresh_dst).child(chname);
                    if (dst_item_path.exists()) {
                        if (_operation == CLFilerActionOperation::CL_OP_COPY) {
                            Log_info("CLFilerAction::fileraction_finished dst child [%s] found, copy tree item", dst_item_path.name().c_str());
                            dst->copy_item_info(src_child);
                        } else {
                            Log_info("CLFilerAction::fileraction_finished dst child [%s] found, move tree item", dst_item_path.name().c_str());
                            dst->move_item_info(src_child);
                        }
                    } else {
                        Log_error("CLFilerAction::fileraction_finished dst child [%s] is not exists!", dst_item_path.name().c_str());
                    }
                } else {
                    Log_error("CLFilerAction::fileraction_finished src child [%s] is not found in [%s]!", chname.c_str(), _refresh_src.c_str());
                }
            }
        }
        if (_operation == CLFilerActionOperation::CL_OP_COPY) {
            // don't refresh src on copy
            src = nullptr;
        }
    }
    if (src) {
        Log_debug("CLFilerAction::fileraction_finished refresh_on_idle src:%s", src->name().c_str());
        g_app_data_model.refresh_on_idle_first(src, _refresh_childs ? 1 : 0, opts);
//            g_app_data_model.refresh_on_idle(src_item, 0);
    }
    if (dst) {
        Log_debug("CLFilerAction::fileraction_finished refresh_on_idle dst:%s", dst->name().c_str());
        g_app_data_model.refresh_on_idle_first(dst, _refresh_childs ? 1 : 0, opts);
    }
    Log_debug("CLFilerAction::fileraction_finished src:%s dst:%s ", _refresh_src.c_str(), _refresh_dst.c_str());
    delete this;
}


void CLSearchModel::start(CLDirectoryItemData* item, const std::string& substring, unsigned int options) {
    _search_options = options;
    if (!(_search_options & CLSEARCH_FLAG_SEARCH_CASE_SENSITIVE)) {
        _search_substring = tbx::to_lower(substring);
    } else {
        _search_substring = substring;
    }
    _search_items.clear();
    tbx::Application::instance()->remove_idle_command(this);
    tbx::Application::instance()->add_idle_command(this);
    add_search_item(item);
    Log_debug("CLSearchModel::start item:%s substr:%s options:%d", item->name().c_str(), substring.c_str(), options);
}

void CLSearchModel::stop() {
    _search_items.clear();
    tbx::Application::instance()->remove_idle_command(this);
    _listener->on_search_state_callback(SearchState::STOPPED, nullptr);
    _search_state = SearchState::STOPPED;
    _searching_in_item = nullptr;
    Log_debug("CLSearchModel::stop",1);
}

void CLSearchModel::pause(bool paused) {
    if (paused) {
        tbx::Application::instance()->remove_idle_command(this);
    } else {
        if (!_search_items.empty()) {
            tbx::Application::instance()->add_idle_command(this);
        }
    }
}


void CLSearchModel::search_in_item(CLDirectoryItemData* item) {
    bool found;
    std::string name;
    _searching_in_item = item;
    _search_state = SearchState::SEARCHING;
    _listener->on_search_state_callback(SearchState::SEARCHING, item);
    if (item->is_modified() || item->childs().empty()) {
        item->refresh(0);
    }
//    Log_debug("CLSearchModel::search_in_item [%s] searching, _search_substring [%s] items count:%d", item->path().name().c_str(), _search_substring.c_str(), _search_items.size());

    for(auto child_item : item->childs()) {
        if (child_item->is_directory()
            || ((_search_options & CLSEARCH_FLAG_SEARCH_IN_APPS) && child_item->is_application())
            || ((_search_options & CLSEARCH_FLAG_SEARCH_IN_IMAGES) && child_item->is_image())) {
            add_search_item(child_item);
        }

        if (_search_options & CLSEARCH_FLAG_SEARCH_CASE_SENSITIVE) {
            name = child_item->name();
        } else {
            name = tbx::to_lower(child_item->name());
        }
        found = (name.find(_search_substring) != std::string::npos);
//        Log_debug("child: [%s] %d [%s]", name.c_str(), found, _search_substring.c_str());
        if (found) {
            Log_debug("CLSearchModel::search_in_item found item id:%d %s substr:%s", child_item->id(), name.c_str(), _search_substring.c_str());
            _listener->on_search_state_callback(SearchState::FOUND, child_item);
        }
    }
}

void CLSearchModel::add_search_item(CLDirectoryItemData* item) {
    for(auto _search_item : _search_items) {
        if (_search_item == item) {
            Logger::warn("CLSearchModel::add_search_item [%s] already searching", item->name().c_str());
            return;
        }
    }
//    Log_debug("CLSearchModel::add_search_item [%s] appened to search list, count:%d", item->path().name().c_str(), _search_items.size());
    _search_items.push_back(item);
}

void CLSearchModel::remove_search_item(CLDirectoryItemData* item) {
    if (!_search_items.empty()) {
        for(int i = _search_items.size() - 1; i >= 0; i--) {
            if (_search_items[i] == item) {
                _search_items.erase(_search_items.begin()+i);
                Log_debug("CLSearchModel::remove_search_item [%s] removed", item->name().c_str());
                return;
            }
        }
    }
}

void CLSearchModel::execute() {
    if (!_search_items.empty()) {
        CLDirectoryItemData* item = _search_items[0];
        _search_items.erase(_search_items.begin());
        search_in_item(item);

        if (_search_items.empty()) {
            Log_debug("CLSearchModel::execute removed idle command",1);
            tbx::Application::instance()->remove_idle_command(this);
            _search_state = SearchState::FINISHED;
            _listener->on_search_state_callback(SearchState::FINISHED, nullptr);
        }
    } else {
        Logger::info("CLSearchModel::execute items_to_refresh is empty");
    }
}

CLSearchModel::~CLSearchModel() {
    tbx::Application::instance()->remove_idle_command(this);
}


//bool ctrl_pressed = false;
//int vector_handler(_kernel_swi_regs *r, void *pw __attribute__ ((unused))) {
//    if (r->r[0] == 11) {
//        if (r->r[2] == os_TRANSITION_KEY_RIGHT_CONTROL) {
//            ctrl_pressed = true;
//        }
//    }
//    return 1;
//}
//
//AppDataModel::AppDataModel() {
//    Log_debug("xos_add_to_vector", 1);
//    xos_add_to_vector(0x10, (asm_routine )&vector_handler, (byte*)this);
//}
//
//AppDataModel::~AppDataModel() {
//    Log_debug("xos_release", 1);
//    xos_release(0x10, (asm_routine )&vector_handler, (byte*)this);
//}
//
//bool AppDataModel::CLPrePollListener::pre_poll() {
//    int bytes_used;
//    Log_debug("xos_delink_application", 1);
//    xos_delink_application((byte*)vector_buffer, sizeof (vector_buffer), &bytes_used);
//    return false;
//}
//
//void AppDataModel::CLPostPollListener::post_poll(int reason_code, tbx::PollBlock &poll_block, tbx::IdBlock &id_block,
//                                                 int reply_to) {
//    Log_debug("xos_relink_application", 1);
//    xos_relink_application((byte*)vector_buffer);
//    if (ctrl_pressed) {
//        Log_debug("ctrl was pressed",1);
//        ctrl_pressed = false;
//    }
//}


std::vector<std::string> AppDataModel::get_favorites() {
    if (!_favorites_loaded && tbx::Path("<CLFiler$ChoicesDir>.favorites").exists()) {
        std::string line;
        std::ifstream in("<CLFiler$ChoicesDir>.favorites");
        while (std::getline(in, line)) {
            _favorites.push_back(line);
        }
        _favorites_loaded = true;
    }
    std::vector<std::string> favs;
    for(auto line : _favorites) {
        if (tbx::Path(line).exists()) {
//            Log_debug("AppDataModel::get_favorites path:%s", line.c_str());
            favs.push_back(line);
        } else {
            Logger::warn("AppDataModel::get_favorites path:%s not exists", line.c_str());
        }
    }
    return favs;
}

const std::vector<std::string>& AppDataModel::add_favorite(const std::string& fav) {
    _favorites.push_back(fav);
    std::ofstream out("<CLFiler$ChoicesDir>.favorites");
    for(auto &l : _favorites)  {
        out << l << std::endl;
    }
    on_favorites_changed();
    return _favorites;
}

const std::vector<std::string>& AppDataModel::remove_favorite(const std::string& fav) {
//    Log_debug("AppDataModel::remove_favorite %s", fav.c_str());
    for(auto it = _favorites.begin(); it != _favorites.end(); it++)  {
        if (*it == fav) {
//            Log_debug("AppDataModel::remove_favorite removed %s", fav.c_str());
            _favorites.erase(it);
            break;
        }
    }
    std::ofstream out("<CLFiler$ChoicesDir>.favorites");
    for(auto &l : _favorites)  {
        out << l << std::endl;
    }
    on_favorites_changed();
    return _favorites;
}

bool AppDataModel::is_favorite(const std::string& fav) {
    for(auto &l : _favorites)  {
        if (l == fav) return true;
    }
    return false;
}

CLDirectoryItemData* AppDataModel::find_item_by_path(const std::string &path) {
    std::vector<std::string> parts;
    size_t parts_size = split(path, parts, '.');
    CLDirectoryItemData* item = get_root_item(parts[0]), *found_child;
    if (!item) {
        Logger::warn("AppDataModel::find_item_by_path (root) dir item not found for: %s", path.c_str());
        return nullptr;
    }
    if (parts_size == 1) {
        return item;
    }
    size_t start = parts[1] == "$" ? 2 : 1;
    for(size_t i = start; i < parts_size; i++) {
        if (!item->is_file()) {
            if (!item->is_cache_loaded()) {
                g_app_data_model.load_diritem_cache(item);
            }
        }
        found_child = item->find_child_by_name(parts[i]);
        if (!found_child) {
            item->refresh(0);
            found_child = item->find_child_by_name(parts[i]);
        }
        if (!found_child) {
            Logger::warn("AppDataModel::find_item_by_path dir item not found for: %s", path.c_str());
            return nullptr;
        }
        item = found_child;
    }
    return item;
}

CLDirectoryItemData* AppDataModel::find_item_by_id(unsigned long id) {
    if (!id) {
        return nullptr;
    }
    auto found = _id_2_item.find(id);
    if (found != _id_2_item.end()) {
//        Log_debug("AppDataModel::find_item_by_id dir item found for: %ld = %p (%s)", id, found->second, found->second->_name.c_str());
        return found->second;
    } else {
        Logger::warn("AppDataModel::find_item_by_id dir item not found for: %ld", id);
        return nullptr;
    }
}

std::vector<CLDirectoryItemData*> AppDataModel::find_items_by_ids(const std::vector<unsigned long> &ids) {
    std::vector<CLDirectoryItemData*> result;
    for(auto item_id : ids) {
        auto found = _id_2_item.find(item_id);
        if (found != _id_2_item.end()) {
            result.push_back(found->second);
        }
    }
    return result;
}

std::vector<std::string> AppDataModel::items_names(const std::vector<CLDirectoryItemData*> &items) {
    std::vector<std::string> result;
    for(auto it : items) {
        result.push_back(it->name());
    }
    return result;
}

std::vector<unsigned long> AppDataModel::items_ids(const std::vector<CLDirectoryItemData*> &items) {
    std::vector<unsigned long> result;
    for(auto it : items) {
        result.push_back(it->id());
    }
    return result;
}

std::vector<std::string> AppDataModel::items_names_by_ids(const std::vector<unsigned long> &ids) {
    std::vector<std::string> result;
    CLDirectoryItemData* item;
    for(auto item_id : ids) {
        item = find_item_by_id(item_id);
        if (item) {
            result.push_back(item->name());
        }
    }
    return result;
}

CLDirectoryItemData* AppDataModel::get_root_item(const std::string &root_path) {
    for(auto item : _root_items) {
        if (stricmp(item->name().c_str(), root_path.c_str()) == 0) {
            return item;
        }
    }
    Logger::warn("AppDataModel::get_root_item Root item not found for [%s] Return null", root_path.c_str());
    return nullptr;
}

CLDirectoryItemData* AppDataModel::get_existing_root_item(const std::string &root_path) {
    if (tbx::Path(root_path).exists()) {
        for(auto item : _root_items) {
            if (stricmp(item->name().c_str(), root_path.c_str()) == 0) {
                return item;
            }
        }
    }
    Logger::warn("AppDataModel::get_root_item Root item not exists for [%s] search any existing root", root_path.c_str());

//    while(!_root_items.empty()) {
//        if (_root_items[0]->path().exists()) {
//            Logger::warn("AppDataModel::get_root_item Root item not exists for [%s] return [%s]", root_path.c_str(), _root_items[0]->name().c_str());
//            return _root_items[0];
//        }
//        CLDirectoryItemData* item = _root_items[0];
//        _root_items.erase(_root_items.begin());
//        on_root_item_removed(item);
//    }
//    Logger::crit("AppDataModel::get_root_item Root item not exists for [%s] and no any existing root items", root_path.c_str());
    return nullptr;
}

void AppDataModel::fs_copy(const std::string &src_dir, const std::vector<std::string> &src_files, const std::string &dst_dir) {
    if (!src_files.empty() && !src_dir.empty() && src_dir != dst_dir) {
        CLFilerAction *act = new CLFilerAction(src_dir, src_files);
        act->fs_copy(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
        Log_debug("AppDataModel::fs_copy from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
    }
}

void AppDataModel::fs_copy_or_move(const std::string &src_dir, const std::vector<std::string> &src_files, const std::string &dst_dir, int flags) {
    if (!src_files.empty() && !src_dir.empty() && src_dir != dst_dir) {
        int src_dot_pos = src_dir.find('.');
        int dst_dot_pos = dst_dir.find('.');
        if (src_dot_pos == std::string::npos) {
            Logger::error("AppDataModel::fs_copy_or_move (invalid source) [%s]", src_dir.c_str());
            return;
        }
        if (dst_dot_pos == std::string::npos) {
            Logger::error("AppDataModel::fs_copy_or_move (invalid destination) [%s]", dst_dir.c_str());
            return;
        } else {
            CLFilerAction *act = new CLFilerAction(src_dir, src_files);
            if (flags == DEFAULT) {
                if (dst_dir.substr(0, dst_dot_pos) == src_dir.substr(0, src_dot_pos)) {
                    act->fs_rename(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                    Log_debug("AppDataModel::fs_copy_or_move (rename) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
                } else {
                    act->fs_copy(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                    Log_debug("AppDataModel::fs_copy_or_move (copy) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
                }
            } else if (flags == FORCE_COPY) {
                act->fs_copy(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                Log_debug("AppDataModel::fs_copy_or_move (copy) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
            } else if (flags == FORCE_MOVE) {
                if (dst_dir.substr(0, dst_dot_pos) == src_dir.substr(0, src_dot_pos)) {
                    act->fs_rename(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                    Log_debug("AppDataModel::fs_copy_or_move (rename) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
                } else {
                    act->fs_move(dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                    Log_debug("AppDataModel::fs_copy_or_move (move) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
                }
            }
        }
    }
}

void AppDataModel::fs_stamp(const std::string &src_dir, const std::vector<std::string> &src_files) {
    if (!src_files.empty() && !src_dir.empty()) {
        CLFilerAction *act = new CLFilerAction(src_dir, src_files);
        act->fs_stamp(tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
        Log_debug("AppDataModel::fs_stamp dir [%s]", src_dir.c_str());
    }
}

void AppDataModel::fs_count(const std::string &src_dir, const std::vector<std::string> &src_files) {
    if (!src_files.empty() && !src_dir.empty()) {
        tbx::FilerAction act;
        act.directory(src_dir);
        for(auto &s : src_files) {
            Log_debug("AppDataModel::fs_count add_obj src=%s", s.c_str());
            act.add_objects(s);
        }
        act.count(tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
        Log_debug("AppDataModel::fs_count dir [%s]", src_dir.c_str());
    }
}

void AppDataModel::fs_access(const std::string &src_dir, const std::vector<std::string> &src_files, unsigned int set_bits, unsigned int leave_bits, bool recursive) {
    if (!src_files.empty() && !src_dir.empty()) {
        CLFilerAction *act = new CLFilerAction(src_dir, src_files);
        int options = tbx::FilerAction::VERBOSE;
        if (recursive) {
            options |= tbx::FilerAction::RECURSE;
        }
        act->fs_set_access(set_bits, leave_bits, options);
        Log_debug("AppDataModel::fs_access dir [%s] set_bits=%x leave_bits=%x recursive=%d", src_dir.c_str(), set_bits, leave_bits, recursive);
    }
}

void AppDataModel::add_root_item(const std::string& root) {
    std::string canonical_path, canonical_path2;

//    try {
//        canonical_path = tbx::Path::canonicalise(root);
//    } catch (std::exception &e) {
//        Logger::error("AppDataModel::add_root_item can't canonicalize path, skipped, path:%s error:%s", root.c_str(), e.what());
//        return;;
//    }
    canonical_path = root;
    if (canonical_path[canonical_path.size() - 2] == '.' && canonical_path[canonical_path.size() - 1] == '$') {
        canonical_path2 = canonical_path.substr(0, canonical_path.size() - 2);
    } else {
        canonical_path2 = canonical_path;
    }
    CLDirectoryItemData* item = get_root_item(canonical_path2);
    if (!item) {
        item = new CLDirectoryItemData(canonical_path2);
        _root_items.push_back(item);
        //g_app_data_model.refresh_on_idle(item, 1, CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
        Log_info("AppDataModel::refresh_roots add_root_item path=%s canonical=%s", root.c_str(), canonical_path2.c_str());
        on_root_item_added(item);
    } else {
        item->_flags |= CLDIRITEM_FLAG_REALLY_EXSITS;
        Log_debug("AppDataModel::refresh_roots add_root_item root already exists path=%s canonical=%s", root.c_str(), canonical_path2.c_str());
    }
}

std::string AppDataModel::get_csd() {
    char *defaultfs_ptr, *csd_ptr = nullptr, defaultfs[200], csd_env_var[200], clfiler_csd[200];
    defaultfs_ptr = getenv("FileSwitch$CurrentFilingSystem");
    if (defaultfs_ptr) {
        strcpy(defaultfs, defaultfs_ptr);
        sprintf(csd_env_var,"FileSwitch$%s$CSD", defaultfs);
        csd_ptr = getenv(csd_env_var);
    }
    if (csd_ptr) {
        sprintf(clfiler_csd, "%s:%s", defaultfs, csd_ptr);
        Log_debug("Found current selected directory: %s", clfiler_csd);
        return std::string(clfiler_csd);
    } else {
        std::vector<std::string> path_pieces;
        strcpy(clfiler_csd, getenv("CLFiler$Dir"));
        split(clfiler_csd, path_pieces, '.');
        Log_debug("No current selected directory, using CLFiler root dir: %s", path_pieces[0].c_str());
        return std::string(path_pieces[0]);
    }
}

void AppDataModel::refresh_roots(bool removable_only) {
//    int i, d, spare, fs_found;
//    fileswitch_fs_no fs_no;
//    char **fsnames;
//    char drive[100], canonical_drive[200];
//    std::vector<std::string> path_pieces;
//    tbx::Path cur_dir;
//    os_error *err;
    std::vector<std::string> found_drives;
    char clfiler_csd[200];
    if (g_app_state.no_search_drives && removable_only) {
        return;
    }

    std::vector<std::string> path_pieces;
    strcpy(clfiler_csd, getenv("CLFiler$Dir"));
    split(clfiler_csd, path_pieces, '.');
    found_drives.push_back(path_pieces[0]);
    Logger::debug("Added CLFiler$Dir root drive: %s", path_pieces[0].c_str());

    if (!g_app_state.no_search_drives) {
        for(auto root_item: _root_items) {
            if (removable_only) {
                for(int i = 0; fsnames_with_drives_removable[i] != nullptr; i++) {
                    if (strnicmp(fsnames_with_drives_removable[i], root_item->name().c_str(), strlen(fsnames_with_drives_removable[i]))==0) {
                        root_item->_flags &= ~CLDIRITEM_FLAG_REALLY_EXSITS;
                    }
                }
            } else {
                root_item->_flags &= ~CLDIRITEM_FLAG_REALLY_EXSITS;
            }
        }

        if (removable_only) {
            DrivesDetect::detect_removable_drives(found_drives);
        } else {
            DrivesDetect::detect_all_drives(found_drives);
        }
    }

/*
    if (removable_only) {
        fsnames = fsnames_with_drives_removable;
    } else {
        if (!g_app_state.no_search_drives) {
            split(get_csd(), path_pieces, '.');
            add_root_item(path_pieces[0]);

            err = xosfscontrol_lookup_fs((osfscontrol_id) "HostFS", false, &fs_no, &fs_found);
            if (!err) {
                if (fs_found && tbx::Path("HostFS::HostFS.$").exists()) {
                    add_root_item("HostFS::HostFS");
                }
            } else {
                Logger::error("Error FS lookup [%s] %d:%s", "HostFS", err->errnum, err->errmess);
            }
            fsnames = fsnames_with_drives;
        }
    }

    if (!g_app_state.no_search_drives) {
        for(i = 0; fsnames[i] != nullptr; i++) {
            err = xosfscontrol_lookup_fs((osfscontrol_id)fsnames[i], false,
                                         &fs_no, &fs_found);
            Log_debug("lookup drives [%s]", fsnames[i]);
            if (err) {
                Logger::error("Error FS lookup [%s] %d:%s", fsnames[i], err->errnum, err->errmess);
                continue;
            }
            if (fs_found) {
                for(d = 0; d < 8; d++) {
                    sprintf(drive,"%s::%d.$", fsnames[i], d);
                    err = xosfscontrol_canonicalise_path(drive, canonical_drive, 0, 0, sizeof(canonical_drive) - 1, &spare);
                    if (!err) {
                        Log_debug("canonicalize drive [%s] [%s]", drive, canonical_drive);
                        if (stricmp(drive, canonical_drive) != 0) {
                            add_root_item(canonical_drive);
                        }
                    } else {
                        Logger::error("Error canonicalize drive [%s] %d:%s", drive, err->errnum, err->errmess);
                        break;
                    }
                }
            }
        }

        if (!removable_only) {
            // detect sharefs
            std::vector<std::string> my_own_shares;
            int sharefs_context = 0;
            char *sharefs_objname, *sharefs_dirname;
            sharefs_attr sharefs_attrs;
            Logger::info("Searching for own ShareFS shares");
            while (true) {
                err = xsharefs_enumerate_shares(0xff,
                                                sharefs_context, 0, &sharefs_objname, &sharefs_dirname, &sharefs_attrs,
                                                &sharefs_context);
                Log_debug("xsharefs_enumerate_shares [%s] [%s]", sharefs_objname, sharefs_dirname);
                if (err) {
                    Logger::error("xsharefs_enumerate_shares err %d (%s)", err->errnum, err->errmess);
                    break;
                }
                if (sharefs_context == -1) {
                    break;
                }
                if (sharefs_objname) {
                    my_own_shares.push_back(std::string(sharefs_objname));
                    Logger::info("xsharefs_enumerate_shares my share obj:%s attrs:%x", sharefs_objname, sharefs_attrs);
                } else {
                    Logger::error("xsharefs_enumerate_shares my share obj:null");
                    break;
                }
            }

            Logger::info("Searching for connected ShareFS shares");
            for (tbx::PathInfo::Iterator it = tbx::PathInfo::begin(tbx::Path("Resources:$.Discs"));
                 it != tbx::PathInfo::end(); ++it) {
                if (it->file_type() == 0xbda) {
                    bool found_own_share = false;
                    for (auto &my_share: my_own_shares) {
                        if (my_share == it->name()) {
                            found_own_share = true;
                            break;
                        }
                    }
                    if (!found_own_share) {
                        Logger::info("Found ShareFS root:%s attr:%x", it->name().c_str(), it->attributes());
                        tbx::Application::instance()->os_cli("Filer_Run Resources:$.Discs." + it->name());
                        add_root_item("Share::" + it->name());
                    } else {
                        Logger::info("Found own ShareFS root:%s attr:%x (skipped)", it->name().c_str(),
                                     it->attributes());
                    }
                }
            }
        }
    }
*/
    Log_debug("Load preconfigured predefined-roots",1);
    if (tbx::Path("<CLFiler$ChoicesDir>.predefined-roots").exists()) {
        std::string line;
        std::vector<std::string> parts;
        std::ifstream in("<CLFiler$ChoicesDir>.predefined-roots");
        while (std::getline(in, line)) {
            trim(line);
            if (line.empty()) {
                continue;
            }
//                tbx::Path p;
//                try {
//                    p = tbx::Path(tbx::Path::canonicalise(line));
//                } catch (std::exception &e) {
//                    Logger::error("Can't canonicalize predefined path, skipped, line:%s path:%s error:%s", line.c_str(), p.name().c_str(), e.what());
//                    continue;
//                }
//                if (p.exists()) {
//                    Logger::info("Found predefined line:%s root:%s", line.c_str(), p.name().c_str());
//                    add_root_item(p.name());
//                } else {
//                    Logger::info("Predefined line:%s root:%s not exists, skipped", line.c_str(), p.name().c_str());
//                }
            found_drives.emplace_back(line);
        }
    }

    for(auto &drive : found_drives) {
        add_root_item(drive);
    }

    Log_debug("Find/remove deleted roots",1);
    if (!_root_items.empty()) {
        for(int i = _root_items.size() - 1; i >= 0; i--) {
            Log_debug("_root_items i=%d flags=%x name=%s", i, _root_items[i]->_flags, _root_items[i]->name().c_str());
            if (!(_root_items[i]->_flags & CLDIRITEM_FLAG_REALLY_EXSITS) && !_root_items[i]->really_exists()) {
                CLDirectoryItemData *item = _root_items[i];
                Logger::info("Delete root item (not exists): %p %s", item, item->name().c_str());
                _root_items.erase(_root_items.begin() + i);
                on_root_item_removed(item);
                delete item;
            } else {
                _root_items[i]->_flags |= CLDIRITEM_FLAG_REALLY_EXSITS;
                if (!removable_only) {
                    _root_items[i]->flush_app_icons();
                }
            }
        }
    }
}

unsigned long AppDataModel::register_item(CLDirectoryItemData* item) {
    _id_sequence++;
    while (_id_2_item.find(_id_sequence) != _id_2_item.end()) {
        _id_sequence++;
    }
    _id_2_item[_id_sequence] = item;
    return _id_sequence;
}

void AppDataModel::deregister_item(CLDirectoryItemData *item) {
    _id_2_item.erase(item->_id);
}

void AppDataModel::refresh_on_idle(CLDirectoryItemData *item, int refresh_depth, unsigned int options) {
    if (item) {
        Log_debug("refresh_on_idle item=%s", item->name().c_str());
        _idle_refresher.add_refreshed_item(item, refresh_depth, options | CLREFRESH_INITIAL_REFRESH_ITEM);
    }
}

void AppDataModel::refresh_on_idle_first(CLDirectoryItemData *item, int refresh_depth, unsigned int options) {
    if (item) {
        Log_debug("refresh_on_idle (first) item=%s", item->name().c_str());
        _idle_refresher.add_refreshed_item(item, refresh_depth, options | CLREFRESH_INITIAL_REFRESH_ITEM, true);
    }
}

void AppDataModel::on_dir_item_added(CLDirectoryItemData *item) {
    for(auto listener: _change_listeners) {
        listener->on_dir_item_added(item);
    }
}

void AppDataModel::on_dir_item_removed(CLDirectoryItemData *item) {
    if (!item->is_file()) {
        _idle_refresher.remove_refresh_item(item);
        _idle_cache_saver.cancel_save(item);
        remove_diritem_cache(item);
    }
    for(auto listener: _change_listeners) {
        listener->on_dir_item_removed(item);
    }
    if (item->parent()) {
        _idle_cache_saver.save_cache_on_idle(item->parent());
    }
}

void AppDataModel::on_dir_item_updated(CLDirectoryItemData *item, int what) {
//    Log_debug("AppDataModel::on_dir_item_updated ptr %p", item);
//    Log_debug("AppDataModel::on_dir_item_updated %s", item->name().c_str());
    for(auto listener: _change_listeners) {
//        Log_debug("AppDataModel::on_dir_item_updated1 %s", item->name().c_str());
        listener->on_dir_item_updated(item, what);
    }
}

void AppDataModel::on_root_item_added(CLDirectoryItemData *item) {
    for(auto listener: _change_listeners) {
        listener->on_root_item_added(item);
    }
    Log_debug("AppDataModel::on_root_item_added %s id:%d", item->name().c_str(), item->id());
}

void AppDataModel::on_root_item_removed(CLDirectoryItemData *item) {
    for(auto listener: _change_listeners) {
        listener->on_root_item_removed(item);
    }
    _idle_cache_saver.cancel_save(item);
    remove_diritem_cache(item);
}

void AppDataModel::on_refresh_started(CLDirectoryItemData* item) {
    _root_refreshing_item = item;
    _refreshing_item = item;
    if (!_refresh_listeners.empty()) {
        for(auto listener: _refresh_listeners) {
            listener->on_refresh_started();
        }
    }
}

void AppDataModel::on_refresh_finished(bool aborted, bool any_item_changed) {
    _root_refreshing_item = nullptr;
    _refreshing_item = nullptr;
    if (!_refresh_listeners.empty()) {
        for(auto listener: _refresh_listeners) {
            listener->on_refresh_finished(aborted, any_item_changed);
        }
    }
}

void AppDataModel::on_info_change() {
    if (!_refresh_listeners.empty()) {
        for (auto listener: _refresh_listeners) {
            listener->on_info_changed();
        }
    }
}

void AppDataModel::on_favorites_changed() {
    Log_debug("AppDataModel::on_favorites_changed favs:%d", _favorites.size());
    for(auto listener: _change_listeners) {
        listener->on_favorites_changed();
    }
}

void AppDataModel::abort_refresh() {
    _idle_refresher.abort();
}

void AppDataModel::add_change_listener(AppDataModelChangedListener* listener) {
    for(AppDataModelChangedListener* l: _change_listeners) {
        if (l == listener) {
            return;
        }
    }
    _change_listeners.push_back(listener);
}

void AppDataModel::remove_change_listener(AppDataModelChangedListener* listener) {
    if (!_change_listeners.empty()) {
        for(int i = _change_listeners.size() - 1; i >= 0; i--) {
            if (_change_listeners[i] == listener) {
                _change_listeners.erase(_change_listeners.begin() + i);
            }
        }
    }
}

void AppDataModel::add_refresh_listener(AppDataModelRefreshListener* listener) {
    for(AppDataModelRefreshListener* l: _refresh_listeners) {
        if (l == listener) {
            return;
        }
    }
    _refresh_listeners.push_back(listener);
}

void AppDataModel::remove_refresh_listener(AppDataModelRefreshListener* listener) {
    if (!_refresh_listeners.empty()) {
        for(int i = _refresh_listeners.size() - 1; i >= 0; i--) {
            if (_refresh_listeners[i] == listener) {
                _refresh_listeners.erase(_refresh_listeners.begin() + i);
            }
        }
    }
}


CLDirectoryItemData * AppDataModel::get_or_refresh_path(std::string dpath) {
    CLDirectoryItemData *item;
    tbx::Path p = dpath;
    std::string p_str;
    std::vector<std::string> refresh_dirs;
    while(true) {
        item = g_app_data_model.find_item_by_path(p.name());
        if (item) {
            break;
        }
        refresh_dirs.push_back(p.leaf_name());
        p = p.parent();
//        Log_debug("AppDataModel::get_or_refresh_path push:%s p:%s", refresh_dirs[refresh_dirs.size()-1].c_str(), p.name().c_str());
    }
    Log_debug("AppDataModel::get_or_refresh_path dir:%s depth:%d", item->path().name().c_str(),refresh_dirs.size());
    if (item != nullptr && !refresh_dirs.empty()) {
        item->refresh(CLREFRESH_RUN_ITEM_CALLBACKS);
        while(!refresh_dirs.empty()) {
            p_str = refresh_dirs[refresh_dirs.size() - 1];
            refresh_dirs.pop_back();
            Log_debug("AppDataModel::get_or_refresh_path refresh_dirs dir:%s item:%s childs:%d", p_str.c_str(), item->path().name().c_str(), item->childs().size());
            for(auto child : item->childs()) {
                Log_debug("AppDataModel::get_or_refresh_path child:%s name:%s p:%s", child->path().name().c_str(), child->name().c_str(), p_str.c_str());
                if (child->name() == p_str) {
                    child->refresh(CLREFRESH_RUN_ITEM_CALLBACKS);
                    item = child;
                    Log_debug("AppDataModel::get_or_refresh_path refresh_dirs child refresh:%s", item->path().name().c_str());
                    break;
                }
            }
        }
    }
    if (item == nullptr) {
        Logger::error("AppDataModel::get_or_refresh_path parent dir not found for:%s", dpath.c_str());
        return nullptr;
    }
    Log_debug("AppDataModel::get_or_refresh_path return:%s", item->path().name().c_str());
    return item;
}



CLThumbSizes* CLThumbSizes::load(const std::string& path) {
    std::string thumbs_sizes_file = CLImageCache::get_thumbs_cache_path(path + "./thumbs-info");
    FILE *f = fopen(thumbs_sizes_file.c_str(), "rb");
    if (f) {
        CLThumbSizes* ts = new CLThumbSizes(path);
        while(!feof(f)) {
            char fname[255];
            int img_w, img_h;
            unsigned long size;
            if (fscanf(f,"%s\t%d\t%d\t%lu\n", fname, &img_w, &img_h, &size) != 4) {
  //              Log_debug("CLThumbSizes::load %s stop load items", thumbs_sizes_file.c_str());
                break;
            }
  //          Log_debug("CLThumbSizes::load %s loaded item:%s", thumbs_sizes_file.c_str(), fname);
            ts->thumb_sizes[fname] = {img_w, img_h, size};
            ts->path = path;
        }
        fclose(f);
//        Log_debug("CLThumbSizes::load %s loaded items:%d", thumbs_sizes_file.c_str(), ts->thumb_sizes.size());
        return ts;
    } else {
        Logger::warn("CLThumbSizes::load can't read thumbs-info (%s), error:%s", thumbs_sizes_file.c_str(), strerror(errno));
        return nullptr;
    }
}

void CLThumbSizes::save() {
    if (!thumb_sizes.empty()) {
        std::string filename = CLImageCache::get_thumbs_cache_path(path + "./thumbs-info");
//        Log_debug("CLThumbSizes::save saving dir:%s path:%s items:%d", path.c_str(), filename.c_str(), thumb_sizes.size());
        FILE *f = fopen(filename.c_str(),"w");
        if (!f) {
            CLUtils::create_directories_for_file(filename);
            f = fopen(filename.c_str(),"w");
        }
        if (f) {
            for(auto &ths : thumb_sizes) {
                fprintf(f, "%s\t%d\t%d\t%lu\n", ths.first.c_str(), ths.second.width, ths.second.height, ths.second.size);
            }
            fclose(f);
            Log_debug("CLThumbSizes::save saved dir:%s path:%s items:%d", path.c_str(), filename.c_str(), thumb_sizes.size());
        } else {
            Logger::warn("CLThumbSizes::save can't save thumbs-info (%s), error:%s", filename.c_str(), strerror(errno));
        }
    }
    needs_to_be_save = false;
}

bool CLThumbSizes::get_thumb_size(CLDirectoryItemData* item, int &w, int &h) {
    auto found = thumb_sizes.find(item->name());
    if (found != thumb_sizes.end()) {
//        Log_debug("CLThumbSizes::get_thumb_size %s found:%p size:%lu item size:%lu", item->name().c_str(), found, found->second.size, item->file_size());
        if (found->second.size == item->file_size()) {
            w = found->second.width;
            h = found->second.height;
//            Log_debug("CLThumbSizes::get_thumb_size %s found w:%d h:%d", item->name().c_str(), w, h);
            return true;
        }
//    } else {
//        Log_debug("CLThumbSizes::get_thumb_size not found %s", item->name().c_str());
    }
    return false;
}

bool CLThumbSizes::set_thumb_size(CLDirectoryItemData* item, int w, int h) {
    if (item) {
        thumb_sizes[item->name()] = {w, h, item->file_size()};
        needs_to_be_save = true;
    }
}

CLThumbSizes* CLThumbnailer::get_thumb_sizes_for_item(CLDirectoryItemData* item) {
    int i;
    auto ts = _thumbnail_sizes.size();
    std::string parent_path = item->parent()->path().name();
    CLThumbSizes* thumbsz = nullptr;
    if (!_thumbnail_sizes.empty()) {
        for(i = 0; i < ts; i++) {
//            Log_debug("_thumbnail_sizes[i]->path %s pp %s", _thumbnail_sizes[i]->path.c_str(), parent_path.c_str());
            if (_thumbnail_sizes[i]->path == parent_path) {
                thumbsz = _thumbnail_sizes[i];
                if (i > 0) {
                    _thumbnail_sizes[i] = _thumbnail_sizes[0];
                    _thumbnail_sizes[0] = thumbsz;
                }
                break;
            }
        }
    }
    if (!thumbsz) {
        Log_debug("CLThumbnailer::get_thumb_sizes_for_item _thumbnail_sizes:%d %s not found, will load", _thumbnail_sizes.size(), parent_path.c_str());
        thumbsz = CLThumbSizes::load(parent_path);
        if (thumbsz) {
            if (ts >= 100) {
                delete _thumbnail_sizes[ts - 1];
                _thumbnail_sizes.pop_back();
            }
            _thumbnail_sizes.push_back(thumbsz);
        }
//    } else {
//        Log_debug("CLThumbnailer::get_thumb_sizes_for_item _thumbnail_sizes:%d %s found", _thumbnail_sizes.size(), parent_path.c_str());
    }
    if (!thumbsz) {
        thumbsz = new CLThumbSizes(parent_path);
        _thumbnail_sizes.push_back(thumbsz);
        Log_debug("CLThumbnailer::get_thumb_sizes_for_item %s created new object", parent_path.c_str());
    }
    return thumbsz;
}

bool CLThumbnailer::get_cached_thumbnail_size(CLDirectoryItemData* item, int &w, int &h) {
    CLThumbSizes* thumbsz = get_thumb_sizes_for_item(item);
    return thumbsz->get_thumb_size(item, w, h);
}

bool CLThumbnailer::set_cached_thumbnail_size(CLDirectoryItemData* item, int w, int h) {
    Log_debug("CLThumbnailer::set_cached_thumbnail_size item:%s w:%d h:%d", item->path().name().c_str(), w, h);
    CLThumbSizes* thumbsz = get_thumb_sizes_for_item(item);
    thumbsz->set_thumb_size(item, w, h);
    _thumb_sizes_saver.save_on_idle();
}

CLBaseImage* CLThumbnailer::create_thumbnail(CLDirectoryItemData* item) {
    tbx::Path item_p = item->path();
    std::string cache_key = CLImageCache::make_thumbnail_key(item_p);

    if (CLImageFactory::can_load(item_p.file_type())) {
//        Log_error("CLThumbnailer::CLThumbIdleCreator::execute create thumb for %s", item_p.name().c_str());
        CLBaseImage *thumb_img = CLImageCache::create_cached_thumbnail(item_p, cache_key, THUMBNAIL_IMAGE_WIDTH,
                                                                       THUMBNAIL_IMAGE_HEIGHT);
        if (thumb_img) {
            Log_error("CLThumbnailer::create_thumbnail create thumb for %s created", item_p.name().c_str());
            int w=0, h=0, sz=0;
            if (!get_cached_thumbnail_size(item, w, h) || w != thumb_img->width() || h != thumb_img->height()) {
                set_cached_thumbnail_size(item, thumb_img->width(), thumb_img->height());
            }
        }
        return thumb_img;
    }
    return nullptr;
}

void CLThumbnailer::CLThumbIdleCreator::execute() {
    unsigned long item_id;
    CLDirectoryItemData* item;

    if (item_ids.empty()) {
        _creating = false;
        tbx::Application::instance()->remove_idle_command(this);
        return;
    }

    item_id = item_ids[0];
    item_ids.erase(item_ids.begin());
    item = g_app_data_model.find_item_by_id(item_id);
    if (!item) {
        Log_error("CLThumbnailer::CLThumbIdleCreator::execute item id:%lld not found!", item_id);
        return;
    }

    CLBaseImage *thumb_img = _thumbnailer->create_thumbnail(item);
    if (thumb_img) {
        g_app_data_model.on_dir_item_updated(item, CLUPDATED_THUMBNAIL);
    }

    if (item_ids.empty()) {
        _creating = false;
        tbx::Application::instance()->remove_idle_command(this);
        g_app_data_model.on_info_change();
    }
};

void CLThumbnailer::CLThumbIdleCreator::create_on_idle(CLDirectoryItemData* item) {
    if (!item_ids.empty()) {
        for(auto iid : item_ids) {
            if (item->id() == iid) {
                return;
            }
        }
    }
    item_ids.push_back(item->id());
    if (!_creating) {
        _creating = true;
        tbx::Application::instance()->add_idle_command(this);
        g_app_data_model.on_info_change();
    }
}

CLBaseImage * CLThumbnailer::get_thumbnail(CLDirectoryItemData* item) {
    if (CLImageFactory::can_load(item->file_type()) && !g_app_state.out_of_memory) {
        Log_debug("CLThumbnailer::get_thumbnail can load item:%s", item->name().c_str());
        tbx::Path item_p = item->path();
        std::string cache_key = CLImageCache::make_thumbnail_key(item_p);
        CLBaseImage *thumb_img = CLImageCache::get_cached_thumbnail(item_p, cache_key);
        if (thumb_img) {
            int w = 0;
            int h = 0;
            if (!get_cached_thumbnail_size(item, w, h)) {
                Log_debug("CLThumbnailer::get_thumbnail %s thum size not found, set", item->path().name().c_str());
                set_cached_thumbnail_size(item, thumb_img->width(), thumb_img->height());
            }
            return thumb_img;
        }

//        Log_debug("CLThumbnailer::get_thumbnail %s thumb will created", item->path().name().c_str());
        _thumb_creator.create_on_idle(item);
    } else {
        Log_debug("CLThumbnailer::get_thumbnail can't load (use sprite) item:%s", item->name().c_str());
    }
    return item->sprite_image();
}

/* old way (single file) cache loading/saving */

//bool AppDataModel::load_childs_state(CLDirectoryItemData *parent_item, size_t num_childs, FILE *in) {
//    CLDirectoryItemDataSerialized data;
//    CLDirectoryItemData *new_item;
//    for(int i = 0; i < num_childs; i++) {
//        if (!data.load(in)) {
//            Logger::error("Can't read cache file! error:%s", strerror(errno));
//            return false;
//        }
////        Log_debug("Loading child %s", data.name);
//        new_item = new CLDirectoryItemData(parent_item, data);
//        if (data.num_childs > 0) {
//            if (!load_childs_state(new_item, data.num_childs, in)) {
//                return false;
//            }
//        }
//        if (_operations_counter++ > 100) {
//            //call_wimp_poll();
//            tbx::Application::instance()->yield();
//            _operations_counter = 0;
//        }
//    }
//    return true;
//}

//bool AppDataModel::load_state() {
//    CLDirectoryItemDataSerialized root_data;
//    size_t read_bytes;
//    for(auto root_item: _root_items) {
//        std::string filename = std::string("<CLFiler$ChoicesDir>.state-")+str_replace_all(str_replace_all(root_item->name(), ":", "_"), ".$", "");
//        FILE *in = fopen(filename.c_str(), "rb");
//        if (!in) {
//            Logger::warn("Can't open file to load state. Filename: %s, error: %s", filename.c_str(), strerror(errno));
//            continue;
//        }
//        if (!root_data.load(in)) {
//            Logger::error("Can't read state file! Filename: %s, error:%s", filename.c_str(), strerror(errno));
//            fclose(in);
//            continue;
//        }
//        _operations_counter = 0;
//        root_item->_dir_size = root_data.size;
//        load_childs_state(root_item, root_data.num_childs, in);
//        fclose(in);
//    }
//}
//
//bool AppDataModel::save_childs_state(CLDirectoryItemData *parent_item, FILE *out) {
//    CLDirectoryItemDataSerialized data;
//    data.serialize(parent_item);
////    Log_debug("Saving parent %s", parent_item->name().c_str());
//    if (!data.save(out)) {
//        Logger::error("Can't write to state file!");
//        return false;
//    }
//    if (data.num_childs > 0) {
//        for(auto child: parent_item->childs()) {
//            if (!child->childs().empty()) {
//                if (!save_childs_state(child, out)) {
//                    return false;
//                }
//            } else {
//                data.serialize(child);
////                Log_debug("Saving child %s -> %s", parent_item->name().c_str(), data.name);
//                if (!data.save(out)) {
//                    Logger::error("Can't write to state file!");
//                    return false;
//                }
//            }
//        }
//        if (_operations_counter++ > 500) {
//            tbx::Application::instance()->yield();
////                call_wimp_poll();
//            _operations_counter = 0;
//        }
//    }
//    return true;
//}

//bool AppDataModel::save_state() {
//    for(auto root_item: _root_items) {
//        save_state(root_item);
//    }
//}
//
//bool AppDataModel::save_state(CLDirectoryItemData* root_item) {
//    std::string filename = std::string("<CLFiler$ChoicesDir>.state-")+str_replace_all(root_item->name(), ":", "_");
//    std::string filename_new = filename + std::string("-new");
//    std::string filename_old = filename + std::string("-old");
//    FILE *out = fopen(filename_new.c_str(), "wb");
//    if (!out) {
//        Logger::error("Can't create/open file to save state. Filename: %s, error: %s", filename_new.c_str(), strerror(errno));
//        return false;
//    }
//    _operations_counter = 0;
//    save_childs_state(root_item, out);
//    fclose(out);
//    remove(filename_old.c_str());
//    rename(filename.c_str(), filename_old.c_str());
//    rename(filename_new.c_str(), filename.c_str());
//    return true;
//}

tbx::Path AppDataModel::find_real_existing_dir_or_parent(tbx::Path dir) {
    if (!dir.name().empty()) {
        if (dir.exists() && (dir.file_type() >= 0x1000 || dir.image_file())) {
            dir.canonicalise();
            return dir;
        }
        while(dir.name() != dir.parent().name()) {
            dir = dir.parent();
            if (dir.exists() && (dir.file_type() >= 0x1000 || dir.image_file())) {
                dir.canonicalise();
                return dir;
            }
        }
    }
    while(!_root_items.empty()) {
        if (_root_items[0]->path().exists()) {
            return _root_items[0]->path();
        }
        CLDirectoryItemData* item = _root_items[0];
        _root_items.erase(_root_items.begin());
        on_root_item_removed(item);
    }
    return tbx::Path();
}

std::string AppDataModel::get_diritem_cache_filename(const tbx::Path& p) {
    return std::string("<CLFiler$ChoicesDir>.cache.")+str_replace_all(str_replace_all(p.name(), ":", "_"),".$", "")+std::string("-cache");
}

std::string AppDataModel::get_diritem_cache_filename(CLDirectoryItemData *item) {
    return get_diritem_cache_filename(item->path());
}

bool AppDataModel::save_diritem_cache(CLDirectoryItemData *item) {
    CLDirectoryItemDataSerialized data;
    //Log_debug("AppDataModel::save_diritem_cache %s", item->path().name().c_str());
    if (!item->childs().empty()) {
        std::string filename = get_diritem_cache_filename(item);
        remove(filename.c_str());
        FILE *out = fopen(filename.c_str(), "wb");
        if (!out) {
            if (!CLUtils::create_directories_for_file(filename)) {
                Logger::error("Can't create directories for: %s", filename.c_str());
                return false;
            }
            out = fopen(filename.c_str(), "wb");
            if (!out) {
                Logger::error("Can't create/open file to save cache. Filename: %s, error: %s", filename.c_str(), strerror(errno));
                return false;
            }
        }

        size_t childs_count = item->childs().size();
        if (fwrite(&childs_count, sizeof(childs_count), 1, out) != 1) {
            goto fail;
        }
        for(auto child: item->childs()) {
            data.serialize(child);
//                Log_debug("Saving child %s -> %s", item->name().c_str(), data.name);
            if (!data.save(out)) {
                Logger::error("Can't write to cache file: %s", filename.c_str());
                goto fail;
            }
        }
    //        if (_operations_counter++ > 500) {
    //            tbx::Application::instance()->yield();
    ////                call_wimp_poll();
    //            _operations_counter = 0;
    //        }
        Log_debug("AppDataModel::save_diritem_cache saved %s", item->path().name().c_str());
        fclose(out);
        return true;
    fail:
        fclose(out);
        remove(filename.c_str());
        return false;
    }
}

void AppDataModel::save_diritem_cache_on_idle(CLDirectoryItemData *item) {
    _idle_cache_saver.save_cache_on_idle(item);
}

bool AppDataModel::load_diritem_cache(CLDirectoryItemData *item, int depth) {
    item->_flags |= CLDIRITEM_FLAG_CACHE_LOADED;
    CLDirectoryItemDataSerialized data;
    std::string filename = get_diritem_cache_filename(item);
    FILE *in = fopen(filename.c_str(), "rb");
    if (!in) {
        Log_debug("AppDataModel::load_diritem_cache can't open file to load cache. Filename: %s, %s", filename.c_str(), strerror(errno));
        return false;
    }
    size_t childs_count = 0;
    if (fread(&childs_count, sizeof(childs_count), 1, in) != 1) {
        goto fail;
    }
    if (childs_count > 0) {
        for(size_t i = 0; i < childs_count; i++) {
            if (!data.load(in)) {
                Logger::error("Can't read cache file: %s error:%s", filename.c_str(), strerror(errno));
                goto fail;
            }
            new CLDirectoryItemData(item, data);
        }
    }
    fclose(in);
    if (depth > 0) {
        depth--;
        for(auto ch_item: item->childs()) {
            if (!ch_item->is_file()) {
                tbx::Application::instance()->yield();
                g_app_data_model.load_diritem_cache(ch_item, depth);
            }
        }
    }
    return true;
fail:
    fclose(in);
    return false;
}

bool AppDataModel::remove_diritem_cache(CLDirectoryItemData *item) {
    // -6 is length of -cache suffx
    std::string filename = get_diritem_cache_filename(item);
    Log_debug("AppDataModel::remove_diritem_cache file:%s", filename.c_str());
    try {
        tbx::Path(filename).remove();
    } catch (tbx::OsError &ex) {
        Log_error("remove_diritem_cache error %s (%d), File: %s", ex.what(), ex.number(), filename.c_str());
    }
    Log_debug("AppDataModel::remove_diritem_cache dir:%s", filename.substr(0, filename.size()-6).c_str());
    CLUtils::remove_recursive(filename.substr(0, filename.size()-6));
}

void AppDataModel::rename_diritem_cache(const tbx::Path& oldpath, const tbx::Path& newpath) {
    std::string old_filename = get_diritem_cache_filename(oldpath);
    std::string new_filename = get_diritem_cache_filename(newpath);
    Log_debug("AppDataModel::rename_diritem_cache file:%s -> %s", old_filename.c_str(), new_filename.c_str());
    try {
        tbx::Path(old_filename).rename(new_filename);
    } catch (tbx::OsError &err)  {
        Log_debug("AppDataModel::rename_diritem_cache rename cache file failed", 1);
    }

    old_filename = old_filename.substr(0, old_filename.size()-6);
    new_filename = new_filename.substr(0, new_filename.size()-6);
    Log_debug("AppDataModel::remove_diritem_cache dir:%s -> %s", old_filename.c_str(), new_filename.c_str());
    try {
        tbx::Path(old_filename).rename(new_filename);
    } catch (tbx::OsError &err)  {
        Log_debug("AppDataModel::rename_diritem_cache rename cache dir failed", 1);
    }
}

void CLDirectoryItemDataSerialized::serialize(CLDirectoryItemData *item) {
    strcpy(name, item->_name.c_str());
    attrs = item->_attrs;
    file_type = item->_file_type;
    mtime = item->_mtime;
    if (item->is_file()) {
        size = item->_file_size;
    } else {
        size = item->_dir_size;
    }
    num_childs = item->childs().size();
    flags = (item->_flags & (CLDIRITEM_FLAG_IS_IMAGE|CLDIRITEM_FLAG_IS_FILE|CLDIRITEM_FLAG_HAS_SUBDIRS|CLDIRITEM_FLAG_HAS_FILES|CLDIRITEM_FLAG_REALLY_EXSITS|CLDIRITEM_FLAG_FULL_SCANNED));
}

bool CLDirectoryItemDataSerialized::save(FILE *out) {
    int datalen = sizeof(CLDirectoryItemDataSerialized) - 255 + strlen(name);
    int countout;
    countout = fwrite(&datalen, sizeof(datalen), 1, out);
//    Log_debug("Save datalen = %d %d cnt:%d", datalen, sizeof(datalen), countout);
    if (countout != 1) {
        return false;
    }
    countout = fwrite(this, datalen, 1, out);
//    Log_debug("Save this = %d cnt:%d", datalen, countout);
    if (countout != 1) {
        return false;
    }
}

bool CLDirectoryItemDataSerialized::load(FILE *in) {
    int datalen;
    int countin;
    countin = fread(&datalen, sizeof(datalen), 1, in);
//    Log_debug("Load datalen = %d cnt:%d", datalen, countin);
    if (countin != 1) {
        return false;
    }
    countin = fread(this, datalen, 1, in);
//    Log_debug("Load this = %d cnt:%d", datalen, countin);
    if (countin != 1) {
        return false;
    }
}

void AppDataModel::load_autofiletypes() {
    FILE *f = fopen("<CLFiler$Dir>.autofiletypes","rt");
    if (f) {
        while(!feof(f)) {
            char extensions_str[1000];
            std::vector<std::string> exts;
            AutoFiletypeExt ft;
            if (fscanf(f,"%x;%s\n", &ft.file_type, extensions_str) != 2) {
                continue;
            }
            split(std::string(extensions_str), exts, ',');
            for(auto e : exts) {
                ft.extensions.push_back("/"+e);
            }
            auto_filetype_extensions.push_back(ft);
        }
        fclose(f);
        for(auto ft : auto_filetype_extensions) {
            Log_debug("loaded ft:%x filetypes[0]: %s filetypes cnt:%d", ft.file_type, ft.extensions[0].c_str(),ft.extensions.size());
        }
    }
}



CLTimelineItemData::CLTimelineItemData(std::string& full_name, tbx::PathInfo& info) {
    update(info);
    _name = full_name;
}

bool CLTimelineModel::load() {
    Log_debug("Loading timeline",1);
    size_t i;
    unsigned int item_id;
    bool duplicates_found = false;
    CLDirectoryItemData *item;
    std::ifstream in("<CLFiler$ChoicesDir>.last_accessed");
    std::string full_name;
    while (std::getline(in, full_name)) {
        item = g_app_data_model.find_item_by_path(full_name);
        if (item) {
            item_id = item->id();
            Log_debug("CLTimelineModel::load id:%d ptr:%p name:%s", item_id, item, item->name().c_str());
            for(i = 0; i < _item_ids.size(); i++) {
                if (_item_ids[i] == item_id) {
                    _item_ids.erase(_item_ids.begin() + i);
                    duplicates_found = true;
                    break;
                }
            }
            _item_ids.push_back(item_id);
        }
    }
    in.close();
    if (duplicates_found) { // resave if duplicates found and erased
        save();
    }
    return true;
}

bool CLTimelineModel::save() {
    Log_debug("Saving timeline",1);
    CLDirectoryItemData *item;
    std::ofstream out("<CLFiler$ChoicesDir>.last_accessed");
    for(auto id : _item_ids) {
        item = g_app_data_model.find_item_by_id(id);
        if (item) {
            out << item->path().name() << std::endl;
        }
    }
}

#define MAX_TIMELINE_ITEMS 1000
#define NUM_ERASED_ITEMS 100
void CLTimelineModel::append(CLDirectoryItemData *item) {
    int i;
    unsigned int item_id = item->id();
    tbx::PathInfo info;

    // erase item if it already in list
    for(i = 0; i < _item_ids.size(); i++) {
        if (_item_ids[i] == item_id) {
            _item_ids.erase(_item_ids.begin() + i);
            break;
        }
    }
    _item_ids.push_back(item_id);

    if (_item_ids.size() > MAX_TIMELINE_ITEMS) {
        _item_ids.erase(_item_ids.begin(), _item_ids.begin() + NUM_ERASED_ITEMS);
    }
    Log_debug("CLTimelineModel::append %s", item->path().name().c_str());
    std::ofstream out("<CLFiler$ChoicesDir>.last_accessed", std::ios_base::app);
    out << item->path().name() << std::endl;
}

std::vector<CLDirectoryItemData *> CLTimelineModel::filtered_items(int timeline_filter) {
    int i;
    std::vector<CLDirectoryItemData *> result;
    CLDirectoryItemData *item;
    for(i = _item_ids.size() - 1; i >= 0; i--) {
        item = g_app_data_model.find_item_by_id(_item_ids[i]);
        if (item) {
            switch(timeline_filter) {
                case TIMELINE_FILTER_FOLDERS:
                    if (item->is_directory()) {
//                        Log_debug("CLTimelineModel::filtered_items TIMELINE_FILTER_FOLDERS item %d %s", i, item->name().c_str());
                        result.push_back(item);
                    }
                    break;
                case TIMELINE_FILTER_APPS:
                    if (item->is_application()) {
//                        Log_debug("CLTimelineModel::filtered_items TIMELINE_FILTER_APPS item %d %s", i, item->name().c_str());
                        result.push_back(item);
                    }
                    break;
                case TIMELINE_FILTER_FILES:
                    if (item->is_file()) {
//                        Log_debug("CLTimelineModel::filtered_items TIMELINE_FILTER_FILES item %d %s", i, item->name().c_str());
                        result.push_back(item);
                    }
                    break;
            }
//            Log_debug("CLTimelineModel::filtered_items item %d id:%d ptr:%p size:%d %s", i, _item_ids[i], item, result.size(), item->name().c_str());
        }
    }
    return result;
}

//
// Created by slenz on 14.12.2022.
//

#include <list>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <sys/stat.h>
#include <errno.h>
#include <unixlib/local.h>
#include "CLUtils.h"
#include "CLException.h"
#include <tbx/path.h>
#include <tbx/oserror.h>
#include "Logger.h"
#include "oslib/os.h"
#include "oslib/osmodule.h"

struct _riscos_file_type_struct {
    int type;
    const char* ext;
};
_riscos_file_type_struct _riscos_file_types[] = {
        {0xFFF, "txt"},
        {0xFFD, "dat"},
        {0xFF9, "spr"},
        {0xFF0, "tiff"},
        {0xFD9, "exe"},
        {0xFD8, "com"},
        {0xFC2, "aiff"},
        {0xFB2, "avi"},
        {0xFB1, "wav"},
        {0xFAF, "html"},
        {0xF98, "psd"},
        {0xF89, "gz"},
        {0xF83, "mng"},
        {0xF81, "js"},
        {0xF80, "xml"},
        {0xF79, "css"},
        {0xF78, "jng"},
        {0xDFE, "csv"},
        {0xDF6, "iso"},
        {0xDEA, "dxf"},
        {0xDDC, "zip"},
        {0xDB0, "wk1"},
        {0xD6D, "rle"},
        {0xCFF, "cff"},
        {0xCE5, "tex"},
        {0xCE4, "dvi"},
        {0xCB6, "mod"},
        {0xC85, "jpg"},
        {0xC46, "tar"},
        {0xC32, "rtf"},
        {0xBF8, "mpg"},
        {0xBA6, "xls"},
        {0xB61, "xbm"},
        {0xB60, "png"},
        {0xB2F, "wmf"},
        {0xADF, "pdf"},
        {0xAE4, "jar"},
        {0xABF, "cab"},
        {0xAAD, "svg"},
        {0xAA7, "m3u"},
        {0xA91, "zip"},
        {0xA8F, "ac3"},
        {0xA8D, "vob"},
        {0xA7F, "xlsx"},
        {0xA7E, "docx"},
        {0xA66, "webp"},
        {0x808, "eps"},
        {0x77F, "ttf"},
        {0x69E, "ppm"},
        {0x69C, "bmp"},
        {0x697, "pcx"},
        {0x695, "gif"},
        {0x1CF, "flac"},
        {0x1AD, "mp3"},
        {0x1A8, "ogg"},
        {0x132,"ico"},
        {0x071, "avi"}
};
#define _riscos_file_types_len (sizeof _riscos_file_types / sizeof _riscos_file_types[0])

const char* CLUtils::filetype2ext(int file_type) {
    unsigned int i;
    _riscos_file_type_struct *t;

    for(i = 0; i < _riscos_file_types_len; i++) {
        t = &_riscos_file_types[i];
        //Log_debug("Filetype %d %d->%s", file_type, t->type, t->ext);
        if (t->type == 0) {
            return NULL;
        }
        if (t->type == file_type) {
            //Log_debug("Filetype %d->%s", file_type, t->ext);
            return t->ext;
        }
    }

    return NULL;
}

std::string CLUtils::file_basename_append_ext(const std::string& file_full_path) {
    const char *ext;
    std::string file_name;
    bits load, exec, attr;
    int size, file_type;
    file_name = std::string(basename(
            str_replace_all(str_replace_all(str_replace_all(file_full_path, "/", "!@!"), ".", "/"), "!@!", ".").c_str()));
    file_type = tbx::Path(file_full_path).file_type();
    if (file_type) {
        ext = filetype2ext(file_type);
        if (ext) {
            std::string ext_str = "." + std::string(ext);
            if (file_name.size() >= ext_str.size() && file_name.compare(file_name.size() - ext_str.size(), ext_str.size(), ext_str) == 0) {
                Logger::error("dont append ext, filename already has it [%s]", file_name.c_str());
                return file_name;
            } else {
                file_name.append(".");
                file_name.append(ext);
                Log_debug("append ext %s", file_name.c_str());
                return file_name;
            }
        } else {
            file_name.append(",");
            file_name.append(to_string(file_type));
            Log_debug("append file_type %s", file_name.c_str());
            return file_name;
        }
    }
    return file_name;
}

std::string CLUtils::to_string(int num)
{
    char numstr[10];
    sprintf(numstr, "%d", num);
    return numstr;
}

std::string CLUtils::str_replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

bool CLUtils::is_file_exist (const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

bool CLUtils::is_directory_exist (const std::string& name) {
    struct stat buffer;
    int res = stat (name.c_str(), &buffer);
    return (res == 0 && (buffer.st_mode & S_IFDIR));
}

bool CLUtils::create_directories_for_file(const std::string& file_name) {
    std::string dirname;
    std::list<std::string> create_dirs;
    for(size_t i = file_name.size()-1; i > 0; i--) {
        if (file_name[i] == '.') {
            dirname = file_name.substr(0, i);
            if (is_directory_exist(dirname)) {
                //Log_debug("dirname exists=%s", dirname.c_str());
                break;
            }
            create_dirs.push_back(dirname);
        }
    }
    while(!create_dirs.empty()) {
        //Log_debug("create dir=%s", create_dirs.back().c_str());
        if (mkdir(create_dirs.back().c_str(), 0777) != 0) {
            Logger::error("CLUtils::create_directories_for_file can't make dir for %s error: (%d) %s", create_dirs.back().c_str(), errno, strerror(errno));
            return false;
        }
        create_dirs.pop_back();
    }
    return true;
}

int CLUtils::get_module_version(const char *module_name)
{
    os_error *e;
    int       module_no;
    int       section;
    char     *this_module_name;
    int       bcd_version;

    module_no = 0;
    section   = -1;
    for (;;)
    {
        e = xosmodule_enumerate_rom_with_info(module_no,
                                              section,
                                              &module_no,
                                              &section,
                                              &this_module_name,
                                              NULL, /* status */
                                              NULL, /* chunk_no */
                                              &bcd_version);
        if (e)
            return -1; /* unknown */

        if (strcmp(this_module_name, module_name) == 0)
            return bcd_version;
    }
}

void CLUtils::remove_recursive(const std::string& src) {
    tbx::Path src_path = tbx::Path(src);
    if (src_path.directory()) {
        for(auto leaf = src_path.begin(); leaf != src_path.end(); leaf++) {
            auto leaf_path = tbx::Path(src, *leaf);
            if (leaf_path.directory()) {
                Log_debug("remove_recursive Dir: %s", leaf_path.name().c_str());
                remove_recursive(leaf_path.name());
            } else {
                try {
                    leaf_path.remove();
                    Log_debug("remove_recursive File: %s", leaf_path.name().c_str());
                } catch (tbx::OsError &ex) {
                    Log_error("remove_recursive error %s (%d), File: %s", ex.what(), ex.number(), leaf_path.name().c_str());
                }
            }
        }
    }
    try {
        src_path.remove();
    } catch (tbx::OsError &ex) {
        Log_error("remove_recursive error %s (%d), File: %s", ex.what(), ex.number(), src_path.name().c_str());
    }
}

std::string CLUtils::unixify(const std::string &src) {
    char tmp[255];
    __unixify(src.c_str(), 0, tmp, sizeof(tmp)-1, 0);
    if (tmp[0] == '/' && strstr(tmp, "::")) {
        return std::string((&tmp[0])+1);
    }
    return std::string(tmp);
}

bool CLUtils::is_application_registered_for(int filetype) {
    char envname[200];
    snprintf(envname, sizeof(envname) - 1, "Alias$@RunType_%03X", filetype);
    Log_debug("CLUtils::is_application_registered_for %s = %s", envname, getenv(envname));
    return getenv(envname) != nullptr;
}

std::string CLUtils::get_unique_numbered_filename(const std::string& filepath) {
    if (!tbx::Path(filepath).exists()) {
//        Log_debug("get_unique_numbered_filename return same name %s", filepath.c_str());
        return filepath;
    }
    std::string fpath = filepath;
    tbx::Path newpath;
    char strbuf[255];
    int i = 1, lastchar_idx = fpath.size() - 1;

    if (lastchar_idx > 3 && fpath[lastchar_idx] == ')') {
        lastchar_idx--;
//        Log_debug("get_unique_numbered_filename found )", 1);
        while(lastchar_idx > 0) {
//            Log_debug("get_unique_numbered_filename strip %d", lastchar_idx);
            switch(fpath[lastchar_idx]) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    lastchar_idx--;
                    break;
                case '(':
                    if (fpath[lastchar_idx - 1] == ' ' || fpath[lastchar_idx - 1] == '\xA0' || fpath[lastchar_idx - 1] == '-') {
                        lastchar_idx--;
                    }
                    fpath = fpath.substr(0, lastchar_idx);
                    goto exit_check;
                default:
                    goto exit_check;
            }
        }
    }
    exit_check:

    while(true) {
        snprintf(strbuf, sizeof(strbuf), "%s-(%d)", fpath.c_str(), i);
        newpath = tbx::Path(std::string(strbuf));
//        Log_debug("get_unique_numbered_filename newpath '%s' exists:%d", strbuf, newpath.exists());
        if (!newpath.exists()) {
            return newpath.name();
        }
        i++;
    }
    return filepath;
}

std::string CLUtils::get_file_contents(const char *filename)
{
    std::FILE *fp = std::fopen(filename, "rb");
    if (fp)
    {
        std::string contents;
        std::fseek(fp, 0, SEEK_END);
        contents.resize(std::ftell(fp));
        std::rewind(fp);
        std::fread(&contents[0], 1, contents.size(), fp);
        std::fclose(fp);
        return(contents);
    }
    throw_exception(std::string("Error open/create file. ") + strerror(errno));
}

void CLUtils::copy_dir_exclude(std::string src, std::string dst, char* exclude) {
    tbx::Path src_path = tbx::Path(src);
    for(auto file = src_path.begin(); file != src_path.end(); file++) {
        if (*file == exclude) {
            continue;
        }
        tbx::Path src_item = tbx::Path(src, *file);
        src_item.copy(dst + "." + *file, tbx::Path::CopyOption::COPY_RECURSE | tbx::Path::CopyOption::COPY_FORCE);
        Log_debug("_copydir_exclude File: %s", file->c_str());
    }
}

void CLUtils::copy_dir(const std::string& src, const std::string& dst, const std::string& exclude) {
    tbx::Path src_path = tbx::Path(src);
    mkdir(dst.c_str(), 0777);
    for(auto file = src_path.begin(); file != src_path.end(); file++) {
        if (*file == exclude) {
            continue;
        }
        tbx::Path src_item = tbx::Path(src, *file);
        src_item.copy(dst + "." + *file, tbx::Path::CopyOption::COPY_RECURSE | tbx::Path::CopyOption::COPY_FORCE);
        Log_debug("_copy_dir File: %s", file->c_str());
    }
}

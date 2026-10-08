//
// Created by slenz on 14.12.2022.
//

#ifndef CLFILER_CLUTILS_H
#define CLFILER_CLUTILS_H

#include <string>
#include <tbx/path.h>

class CLUtils {
public:
    static std::string str_replace_all(std::string str, const std::string& from, const std::string& to);
    static bool is_file_exist (const std::string& name);
    static bool is_directory_exist (const std::string& name);
    static bool create_directories_for_file(const std::string& file_name);
    static void copy_dir(const std::string& src, const std::string& dst, const std::string& exclude);
    static void copy_dir_exclude(std::string src, std::string dst, char* exclude);
    static int get_module_version(const char *module_name);
    static void remove_recursive(const std::string& src);
    static std::string unixify(const std::string& src);
    static bool is_application_registered_for(int filetype);
    static std::string get_unique_numbered_filename(const std::string& filename);
    static std::string get_file_contents(const char *filename);
    static const char* filetype2ext(int file_type);
    static std::string to_string(int num);
    static std::string file_basename_append_ext(const std::string& file_full_path);
};


#endif //CLFILER_CLUTILS_H

//
// Created by lenz on 11/7/21.
//

#ifndef CLFILER_APPSETTINGS_H
#define CLFILER_APPSETTINGS_H

#include <string>
#include <map>
#include <tbx/path.h>
#include <inipp/inipp.h>

class DirSettings {
private:
    std::string _ini_file_name = "<CLFiler$ChoicesDir>.dir-settings/ini";
    inipp::Ini<char> _ini;
public:
    std::basic_string<char> get_value(const tbx::Path& path, const std::string &key);
    int get_value(const tbx::Path& path, const std::string &key, int default_val);
    unsigned int get_value(const tbx::Path& path, const std::string &key, unsigned int default_val);
    void set_value(const tbx::Path& path, const std::string &key, int val);
    void set_value(const tbx::Path& path, const std::string &key, unsigned int val);
    void set_value(const tbx::Path& path, const std::string &key,  char *val);
    void set_value(const tbx::Path& path, const std::string &key, std::string &val);
    void del_value(const tbx::Path& path, const std::string &key);
    void save();
    void load();
};
extern DirSettings g_dir_settings;

class AppSettings {
protected:
    std::string _ini_file_name = "<CLFiler$ChoicesDir>.app-settings";
    inipp::Ini<char> _ini;
public:
    std::basic_string<char> get_value(const std::string &section, const std::string &key);
    int get_value(const std::string &section, const std::string &key, int default_val);
    void set_value(const std::string &section, const std::string &key, int val);
    void set_value(const std::string &section, const std::string &key,  char *val);
    void set_value(const std::string &section, const std::string &key, const std::string &val);
    void del_value(const std::string &section, const std::string &key);
    void save();
    bool load();
};

extern AppSettings g_app_settings;

class AppChoices : public AppSettings {
public:
    AppChoices() {
        _ini_file_name = "<CLFiler$ChoicesDir>.settings";
    }
    bool load(bool initial=false);
    void set_OpenNewWindow(int toolbars_visible);
};

extern AppChoices g_app_choices;

#endif //CLFILER_APPSETTINGS_H

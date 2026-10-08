//
// Created by lenz on 11/7/21.
//

#include <fstream>
#include <tbx/sprite.h>
#include <tbx/questionwindow.h>
#include "AppSettings.h"
#include <cloverleaf/Logger.h>
#include "../utils.h"
#include "../global.h"

DirSettings g_dir_settings;
AppSettings g_app_settings;
AppChoices g_app_choices;

void DirSettings::load() {
    if (is_file_exist(DirSettings::_ini_file_name)) {
        std::ifstream is(_ini_file_name);
        _ini.parse(is);
        is.close();
    }
}

void DirSettings::save() {
    std::ofstream os(DirSettings::_ini_file_name);
    _ini.generate(os);
    os.close();
}

std::basic_string<char> DirSettings::get_value(const tbx::Path& path, const std::string &key) {
    auto &sections = _ini.sections;
    tbx::Path current = path;
    tbx::Path parent = current.parent();
    do {
        auto sfound = sections.find(current.name());
        if (sfound != sections.end()) {
            auto keyfound = sfound->second.find(key);
            if (keyfound != sfound->second.end()) {
                return keyfound->second;
            }
        }
        current = parent;
        parent = current.parent();
    } while(parent.name() != current.name());

    return std::basic_string<char>();
}

int DirSettings::get_value(const tbx::Path& path, const std::string &key, int default_val) {
    std::basic_string<char> strval = get_value(path, key);
    int val = default_val;
    if (inipp::extract(strval, val)) {
        return val;
    }
    return default_val;
}

unsigned int DirSettings::get_value(const tbx::Path& path, const std::string &key, unsigned int default_val) {
    std::basic_string<char> strval = get_value(path, key);
    unsigned int val = default_val;
    if (inipp::extract(strval, val)) {
        return val;
    }
    return default_val;
}

void DirSettings::set_value(const tbx::Path& path, const std::string &key, std::string &val) {
    _ini.sections[path.name()][key] = val;
    save();
}

void DirSettings::set_value(const tbx::Path& path, const std::string &key,  char *val) {
    _ini.sections[path.name()][key] = std::string(val);
    save();
}

void DirSettings::set_value(const tbx::Path& path, const std::string &key, int val) {
    _ini.sections[path.name()][key] = to_string(val);
    save();
}

void DirSettings::set_value(const tbx::Path& path, const std::string &key, unsigned int val) {
    _ini.sections[path.name()][key] = to_hex_string(val);
    save();
}

void  DirSettings::del_value(const tbx::Path& path, const std::string &key) {
    auto &sections = _ini.sections;
    auto sfound = sections.find(path.name());
    if (sfound != sections.end()) {
        auto keyfound = sfound->second.find(key);
        if (keyfound != sfound->second.end()) {
            sfound->second.erase(keyfound);
            if (sfound->second.empty()) {
                sections.erase(sfound);
            }
            save();
        }
    }
}


bool AppSettings::load() {
    if (is_file_exist(AppSettings::_ini_file_name)) {
        std::ifstream is(_ini_file_name);
        _ini.clear();
        _ini.parse(is);
        is.close();
        return true;
    }
    return false;
}

void AppSettings::save() {
    std::ofstream os(AppSettings::_ini_file_name);
    _ini.generate(os);
    os.close();
}

std::basic_string<char> AppSettings::get_value(const std::string &section, const std::string &key) {
    auto &sections = _ini.sections;
    auto sfound = sections.find(section);
    if (sfound != sections.end()) {
        auto keyfound = sfound->second.find(key);
        if (keyfound != sfound->second.end()) {
            return keyfound->second;
        }
    }
    return std::basic_string<char>();
}

int AppSettings::get_value(const std::string &section, const std::string &key, int default_val) {
    std::basic_string<char> strval = get_value(section, key);
    int val = default_val;
    if (inipp::extract(strval, val)) {
        return val;
    }
    return default_val;
}

void AppSettings::set_value(const std::string &section, const std::string &key, const std::string &val) {
    _ini.sections[section][key] = val;
    save();
}

void AppSettings::set_value(const std::string &section, const std::string &key,  char *val) {
    _ini.sections[section][key] = std::string(val);
    save();
}

void AppSettings::set_value(const std::string &section, const std::string &key, int val) {
    _ini.sections[section][key] = to_string(val);
    save();
}

void  AppSettings::del_value(const std::string &section, const std::string &key) {
    auto &sections = _ini.sections;
    auto sfound = sections.find(section);
    if (sfound != sections.end()) {
        auto keyfound = sfound->second.find(key);
        if (keyfound != sfound->second.end()) {
            sfound->second.erase(keyfound);
            if (sfound->second.empty()) {
                sections.erase(sfound);
            }
            save();
        }
    }
}


class RestartCLFilerCommand : public tbx::Command {
    void execute() override {
        tbx::Application::instance()->start_wimp_task("<CLFiler$Dir>.!Run");
        tbx::Application::instance()->quit();
    }
};

bool AppChoices::load(bool initial) {
    if (AppSettings::load()) {
        int prev_TreeviewItemsShown = g_app_config.TreeviewItemsShown;
        g_app_config.OpenWindowClick = get_value("Mouse", "OpenWindowClick", 2);
        g_app_config.CopyMoveConfirm = get_value("Files", "CopyMoveConfirm", 1);
        g_app_config.OpenNewWindow = get_value("Windows", "OpenNewWindow", 3);
        g_app_config.ClipboardKeys = get_value("KeyShortcuts", "ClipboardKeys", 2);
        g_app_config.TreeviewItemsShown = get_value("TreeOptions", "TreeviewItemsShown", 1);
        Log_debug("AppChoices::load g_app_config.OpenNewWindow %d", g_app_config.OpenNewWindow);
        if (!initial && prev_TreeviewItemsShown != g_app_config.TreeviewItemsShown) {
            tbx::show_question("CLFiler needs to be restarted to apply the settings. Restart it now?",
                               "Restart CLFiler?",
                               new RestartCLFilerCommand(), nullptr, true);
        }
        return true;
    }
    return false;
}

void AppChoices::set_OpenNewWindow(int toolbars_visible) {
//    switch(toolbars_visible) {
//        case 3:
//            g_app_config.OpenNewWindow = 1;
//            break;
//        case 2:
//            g_app_config.OpenNewWindow = 4;
//            break;
//        case 1:
//            g_app_config.OpenNewWindow = 3;
//            break;
//        case 0:
//            g_app_config.OpenNewWindow = 2;
//            break;
//    }
    g_app_config.OpenNewWindow = toolbars_visible;
    set_value("Windows", "OpenNewWindow", g_app_config.OpenNewWindow);
}

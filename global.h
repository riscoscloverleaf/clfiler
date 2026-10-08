/*
 * global.h
 *
 * global application specific defines and structures
 */

#ifndef __global_h
#define __global_h

#include "oslib/os.h"
#include <tbx/hourglass.h>
#include <tbx/path.h>
#include <tbx/application.h>

#ifndef _DEBUG_
#define _DEBUG_ 1
#endif

#define SELECTED_OPERATION_NONE 0
#define SELECTED_OPERATION_CUT 1
#define SELECTED_OPERATION_COPY 2

#define MAX_SELECTED_FILES_LENGTH 50

class CLDirectoryWindow;

class AppConfig {
public:
    int OpenWindowClick = 2;
    int CopyMoveConfirm = 1;
    int OpenNewWindow = 3;
    int ClipboardKeys = 2;
    int ShowToolbar = 1;
    int ShowTreeview = 1;
    int TreeviewItemsShown = 1;
};

class AppState {
public:
    bool IKDEBUG = false;
    int filer_ver = 0;
    int confix_task_handle = 0;
    std::string app_version;
    std::string app_name;
    std::string app_date;

    std::vector<std::string> selected_files;
    std::string startup_dir;
    std::string selected_dir;
    int selected_operation = 0;
    std::vector<CLDirectoryWindow *> dir_windows;
    int view_mode = 0x10; // VIEW_MODE_TILE_BIG
    bool initialized = false;
    bool open_at_start = false;
    bool out_of_memory = false;
    bool needs_refresh_root = false;
    bool no_search_drives = false;

    bool is_dir_window_exists(CLDirectoryWindow* win);
};

/* global variables */
extern AppState g_app_state;
extern AppConfig g_app_config;
extern tbx::Application *g_my_app;

void g_hourglass_off();
void g_hourglass_on();
void g_hourglass_percentage(int percent);
bool is_enough_memory(int amount);
#endif

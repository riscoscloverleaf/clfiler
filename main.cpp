#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cstring>

#include "kernel.h"
#include "oslib/osmodule.h"
#include "oslib/osfscontrol.h"
#include "oslib/hourglass.h"
#include "oslib/taskmanager.h"
#include "oslib/sharefs.h"
#include "oslib/osfile.h"
#include "rufl.h"
#include <tbx/application.h>
#include <tbx/swixcheck.h>
#include <tbx/monotonictime.h>
#include <tbx/command.h>
#include <tbx/button.h>
#include <tbx/autocreate.h>
#include <tbx/timer.h>
#include <tbx/path.h>
#include <tbx/matchlifetime.h>
#include <tbx/uncaughthandler.h>
#include <tbx/oserror.h>
#include <tbx/messagewindow.h>
#include "cloverleaf/Logger.h"
#include "cloverleaf/CLImageCache.h"
#include "cloverleaf/CLImageMagickLoader.h"
#include "cloverleaf/CLFFmpegLoader.h"
#include "global.h"
#include "utils.h"
#include "ui/CLDirectoryWindow.h"
#include "ui/CLDirectoryMenu.h"
#include "ui/CLImageViewWindow.h"
#include "ui/CLSplash.h"
#include "ui/CLDisclaimer.h"
#include "ui/AppIconbar.h"
#include "ui/ProgInfo.h"
#include "model/AppSettings.h"

#define CMD_QUIT_APP            0x100
#define CMD_OPEN_MAIN_WINDOW    0x101
#define CMD_OPEN_CHOICES        0x102

AppState g_app_state;
AppConfig g_app_config;
tbx::Hourglass hourglass;
tbx::Application *g_my_app;

bool is_enough_memory(int amount) {
    void *dummy = malloc(amount);
    if (!dummy) {
        if (!g_app_state.out_of_memory) {
            g_app_state.out_of_memory = true;
            show_alert_error("You are running out of memory, some functions will be disabled");
        }
        return false;
    } else {
        free(dummy);
    }
    return true;
}

bool AppState::is_dir_window_exists(CLDirectoryWindow* win) {
    for(auto &w : dir_windows) {
        if (w == win) {
            return true;
        }
    }
    return false;
}


class MyUncaughtHandler : public tbx::UncaughtHandler {
public:
    void uncaught_exception(std::exception *e, int event_code) override {
        char errstr[1024];
        tbx::OsError *oserr = dynamic_cast<tbx::OsError*>(e);
        if (oserr) {
            snprintf(errstr, sizeof(errstr) - 1, "%s\nUncaught Exception: code %d (event: %d)", oserr->what(), oserr->number(), event_code);
        } else {
            std::bad_alloc *alloc_err = dynamic_cast<std::bad_alloc*>(e);
            if (alloc_err) {
                snprintf(errstr, sizeof(errstr) - 1, "Memory allocation failed\n");
            } else {
                if (e) {
                    snprintf(errstr, sizeof(errstr) - 1, "%s\nUncaught Exception: (event: %d)", e->what(), event_code);
                } else {
                    snprintf(errstr, sizeof(errstr) - 1, "Uncaught Exception! (event %d)", event_code);
                }
            }
        }
        Logger::error(errstr);
        tbx::show_message(errstr, "CLFiler error", "error");
    }
} my_uncaught_handler;


class AppShowMainUICommand : public tbx::Command {
public:
    void execute() override {
        if (g_app_state.initialized) {
            auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
            tbx::Path startup_path;
            if (!g_app_state.startup_dir.empty()) {
                startup_path = g_app_data_model.find_real_existing_dir_or_parent(g_app_state.startup_dir);
            } else {
                startup_path = g_app_data_model.find_real_existing_dir_or_parent(g_app_data_model.get_csd());
            }
            instance->init(startup_path);
        } else {
            g_app_state.open_at_start = true;
            Logger::info("App is not initialized yet, open MainUI request postponed");
        }
    }
} app_show_main_ui_cmd;

class AppQuitCommand : public tbx::Command {
public:
    void execute() override {
        if (g_app_state.initialized) {
            tbx::Application::instance()->quit();
        }
    }
} app_quit_cmd;

class AppChoicesCommand : public tbx::Command {
public:
    void execute() override {
        if (!g_app_state.confix_task_handle) {
            hourglass.on();
            tbx::Application::instance()->start_wimp_task("<ConfiX$Dir> -res <CLFiler$Dir> -file <Choices$Write>.CLFiler.settings -pos centre,centre -task CLFiler");
        }
    }
} app_choices_cmd;

class MyWimpUserMessageHander : public tbx::WimpUserMessageListener {
public:
    void user_message(tbx::WimpMessageEvent &event) override {
        tbx::WimpMessage msg = event.message();
        int task_handle = msg.sender_task_handle();
        char *task_name;
//        Log_debug("user_message %x task %x", msg.message_id(), task_handle);
        switch (msg.message_id()) {
            case 0x400C2: // Message_TaskInitialise
                task_name = msg.str(7);
                if (stricmp(task_name, "ConfiX") == 0) {
                    g_app_state.confix_task_handle = task_handle;
                    Log_debug("ConfiX started",1);
                }
                break;
            case 0x400C3: // Message_TaskCloseDown
                if (g_app_state.confix_task_handle == task_handle) {
                    g_app_state.confix_task_handle = 0;
                    Log_debug("ConfiX stopped",1);
                    g_app_choices.load();
                }
                break;
        }
    }
} my_wimp_user_message_handler;

class CLIconBar : public tbx::Loader {
    tbx::Iconbar _iconbar;
    static CLIconBar* _app_iconbar;
public:
    CLIconBar(tbx::Object object) {
        _iconbar = object;
        tbx::Iconbar(object).add_loader(this);
        _app_iconbar = this;
    }

    bool load_file(tbx::LoadEvent &event) override {
        if (g_app_state.initialized) {
            tbx::Path dropped_file = event.file_name();
            auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
            if (event.file_type() < 0x1000) {
                instance->init(dropped_file.parent());
            } else {
                instance->init(dropped_file);
            }
        } else {
            Logger::info("App is not initialized yet, ignore drop on the iconbar request");
        }
    }

    static void set_icon(char* icon) {
        if (_app_iconbar) {
            _app_iconbar->_iconbar.sprite(icon);
        }
    }

};
CLIconBar* CLIconBar::_app_iconbar = nullptr;

class AppInitCmd : public tbx::Command {
public:
    void execute() override {
        tbx::Path startup_path;
        g_my_app->remove_idle_command(this);

        g_hourglass_on();
        CLIconBar::set_icon("gr!clfiler");

        Log_debug("Starting app..",1);

        rufl_init();
        g_my_app->yield();

        g_dir_settings.load();
        g_app_settings.load();

        g_my_app->yield();
        g_app_data_model.refresh_roots();
        g_my_app->yield();

        if (g_app_data_model.get_root_items().empty()) {
            if (g_app_state.no_search_drives) {
                show_alert_error("No drives found! You are run CLFiler with -no-search-drives option but not specified the drives roots in the <CLFiler$ChoicesDir>.predefined-roots");
            } else {
                show_alert_error("No drives found!");
            }
            g_my_app->quit();
            return;
        }

        Log_debug("Loading state",1);
//        g_app_data_model.load_state();
//        g_my_app->yield();

        // find and refresh initial root
        Log_debug("Find and refresh initial root dir",1);
//        CLDirectoryItemData *initial_root_item = nullptr;
//        tbx::Path initial_dir = tbx::Path(g_app_settings.get_value("Last","Dir"));
//        if (!initial_dir.name().empty() && initial_dir.exists()) {
//            std::vector<std::string> parts;
//            size_t parts_size = split(initial_dir.name(), parts, '.');
//            initial_root_item = g_app_data_model.get_root_item(parts[0]);
//        }
//        if (!initial_root_item) {
//            initial_root_item = g_app_data_model.get_root_items()[0];
//        }

        if (!g_app_state.startup_dir.empty()) {
            startup_path = g_app_data_model.find_real_existing_dir_or_parent(g_app_state.startup_dir);
        } else {
            startup_path = g_app_data_model.find_real_existing_dir_or_parent(g_app_data_model.get_csd());
        }
        std::vector<std::string> parts;
        split(startup_path, parts, '.');
        CLDirectoryItemData* root_item = g_app_data_model.get_root_item(parts[0]);
        if (!root_item->is_cache_loaded()) {
            if (!g_app_data_model.load_diritem_cache(root_item)) {
                g_app_state.needs_refresh_root = true;
            }
        }

        g_app_data_model.get_or_refresh_path(startup_path);
        g_my_app->yield();

        Log_debug("Load timeline and favorites",1);
        //g_app_data_model.refresh_on_idle(initial_root_item, 1, CLREFRESH_RUN_ITEM_CALLBACKS | CLREFRESH_WITH_YIELD);
        g_timeline_model.load();
        g_app_data_model.get_favorites();
        g_my_app->yield();

        g_app_state.view_mode = g_app_settings.get_value("Last","view_mode", VIEW_MODE_TILE_BIG);

        Log_debug("App initialized. Run.",1);
        g_app_state.initialized = true;
        g_hourglass_off();
        CLIconBar::set_icon("!clfiler");

        int disclaimer_accepted = g_app_settings.get_value("Startup", "DisclaimerAccepted", 0);
        if (!disclaimer_accepted && is_file_exist("<CLFiler$Dir>.disclaimer")) {
            new CLDisclaimer();
        } else {
#if DONATE == 1
            CLSplash::open();
#endif
        }

        std::string msg;
        int warn_shown = g_app_settings.get_value("Startup", "ImageMagickInstallShown", 0);
        if (!CLImageMagickLoader::check_imagemagick_installed() && !warn_shown) {
            msg.append("ImageMagick not found. The 'Alias$convert' environment variable is not set. Some graphics file formats will be unsupported! ");
            g_app_settings.set_value("Startup", "ImageMagickInstallShown", 1);
        }
        warn_shown = g_app_settings.get_value("Startup", "FFMpegInstallShown", 0);
        if (!CLFFmpegLoader::check_ffmpeg_installed() && !warn_shown) {
            if (!msg.empty()) {
                msg.append("\n");
            }
            msg.append("FFMpeg not found. The 'Alias$ffmpeg' environment variable is not set. Video file format thumbnails will be unsupported!");
            g_app_settings.set_value("Startup", "FFMpegInstallShown", 1);
        }
        warn_shown = g_app_settings.get_value("Startup", "ArtWorksInstallShown", 0);
        if (!CLArtWorksImage::load_artworks_modules() && !warn_shown) {
            if (!msg.empty()) {
                msg.append("\n");
            }
            msg.append("Failed to load ArtWorks modules. The 'LoadArtWorksModules' command failed. AWViewer seems not installed. ArtWorks files will be unsupported!");
            g_app_settings.set_value("Startup", "ArtWorksInstallShown", 1);
        }
        if (!msg.empty()) {
            Log_debug("Show warning message: %s", msg.c_str());
            tbx::show_message(msg);
        }

        if (g_app_state.open_at_start) {
            auto instance = new CLDirectoryWindow(tbx::Window("WFilerDir"));
            instance->init(startup_path);
        }
    }
} app_init;


void app_cleanup() {
    xhourglass_off();
    rufl_quit();
    /* Uninstall fonts */
}

void g_hourglass_on() {
//    if (g_app_state.is_main_window_shown) {
        hourglass.on();
//    }
}

void g_hourglass_percentage(int percent) {
//    if (g_app_state.is_main_window_shown) {
        hourglass.percentage(percent);
//    }
}

void g_hourglass_off() {
    hourglass.off();
}

bool is_already_running() {
    int ctx = 0;
    char *dummy_end;
    taskmanager_task task;
    os_error *err;
    char errmsg[300];
    while(true) {
        err = xtaskmanager_enumerate_tasks(ctx, &task, sizeof(task), &ctx, &dummy_end);
        if (err) {
            sprintf(errmsg,"Error enumerate tasks, err:%s\n", err->errmess);
            fputs(errmsg, stderr);
            show_alert_error(errmsg);
            return true;
        }
        if (ctx < 0) break;
        if (strnicmp(task.name,"CLFiler", 7) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    int i;
    std::string open_dir_at_start;
    g_app_state.app_version = "1.75";
    g_app_state.app_date = "07-Sep-2024";
    g_app_state.app_name = "CLFiler";

    if (is_already_running()) {
        show_alert_error("!CLFiler already running. Quit !CLFiler before starting another.");
        exit(0);
    }

    atexit(app_cleanup);

    g_my_app = new tbx::Application("<CLFiler$Dir>");
    char tmstr[200];
    time_t t = time(NULL);
    strftime(tmstr, sizeof(tmstr), "%Y-%m-%d %H:%M:%S", localtime(&t));
    LogLevel loglevel = LOG_INFO;

    for(i = 0; i < argc; i++) {
        if (stricmp(argv[i], "-debug") == 0) {
            g_app_state.IKDEBUG = true;
            loglevel = LOG_DEBUG;
        }
    }

    Logger::init("<CLFiler$ChoicesDir>.log", loglevel);
    Logger::info("======= CLFiler %s (%s %s) started at %s =======", g_app_state.app_version.c_str(), __DATE__, __TIME__, tmstr);

    g_app_choices.load(true);

    for(i = 0; i < argc; i++) {
        if (stricmp(argv[i], "-open") == 0) {
            g_app_state.open_at_start = true;
        } else if (stricmp(argv[i], "-no-search-drives") == 0) {
            g_app_state.no_search_drives = true;
        } else if (stricmp(argv[i], "-dir") == 0) {
            if ((i < argc - 1) && (argv[i+1][0] != '-')) {
                i++;
                g_app_state.startup_dir = argv[i];
            }
        } else if (stricmp(argv[i], "-toolbars") == 0) {
            if ((i < argc - 1) && (argv[i+1][0] != '-')) {
                i++;
                g_app_config.OpenNewWindow = atoi(argv[i]);
                if (g_app_config.OpenNewWindow > 3 || g_app_config.OpenNewWindow < 0) {
                    g_app_config.OpenNewWindow = 3;
                }
            }
        }
    }
//    int block_size;
//    xosfile_read_block_size("<CLFiler$Dir>.!RunImage", &block_size);
//    Log_debug("Block size=%d", block_size);
    g_my_app->uncaught_handler(&my_uncaught_handler);
    g_app_state.filer_ver = module_version("Filer");
    g_app_data_model.load_autofiletypes();

//    hlp = (char*)(((char*)module) + help_offset);
//    Log_debug("module %p no=%d hlp=%d hlp=%p hs=%s", module, module_no, help_offset, *hlp, hlp);
    CLImageCache::init_images_cache(6000000, "<CLFiler$ChoicesDir>.thumbnails"); // max 20 Mb images cache
    CLDesktopFontStyle::find_desktop_font();
//    CLSpriteImage* img = new CLSpriteImage("HostFS::HostFS.1.2/png");
//    CLImageMagickLoader::save(img, "HostFS::HostFS.1.2/gif", FILE_TYPE_GIF);
//    CLSpriteImage* thumb = img->make_thumbnail(100,100);
//    thumb->save_as_sprite("HostFS::HostFS.1.3");
//    exit(0);
    CLUserSpriteImage::detect_mode();
    CLUserSpriteImage::read_vdu_info();

    flex_init("CLFiler", 0, -1);
    flex_set_budge(1);

    g_my_app->add_command(CMD_QUIT_APP, &app_quit_cmd);
    g_my_app->add_command(CMD_OPEN_MAIN_WINDOW, &app_show_main_ui_cmd);
    g_my_app->add_command(CMD_OPEN_CHOICES, &app_choices_cmd);

    tbx::MatchLifetime<ProgInfo> mlt_proginfo("ProgInfo");
    tbx::MatchLifetime<CLDirectoryMenu> mlt_dir_menu("MDirectory");
    tbx::MatchLifetime<CLDirectoryDisplayMenu> mlt_dir_disp_menu_listener("MDisplay");
    tbx::MatchLifetime<CLViewModeMenu> mlt_view_mode_menu("MViewMode");
    tbx::MatchLifetime<CLThumbnailsSubViewModeMenu> mlt_thumbnails_sub_view_mode_menu("MThumbView");
    tbx::MatchLifetime<CLActionMenu> mlt_action_menu("MAction");
    tbx::MatchLifetime<CLDirectoryFolderDisplayMenu> mlt_dir_folder_disp_menu_listener("MDispFolder");
    tbx::MatchLifetime<CLDirectorySelectionMenu> mlt_selection_menu("MSelection");
    tbx::MatchLifetime<CLDirectoryNewMenu> mlt_directory_new_menu("MNew");
//    tbx::MatchLifetime<CLFavoriteMenu> mlt_favorite_menu("MFavorite");
    tbx::MatchLifetime<CLRootsMenu> mlt_roots_menu("MRoots");
    tbx::MatchLifetime<CLRenameWin> mlt_rename_win("WRename");
    tbx::MatchLifetime<CLCreateDirWin> mlt_create_dir_win("WCreateDir");
    tbx::MatchLifetime<CLSetFileTypeWin> mlt_set_file_type_win("WFileType");
    tbx::MatchLifetime<CLCopyAsWin> mlt_copy_as_dir_win("WCopyAs");
    //tbx::MatchLifetime<CLBgColorMenu> mlt_bg_color_menu_listener("MDirColor");
    tbx::MatchLifetime<CLBgColorDialog> mlt_bg_color_dialog("DDirColor");
    tbx::MatchLifetime<CLSaveAsImage> mlt_save_as("CSaveAs");
    tbx::MatchLifetime<CLSaveAsImgMenu> mlt_save_as_img_mnu("MSaveAsImg");
    tbx::MatchLifetime<CLSaveAsPNG> mlt_csave_as_png("CSavePNG");
    tbx::MatchLifetime<CLSaveAsJPEG> mlt_csave_as_jpeg("CSaveJPEG");
    tbx::MatchLifetime<CLSaveAsWEBP> mlt_csave_as_webp("CSaveWEBP");
    tbx::MatchLifetime<CLImageInfoWindow> mlt_image_info("WImageInfo");
    tbx::MatchLifetime<CLMainImageViewMenu> mlt_main_image_view_menu("MImageView");
    tbx::MatchLifetime<CLExportAsCSV> mlt_export_as_csv("CSaveAsCsv");
    tbx::MatchLifetime<CLIconBar> mlt_iconbar("Iconbar");
    tbx::MatchLifetime<CLMExpCSV> mlt_mexp_csv("MExpCSV");
    tbx::MatchLifetime<CLAccessMenu> mlt_access_mnu("MAccess");
    tbx::MatchLifetime<CLAccessWin> mlt_access_win("WAccess");

    g_my_app->set_autocreate_listener("MIconbar", new tbx::AutoCreateClassOnce<MainMenu>());
    g_my_app->add_user_message_listener(0x400C2, &my_wimp_user_message_handler);
    g_my_app->add_user_message_listener(0x400C3, &my_wimp_user_message_handler);

    g_hourglass_off();

    g_my_app->add_idle_command(&app_init);
//    g_my_app->set_pre_poll_listener(&g_app_data_model.pre_poll_listener);
//    g_my_app->set_post_poll_listener(&g_app_data_model.post_poll_listener);
    g_my_app->run();

    Log_debug("Saving state", 1);
    g_timeline_model.save();
//    g_app_data_model.save_state();
    delete g_my_app;
    Logger::info("Exit");
}

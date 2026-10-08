#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <list>
#include <map>
#include <cstdio>
#include <cerrno>
#include <tbx/sprite.h>
#include <tbx/application.h>
#include <tbx/uri.h>
#include <tbx/path.h>
#include "oslib/os.h"
#include "oslib/squash.h"
#include "oslib/toolbox.h"
#include "oslib/window.h"
#include "oslib/wimp.h"
#include "oslib/Wimptypes.h"
#include "oslib/Event.h"
#include "oslib/messagetrans.h"
#include "oslib/osfile.h"
#include "oslib/jpeg.h"
#include "oslib/osspriteop.h"
#include "oslib/wimpspriteop.h"
#include "oslib/osmodule.h"
#include "oslib/osfscontrol.h"
#include "swis.h"

#include "utils.h"
#include "global.h"
#include "cloverleaf/CLException.h"
#include "cloverleaf/Logger.h"


time_t local_tzoffset = 0x7fffffff;

extern char *app_name;

static unsigned int contents_of(char *a)
{
   unsigned char *ad = (unsigned char *) a;
   unsigned int v = (((*(ad+3)) <<24) +
          ((*(ad+2)) <<16) +
          ((*(ad+1)) <<8 ) + *ad);

   return v;
}

#define SQUASH  0x48535153                /* SQSH */

/* to save space we allow compressed draw files */


bool decompress(char **file, int *size)
{
   if (contents_of(*file) == SQUASH) {
      /* it's squashed */

      char *wkspc=0;
      int ss,q;
      char *s;
      q = contents_of((*file)+4);        /* size */
      s = (char *)malloc(q);
      if (!s) return false;

      if (_swix(Squash_Decompress,_IN(0) | _IN(1)  | _OUT(0),
              8,(*size)-20,&ss)) return false;

      wkspc = (char *)malloc(ss);
      if ((!wkspc) || (_swix(Squash_Decompress,_IN(0) | _IN(1) |_IN(2) |
                            _IN(3) |_IN(4) |_IN(5),
                            4,wkspc,(*file)+20,(*size)-20,s,q))) {
          free(s);
          if (wkspc) free (wkspc);
          return false;
      }
      free(wkspc);
      free(*file);

      *file =s;
      *size = q;

   }
   return true;
}

int file_size(char *name)
{
  _kernel_swi_regs regs;
  regs.r[0] = 17;
  regs.r[1] = (int) name;

  if(_kernel_swi(OS_File,&regs,&regs)) regs.r[4] = 0;

  return regs.r[4];
}


_kernel_oserror * file_load(char *name, char *add)
{
  _kernel_swi_regs regs;
  regs.r[0] = 16;
  regs.r[1] = (int) name;
  regs.r[2] = (int) add;
  regs.r[3] = 0;

  return (_kernel_swi(OS_File,&regs,&regs));
}

void CLG(void)
{
   _swix(0x110,0);
}

void set_file_type(const char *file_name, int file_type)
{
   _swix(OS_File, _INR(0,2), 18, file_name, file_type);
}


static wimp_error_box_selection error_wimp_os_report(const char* message, wimp_error_box_flags type,
                                                     wimp_error_box_flags buttons, const char *custom_buttons)
{
    os_error e;
    e.errnum = 255;
    strncpy(e.errmess, message, os_ERROR_LIMIT);
    e.errmess[os_ERROR_LIMIT - 1] = '\0';
    wimp_error_box_selection        click;
    wimp_error_box_flags            flags;

    if (custom_buttons != NULL && *custom_buttons != '\0') {
        flags = wimp_ERROR_BOX_GIVEN_CATEGORY | buttons |
                (type << wimp_ERROR_BOX_CATEGORY_SHIFT);
        click = wimp_report_error_by_category(&e, flags, g_app_state.app_name.c_str(), "",
                                              NULL, custom_buttons);
    } else {
        flags = wimp_ERROR_BOX_GIVEN_CATEGORY | buttons |
                (type << wimp_ERROR_BOX_CATEGORY_SHIFT);
        click = wimp_report_error_by_category(&e, flags, g_app_state.app_name.c_str(), "",
                                              NULL, NULL);
    }

    return click;
}

void show_int_alert(int num)
{
  char numstr[10];
  sprintf(numstr, "%d", num);
    show_alert_error(numstr);
}

void show_alert_error(const char *message)
{
   os_error e;

   strcpy(e.errmess, message);
   wimp_report_error(&e,0,0);
}

void show_alert_info(const char *message)
{
    error_wimp_os_report(message, wimp_ERROR_BOX_CATEGORY_INFO, wimp_ERROR_BOX_OK_ICON, NULL);
}

int show_question(const char *message, const char* buttons, int flags)
{
    return error_wimp_os_report(message, wimp_ERROR_BOX_CATEGORY_QUESTION, flags, buttons);
}

std::string to_string(int num)
{
  char numstr[10];
  sprintf(numstr, "%d", num);
  return numstr;
}

std::string to_hex_string(unsigned int num)
{
    char numstr[14];
    sprintf(numstr, "%08x", num);
    return numstr;
}

std::string to_string(long num)
{
    char numstr[10];
    sprintf(numstr, "%ld", num);
    return numstr;
}

std::string to_string(int64_t num)
{
    char numstr[22];
    sprintf(numstr, "%lld", num);
    return numstr;
}

std::string to_string_2digit_num(int num)
{
  char numstr[2];
  sprintf(numstr, "%02d", num);
  return numstr;
}

int string_to_int(std::string s)
{
    int res = 0;
    sscanf(s.c_str(), "%d", &res);    
    return res;
}

unsigned int hex_string_to_int(std::string s)
{
    unsigned int res = 0xffffffff;
    sscanf(s.c_str(), "%x", &res);
    return res;
}

size_t split(const std::string &txt, std::vector<std::string> &strs, char ch)
{
    size_t pos = txt.find( ch );
    size_t initialPos = 0;
    strs.clear();
 
    // Decompose statement
    while( pos != std::string::npos ) {
        strs.push_back( txt.substr( initialPos, pos - initialPos ) );
        initialPos = pos + 1;
 
        pos = txt.find( ch, initialPos );
    }
 
    // Add the last one
    strs.push_back( txt.substr( initialPos, std::min( pos, txt.size() ) - initialPos + 1 ) );
 
    return strs.size();
}

std::string str_join(const std::vector<std::string>& vec, const char *delim) {
    std::string result;
    for(const auto &item : vec) {
        if (!result.empty()) {
            result.append(delim);
        }
        result.append(item);
    }
    return result;
}

time_t str_to_timet(const char* str)
{
    int tm_ms, tz_h, tz_m, items, tm_gmtoff = 0;
    char tz_sign;
    tm _t;
//    Log_debug("str_to_local_time %s", str);
    items = sscanf(str, "%4d-%2d-%2dT%2d:%2d:%2d.%dZ", &_t.tm_year, &_t.tm_mon, &_t.tm_mday, &_t.tm_hour, &_t.tm_min, &_t.tm_sec,
                   &tm_ms);
//    Log_debug("str_to_local_time1 %s _dir_entries=%d", str, _dir_entries);
    if (items != 7) {
        items = sscanf(str, "%4d-%2d-%2dT%2d:%2d:%2d.%d%c%2d:%2d", &_t.tm_year, &_t.tm_mon, &_t.tm_mday, &_t.tm_hour, &_t.tm_min, &_t.tm_sec,
                       &tm_ms, &tz_sign, &tz_h, &tz_m);
//        Log_debug("str_to_local_time2 %s _dir_entries=%d", str, _dir_entries);
        if (items != 10) {
            Logger::error("str_to_local_time() incorrect time: %s", str);
            return 0;
        }
        tm_gmtoff = 3600*tz_h + 60*tz_m;
        if (tz_sign == '-') {
            tm_gmtoff = -tm_gmtoff;
        }
    }
    _t.tm_mon -= 1;
    _t.tm_year -= 1900;
    _t.tm_gmtoff = 0;
    if (local_tzoffset == 0x7fffffff) {
        tm _t2;
        _t2.tm_year = 70;
        _t2.tm_mon = 0;
        _t2.tm_mday = 1;
        _t2.tm_hour = 0;
        _t2.tm_min = 0;
        _t2.tm_sec = 0;
        _t2.tm_gmtoff = 0;
        local_tzoffset = mktime(&_t2);
//        tm *tm1 = localtime(&local_tzoffset);
//        Log_debug("str_to_local_time2 localoffset %d %d %d",  tm1->tm_hour, tm1->tm_min, tm1->tm_gmtoff);
    }
    time_t result = mktime(&_t) - local_tzoffset - tm_gmtoff;
//    Log_debug("str_to_local_time2 mktime %s res=%d localoff=%d timezone=%d gmoff=%d", str, result, local_tzoffset, timezone, tm_gmtoff);
    return result;
}

void str_to_local_time(const char* str, const char* format, char* buf, int len)
{
    time_t t = str_to_timet(str);
    tm *lt = localtime(&t);
    strftime(buf, len, format, lt);
}

void timet_to_local_time(const time_t *t, const char* format, char* buf, int len)
{
    tm *lt = localtime(t);
    strftime(buf, len, format, lt);
}

std::string convert_time_full_to_HM(const time_t t) {
    char buf[200];
    timet_to_local_time(&t, "%H:%M", buf, sizeof(buf));
//    Log_debug("convert_time_full_to_HM=%s", buf);
    return std::string(buf);
}

std::string convert_time_full_to_DMY(const time_t t) {
    char buf[200];
    timet_to_local_time(&t, "%d.%m.%Y", buf, sizeof(buf));
    return std::string(buf);
}

std::string convert_time_full_to_string(const time_t t, const char *format) {
    char buf[200];
    timet_to_local_time(&t, format, buf, sizeof(buf));
    return std::string(buf);
}

static std::string _file_sizes[] = {"B", "KB", "MB", "GB", "TB" };
std::string file_size_to_displayed_string(int64_t num) {
    int64_t len = num;
    int remainder;
    int order = 0;
    remainder = (num % 1000) / 100;
    while (len >= 10000 && order < 5) {
        order++;
        len = len/1024;
    }

    char buffer[50];
    if (remainder == 0 || num < 10000) {
        sprintf(buffer, "%5lld%s", len, _file_sizes[order].c_str());
    } else {
        sprintf(buffer, "%5lld.%d%s", len, remainder, _file_sizes[order].c_str());
    }
    return std::string(buffer);
}

void file_size_to_unit_size_string(int64_t num, std::string& unit, std::string& fsize) {
    int64_t fsz = num;
    int order = 0;
    while (fsz >= 10000 && order < 5) {
        order++;
        fsz = fsz/1024;
    }
    unit.assign(_file_sizes[order]);
    fsize.assign(to_string(fsz));
}

bool is_file_exist (const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0); 
}

bool is_directory_exist (const std::string& name) {
  struct stat buffer;   
  int res = stat (name.c_str(), &buffer);
  return (res == 0 && (buffer.st_mode & S_IFDIR)); 
}

long get_filesize(const char *filename) {
  struct stat buffer;   
  int res = stat (filename, &buffer);
  return (res == 0 ? buffer.st_size : -1);     
}

std::string file_basename(const std::string& file_full_path) {
    return std::string(basename(
            str_replace_all(str_replace_all(str_replace_all(file_full_path, "/", "!@!"), ".", "/"), "!@!", ".").c_str()));
}

void copy_file(const char *from, const char *to)
{
    std::FILE *fpfrom = std::fopen(from, "rb");
    if (!fpfrom) {
        throw_exception(std::string("Error open/create file ") + from + " " + strerror(errno));
    }
    std::FILE *fpto = std::fopen(to, "w+b");
    if (!fpto) {
        throw_exception(std::string("Error open/create file ") + to + " " + strerror(errno));
    }
    int readed;
    char buf[16384];
    if (fpfrom && fpto) {
        do {
            readed = std::fread(buf, 1, 16384, fpfrom);
            std::fwrite(buf, 1, readed, fpto);
        } while (readed == 16384);
        fclose(fpfrom);
        fclose(fpto);
    }
}

// string
char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

std::string& ltrim(std::string& str, const std::string& chars)
{
    str.erase(0, str.find_first_not_of(chars));
    return str;
}

std::string& rtrim(std::string& str, const std::string& chars)
{
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

std::string& trim(std::string& str, const std::string& chars)
{
    return ltrim(rtrim(str, chars), chars);
}

std::string trim_prefix(const std::string& s, const std::string& prefix)
{
    return s.find(prefix) == 0 ? s.substr(prefix.length()) : s;
}

std::string str_replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

void open_browser_url(const std::string& url) {
    Logger::info("Open URL: %s", url.c_str());
    if (!tbx::URI::dispatch(url)) {
        tbx::Application *app = tbx::Application::instance();
        if (url.substr(0, 5) == "https") {
            app->start_wimp_task("URLOpen_https "+ url);
        } else {
            app->start_wimp_task("URLOpen_http "+ url);
        }
    }
}

bool can_run_file_type(int type) {
    char varname[200];
    os_var_type out;
    int used;
    sprintf(varname, "Alias$@RunType_%X", type);
    os_read_var_val_size(varname, 0, 0, &used, &out);
//    Log_debug("can_run_file_type var=%s type=%X user=%d", varname, type, used);
    return (used != 0);
}

static std::map<int,std::string> _file_types;
static char *_unknown_file_type_text = "Unknown";

std::string get_file_type_text(int file_type) {
/*
    char c1[4];
    char c2[4];
    xosfscontrol_read_file_type(file_type, reinterpret_cast<bits *>(&c1), reinterpret_cast<bits *>(&c2));
    std::string out;
    out.append(c1);
    out.append(c2);
    return out;
*/
    if (file_type == 0) {
        return std::string(_unknown_file_type_text);
    }

    if (file_type == 0x1000) {
        return "Directory";
    }

    auto found = _file_types.find(file_type);
    if (found != _file_types.end()) {
        return found->second;
    }

    char *file_type_txt;
    char env_var_name[200];
    std::string file_type_str;
    sprintf(env_var_name, "File$Type_%03x", file_type);
    file_type_txt = getenv(env_var_name);
    if (!file_type_txt) {
        char buf[100];
        sprintf(buf, "&%03x", file_type);
        file_type_str = std::string(buf);
    } else {
        file_type_str = std::string(file_type_txt);
    }
    _file_types[file_type] = file_type_str;
    return file_type_str;
}

tbx::WimpSprite get_sprite_for_file(const tbx::Path& path) {
    int file_type;

    if (path.image_file()) {
        file_type = path.raw_file_type();
    } else {
        file_type = path.file_type();
    }

    return tbx::WimpSprite(file_type, path.leaf_name());
}

int module_version(char* module_name) {
    int module_no, instance_no, help_offset;
    int* module;
    void* workspace;
    char *postfix, *hlp, *ver_start, *ver_end, ver[16];
    os_error *err = xosmodule_lookup(module_name, &module_no, &instance_no,
                                     reinterpret_cast<byte **>(&module),
                                     &workspace, &postfix);
    if (err) {
        Log_debug("module_version error, mod:%s  err:%s", module_name, err->errmess);
        return 0;
    }
    help_offset = *(module + 5);
    hlp = ((char*)module) + help_offset;
//    Log_debug("module %p no=%d hlp=%d hp=%p hs=%s", module, module_no, help_offset, hlp, hlp);
    ver_start = strrchr(hlp, '\t') + 1;
    ver_end = strchr(ver_start,' ');
    memset(ver, 0, sizeof(ver));
    memcpy(ver, ver_start, ver_end - ver_start);
//    Log_debug("ver = %s", ver);

    std::string v = ver;
    std::vector<std::string> parts;
    split(ver, parts, '.');
    int multiplier = 1;
    int version = 0, p_int;
    for(auto it = parts.end() - 1; it >= parts.begin(); it--) {
        p_int = atoi(it->c_str()) * multiplier;
        version += p_int;
//        Log_debug("ver=%d p_int=%d p=%s m=%d", version, p_int, it->c_str(), multiplier);
        multiplier = multiplier * 100;
    }
//    Log_debug("ver res=%d", version);
    return version;
}

void call_wimp_poll() {
    int _poll_block[64];
    _kernel_swi_regs regs;
    regs.r[1] = reinterpret_cast<int>(&_poll_block);
//    regs.r[0] = 0b11100011100101110010; // poll mask (mask everyting except null events)
    regs.r[0] = 0; // poll mask
    _kernel_swi(Wimp_Poll, &regs, &regs);
}

bool is_shift_pressed() {
    // read shift key state
    _kernel_swi_regs regs;
    regs.r[0] = 121;
    regs.r[1] = 0x80;
    _kernel_swi(0x6, &regs, &regs); // 0x6 is OS_Byte
    return regs.r[1] == 0xff;
}

bool is_alt_pressed() {
    // read shift key state
    _kernel_swi_regs regs;
    regs.r[0] = 121;
    regs.r[1] = 0x2 ^ 0x80;
    _kernel_swi(0x6, &regs, &regs); // 0x6 is OS_Byte
    return regs.r[1] == 0xff;
}

bool is_ctrl_pressed() {
    // read shift key state
    _kernel_swi_regs regs;
    regs.r[0] = 121;
    regs.r[1] = 0x1 ^ 0x80;
    _kernel_swi(0x6, &regs, &regs); // 0x6 is OS_Byte
    return regs.r[1] == 0xff;
}
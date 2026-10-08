//
// Created by slenz on 6/1/24.
//

#ifndef CLFILER_DRIVESDETECT_H
#define CLFILER_DRIVESDETECT_H
#include <vector>
#include <string>


class DrivesDetect {
public:
    static void detect_by_canonicalize(std::vector<std::string>& found_drives, char* fs, int start_from = 0, int num_drives=8);
    static void detect_lm98_drives(std::vector<std::string>& found_drives);
    static void detect_filecore_drives(std::vector<std::string>& found_drives, char* drives_swi, char* fs);
    static void detect_sharefs_drives(std::vector<std::string>& found_drives);
    static void detect_cdfs_drives(std::vector<std::string>& found_drives);
    static void detect_removable_drives(std::vector<std::string>& found_drives);
    static void detect_all_drives(std::vector<std::string>& found_drives);
};


#endif //CLFILER_DRIVESDETECT_H

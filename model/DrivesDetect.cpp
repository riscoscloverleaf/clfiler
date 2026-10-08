//
// Created by slenz on 6/1/24.
//

#include <cstring>
#include "DrivesDetect.h"
#include "oslib/adfs.h"
#include "oslib/cdfs.h"
#include "oslib/scsifs.h"
#include "oslib/osfscontrol.h"
#include "oslib/sharefs.h"
#include "oslib/os.h"
#include <kernel.h>
#include <stdio.h>
#include <tbx/path.h>
#include <cloverleaf/Logger.h>

void DrivesDetect::detect_by_canonicalize(std::vector<std::string>& found_drives, char* fs, int start_from, int num_drives) {
    char drive[255], canonical_drive[255];
    int spare;
    os_error* err;
    for(int i = start_from; i < start_from + num_drives; i++) {
        sprintf(drive,"%s::%d.$", fs, i);
        err = xosfscontrol_canonicalise_path(drive, canonical_drive, 0, 0, sizeof(canonical_drive) - 1, &spare);
        if (err) {
            Log_error("DrivesDetect::detect_by_canonicalize error canonicalise_path %s: error: (%d) %s", drive, err->errnum, err->errmess);
            break;
        } else {
            if (stricmp(drive, canonical_drive) != 0) {
                Logger::info("DrivesDetect::detect_by_canonicalize %s found %s", drive, canonical_drive);
                found_drives.emplace_back(canonical_drive);
            } else {
                Logger::info("DrivesDetect::detect_by_canonicalize %s found same %s, stop searching", drive, canonical_drive);
                break;
            }
        }
    }
}

void DrivesDetect::detect_filecore_drives(std::vector<std::string>& found_drives, char* drives_swi, char* fs) {
    os_error* err;
    _kernel_oserror *kerr;
    char drive[255], canonical_drive[255];
    int i, spare, swi, flops, hards;

    err = xos_swi_number_from_string(drives_swi, &swi);
    if (err) {
        Logger::info("DrivesDetect::detect_filecore_drives SWI %s not found: %s", drives_swi, err->errmess);
        return;
    }

    _kernel_swi_regs regs;
    kerr = _kernel_swi(swi, &regs, &regs);
    if (kerr) {
        Logger::info("DrivesDetect::detect_filecore_drives %s error %s", drives_swi, kerr->errmess);
        return;
    }
    flops = regs.r[1];
    hards = regs.r[2];

    detect_by_canonicalize(found_drives, fs, 0, flops);
    detect_by_canonicalize(found_drives, fs, 4, hards);
}

void DrivesDetect::detect_lm98_drives(std::vector<std::string>& found_drives) {
    tbx::Path lm98discs = tbx::Path("LM98:Discs");
    std::string drive;
    for(tbx::PathInfo::Iterator it = tbx::PathInfo::begin(lm98discs); it != tbx::PathInfo::end(); ++it) {
        drive = "LanMan98::"+it->name();
        if (tbx::Path(drive).exists()) {
            Logger::info("DrivesDetect::detect_lm98_drives found %s", drive.c_str());
            found_drives.emplace_back(drive);
        } else {
            Logger::error("DrivesDetect::detect_lm98_drives not found %s", drive.c_str());
        }
    }
}

void DrivesDetect::detect_cdfs_drives(std::vector<std::string>& found_drives) {
    int num_drives;
    os_error* err;
    err = xcdfs_get_number_of_drives(&num_drives);
    if (err) {
        Logger::info("DrivesDetect::detect_cdfs_drives error: %s", err->errmess);
        return;
    }
    if (num_drives) {
        detect_by_canonicalize(found_drives, "CDFS", 0, num_drives);
    } else {
        Logger::info("DrivesDetect::detect_cdfs_drives no CD drives");
    }
}

void DrivesDetect::detect_sharefs_drives(std::vector<std::string>& found_drives) {
    std::vector<std::string> my_own_shares;
    int sharefs_context = 0;
    char *sharefs_objname, *sharefs_dirname;
    sharefs_attr sharefs_attrs;
    os_error *err;

    Logger::info("DrivesDetect Searching for own ShareFS shares");
    while (true) {
        err = xsharefs_enumerate_shares(0xff,
                                        sharefs_context, 0, &sharefs_objname, &sharefs_dirname, &sharefs_attrs,
                                        &sharefs_context);
        Log_debug("DrivesDetect xsharefs_enumerate_shares [%s] [%s]", sharefs_objname, sharefs_dirname);
        if (err) {
            Logger::error("xsharefs_enumerate_shares err %d (%s)", err->errnum, err->errmess);
            break;
        }
        if (sharefs_context == -1) {
            break;
        }
        if (sharefs_objname) {
            my_own_shares.push_back(std::string(sharefs_objname));
            Logger::info("DrivesDetect xsharefs_enumerate_shares my share obj:%s attrs:%x", sharefs_objname, sharefs_attrs);
        } else {
            Logger::error("DrivesDetect xsharefs_enumerate_shares my share obj:null");
            break;
        }
    }

    Logger::info("DrivesDetect Searching for connected ShareFS shares");
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
                Logger::info("DrivesDetect Found ShareFS root:%s attr:%x", it->name().c_str(), it->attributes());
                //tbx::Application::instance()->os_cli("Filer_Run Resources:$.Discs." + it->name());
                found_drives.emplace_back("Share::" + it->name());
            } else {
                Logger::info("DrivesDetect Found own ShareFS root:%s attr:%x (skipped)", it->name().c_str(),
                             it->attributes());
            }
        }
    }
}

void DrivesDetect::detect_all_drives(std::vector<std::string>& found_drives) {
    if (tbx::Path("HostFS::HostFS").exists()) {
        found_drives.emplace_back("HostFS::HostFS");
    }
    detect_filecore_drives(found_drives, "XSDFS_Drives", "SDFS");
    detect_by_canonicalize(found_drives, "Fat32fs",0 , 8);
    detect_by_canonicalize(found_drives, "Fat32fs",8 , 8);
    detect_by_canonicalize(found_drives, "Fat32fs",16 , 8);
//    detect_by_canonicalize(found_drives, "Fat32fs",24 , 8);
//    detect_by_canonicalize(found_drives, "Fat32fs",32 , 8);
    detect_filecore_drives(found_drives, "XADFS_Dives", "ADFS");
    detect_filecore_drives(found_drives, "XSCSIFS_Dives", "SCSIFS");
    detect_filecore_drives(found_drives, "XIDEFS_Dives", "IDEFS");
    detect_filecore_drives(found_drives, "XRamFS_Drives", "RAM");
    detect_by_canonicalize(found_drives, "CDFS");
    detect_lm98_drives(found_drives);
    detect_sharefs_drives(found_drives);
}

void DrivesDetect::detect_removable_drives(std::vector<std::string>& found_drives) {
    detect_filecore_drives(found_drives, "XSDFS_Drives", "SDFS");
    detect_by_canonicalize(found_drives, "Fat32fs",0 , 8);
    detect_by_canonicalize(found_drives, "Fat32fs",8 , 8);
    detect_by_canonicalize(found_drives, "Fat32fs",16 , 8);
//    detect_by_canonicalize(found_drives, "Fat32fs",24 , 8);
//    detect_by_canonicalize(found_drives, "Fat32fs",32 , 8);
    detect_cdfs_drives(found_drives);
}

//
// Created by lenz on 12/6/21.
//

#include "CopyMoveConfirm.h"

CopyMoveConfirm::CopyMoveConfirm(const tbx::Path &srcDir, const std::vector<std::string> &srcFiles, const tbx::Path &dstDir) :
    _win("WCopyMove"),
    _src_dir(srcDir),
    _src_files(srcFiles),
    _dst_dir(dstDir)
{
    tbx::ActionButton(_win.gadget(2)).add_selected_listener(this);
    tbx::ActionButton(_win.gadget(3)).add_selected_listener(this);
    tbx::ActionButton(_win.gadget(4)).add_selected_listener(this);
    _win.add_has_been_hidden_listener(this);
    _win.add_key_listener(this);
    _win.show_centered();
    if (srcFiles.size() == 1) {
        std::vector<std::string> src_files;
        split(srcFiles[0], src_files, ' ');
        if (src_files.size() == 1) {
            tbx::Path f = srcDir.child(src_files[0]);

            if (f.exists()) {
                std::string ft_text = get_file_type_text(f.file_type());
                if (ft_text != "Unknown") {
                    _win.title("Copy or move: " + f.name() + " ("+ft_text+")?");
                } else {
                    _win.title("Copy or move: " + f.name() + "?");
                }
            }
        } else {
            std::string files_title = str_replace_all(srcFiles[0], " ", ", ");
            if (files_title.size() > 25) {
                _win.title("Copy or move: " + files_title.substr(0, 25) + "... ?");
            } else {
                _win.title("Copy or move: " + files_title + "?");
            }
        }
    }
    tbx::Button(_win.gadget(5)).value(dstDir.name() + " ?");

    Log_debug("CopyMoveConfirm::CopyMoveConfirm src:%s dst:%s", _src_dir.name().c_str(), _dst_dir.name().c_str());
}

CopyMoveConfirm::~CopyMoveConfirm() {
    _win.delete_object();
    Log_debug("CopyMoveConfirm::~CopyMoveConfirm()",1);
}

void CopyMoveConfirm::button_selected(tbx::ButtonSelectedEvent &event) {
    Log_debug("CopyMoveConfirm::button_selected %d", event.id_block().self_component().id());
    switch(event.id_block().self_component().id()) {
        case 3:
            copy_or_move(OPT_COPY);
            break;
        case 4:
            copy_or_move(OPT_MOVE);
            break;
    }
}

void CopyMoveConfirm::has_been_hidden(const tbx::EventInfo &event_info) {
    delete this;
}

void CopyMoveConfirm::copy_or_move(int opt) {
    const std::string &src_dir = _src_dir.name();
    const std::string &dst_dir = _dst_dir.name();
    if (!_src_files.empty() && src_dir != dst_dir) {
        int src_dot_pos = src_dir.find('.');
        int dst_dot_pos = dst_dir.find('.');
        if (src_dot_pos == std::string::npos) {
            Logger::error("CopyMoveConfirm::copy_or_move (invalid source) [%s]", src_dir.c_str());
            return;
        }
        if (dst_dot_pos == std::string::npos) {
            Logger::error("CopyMoveConfirm::copy_or_move (invalid destination) [%s]", dst_dir.c_str());
            return;
        }
        CLFilerAction *act = new CLFilerAction(_src_dir, _src_files);
        if (opt == OPT_COPY) {
            act->fs_copy(_dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
            Log_debug("CopyMoveConfirm::copy_or_move (copy) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
        } else {
            if (dst_dir.substr(0, dst_dot_pos) == src_dir.substr(0, src_dot_pos)) {
                act->fs_rename(_dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                Log_debug("CopyMoveConfirm::copy_or_move (rename) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
            } else {
                act->fs_move(_dst_dir, tbx::FilerAction::VERBOSE | tbx::FilerAction::RECURSE);
                Log_debug("CopyMoveConfirm::copy_or_move (move) from [%s] to [%s]", src_dir.c_str(), dst_dir.c_str());
            }
        }
    }

}

void CopyMoveConfirm::key(tbx::KeyEvent &event) {
    switch(event.key()) {
        case 'c':
        case 'C':
            copy_or_move(OPT_COPY);
            _win.hide();
            break;
        case 'm':
        case 'M':
            copy_or_move(OPT_MOVE);
            _win.hide();
            break;
        case wimp_KEY_ESCAPE:
            _win.hide();
            break;
    }
}

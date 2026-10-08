//
// Created by lenz on 12/6/21.
//

#ifndef CLFILER_COPYMOVECONFIRM_H
#define CLFILER_COPYMOVECONFIRM_H

#include <vector>
#include <tbx/buttonselectedlistener.h>
#include "CLDirectoryWindow.h"
#include "../model/AppDataModel.h"

class CopyMoveConfirm :
    public tbx::HasBeenHiddenListener,
    public tbx::KeyListener,
    public tbx::ButtonSelectedListener
{
private:
    tbx::Window _win;
    tbx::Path _src_dir;
    std::vector<std::string> _src_files;
    tbx::Path _dst_dir;
public:
    CopyMoveConfirm(const tbx::Path &srcDir, const std::vector<std::string> &srcFiles,
                    const tbx::Path &dstDir);

    virtual ~CopyMoveConfirm();
    enum CopyOrMoveOptions {
        OPT_COPY,
        OPT_MOVE
    };

    void copy_or_move(int opt);

    void button_selected(tbx::ButtonSelectedEvent &event) override;
    void key(tbx::KeyEvent &event) override;
    void has_been_hidden(const tbx::EventInfo &event_info) override;
};


#endif //CLFILER_COPYMOVECONFIRM_H

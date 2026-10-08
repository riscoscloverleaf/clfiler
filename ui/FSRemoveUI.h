//
// Created by slenz on 09.02.2023.
//

#ifndef CLFILER_FSREMOVEUI_H
#define CLFILER_FSREMOVEUI_H

#include <vector>
#include <string>
#include <tbx/command.h>

class FSRemoveConfirmCommand : public tbx::Command {
    unsigned long _item_id;
    int _win_handle;
    std::vector <std::string> _selected_items;
public:
    FSRemoveConfirmCommand(unsigned long itemId, const std::vector <std::string> &selectedItems, int whandle) :
            _item_id(itemId),
            _selected_items(selectedItems),
            _win_handle(whandle)
    {};

    void execute() override;
};

class SetFocusBackCommand : public tbx::Command {
    int _win_handle;
public:
    SetFocusBackCommand(int whandle) : _win_handle(whandle) {};

    void execute() override;
};


#endif //CLFILER_FSREMOVEUI_H

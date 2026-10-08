//
// Created by slenz on 25.05.2022.
//

#ifndef CLFILER_CLASKDONATE_H
#define CLFILER_CLASKDONATE_H

#include <tbx/window.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/buttonselectedlistener.h>
#undef self_component

class CLAskDonate  : public tbx::HasBeenHiddenListener,
                     public tbx::MouseClickListener,
                     public tbx::ButtonSelectedListener {
private:
    static CLAskDonate *_instance;
    tbx::Window _win;
public:
    CLAskDonate();
    virtual ~CLAskDonate();

    static void open();

    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
    void mouse_click(tbx::MouseClickEvent &event) override;
};

#endif //CLFILER_CLASKDONATE_H

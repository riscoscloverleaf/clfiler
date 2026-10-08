//
// Created by lenz on 12/13/21.
//

#ifndef CLFILER_CLSPLASH_H
#define CLFILER_CLSPLASH_H

#include <tbx/proginfo.h>
#include <tbx/window.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/buttonselectedlistener.h>
#include <tbx/timer.h>
#undef self_component

class CLSplash : public tbx::HasBeenHiddenListener,
        public tbx::Timer,
        public tbx::MouseClickListener,
        public tbx::ButtonSelectedListener {
private:
    static CLSplash *_instance;
    tbx::Window _win;
public:
    CLSplash();
    virtual ~CLSplash();

    static void open();

    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
    void mouse_click(tbx::MouseClickEvent &event) override;
    void timer(unsigned int elapsed) override;
};


#endif //CLFILER_CLSPLASH_H

//
// Created by lenz on 4/28/22.
//

#ifndef CLFILER_CLDISCLAIMER_H
#define CLFILER_CLDISCLAIMER_H

#include <tbx/window.h>
#include <tbx/hasbeenhiddenlistener.h>
#include <tbx/buttonselectedlistener.h>
#include <tbx/abouttobeshownlistener.h>

class CLDisclaimer : public tbx::HasBeenHiddenListener,
                     public tbx::ButtonSelectedListener
{
private:
    tbx::Window _win;
    bool _accepted = false;
public:
    CLDisclaimer();
    virtual ~CLDisclaimer();

    void has_been_hidden(const tbx::EventInfo &event_info) override;
    void button_selected(tbx::ButtonSelectedEvent &event) override;
};


#endif //CLFILER_CLDISCLAIMER_H

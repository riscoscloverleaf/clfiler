//
// Created by lenz on 10/13/21.
//

#ifndef CLFILER_PROGINFO_H
#define CLFILER_PROGINFO_H

#include <tbx/proginfo.h>
#include <tbx/window.h>
#include <tbx/abouttobeshownlistener.h>

class ProgInfo : public tbx::AboutToBeShownListener {
private:
    tbx::ProgInfo _pi;
public:
    ProgInfo(tbx::Object& obj);

    void about_to_be_shown(tbx::AboutToBeShownEvent &event) override;
};


#endif //CLFILER_PROGINFO_H

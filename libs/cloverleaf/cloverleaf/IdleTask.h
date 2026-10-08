//
// Created by lenz on 2/15/20.
//

#ifndef CL_IDLETASK_H
#define CL_IDLETASK_H
#include <list>
#include <functional>
#include <tbx/application.h>
#include <tbx/command.h>

class IdleTask : tbx::Command {
private:
    std::list<std::function<void()>> _on_next_idle_run_list;
public:
    void execute() override;
    void run_at_next_idle(std::function<void()> fun);
};

extern IdleTask g_idle_task;

#endif //CL_IDLETASK_H

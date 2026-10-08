//
// Created by lenz on 2/15/20.
//
#include "IdleTask.h"

void IdleTask::run_at_next_idle(std::function<void()> fun) {
    if (_on_next_idle_run_list.empty()) {
        tbx::Application::instance()->add_idle_command(this);
    }
    _on_next_idle_run_list.push_back(fun);
}

void IdleTask::execute() {
    if (_on_next_idle_run_list.empty()) {
        tbx::Application::instance()->remove_idle_command(this);
        return;
    }
    int size = _on_next_idle_run_list.size();
    while(size--)  {
        auto task = _on_next_idle_run_list.front();
        task();
        _on_next_idle_run_list.pop_front();
    }
}

IdleTask g_idle_task;
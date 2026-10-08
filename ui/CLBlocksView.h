//
// Created by slenz on 01.07.2022.
//

#ifndef CLFILER_CLBLOCKSVIEW_H
#define CLFILER_CLBLOCKSVIEW_H

#include <iostream>
#include <vector>
#include <numeric>
#include <memory>
#include <assert.h>
#include <utility>
#include <algorithm>
#include <tbx/view/itemview.h>
#include <tbx/openwindowlistener.h>
#include "cloverleaf/CLGraphics.h"
#include "../model/AppDataModel.h"
#include "TreeMap.hpp"

class CLDirectoryWindow;

class CLBlocksView : public tbx::view::ItemView, tbx::OpenWindowListener
{
private:
    CLFontStyle *_name_style = nullptr;
    CLFontStyle *_size_style = nullptr;
protected:
    CLDirectoryWindow *_dir_win;
    std::vector<std::pair<Rectangle,CLDirectoryItemData*>> squarified_data;
    bool recalc_layout(const tbx::BBox &work_area);

public:
    CLBlocksView(CLDirectoryWindow *dir_win);
    virtual ~CLBlocksView();

    virtual void auto_size(bool on);
    // Window events used
    virtual void redraw(const tbx::RedrawEvent &event);
    virtual void open_window(tbx::OpenWindowEvent &event);

    void margin(const tbx::Margin &margin) override;

    virtual void update_window_extent();
    virtual void refresh();

    virtual void inserted(unsigned int where, unsigned int how_many);
    virtual void removed(unsigned int where, unsigned int how_many);
    virtual void changed(unsigned int where, unsigned int how_many);
    virtual void cleared();

    virtual unsigned int insert_index(const tbx::Point &scr_pt) const;
    virtual unsigned int screen_index(const tbx::Point &scr_pt) const;
    virtual unsigned int hit_test(const tbx::Point &scr_pt) const;

    virtual void get_bounds(tbx::BBox &bounds, unsigned int index) const;
    /**
     * Get bounding box of the range of indices in work area coordinates
     */
    virtual void get_bounds(tbx::BBox &bounds, unsigned int first, unsigned int last) const;
};

#endif //CLFILER_CLBLOCKSVIEW_H

// Copyright (c) 2019 chendi
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef CLFILER_TREEMAP_H
#define CLFILER_TREEMAP_H

#include <iostream>
#include <vector>
#include <numeric>
#include <memory>
#include <assert.h>
#include <utility>
#include <algorithm>
#include "../model/AppDataModel.h"
#define MakePair std::make_pair

template <typename K, typename V>
using Pair = std::pair<K, V>;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

float clamp( const float v, const float lo, const float hi );

enum class Orientation
{
    Vertical,
    Horizontal
};

struct Rectangle
{
    int width;
    int height;

    int x = 0;
    int y = 0;
    int item_idx = -1;

    Orientation get_orientation() const
    {
        return width < height
                   ? Orientation::Vertical
                   : Orientation::Horizontal;
    }

    float get_aspect_ratio() const
    {
        return width > height ? float(width) / float(height) : float(height) / float(width);
    }

    Rectangle operator-(const Rectangle &other) const
    {
        assert(other.width <= width);
        assert(other.height <= height);
        assert((other.width == width) || (other.height == height));
        if (other.width == width) {
            Rectangle r;
            r.width = width;
            r.height = height - other.height;
            r.x = x;
            r.y = y + other.height;
            return r;
        } else {
            Rectangle r;
            r.width = width - other.width;
            r.height = height;
            r.x = x + other.width;
            r.y = y;
            return r;
        }
    }

    Rectangle mult(float ratio) const {
        ratio = clamp(ratio, 0.0, 1.0);
        if (width > height)
        {
            Rectangle r;
            r.width = float(width) * ratio;
            r.height = height;
            r.x = x;
            r.y = y;
            return r;
        }
        else
        {
            Rectangle r;
            r.width = width;
            r.height = float(height) * ratio;
            r.x = x;
            r.y = y;
            return r;
        }
    }

    Rectangle mult(Orientation orientation, float ratio) const
    {
        ratio = clamp(ratio, 0, 1.0);
        if (orientation == Orientation::Horizontal)
        {
            Rectangle r;
            r.width = float(width) * ratio;
            r.height = height;
            r.x = x;
            r.y = y;
            return r;
        }
        else
        {
            Rectangle r;
            r.width = width;
            r.height = float(height) * ratio;
            r.x = x;
            r.y = y;
            return r;
        }
    }
};

struct Layout
{
    Rectangle container;
    Orientation data_orientation;
    std::vector<CLDirectoryItemData*> current_items;
    float total_size = 0;
    std::unique_ptr<Layout> next_layout;

    Layout *add_item(CLDirectoryItemData* item)
    {
        float item_size = item->dir_or_file_size();
        if (current_items.empty())
        {
            current_items.push_back(item);
            Rectangle new_data = container.mult(item_size / total_size);
            data_orientation = new_data.get_orientation();
            if (container.get_orientation() == data_orientation && total_size > item_size) {
                next_layout = make_unique<Layout>();
                next_layout->container = container - new_data;
                next_layout->total_size = total_size - item_size;
                return next_layout.get();
            }
        }
        else
        {
            float first_ratio = 0;
            {
                float current_total = item_size;
                for(auto item: current_items) {
                    current_total += item->dir_or_file_size();
                }
                Rectangle current_block = container.mult(current_total / total_size);
                Rectangle last_data = current_block.mult(data_orientation, (float)(current_items.back()->dir_or_file_size() / current_total));
                first_ratio = last_data.get_aspect_ratio();
            }
            float second_ratio = 0;
            std::unique_ptr<Layout> may_be_next_layout = make_unique<Layout>();
            {
                float current_total = 0;
                for(auto item: current_items) {
                    current_total += item->dir_or_file_size();
                }
                Rectangle current_block = container.mult(current_total / total_size);
                Rectangle last_data = current_block.mult(data_orientation, ((float)current_items.back()->dir_or_file_size() / current_total));
                second_ratio = last_data.get_aspect_ratio();

                may_be_next_layout->container = container - current_block;
                may_be_next_layout->total_size = total_size - current_total;
                Rectangle new_data = may_be_next_layout->container.mult(item->dir_or_file_size() / may_be_next_layout->total_size);
                may_be_next_layout->data_orientation = new_data.get_orientation();
            }

            if (first_ratio < second_ratio)
            {
                current_items.push_back(item);
            }
            else
            {
                may_be_next_layout->current_items.push_back(item);
                next_layout = move(may_be_next_layout);
                return next_layout.get();
            }
        }
        return nullptr;
    }

    std::vector<std::pair<Rectangle,CLDirectoryItemData*>> get_layout_rectangles() const
    {
        std::vector<std::pair<Rectangle,CLDirectoryItemData*>> result;
        float last_total = 0;
        for(auto item: current_items) {
            last_total += item->dir_or_file_size();
        }
        Rectangle last_block = container.mult(last_total / total_size);
        for (auto item : current_items)
        {
            Rectangle this_block = last_block.mult(data_orientation, item->dir_or_file_size() / last_total);
            last_block = last_block - this_block;
            last_total = last_total - item->dir_or_file_size();
            result.push_back(MakePair(this_block, item));
        }
        return result;
    }
};

std::vector<std::pair<Rectangle,CLDirectoryItemData*>> TravelLayout(Layout *current_layout, size_t count);
std::vector<std::pair<Rectangle,CLDirectoryItemData*>> SolveSquarifiedTreemap(const std::vector<CLDirectoryItemData*> &dir_items, const Rectangle &container);

#endif
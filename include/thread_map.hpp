#pragma once

#include <map>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <vector>

template <class T>
struct ThreadMap {
    ThreadMap() = default;
    ThreadMap(ThreadMap const&) = delete;

    T* get(int id) {
        std::shared_lock lock(mutex);
        auto iter = items.find(id);
        if (iter == items.end())
            return nullptr;
        return &iter->second;
    }

    template <class... Ps>
    int add(Ps... params) {
        std::unique_lock lock(mutex);
        items.emplace(++max, T(params...));
        return max;
    }

    void remove(int id) {
        std::unique_lock lock(mutex);
        items.erase(id);
    }

   private:
    unsigned short max = 0;
    std::map<unsigned short, T> items = {};
    std::shared_mutex mutex;
};

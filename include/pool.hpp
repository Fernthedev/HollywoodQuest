#pragma once

#include <set>
#include <tuple>
#include <vector>

template <class T, class... Ps>
struct Pool {
    Pool() = delete;
    Pool(Pool const&) = delete;
    Pool(int initialSize, Ps... params) : params(params...), items(initialSize, make()) {}

    T& get() {
        for (int i = 0; i < items.size(); i++) {
            if (used.contains(i))
                continue;
            used.insert(i);
            return items[i];
        }

        used.insert(items.size());
        return items.emplace_back(make());
    }

   private:
    std::vector<T> items;
    std::set<int> used;

    std::tuple<Ps...> const params;
    std::shared_ptr<T> const make() { return std::make_from_tuple<T>(params); }
};

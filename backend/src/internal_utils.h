#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace di::internal {

std::string trim(std::string value);
std::vector<std::string> split(std::string_view value, char delimiter);
std::string iso_now_utc();
std::string date_only(const std::string& value);

template <typename MapType>
std::vector<std::string> top_keys(const MapType& counts, std::size_t limit) {
    std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });

    std::vector<std::string> result;
    for (std::size_t index = 0; index < sorted.size() && index < limit; ++index) {
        result.push_back(sorted[index].first);
    }
    return result;
}

}  // namespace di::internal


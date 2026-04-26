#include "defect_intelligence/core.h"

#include "internal_utils.h"

#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

namespace di {
namespace {

std::string to_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char c : value) {
        lowered.push_back(static_cast<char>(std::tolower(c)));
    }
    return lowered;
}

}  // namespace

std::vector<std::string> extract_defect_ids(std::string_view text) {
    static const std::regex bracketed_pattern(R"(\[(?:art|ART)[-_ ]?(\d{5,})\])");
    static const std::regex inline_pattern(R"(\b(?:art|ART)[-_ ]?(\d{5,})\b)");
    std::set<std::string> found;
    const std::string input(text);

    for (auto it = std::sregex_iterator(input.begin(), input.end(), bracketed_pattern); it != std::sregex_iterator(); ++it) {
        found.insert("ART-" + (*it)[1].str());
    }
    for (auto it = std::sregex_iterator(input.begin(), input.end(), inline_pattern); it != std::sregex_iterator(); ++it) {
        found.insert("ART-" + (*it)[1].str());
    }

    return std::vector<std::string>(found.begin(), found.end());
}

std::vector<int> extract_issue_refs(std::string_view text) {
    static const std::regex issue_pattern(R"((?:^|[^A-Za-z0-9_])#(\d+)\b)");
    std::set<int> refs;
    const std::string input(text);
    for (auto it = std::sregex_iterator(input.begin(), input.end(), issue_pattern); it != std::sregex_iterator(); ++it) {
        refs.insert(std::stoi((*it)[1].str()));
    }
    return std::vector<int>(refs.begin(), refs.end());
}

std::string classify_file_kind(std::string_view path) {
    const std::string lowered = to_lower(path);
    const auto dot = lowered.rfind('.');
    const std::string extension = dot == std::string::npos ? "" : lowered.substr(dot);

    if (lowered.find("/test/") != std::string::npos || lowered.find("/tests/") != std::string::npos ||
        lowered.ends_with("_test.cpp") || lowered.ends_with(".spec.ts") || lowered.ends_with(".test.tsx")) {
        return "test";
    }
    if (lowered == "makefile" || lowered.ends_with(".mk") || lowered.ends_with(".cmake")) {
        return "build";
    }
    if (extension == ".md" || extension == ".rst" || extension == ".txt") {
        return "doc";
    }
    if (extension == ".yaml" || extension == ".yml" || extension == ".json" || extension == ".toml" ||
        extension == ".ini" || extension == ".cfg") {
        return "config";
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".svg") {
        return "asset";
    }
    if (extension == ".ts" || extension == ".tsx" || extension == ".js" || extension == ".jsx" ||
        extension == ".py" || extension == ".go" || extension == ".rs" || extension == ".java" ||
        extension == ".c" || extension == ".cc" || extension == ".cpp" || extension == ".cxx") {
        return "code";
    }
    if (extension == ".h" || extension == ".hh" || extension == ".hpp" || extension == ".hxx") {
        return "header";
    }
    if (extension == ".sql" || extension == ".sh" || extension == ".bash" || extension == ".zsh") {
        return "script";
    }
    return "other";
}

std::string infer_component(std::string_view path) {
    const std::vector<std::string> parts = internal::split(path, '/');
    if (parts.empty() || parts.front().empty()) {
        return "(root)";
    }

    static const std::unordered_set<std::string> two_segment_roots {
        "src", "services", "packages", "apps", "backend", "frontend", "lib", "libs",
    };
    if (parts.size() >= 2 && two_segment_roots.contains(parts[0])) {
        return parts[0] + "/" + parts[1];
    }
    return parts[0];
}

std::string history_mode_to_string(HistoryMode mode) {
    return mode == HistoryMode::kFirstParent ? "first-parent" : "full";
}

HistoryMode history_mode_from_string(std::string_view value) {
    return value == "first-parent" ? HistoryMode::kFirstParent : HistoryMode::kFull;
}

std::string scan_config_cache_key(const ScanConfig& config) {
    std::ostringstream stream;
    stream << config.repo_path << "|" << config.revision << "|" << history_mode_to_string(config.history_mode) << "|"
           << config.since.value_or("") << "|" << config.until.value_or("") << "|" << config.max_commits << "|"
           << (config.sample_mode ? "sample" : "real");
    return stream.str();
}

}  // namespace di

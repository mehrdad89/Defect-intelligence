#include "defect_intelligence/core.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iomanip>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace di {
namespace {

struct CommandResult {
    int exit_code {0};
    std::string output;
};

struct BasicCommit {
    std::string hash;
    std::string author;
    std::string authored_at;
    std::string subject;
};

std::string trim(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !is_space(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !is_space(c); }).base(), value.end());
    return value;
}

std::string to_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char c : value) {
        lowered.push_back(static_cast<char>(std::tolower(c)));
    }
    return lowered;
}

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : value) {
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(c);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

CommandResult run_command(const std::string& command) {
    std::array<char, 4096> buffer {};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("Unable to launch command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }

    const int status = pclose(pipe);
    int exit_code = status;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    }

    return CommandResult {exit_code, output};
}

std::string join_shell_args(const std::vector<std::string>& args) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            stream << ' ';
        }
        stream << shell_quote(args[index]);
    }
    return stream.str();
}

std::string run_git(const std::string& repo_path, const std::vector<std::string>& args) {
    std::ostringstream command;
    command << "git -C " << shell_quote(repo_path) << ' ' << join_shell_args(args) << " 2>&1";
    CommandResult result = run_command(command.str());
    if (result.exit_code != 0) {
        throw std::runtime_error("Git command failed: " + result.output);
    }
    return result.output;
}

std::string iso_now_utc() {
    using clock = std::chrono::system_clock;
    const std::time_t now = clock::to_time_t(clock::now());
    std::tm utc_time {};
#if defined(_WIN32)
    gmtime_s(&utc_time, &now);
#else
    gmtime_r(&now, &utc_time);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string date_only(const std::string& value) {
    if (value.size() >= 10) {
        return value.substr(0, 10);
    }
    return value;
}

int to_int(std::string_view value) {
    if (value == "-" || value.empty()) {
        return 0;
    }
    return std::stoi(std::string(value));
}

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

bool has_any_signal(const CommitSummary& commit) {
    return !commit.defect_ids.empty() || !commit.issue_refs.empty();
}

CommitSummary build_commit_summary(const std::string& repo_path, const BasicCommit& basic_commit) {
    CommitSummary commit;
    commit.hash = basic_commit.hash;
    commit.author = basic_commit.author;
    commit.authored_at = basic_commit.authored_at;
    commit.subject = basic_commit.subject;
    commit.message = trim(run_git(repo_path, {"show", "-s", "--format=%B", basic_commit.hash}));

    const std::string parent_line = trim(run_git(repo_path, {"rev-list", "--parents", "-n", "1", basic_commit.hash}));
    const std::vector<std::string> parent_fields = split(parent_line, ' ');
    for (std::size_t index = 1; index < parent_fields.size(); ++index) {
        if (!parent_fields[index].empty()) {
            commit.parents.push_back(parent_fields[index]);
        }
    }

    const std::string file_output = commit.parents.size() >= 2
        ? run_git(repo_path, {"diff", "--numstat", commit.parents.front(), basic_commit.hash})
        : run_git(repo_path, {"show", "--numstat", "--format=", basic_commit.hash});

    std::unordered_set<std::string> unique_components;
    std::istringstream file_stream(file_output);
    std::string line;
    while (std::getline(file_stream, line)) {
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> fields = split(line, '\t');
        if (fields.size() < 3) {
            continue;
        }

        FileChange change;
        change.added = to_int(fields[0]);
        change.deleted = to_int(fields[1]);
        change.path = fields[2];
        change.component = infer_component(change.path);
        change.file_kind = classify_file_kind(change.path);
        commit.total_churn += change.churn();
        commit.files.push_back(change);
        unique_components.insert(change.component);
    }

    commit.components.assign(unique_components.begin(), unique_components.end());
    std::sort(commit.components.begin(), commit.components.end());
    commit.defect_ids = extract_defect_ids(commit.message);
    commit.issue_refs = extract_issue_refs(commit.message);
    return commit;
}

std::vector<BasicCommit> list_basic_commits(const ScanConfig& config) {
    std::vector<std::string> args {
        "log",
        config.revision,
        "--date=iso-strict",
        "--pretty=format:%H%x1f%an%x1f%ad%x1f%s",
    };

    if (config.history_mode == HistoryMode::kFirstParent) {
        args.push_back("--first-parent");
    }
    if (config.since.has_value()) {
        args.push_back("--since=" + *config.since);
    }
    if (config.until.has_value()) {
        args.push_back("--until=" + *config.until);
    }
    if (config.max_commits > 0) {
        args.push_back("-n");
        args.push_back(std::to_string(config.max_commits));
    }

    std::vector<BasicCommit> commits;
    std::istringstream stream(run_git(config.repo_path, args));
    std::string line;
    while (std::getline(stream, line)) {
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> fields = split(line, '\x1f');
        if (fields.size() != 4) {
            continue;
        }
        commits.push_back(BasicCommit {fields[0], fields[1], fields[2], fields[3]});
    }
    return commits;
}

struct ComponentAccumulator {
    std::unordered_set<std::string> defect_ids;
    std::unordered_set<std::string> files;
    std::unordered_set<std::string> authors;
    std::unordered_set<std::string> active_days;
    std::unordered_map<std::string, int> author_counts;
    std::unordered_map<std::string, int> file_counts;
    int commits {0};
    int total_churn {0};
};

struct AuthorAccumulator {
    std::unordered_set<std::string> defect_ids;
    std::unordered_set<std::string> components;
    std::unordered_map<std::string, int> component_counts;
    int commits {0};
    int total_churn {0};
};

std::string escape_json(std::string_view value) {
    std::ostringstream stream;
    for (char c : value) {
        switch (c) {
            case '"':
                stream << "\\\"";
                break;
            case '\\':
                stream << "\\\\";
                break;
            case '\b':
                stream << "\\b";
                break;
            case '\f':
                stream << "\\f";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                } else {
                    stream << c;
                }
                break;
        }
    }
    return stream.str();
}

std::string quote_json(std::string_view value) {
    return "\"" + escape_json(value) + "\"";
}

template <typename T, typename F>
std::string json_array(const std::vector<T>& values, F formatter) {
    std::ostringstream stream;
    stream << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << formatter(values[index]);
    }
    stream << "]";
    return stream.str();
}

std::string json_string_array(const std::vector<std::string>& values) {
    return json_array<std::string>(values, [](const std::string& value) { return quote_json(value); });
}

std::string json_int_array(const std::vector<int>& values) {
    return json_array<int>(values, [](int value) { return std::to_string(value); });
}

std::string file_change_to_json(const FileChange& change) {
    std::ostringstream stream;
    stream << "{"
           << "\"path\":" << quote_json(change.path) << ","
           << "\"component\":" << quote_json(change.component) << ","
           << "\"fileKind\":" << quote_json(change.file_kind) << ","
           << "\"added\":" << change.added << ","
           << "\"deleted\":" << change.deleted << ","
           << "\"churn\":" << change.churn()
           << "}";
    return stream.str();
}

std::string commit_to_json(const CommitSummary& commit) {
    std::ostringstream stream;
    stream << "{"
           << "\"hash\":" << quote_json(commit.hash) << ","
           << "\"author\":" << quote_json(commit.author) << ","
           << "\"authoredAt\":" << quote_json(commit.authored_at) << ","
           << "\"subject\":" << quote_json(commit.subject) << ","
           << "\"message\":" << quote_json(commit.message) << ","
           << "\"parents\":" << json_string_array(commit.parents) << ","
           << "\"defectIds\":" << json_string_array(commit.defect_ids) << ","
           << "\"issueRefs\":" << json_int_array(commit.issue_refs) << ","
           << "\"components\":" << json_string_array(commit.components) << ","
           << "\"totalChurn\":" << commit.total_churn << ","
           << "\"files\":" << json_array<FileChange>(commit.files, file_change_to_json)
           << "}";
    return stream.str();
}

std::string component_metric_to_json(const ComponentMetric& component) {
    std::ostringstream stream;
    stream << "{"
           << "\"component\":" << quote_json(component.component) << ","
           << "\"commits\":" << component.commits << ","
           << "\"uniqueDefects\":" << component.unique_defects << ","
           << "\"filesTouched\":" << component.files_touched << ","
           << "\"authors\":" << component.authors << ","
           << "\"totalChurn\":" << component.total_churn << ","
           << "\"activeDays\":" << component.active_days << ","
           << "\"hotspotScore\":" << std::fixed << std::setprecision(2) << component.hotspot_score << ","
           << "\"topAuthors\":" << json_string_array(component.top_authors) << ","
           << "\"topFiles\":" << json_string_array(component.top_files)
           << "}";
    return stream.str();
}

std::string author_metric_to_json(const AuthorMetric& author) {
    std::ostringstream stream;
    stream << "{"
           << "\"author\":" << quote_json(author.author) << ","
           << "\"commits\":" << author.commits << ","
           << "\"uniqueDefects\":" << author.unique_defects << ","
           << "\"components\":" << author.components << ","
           << "\"totalChurn\":" << author.total_churn << ","
           << "\"topComponents\":" << json_string_array(author.top_components)
           << "}";
    return stream.str();
}

std::string trend_bucket_to_json(const TrendBucket& bucket) {
    std::ostringstream stream;
    stream << "{"
           << "\"date\":" << quote_json(bucket.date) << ","
           << "\"commits\":" << bucket.commits << ","
           << "\"defectRefs\":" << bucket.defect_refs
           << "}";
    return stream.str();
}

std::string ai_summary_to_json_internal(const AiSummary& summary) {
    std::ostringstream stream;
    stream << "{"
           << "\"available\":" << (summary.available ? "true" : "false") << ","
           << "\"provider\":" << quote_json(summary.provider) << ","
           << "\"narrative\":" << quote_json(summary.narrative) << ","
           << "\"highlights\":" << json_string_array(summary.highlights) << ","
           << "\"nextActions\":" << json_string_array(summary.next_actions)
           << "}";
    return stream.str();
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
    const std::vector<std::string> parts = split(path, '/');
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

ScanReport build_sample_report() {
    ScanReport report;
    report.generated_at = iso_now_utc();
    report.repository = "sample-data";
    report.revision = "demo-main";
    report.history_mode = "full";
    report.summary = ScanSummary {
        420,
        37,
        19,
        4,
        7,
        "2026-03-01",
        "2026-04-24",
        0.0881,
    };
    report.components = {
        ComponentMetric {"src/scanner", 12, 8, 14, 4, 638, 11, 39.24, {"A. Chen", "M. Diaz", "S. Patel"}, {"src/scanner/repository_scanner.cpp", "src/scanner/tag_parser.cpp", "src/scanner/git_client.cpp"}},
        ComponentMetric {"src/api", 9, 6, 10, 3, 411, 8, 29.88, {"M. Diaz", "L. Brown"}, {"src/api/report_controller.cpp", "src/api/http_router.cpp"}},
        ComponentMetric {"src/analytics", 8, 5, 9, 3, 352, 7, 25.61, {"S. Patel", "A. Chen"}, {"src/analytics/hotspot_service.cpp", "src/analytics/summary_engine.cpp"}},
        ComponentMetric {"frontend/dashboard", 8, 4, 11, 4, 284, 9, 22.49, {"L. Brown", "J. Kim"}, {"frontend/src/App.tsx", "frontend/src/components/HotspotTable.tsx"}},
    };
    report.authors = {
        AuthorMetric {"A. Chen", 11, 8, 3, 544, {"src/scanner", "src/analytics"}},
        AuthorMetric {"M. Diaz", 8, 6, 2, 438, {"src/api", "src/scanner"}},
        AuthorMetric {"S. Patel", 7, 5, 2, 341, {"src/analytics", "src/scanner"}},
        AuthorMetric {"L. Brown", 6, 4, 2, 273, {"frontend/dashboard", "src/api"}},
    };
    report.timeline = {
        TrendBucket {"2026-04-18", 3, 4},
        TrendBucket {"2026-04-19", 4, 5},
        TrendBucket {"2026-04-20", 6, 8},
        TrendBucket {"2026-04-21", 5, 7},
        TrendBucket {"2026-04-22", 7, 10},
        TrendBucket {"2026-04-23", 6, 8},
        TrendBucket {"2026-04-24", 6, 9},
    };
    report.commits = {
        CommitSummary {
            "d6f0c7b1a2c3",
            "A. Chen",
            "2026-04-24T18:12:30Z",
            "Harden scanner fallback for [art123456] correlation",
            "Harden scanner fallback for [art123456] correlation and preserve issue link #248 during batch runs.",
            {"a1b2c3d4e5f6"},
            {"ART-123456"},
            {248},
            {"src/scanner", "src/api"},
            {
                FileChange {"src/scanner/repository_scanner.cpp", "src/scanner", "code", 42, 11},
                FileChange {"src/api/report_controller.cpp", "src/api", "code", 13, 3},
            },
            69,
        },
        CommitSummary {
            "e7a1b2c3d4e5",
            "M. Diaz",
            "2026-04-23T14:40:02Z",
            "Tune hotspot ranking after ART-123777 review",
            "Tune hotspot ranking after ART-123777 review so component density and churn do not overreact to one noisy merge.",
            {"a0f9e8d7c6b5"},
            {"ART-123777"},
            {},
            {"src/analytics"},
            {
                FileChange {"src/analytics/hotspot_service.cpp", "src/analytics", "code", 28, 7},
            },
            35,
        },
        CommitSummary {
            "f8b2c3d4e5f6",
            "L. Brown",
            "2026-04-22T09:04:55Z",
            "Refresh dashboard filters for [art124110] and issue #251",
            "Refresh dashboard filters for [art124110] and issue #251 to keep frontend and API summaries aligned.",
            {"9b8a7c6d5e4f"},
            {"ART-124110"},
            {251},
            {"frontend/dashboard", "src/api"},
            {
                FileChange {"frontend/src/App.tsx", "frontend/dashboard", "code", 33, 12},
                FileChange {"src/api/http_router.cpp", "src/api", "code", 8, 2},
            },
            55,
        },
    };
    report.ai_summary = AiSummary {
        true,
        "built-in",
        "The strongest concentration of defect-linked activity is in src/scanner, where eight unique customer markers appear across twelve commits. The shape suggests repeated stabilization work rather than one isolated incident.",
        {
            "src/scanner leads the backlog of recurring defect-linked commits and also has the highest churn.",
            "A. Chen appears on the largest share of defect-linked work, which is useful for code review routing and ownership mapping.",
            "The last seven days show steady daily signal volume rather than one single spike, which often points to a sustained reliability effort.",
        },
        {
            "Review the top scanner files first because they combine repeat defect tags with high change volume.",
            "Compare scanner hotspots with recent API changes to see whether contract instability is amplifying defect work.",
            "Turn the current daily scan into a scheduled background job before scaling to additional repositories.",
        },
    };
    return report;
}

ScanReport GitRepositoryScanner::scan(const ScanConfig& config) const {
    if (config.sample_mode) {
        return build_sample_report();
    }

    if (config.repo_path.empty()) {
        throw std::runtime_error("A repository path is required unless sample mode is enabled.");
    }

    if (!std::filesystem::exists(config.repo_path)) {
        throw std::runtime_error("Repository path does not exist: " + config.repo_path);
    }

    const std::vector<BasicCommit> basic_commits = list_basic_commits(config);
    std::vector<CommitSummary> relevant_commits;
    relevant_commits.reserve(basic_commits.size());

    const std::size_t concurrency = std::max<std::size_t>(
        1,
        std::min<std::size_t>(4, std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency()));
    const std::size_t batch_size = std::max<std::size_t>(1, concurrency * 2);

    for (std::size_t start = 0; start < basic_commits.size(); start += batch_size) {
        std::vector<std::future<CommitSummary>> batch;
        const std::size_t end = std::min<std::size_t>(basic_commits.size(), start + batch_size);
        for (std::size_t index = start; index < end; ++index) {
            batch.push_back(std::async(std::launch::async, [&, index] {
                return build_commit_summary(config.repo_path, basic_commits[index]);
            }));
        }
        for (auto& future : batch) {
            CommitSummary commit = future.get();
            if (has_any_signal(commit)) {
                relevant_commits.push_back(std::move(commit));
            }
        }
    }

    std::unordered_set<std::string> unique_defects;
    std::unordered_map<std::string, ComponentAccumulator> component_map;
    std::unordered_map<std::string, AuthorAccumulator> author_map;
    std::map<std::string, TrendBucket> timeline_map;
    std::string period_start;
    std::string period_end;

    for (const auto& commit : relevant_commits) {
        const std::string day = date_only(commit.authored_at);
        if (period_start.empty() || day < period_start) {
            period_start = day;
        }
        if (period_end.empty() || day > period_end) {
            period_end = day;
        }

        TrendBucket& bucket = timeline_map[day];
        bucket.date = day;
        bucket.commits += 1;
        bucket.defect_refs += static_cast<int>(commit.defect_ids.size());

        AuthorAccumulator& author_accumulator = author_map[commit.author];
        author_accumulator.commits += 1;
        author_accumulator.total_churn += commit.total_churn;
        for (const auto& defect_id : commit.defect_ids) {
            unique_defects.insert(defect_id);
            author_accumulator.defect_ids.insert(defect_id);
        }
        for (const auto& component_name : commit.components) {
            author_accumulator.components.insert(component_name);
            author_accumulator.component_counts[component_name] += 1;
        }

        for (const auto& component_name : commit.components) {
            ComponentAccumulator& component_accumulator = component_map[component_name];
            component_accumulator.commits += 1;
            component_accumulator.authors.insert(commit.author);
            component_accumulator.author_counts[commit.author] += 1;
            component_accumulator.total_churn += commit.total_churn;
            component_accumulator.active_days.insert(day);
            for (const auto& defect_id : commit.defect_ids) {
                component_accumulator.defect_ids.insert(defect_id);
            }
        }

        for (const auto& change : commit.files) {
            ComponentAccumulator& component_accumulator = component_map[change.component];
            component_accumulator.files.insert(change.path);
            component_accumulator.file_counts[change.path] += 1;
        }
    }

    std::vector<ComponentMetric> component_metrics;
    component_metrics.reserve(component_map.size());
    for (const auto& [name, accumulator] : component_map) {
        ComponentMetric metric;
        metric.component = name;
        metric.commits = accumulator.commits;
        metric.unique_defects = static_cast<int>(accumulator.defect_ids.size());
        metric.files_touched = static_cast<int>(accumulator.files.size());
        metric.authors = static_cast<int>(accumulator.authors.size());
        metric.total_churn = accumulator.total_churn;
        metric.active_days = static_cast<int>(accumulator.active_days.size());
        metric.hotspot_score = (metric.unique_defects * 3.0) + (metric.commits * 1.5) +
            std::log1p(static_cast<double>(metric.total_churn)) * 2.0 + (metric.authors * 0.75);
        metric.top_authors = top_keys(accumulator.author_counts, 3);
        metric.top_files = top_keys(accumulator.file_counts, 3);
        component_metrics.push_back(std::move(metric));
    }
    std::sort(component_metrics.begin(), component_metrics.end(), [](const auto& left, const auto& right) {
        if (left.hotspot_score != right.hotspot_score) {
            return left.hotspot_score > right.hotspot_score;
        }
        return left.component < right.component;
    });

    std::vector<AuthorMetric> author_metrics;
    author_metrics.reserve(author_map.size());
    for (const auto& [name, accumulator] : author_map) {
        AuthorMetric metric;
        metric.author = name;
        metric.commits = accumulator.commits;
        metric.unique_defects = static_cast<int>(accumulator.defect_ids.size());
        metric.components = static_cast<int>(accumulator.components.size());
        metric.total_churn = accumulator.total_churn;
        metric.top_components = top_keys(accumulator.component_counts, 3);
        author_metrics.push_back(std::move(metric));
    }
    std::sort(author_metrics.begin(), author_metrics.end(), [](const auto& left, const auto& right) {
        if (left.unique_defects != right.unique_defects) {
            return left.unique_defects > right.unique_defects;
        }
        if (left.commits != right.commits) {
            return left.commits > right.commits;
        }
        return left.author < right.author;
    });

    std::vector<TrendBucket> timeline;
    timeline.reserve(timeline_map.size());
    for (const auto& [_, bucket] : timeline_map) {
        timeline.push_back(bucket);
    }

    std::sort(relevant_commits.begin(), relevant_commits.end(), [](const auto& left, const auto& right) {
        return left.authored_at > right.authored_at;
    });

    ScanReport report;
    report.generated_at = iso_now_utc();
    report.repository = config.repo_path;
    report.revision = config.revision;
    report.history_mode = history_mode_to_string(config.history_mode);
    report.summary.total_commits_scanned = static_cast<int>(basic_commits.size());
    report.summary.relevant_commits = static_cast<int>(relevant_commits.size());
    report.summary.unique_defects = static_cast<int>(unique_defects.size());
    report.summary.components = static_cast<int>(component_metrics.size());
    report.summary.authors = static_cast<int>(author_metrics.size());
    report.summary.period_start = period_start;
    report.summary.period_end = period_end;
    report.summary.coverage_ratio = basic_commits.empty()
        ? 0.0
        : static_cast<double>(relevant_commits.size()) / static_cast<double>(basic_commits.size());
    report.components = std::move(component_metrics);
    report.authors = std::move(author_metrics);
    report.timeline = std::move(timeline);
    report.commits = std::move(relevant_commits);
    return report;
}

AiSummary AiSummarizer::summarize(
    const ScanReport& report,
    const std::optional<std::string>& focus_component) const {
    AiSummary summary;
    summary.available = true;
    summary.provider = std::getenv("DI_AI_PROVIDER") == nullptr ? "built-in" : std::getenv("DI_AI_PROVIDER");

    if (report.commits.empty()) {
        summary.narrative =
            "No defect-linked commits were found in the current scope. Try widening the date range, switching to full history, or using sample mode to preview the dashboard.";
        summary.highlights = {
            "The current filter set produced zero relevant commits.",
            "Issue references and ART-style markers are both considered signal sources.",
        };
        summary.next_actions = {
            "Re-run with --history-mode full.",
            "Point the service at a repository with known defect-linked commit tags.",
        };
        return summary;
    }

    const ComponentMetric* top_component = report.components.empty() ? nullptr : &report.components.front();
    if (focus_component.has_value()) {
        for (const auto& component : report.components) {
            if (component.component == *focus_component) {
                top_component = &component;
                break;
            }
        }
    }

    const AuthorMetric* top_author = report.authors.empty() ? nullptr : &report.authors.front();
    const TrendBucket* hottest_day = report.timeline.empty() ? nullptr : &report.timeline.front();
    for (const auto& bucket : report.timeline) {
        if (hottest_day == nullptr || bucket.defect_refs > hottest_day->defect_refs) {
            hottest_day = &bucket;
        }
    }

    std::ostringstream narrative;
    narrative << "The scan found " << report.summary.relevant_commits << " defect-linked commits across "
              << report.summary.components << " components";
    if (top_component != nullptr) {
        narrative << ". " << top_component->component << " is the strongest hotspot with "
                  << top_component->unique_defects << " unique markers and " << top_component->commits
                  << " linked commits";
    }
    if (top_author != nullptr) {
        narrative << ", while " << top_author->author << " appears on the broadest share of defect-linked work";
    }
    narrative << ".";
    summary.narrative = narrative.str();

    if (top_component != nullptr) {
        summary.highlights.push_back(
            top_component->component + " leads on both recurring defect identifiers and hotspot score.");
        if (!top_component->top_files.empty()) {
            summary.next_actions.push_back(
                "Inspect " + top_component->top_files.front() + " first because it sits at the center of the hottest component.");
        }
    }
    if (top_author != nullptr) {
        summary.highlights.push_back(
            top_author->author + " is a useful review-routing signal because their commits span " +
            std::to_string(top_author->components) + " components.");
    }
    if (hottest_day != nullptr) {
        summary.highlights.push_back(
            hottest_day->date + " has the highest daily defect signal count in the current view.");
    }

    summary.next_actions.push_back("Compare hotspot-heavy components with ownership and release data before prioritizing fixes.");
    summary.next_actions.push_back("Schedule incremental scans if you plan to use this across larger repositories.");
    return summary;
}

ScanReport AnalyticsService::analyze(const ScanConfig& config) const {
    ScanReport report = GitRepositoryScanner {}.scan(config);
    report.ai_summary = AiSummarizer {}.summarize(report);
    return report;
}

std::string to_json(const ScanReport& report) {
    std::ostringstream stream;
    stream << "{"
           << "\"metadata\":{"
           << "\"generatedAt\":" << quote_json(report.generated_at) << ","
           << "\"repository\":" << quote_json(report.repository) << ","
           << "\"revision\":" << quote_json(report.revision) << ","
           << "\"historyMode\":" << quote_json(report.history_mode)
           << "},"
           << "\"summary\":{"
           << "\"totalCommitsScanned\":" << report.summary.total_commits_scanned << ","
           << "\"relevantCommits\":" << report.summary.relevant_commits << ","
           << "\"uniqueDefects\":" << report.summary.unique_defects << ","
           << "\"components\":" << report.summary.components << ","
           << "\"authors\":" << report.summary.authors << ","
           << "\"periodStart\":" << quote_json(report.summary.period_start) << ","
           << "\"periodEnd\":" << quote_json(report.summary.period_end) << ","
           << "\"coverageRatio\":" << std::fixed << std::setprecision(4) << report.summary.coverage_ratio
           << "},"
           << "\"componentInsights\":" << json_array<ComponentMetric>(report.components, component_metric_to_json) << ","
           << "\"authorInsights\":" << json_array<AuthorMetric>(report.authors, author_metric_to_json) << ","
           << "\"timeline\":" << json_array<TrendBucket>(report.timeline, trend_bucket_to_json) << ","
           << "\"commits\":" << json_array<CommitSummary>(report.commits, commit_to_json) << ","
           << "\"aiSummary\":" << ai_summary_to_json_internal(report.ai_summary)
           << "}";
    return stream.str();
}

std::string summary_to_json(const ScanReport& report) {
    std::ostringstream stream;
    stream << "{"
           << "\"summary\":{"
           << "\"totalCommitsScanned\":" << report.summary.total_commits_scanned << ","
           << "\"relevantCommits\":" << report.summary.relevant_commits << ","
           << "\"uniqueDefects\":" << report.summary.unique_defects << ","
           << "\"components\":" << report.summary.components << ","
           << "\"authors\":" << report.summary.authors << ","
           << "\"periodStart\":" << quote_json(report.summary.period_start) << ","
           << "\"periodEnd\":" << quote_json(report.summary.period_end) << ","
           << "\"coverageRatio\":" << std::fixed << std::setprecision(4) << report.summary.coverage_ratio
           << "},"
           << "\"timeline\":" << json_array<TrendBucket>(report.timeline, trend_bucket_to_json) << ","
           << "\"aiSummary\":" << ai_summary_to_json_internal(report.ai_summary)
           << "}";
    return stream.str();
}

std::string components_to_json(const ScanReport& report) {
    std::ostringstream stream;
    stream << "{"
           << "\"count\":" << report.components.size() << ","
           << "\"componentInsights\":" << json_array<ComponentMetric>(report.components, component_metric_to_json)
           << "}";
    return stream.str();
}

std::string authors_to_json(const ScanReport& report) {
    std::ostringstream stream;
    stream << "{"
           << "\"count\":" << report.authors.size() << ","
           << "\"authorInsights\":" << json_array<AuthorMetric>(report.authors, author_metric_to_json)
           << "}";
    return stream.str();
}

std::string commits_to_json(
    const ScanReport& report,
    const std::optional<std::string>& component_filter) {
    std::vector<CommitSummary> filtered;
    for (const auto& commit : report.commits) {
        if (!component_filter.has_value()) {
            filtered.push_back(commit);
            continue;
        }
        if (std::find(commit.components.begin(), commit.components.end(), *component_filter) != commit.components.end()) {
            filtered.push_back(commit);
        }
    }

    std::ostringstream stream;
    stream << "{"
           << "\"count\":" << filtered.size() << ","
           << "\"commits\":" << json_array<CommitSummary>(filtered, commit_to_json)
           << "}";
    return stream.str();
}

std::string ai_summary_to_json(const AiSummary& summary) {
    return ai_summary_to_json_internal(summary);
}

std::string health_to_json() {
    return "{"
           "\"status\":\"ok\","
           "\"service\":\"defect-intelligence-api\","
           "\"generatedAt\":" + quote_json(iso_now_utc()) +
           "}";
}

std::string error_to_json(std::string_view message, int code) {
    std::ostringstream stream;
    stream << "{"
           << "\"error\":{"
           << "\"message\":" << quote_json(message) << ","
           << "\"code\":" << code
           << "}"
           << "}";
    return stream.str();
}

}  // namespace di

#include "defect_intelligence/core.h"

#include "internal_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
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

int to_int(std::string_view value) {
    if (value == "-" || value.empty()) {
        return 0;
    }
    return std::stoi(std::string(value));
}

bool has_any_signal(const CommitSummary& commit) {
    return !commit.defect_ids.empty() || !commit.issue_refs.empty();
}

std::vector<std::string> normalized_signal_ids(const CommitSummary& commit) {
    std::vector<std::string> ids = commit.defect_ids;
    ids.reserve(commit.defect_ids.size() + commit.issue_refs.size());
    for (int issue_ref : commit.issue_refs) {
        ids.push_back("ISSUE-" + std::to_string(issue_ref));
    }
    return ids;
}

CommitSummary build_commit_summary(const std::string& repo_path, const BasicCommit& basic_commit) {
    CommitSummary commit;
    commit.hash = basic_commit.hash;
    commit.author = basic_commit.author;
    commit.authored_at = basic_commit.authored_at;
    commit.subject = basic_commit.subject;
    commit.message = internal::trim(run_git(repo_path, {"show", "-s", "--format=%B", basic_commit.hash}));

    const std::string parent_line = internal::trim(run_git(repo_path, {"rev-list", "--parents", "-n", "1", basic_commit.hash}));
    const std::vector<std::string> parent_fields = internal::split(parent_line, ' ');
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
        if (internal::trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> fields = internal::split(line, '\t');
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
        if (internal::trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> fields = internal::split(line, '\x1f');
        if (fields.size() != 4) {
            continue;
        }
        commits.push_back(BasicCommit {fields[0], fields[1], fields[2], fields[3]});
    }
    return commits;
}

}  // namespace

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
        const std::string day = internal::date_only(commit.authored_at);
        const std::vector<std::string> signal_ids = normalized_signal_ids(commit);
        if (period_start.empty() || day < period_start) {
            period_start = day;
        }
        if (period_end.empty() || day > period_end) {
            period_end = day;
        }

        TrendBucket& bucket = timeline_map[day];
        bucket.date = day;
        bucket.commits += 1;
        bucket.defect_refs += static_cast<int>(signal_ids.size());

        AuthorAccumulator& author_accumulator = author_map[commit.author];
        author_accumulator.commits += 1;
        author_accumulator.total_churn += commit.total_churn;
        for (const auto& signal_id : signal_ids) {
            unique_defects.insert(signal_id);
            author_accumulator.defect_ids.insert(signal_id);
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
            for (const auto& signal_id : signal_ids) {
                component_accumulator.defect_ids.insert(signal_id);
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
        metric.top_authors = internal::top_keys(accumulator.author_counts, 3);
        metric.top_files = internal::top_keys(accumulator.file_counts, 3);
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
        metric.top_components = internal::top_keys(accumulator.component_counts, 3);
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
    report.generated_at = internal::iso_now_utc();
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

}  // namespace di

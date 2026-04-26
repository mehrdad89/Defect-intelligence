#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace di {

enum class HistoryMode {
    kFull,
    kFirstParent,
};

struct ScanConfig {
    std::string repo_path;
    std::string revision {"HEAD"};
    HistoryMode history_mode {HistoryMode::kFull};
    std::optional<std::string> since;
    std::optional<std::string> until;
    std::size_t max_commits {0};
    bool sample_mode {false};
};

struct FileChange {
    std::string path;
    std::string component;
    std::string file_kind;
    int added {0};
    int deleted {0};

    [[nodiscard]] int churn() const {
        return added + deleted;
    }
};

struct CommitSummary {
    std::string hash;
    std::string author;
    std::string authored_at;
    std::string subject;
    std::string message;
    std::vector<std::string> parents;
    std::vector<std::string> defect_ids;
    std::vector<int> issue_refs;
    std::vector<std::string> components;
    std::vector<FileChange> files;
    int total_churn {0};
};

struct ComponentMetric {
    std::string component;
    int commits {0};
    int unique_defects {0};
    int files_touched {0};
    int authors {0};
    int total_churn {0};
    int active_days {0};
    double hotspot_score {0.0};
    std::vector<std::string> top_authors;
    std::vector<std::string> top_files;
};

struct AuthorMetric {
    std::string author;
    int commits {0};
    int unique_defects {0};
    int components {0};
    int total_churn {0};
    std::vector<std::string> top_components;
};

struct TrendBucket {
    std::string date;
    int commits {0};
    int defect_refs {0};
};

struct ScanSummary {
    int total_commits_scanned {0};
    int relevant_commits {0};
    int unique_defects {0};
    int components {0};
    int authors {0};
    std::string period_start;
    std::string period_end;
    double coverage_ratio {0.0};
};

struct InsightSummary {
    bool available {false};
    std::string source;
    std::string narrative;
    std::vector<std::string> highlights;
    std::vector<std::string> next_actions;
};

struct ScanReport {
    std::string generated_at;
    std::string repository;
    std::string revision;
    std::string history_mode;
    ScanSummary summary;
    std::vector<ComponentMetric> components;
    std::vector<AuthorMetric> authors;
    std::vector<TrendBucket> timeline;
    std::vector<CommitSummary> commits;
    InsightSummary insight_summary;
};

std::vector<std::string> extract_defect_ids(std::string_view text);
std::vector<int> extract_issue_refs(std::string_view text);
std::string classify_file_kind(std::string_view path);
std::string infer_component(std::string_view path);
std::string history_mode_to_string(HistoryMode mode);
HistoryMode history_mode_from_string(std::string_view value);
std::string scan_config_cache_key(const ScanConfig& config);
ScanReport build_sample_report();

class GitRepositoryScanner {
  public:
    [[nodiscard]] ScanReport scan(const ScanConfig& config) const;
};

class InsightComposer {
  public:
    [[nodiscard]] InsightSummary compose(
        const ScanReport& report,
        const std::optional<std::string>& focus_component = std::nullopt) const;
};

class AnalyticsService {
  public:
    [[nodiscard]] ScanReport analyze(const ScanConfig& config) const;
};

std::string to_json(const ScanReport& report);
std::string summary_to_json(const ScanReport& report);
std::string components_to_json(const ScanReport& report);
std::string authors_to_json(const ScanReport& report);
std::string commits_to_json(
    const ScanReport& report,
    const std::optional<std::string>& component_filter = std::nullopt);
std::string insight_summary_to_json(const InsightSummary& summary);
std::string health_to_json();
std::string error_to_json(std::string_view message, int code);

}  // namespace di

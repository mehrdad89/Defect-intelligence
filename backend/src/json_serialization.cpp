#include "defect_intelligence/core.h"

#include "internal_utils.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace di {
namespace {

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

std::string insight_summary_to_json_internal(const InsightSummary& summary) {
    std::ostringstream stream;
    stream << "{"
           << "\"available\":" << (summary.available ? "true" : "false") << ","
           << "\"source\":" << quote_json(summary.source) << ","
           << "\"narrative\":" << quote_json(summary.narrative) << ","
           << "\"highlights\":" << json_string_array(summary.highlights) << ","
           << "\"nextActions\":" << json_string_array(summary.next_actions)
           << "}";
    return stream.str();
}

}  // namespace

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
           << "\"insightSummary\":" << insight_summary_to_json_internal(report.insight_summary)
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
           << "\"insightSummary\":" << insight_summary_to_json_internal(report.insight_summary)
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

std::string insight_summary_to_json(const InsightSummary& summary) {
    return insight_summary_to_json_internal(summary);
}

std::string health_to_json() {
    return "{"
           "\"status\":\"ok\","
           "\"service\":\"defect-intelligence-api\","
           "\"generatedAt\":" + quote_json(internal::iso_now_utc()) +
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

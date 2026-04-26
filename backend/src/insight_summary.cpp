#include "defect_intelligence/core.h"

#include <cstdlib>
#include <optional>
#include <sstream>

namespace di {

InsightSummary InsightComposer::compose(
    const ScanReport& report,
    const std::optional<std::string>& focus_component) const {
    InsightSummary summary;
    summary.available = true;
    summary.source = std::getenv("DI_SUMMARY_SOURCE") == nullptr ? "local" : std::getenv("DI_SUMMARY_SOURCE");

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
    report.insight_summary = InsightComposer {}.compose(report);
    return report;
}

}  // namespace di

#include "defect_intelligence/core.h"

#include "internal_utils.h"

namespace di {

ScanReport build_sample_report() {
    ScanReport report;
    report.generated_at = internal::iso_now_utc();
    report.repository = "sample://demo-repository";
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
        ComponentMetric {"frontend/dashboard", 8, 4, 11, 4, 284, 9, 22.49, {"L. Brown", "J. Kim"}, {"frontend/src/app.js", "frontend/src/demo-data.js"}},
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
                FileChange {"frontend/src/app.js", "frontend/dashboard", "code", 33, 12},
                FileChange {"src/api/http_router.cpp", "src/api", "code", 8, 2},
            },
            55,
        },
    };
    report.insight_summary = InsightSummary {
        true,
        "local",
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

}  // namespace di

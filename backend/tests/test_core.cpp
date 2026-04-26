#include "defect_intelligence/core.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    using di::extract_defect_ids;
    using di::extract_issue_refs;

    const auto defects = extract_defect_ids("Fix regression for [art123456], ART-999999 and unrelated note.");
    require(defects.size() == 2, "expected two normalized defect identifiers");
    require(defects[0] == "ART-123456", "expected first normalized defect identifier");
    require(defects[1] == "ART-999999", "expected second normalized defect identifier");

    const auto issue_refs = extract_issue_refs("Connects to #42 and #77 but not abc#3.");
    require(issue_refs.size() == 2, "expected two issue references");
    require(issue_refs[0] == 42, "expected issue 42");
    require(issue_refs[1] == 77, "expected issue 77");

    require(di::classify_file_kind("src/api/server.cpp") == "code", "expected code classification");
    require(di::classify_file_kind("frontend/src/app.js") == "code", "expected js code classification");
    require(di::classify_file_kind("docs/readme.md") == "doc", "expected doc classification");
    require(di::infer_component("src/api/server.cpp") == "src/api", "expected inferred src/api component");
    require(di::infer_component("scripts/reindex.sh") == "scripts", "expected top-level scripts component");

    const auto sample = di::build_sample_report();
    require(sample.summary.relevant_commits > 0, "sample report should contain relevant commits");
    require(!sample.insight_summary.narrative.empty(), "sample report should include a summary narrative");

    std::cout << "All backend tests passed.\n";
    return EXIT_SUCCESS;
}

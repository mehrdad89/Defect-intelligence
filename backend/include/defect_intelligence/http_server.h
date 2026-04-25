#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "defect_intelligence/core.h"

namespace di {

struct ApiServerConfig {
    int port {8080};
    std::optional<std::string> default_repo_path;
    bool sample_mode {false};
};

class ApiServer {
  public:
    explicit ApiServer(ApiServerConfig config);
    void serve();

  private:
    ApiServerConfig config_;
    AnalyticsService analytics_;
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, ScanReport> cache_;
};

}  // namespace di


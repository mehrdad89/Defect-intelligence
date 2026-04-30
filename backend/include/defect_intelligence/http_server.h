#pragma once

#include <mutex>
#include <optional>
#include <string>

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
    [[nodiscard]] ScanReport get_report(const ScanConfig& scan_config);
    void handle_client(int client_fd);

    ApiServerConfig config_;
    AnalyticsService analytics_;
    mutable std::mutex cache_mutex_;
    std::string last_cache_key_;
    std::optional<ScanReport> last_report_;
};

}  // namespace di

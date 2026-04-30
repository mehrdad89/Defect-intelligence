#include "defect_intelligence/core.h"
#include "defect_intelligence/http_server.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void print_help() {
    std::cout
        << "Defect Intelligence Platform\n\n"
        << "Usage:\n"
        << "  defect-intelligence scan [--repo PATH] [--rev REV] [--history-mode full|first-parent]\n"
        << "                            [--since YYYY-MM-DD] [--until YYYY-MM-DD] [--max-commits N] [--sample]\n"
        << "  defect-intelligence serve [--repo PATH] [--port PORT] [--sample]\n";
}

std::optional<std::string> read_flag(
    const std::vector<std::string>& args,
    const std::string& flag) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == flag) {
            return args[index + 1];
        }
    }
    return std::nullopt;
}

bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& arg : args) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

bool looks_unsigned_integer(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::size_t parse_max_commits(std::string_view value) {
    if (!looks_unsigned_integer(value)) {
        throw std::runtime_error("--max-commits must be a non-negative integer.");
    }

    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(std::string(value), &consumed);
    if (consumed != value.size() ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("--max-commits must be a non-negative integer.");
    }
    return static_cast<std::size_t>(parsed);
}

int parse_port(std::string_view value, std::string_view source_name) {
    if (!looks_unsigned_integer(value)) {
        throw std::runtime_error(std::string(source_name) + " must be an integer between 1 and 65535.");
    }

    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(std::string(value), &consumed);
    if (consumed != value.size() || parsed == 0 || parsed > 65535UL) {
        throw std::runtime_error(std::string(source_name) + " must be an integer between 1 and 65535.");
    }
    return static_cast<int>(parsed);
}

di::HistoryMode parse_history_mode(std::string_view value) {
    if (value == "full") {
        return di::HistoryMode::kFull;
    }
    if (value == "first-parent") {
        return di::HistoryMode::kFirstParent;
    }
    throw std::runtime_error("--history-mode must be either full or first-parent.");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc <= 1 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            print_help();
            return 0;
        }

        const std::string command = argv[1];
        const std::vector<std::string> args(argv + 2, argv + argc);

        if (command == "scan") {
            di::ScanConfig config;
            config.sample_mode = has_flag(args, "--sample");
            if (const auto repo = read_flag(args, "--repo"); repo.has_value()) {
                config.repo_path = *repo;
            }
            if (const auto revision = read_flag(args, "--rev"); revision.has_value()) {
                config.revision = *revision;
            }
            if (const auto history_mode = read_flag(args, "--history-mode"); history_mode.has_value()) {
                config.history_mode = parse_history_mode(*history_mode);
            }
            config.since = read_flag(args, "--since");
            config.until = read_flag(args, "--until");
            if (const auto max_commits = read_flag(args, "--max-commits"); max_commits.has_value()) {
                config.max_commits = parse_max_commits(*max_commits);
            }

            const di::ScanReport report = di::AnalyticsService {}.analyze(config);
            std::cout << di::to_json(report) << std::endl;
            return 0;
        }

        if (command == "serve") {
            di::ApiServerConfig config;
            config.sample_mode = has_flag(args, "--sample");
            if (const auto repo = read_flag(args, "--repo"); repo.has_value()) {
                config.default_repo_path = *repo;
            } else if (const char* env_repo = std::getenv("DI_DEFAULT_REPO_PATH"); env_repo != nullptr) {
                config.default_repo_path = std::string(env_repo);
            }

            if (const auto port = read_flag(args, "--port"); port.has_value()) {
                config.port = parse_port(*port, "--port");
            } else if (const char* env_port = std::getenv("DI_PORT"); env_port != nullptr) {
                config.port = parse_port(env_port, "DI_PORT");
            }

            di::ApiServer server(config);
            std::cout << "Serving Defect Intelligence API on http://localhost:" << config.port << '\n';
            if (config.default_repo_path.has_value()) {
                std::cout << "Default repository: " << *config.default_repo_path << '\n';
            } else if (config.sample_mode) {
                std::cout << "Sample mode enabled.\n";
            }
            server.serve();
            return 0;
        }

        print_help();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

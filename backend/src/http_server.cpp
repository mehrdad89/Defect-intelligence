#include "defect_intelligence/http_server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace di {
namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> query;
};

struct HttpResponse {
    int status {200};
    std::string body;
};

struct BadRequestError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string trim(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !is_space(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !is_space(c); }).base(), value.end());
    return value;
}

std::string url_decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '+' ) {
            decoded.push_back(' ');
            continue;
        }
        if (c == '%' && index + 2 < value.size()) {
            const std::string hex = std::string(value.substr(index + 1, 2));
            char decoded_char = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            decoded.push_back(decoded_char);
            index += 2;
            continue;
        }
        decoded.push_back(c);
    }
    return decoded;
}

std::string json_escape(std::string_view value) {
    std::ostringstream stream;
    for (unsigned char c : value) {
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
                if (c < 0x20) {
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(c) << std::dec;
                } else {
                    stream << static_cast<char>(c);
                }
                break;
        }
    }
    return stream.str();
}

std::string json_quote(std::string_view value) {
    return "\"" + json_escape(value) + "\"";
}

std::unordered_map<std::string, std::string> parse_query_string(std::string_view query_string) {
    std::unordered_map<std::string, std::string> query;
    std::size_t start = 0;
    while (start <= query_string.size()) {
        const std::size_t end = query_string.find('&', start);
        const std::string_view entry = query_string.substr(start, end == std::string_view::npos ? query_string.size() - start : end - start);
        if (!entry.empty()) {
            const std::size_t split_index = entry.find('=');
            if (split_index == std::string_view::npos) {
                query.emplace(url_decode(entry), "");
            } else {
                query.emplace(
                    url_decode(entry.substr(0, split_index)),
                    url_decode(entry.substr(split_index + 1)));
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return query;
}

HttpRequest parse_request(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string request_line;
    std::getline(stream, request_line);
    request_line = trim(request_line);
    const std::vector<std::string> parts = [] (const std::string& line) {
        std::istringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string token;
        while (line_stream >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }(request_line);

    if (parts.size() < 2) {
        throw std::runtime_error("Malformed HTTP request line.");
    }

    HttpRequest request;
    request.method = parts[0];
    const std::string target = parts[1];
    const std::size_t query_index = target.find('?');
    if (query_index == std::string::npos) {
        request.path = target;
    } else {
        request.path = target.substr(0, query_index);
        request.query = parse_query_string(target.substr(query_index + 1));
    }
    return request;
}

std::string status_text(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        default:
            return "Internal Server Error";
    }
}

std::string response_to_http(const HttpResponse& response) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.status << ' ' << status_text(response.status) << "\r\n"
           << "Content-Type: application/json; charset=utf-8\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
           << "Access-Control-Allow-Headers: Content-Type\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << response.body;
    return stream.str();
}

bool query_truthy(const std::unordered_map<std::string, std::string>& query, std::string_view key) {
    const auto it = query.find(std::string(key));
    if (it == query.end()) {
        return false;
    }
    return it->second == "1" || it->second == "true" || it->second == "yes";
}

std::optional<std::string> query_value(const std::unordered_map<std::string, std::string>& query, std::string_view key) {
    const auto it = query.find(std::string(key));
    if (it == query.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second;
}

HttpResponse json_response(int status, std::string body) {
    return HttpResponse {status, std::move(body)};
}

std::size_t parse_size_t_query(std::string_view value, std::string_view field_name) {
    try {
        std::size_t consumed = 0;
        const std::size_t parsed = std::stoull(std::string(value), &consumed);
        if (consumed != value.size()) {
            throw BadRequestError("");
        }
        return parsed;
    } catch (...) {
        throw BadRequestError(std::string(field_name) + " must be a non-negative integer.");
    }
}

HistoryMode parse_history_mode_query(std::string_view value) {
    if (value == "full") {
        return HistoryMode::kFull;
    }
    if (value == "first-parent") {
        return HistoryMode::kFirstParent;
    }
    throw BadRequestError("historyMode must be either full or first-parent.");
}

void send_response_body(int client_fd, std::string_view wire) {
    std::size_t total_sent = 0;
    while (total_sent < wire.size()) {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        const ssize_t sent = ::send(
            client_fd,
            wire.data() + total_sent,
            wire.size() - total_sent,
            flags);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (sent == 0) {
            break;
        }
        total_sent += static_cast<std::size_t>(sent);
    }
}

std::string config_to_json(const ApiServerConfig& config) {
    std::ostringstream stream;
    stream << "{"
           << "\"port\":" << config.port << ","
           << "\"defaultRepoPath\":";
    if (config.default_repo_path.has_value()) {
        stream << json_quote(config.default_repo_path.value());
    } else {
        stream << "null";
    }
    stream << ",\"sampleMode\":" << (config.sample_mode ? "true" : "false") << "}";
    return stream.str();
}

}  // namespace

ApiServer::ApiServer(ApiServerConfig config)
    : config_(std::move(config)) {}

void ApiServer::serve() {
    const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create socket.");
    }

    int opt = 1;
    ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(config_.port));

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(server_fd);
        throw std::runtime_error("Failed to bind port " + std::to_string(config_.port) + ": " + std::strerror(errno));
    }

    if (::listen(server_fd, 16) < 0) {
        ::close(server_fd);
        throw std::runtime_error("Failed to listen on socket.");
    }

    while (true) {
        sockaddr_in client_address {};
        socklen_t client_length = sizeof(client_address);
        const int client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client_fd < 0) {
            continue;
        }

#ifdef SO_NOSIGPIPE
        ::setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

        std::thread([this, client_fd]() {
            std::string raw_request;
            char buffer[4096];
            while (raw_request.find("\r\n\r\n") == std::string::npos) {
                const ssize_t bytes_read = ::read(client_fd, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }
                raw_request.append(buffer, static_cast<std::size_t>(bytes_read));
                if (raw_request.size() > 64 * 1024) {
                    break;
                }
            }

            HttpResponse response;
            try {
                const HttpRequest request = parse_request(raw_request);
                if (request.method == "OPTIONS") {
                    response = json_response(204, "");
                } else if (request.method != "GET") {
                    response = json_response(405, error_to_json("Only GET and OPTIONS are supported.", 405));
                } else if (request.path == "/" || request.path == "/api" || request.path == "/api/v1") {
                    response = json_response(200, "{\"service\":\"defect-intelligence-api\",\"message\":\"Use /api/v1/report or related endpoints.\"}");
                } else if (request.path == "/api/v1/health") {
                    response = json_response(200, health_to_json());
                } else if (request.path == "/api/v1/config") {
                    response = json_response(200, config_to_json(config_));
                } else {
                    ScanConfig scan_config;
                    scan_config.sample_mode = config_.sample_mode || query_truthy(request.query, "sample");
                    if (const auto repo = query_value(request.query, "repoPath"); repo.has_value()) {
                        scan_config.repo_path = *repo;
                    } else if (config_.default_repo_path.has_value()) {
                        scan_config.repo_path = *config_.default_repo_path;
                    }
                    if (scan_config.repo_path.empty() && !scan_config.sample_mode) {
                        response = json_response(400, error_to_json("repoPath is required unless sample=true or a default repo is configured.", 400));
                    } else {
                        if (const auto revision = query_value(request.query, "rev"); revision.has_value()) {
                            scan_config.revision = *revision;
                        }
                        if (const auto history_mode = query_value(request.query, "historyMode"); history_mode.has_value()) {
                            scan_config.history_mode = parse_history_mode_query(*history_mode);
                        }
                        scan_config.since = query_value(request.query, "since");
                        scan_config.until = query_value(request.query, "until");
                        if (const auto max_commits = query_value(request.query, "maxCommits"); max_commits.has_value()) {
                            scan_config.max_commits = parse_size_t_query(*max_commits, "maxCommits");
                        }

                        const std::string cache_key = scan_config_cache_key(scan_config);
                        ScanReport report;
                        {
                            std::lock_guard<std::mutex> lock(cache_mutex_);
                            const auto cached = cache_.find(cache_key);
                            if (cached != cache_.end()) {
                                report = cached->second;
                            }
                        }
                        if (report.generated_at.empty()) {
                            report = analytics_.analyze(scan_config);
                            std::lock_guard<std::mutex> lock(cache_mutex_);
                            cache_[cache_key] = report;
                        }

                        if (request.path == "/api/v1/report") {
                            response = json_response(200, to_json(report));
                        } else if (request.path == "/api/v1/summary") {
                            response = json_response(200, summary_to_json(report));
                        } else if (request.path == "/api/v1/components") {
                            response = json_response(200, components_to_json(report));
                        } else if (request.path == "/api/v1/authors") {
                            response = json_response(200, authors_to_json(report));
                        } else if (request.path == "/api/v1/commits") {
                            response = json_response(200, commits_to_json(report, query_value(request.query, "component")));
                        } else if (request.path == "/api/v1/insights") {
                            const InsightSummary summary = InsightComposer {}.compose(report, query_value(request.query, "component"));
                            response = json_response(200, insight_summary_to_json(summary));
                        } else {
                            response = json_response(404, error_to_json("Unknown endpoint.", 404));
                        }
                    }
                }
            } catch (const BadRequestError& error) {
                response = json_response(400, error_to_json(error.what(), 400));
            } catch (const std::exception& error) {
                response = json_response(500, error_to_json(error.what(), 500));
            }

            const std::string wire = response_to_http(response);
            send_response_body(client_fd, wire);
            ::close(client_fd);
        }).detach();
    }
}

}  // namespace di

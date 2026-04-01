#pragma once

#include <string>
#include <utility>
#include <vector>

namespace mini_git {

struct RemoteSnapshot {
    std::string current_branch;
    std::vector<std::pair<std::string, std::string>> refs;
    std::string stash;
    std::vector<std::pair<std::string, std::string>> objects;
};

std::string hex_encode(const std::string& data);
std::string hex_decode(const std::string& hex);
std::string url_encode(const std::string& value);
std::string serialize_remote_snapshot(const RemoteSnapshot& snapshot);
RemoteSnapshot parse_remote_snapshot(const std::string& payload);
std::string http_get_text(const std::string& url);
std::string http_post_text(const std::string& url, const std::string& body);

}  // namespace mini_git

#include "index.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace mini_git {

Index::Index(const Config& config) : config_(config) {}

std::vector<IndexEntry> Index::read_entries() const {
    if (!fs::exists(config_.index_file)) {
        return {};
    }

    std::ifstream input(config_.index_file);
    if (!input) {
        throw std::runtime_error("Could not open index file: " + config_.index_file.string());
    }

    std::vector<IndexEntry> entries;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        IndexEntry entry;
        if (stream >> entry.path >> entry.sha1) {
            entries.push_back(entry);
        }
    }
    return entries;
}

void Index::write_entries(const std::vector<IndexEntry>& entries) const {
    fs::create_directories(config_.index_file.parent_path());
    std::ofstream output(config_.index_file);
    if (!output) {
        throw std::runtime_error("Could not write index file: " + config_.index_file.string());
    }

    for (const auto& entry : entries) {
        output << entry.path << ' ' << entry.sha1 << '\n';
    }
}

void Index::update_entry(const std::string& path, const std::string& sha1) const {
    auto entries = read_entries();
    for (auto& entry : entries) {
        if (entry.path == path) {
            entry.sha1 = sha1;
            write_entries(entries);
            return;
        }
    }

    entries.push_back({path, sha1});
    write_entries(entries);
}

}  // namespace mini_git

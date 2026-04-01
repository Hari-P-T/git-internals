#include "working_tree.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "sha1.hpp"

namespace fs = std::filesystem;

namespace mini_git {

namespace {

std::string read_binary_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_binary_file(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}

}  // namespace

WorkingTree::WorkingTree(const Config& config, const ObjectStore& object_store, const Index& index)
    : config_(config), object_store_(object_store), index_(index) {}

std::string WorkingTree::hash_file(const fs::path& path) const {
    const std::string data = read_binary_file(path);
    const std::string store = "blob " + std::to_string(data.size()) + '\0' + data;
    return Sha1::hex_digest(store);
}

std::vector<IndexEntry> WorkingTree::scan_files() const {
    std::vector<IndexEntry> entries;
    if (!fs::exists(config_.working_dir)) {
        return entries;
    }

    for (fs::recursive_directory_iterator iterator(config_.working_dir), end; iterator != end; ++iterator) {
        if (iterator->is_directory() && iterator->path().filename() == ".git") {
            iterator.disable_recursion_pending();
            continue;
        }

        if (iterator->is_regular_file()) {
            entries.push_back({
                relative_repo_path(config_.working_dir, iterator->path()),
                hash_file(iterator->path()),
            });
        }
    }

    return entries;
}

void WorkingTree::clear() const {
    if (!fs::exists(config_.working_dir)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(config_.working_dir)) {
        if (entry.path().filename() == ".git") {
            continue;
        }
        fs::remove_all(entry.path());
    }
}

void WorkingTree::restore_tree(const std::string& tree_sha1, const fs::path& base_path) const {
    for (const auto& entry : object_store_.read_tree_entries(tree_sha1)) {
        const fs::path full_path = base_path / entry.name;
        if (entry.mode == "40000") {
            fs::create_directories(full_path);
            restore_tree(entry.sha1, full_path);
            continue;
        }

        const std::string content = object_store_.read_object_body(entry.sha1);
        fs::create_directories(full_path.parent_path());
        write_binary_file(full_path, content);
    }
}

void WorkingTree::rebuild_index() const {
    index_.write_entries(scan_files());
}

}  // namespace mini_git

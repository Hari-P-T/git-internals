#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.hpp"
#include "index.hpp"
#include "object_store.hpp"

namespace mini_git {

class WorkingTree {
  public:
    WorkingTree(const Config& config, const ObjectStore& object_store, const Index& index);

    std::string hash_file(const std::filesystem::path& path) const;
    std::vector<IndexEntry> scan_files() const;
    void clear() const;
    void restore_tree(const std::string& tree_sha1, const std::filesystem::path& base_path) const;
    void rebuild_index() const;

  private:
    const Config& config_;
    const ObjectStore& object_store_;
    const Index& index_;
};

}  // namespace mini_git

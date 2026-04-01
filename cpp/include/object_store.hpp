#pragma once

#include <string>
#include <vector>

#include "config.hpp"

namespace mini_git {

struct TreeObjectEntry {
    std::string mode;
    std::string name;
    std::string sha1;
};

class ObjectStore {
  public:
    explicit ObjectStore(const Config& config);

    std::string write_object(const std::string& data, const std::string& type) const;
    std::string read_object_body(const std::string& sha1) const;
    std::string read_object_text(const std::string& sha1) const;
    std::vector<TreeObjectEntry> read_tree_entries(const std::string& sha1) const;

  private:
    std::string object_path_text(const std::string& sha1) const;
    std::string read_object_full(const std::string& sha1) const;

    const Config& config_;
};

}  // namespace mini_git

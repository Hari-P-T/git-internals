#pragma once

#include <string>
#include <vector>

#include "config.hpp"

namespace mini_git {

struct IndexEntry {
    std::string path;
    std::string sha1;
};

class Index {
  public:
    explicit Index(const Config& config);

    std::vector<IndexEntry> read_entries() const;
    void write_entries(const std::vector<IndexEntry>& entries) const;
    void update_entry(const std::string& path, const std::string& sha1) const;

  private:
    const Config& config_;
};

}  // namespace mini_git

#pragma once

#include <filesystem>
#include <string>

#include "config.hpp"

namespace mini_git {

class Refs {
  public:
    explicit Refs(const Config& config);

    std::string read_head_ref() const;
    std::string current_branch() const;
    std::filesystem::path branch_path(const std::string& name) const;
    bool branch_exists(const std::string& name) const;
    std::string read_branch(const std::string& name) const;
    void write_branch(const std::string& name, const std::string& commit) const;
    void set_head_branch(const std::string& name) const;
    bool ref_exists(const std::string& ref) const;
    std::string read_ref_value(const std::string& ref) const;

  private:
    const Config& config_;
};

}  // namespace mini_git

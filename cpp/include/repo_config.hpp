#pragma once

#include <optional>
#include <string>
#include <vector>

#include "config.hpp"

namespace mini_git {

struct RemoteConfig {
    std::string name;
    std::string url;
    std::string repo;
};

class RepoConfig {
  public:
    explicit RepoConfig(const Config& config);

    std::vector<RemoteConfig> list_remotes() const;
    std::optional<RemoteConfig> get_remote(const std::string& name) const;
    void add_remote(const RemoteConfig& remote) const;

  private:
    const Config& config_;
};

}  // namespace mini_git

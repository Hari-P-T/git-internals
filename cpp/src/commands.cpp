#include <algorithm>
#include "commands.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.hpp"
#include "index.hpp"
#include "object_store.hpp"
#include "repo_config.hpp"
#include "remote.hpp"
#include "refs.hpp"
#include "sha1.hpp"
#include "working_tree.hpp"

namespace fs = std::filesystem;

namespace mini_git {

namespace {

struct OrderedTreeEntry;

struct OrderedTreeNode {
    std::vector<OrderedTreeEntry> entries;
};

struct OrderedTreeEntry {
    std::string name;
    bool is_tree;
    std::string sha1;
    std::unique_ptr<OrderedTreeNode> child;
};

void write_text_exact(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output << content;
}

std::string read_binary_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void ensure_repo_layout(const Config& config) {
    fs::create_directories(config.working_dir);
    fs::create_directories(config.objects_dir);
    fs::create_directories(config.heads_dir);
    fs::create_directories(config.tags_dir);

    if (!fs::exists(config.head_file)) {
        write_text_exact(config.head_file, "ref: refs/heads/main\n");
    }

    const fs::path main_branch = config.heads_dir / "main";
    if (!fs::exists(main_branch)) {
        write_text_exact(main_branch, "");
    }

    if (!fs::exists(config.index_file)) {
        write_text_exact(config.index_file, "");
    }

    const fs::path config_file = config.git_dir / "config";
    if (!fs::exists(config_file)) {
        write_text_exact(
            config_file,
            "[core]\n"
            "    repositoryformatversion = 0\n"
            "    filemode = true\n"
            "    bare = false\n"
        );
    }

    const fs::path description_file = config.git_dir / "description";
    if (!fs::exists(description_file)) {
        write_text_exact(
            description_file,
            "Unnamed repository; edit this file 'description' to name the repository.\n"
        );
    }
}

OrderedTreeNode build_structure(const std::vector<IndexEntry>& entries) {
    OrderedTreeNode root;

    for (const auto& entry : entries) {
        OrderedTreeNode* current = &root;
        std::vector<std::string> parts;
        std::stringstream stream(entry.path);
        std::string part;
        while (std::getline(stream, part, '/')) {
            parts.push_back(part);
        }

        for (std::size_t index = 0; index < parts.size(); ++index) {
            const bool is_last = index + 1 == parts.size();
            OrderedTreeEntry* found = nullptr;
            for (auto& candidate : current->entries) {
                if (candidate.name == parts[index]) {
                    found = &candidate;
                    break;
                }
            }

            if (!found) {
                OrderedTreeEntry created;
                created.name = parts[index];
                created.is_tree = !is_last;
                if (!is_last) {
                    created.child = std::make_unique<OrderedTreeNode>();
                } else {
                    created.sha1 = entry.sha1;
                }
                current->entries.push_back(std::move(created));
                found = &current->entries.back();
            } else if (is_last) {
                found->sha1 = entry.sha1;
            }

            if (!is_last) {
                current = found->child.get();
            }
        }
    }

    return root;
}

std::string write_tree(ObjectStore& object_store, const OrderedTreeNode& node) {
    std::string content;

    for (const auto& entry : node.entries) {
        if (entry.is_tree) {
            const std::string tree_hash = write_tree(object_store, *entry.child);
            const auto bytes = Sha1::hex_to_bytes(tree_hash);
            content += "40000 " + entry.name + '\0';
            content.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        } else {
            const auto bytes = Sha1::hex_to_bytes(entry.sha1);
            content += "100644 " + entry.name + '\0';
            content.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
    }

    return object_store.write_object(content, "tree");
}

std::string create_blob(ObjectStore& object_store, const fs::path& file_path) {
    return object_store.write_object(read_binary_file(file_path), "blob");
}

std::string read_parent_ref(const Config& config, const std::string& ref) {
    const fs::path path = config.git_dir / ref;
    if (!fs::exists(path)) {
        return "";
    }
    return trim_copy(read_text_file(path));
}

std::string get_tree_hash(const ObjectStore& object_store, const std::string& commit_hash) {
    const std::string data = object_store.read_object_text(commit_hash);
    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("tree ", 0) == 0) {
            return line.substr(5);
        }
    }
    throw std::runtime_error("Missing tree line in commit");
}

std::string get_first_parent(const ObjectStore& object_store, const std::string& commit_hash) {
    const std::string data = object_store.read_object_text(commit_hash);
    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("parent ", 0) == 0) {
            return line.substr(7);
        }
    }
    return "";
}

std::string read_commit_message_body(const ObjectStore& object_store, const std::string& commit_hash) {
    const std::string data = object_store.read_object_text(commit_hash);
    const std::size_t separator = data.find("\n\n");
    if (separator == std::string::npos) {
        return "";
    }
    return data.substr(separator + 2);
}

std::pair<std::string, std::string> parse_commit_for_log(
    const ObjectStore& object_store,
    const std::string& commit_hash
) {
    const std::string data = object_store.read_object_text(commit_hash);
    std::string parent;
    std::string message;

    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("parent ", 0) == 0 && parent.empty()) {
            parent = line.substr(7);
        }
        if (line.empty()) {
            const std::size_t separator = data.find("\n\n");
            if (separator != std::string::npos) {
                message = trim_copy(data.substr(separator + 2));
            }
            break;
        }
    }

    return {parent, message};
}

std::vector<IndexEntry> read_tree(const ObjectStore& object_store, const std::string& tree_hash, const std::string& base) {
    std::vector<IndexEntry> result;

    for (const auto& entry : object_store.read_tree_entries(tree_hash)) {
        const std::string path = base.empty() ? entry.name : base + "/" + entry.name;
        if (entry.mode == "40000") {
            auto nested = read_tree(object_store, entry.sha1, path);
            result.insert(result.end(), nested.begin(), nested.end());
        } else {
            result.push_back({path, entry.sha1});
        }
    }

    return result;
}

std::unordered_map<std::string, std::string> to_lookup(const std::vector<IndexEntry>& entries) {
    std::unordered_map<std::string, std::string> lookup;
    for (const auto& entry : entries) {
        lookup[entry.path] = entry.sha1;
    }
    return lookup;
}

std::vector<std::string> collect_commits_until(
    const ObjectStore& object_store,
    const std::string& base,
    const std::string& start
) {
    std::vector<std::string> commits;
    std::string current = start;
    while (!current.empty() && current != base) {
        commits.push_back(current);
        current = get_first_parent(object_store, current);
    }
    std::reverse(commits.begin(), commits.end());
    return commits;
}

std::vector<std::pair<std::string, std::string>> collect_branch_refs(const Config& config) {
    std::vector<std::pair<std::string, std::string>> refs;
    if (!fs::exists(config.heads_dir)) {
        return refs;
    }

    for (const auto& entry : fs::directory_iterator(config.heads_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        refs.push_back({
            entry.path().filename().string(),
            trim_copy(read_text_file(entry.path())),
        });
    }
    return refs;
}

std::vector<std::pair<std::string, std::string>> collect_objects(const Config& config) {
    std::vector<std::pair<std::string, std::string>> objects;
    if (!fs::exists(config.objects_dir)) {
        return objects;
    }

    for (fs::recursive_directory_iterator iterator(config.objects_dir), end; iterator != end; ++iterator) {
        if (!iterator->is_regular_file()) {
            continue;
        }
        objects.push_back({
            relative_repo_path(config.objects_dir, iterator->path()),
            hex_encode(read_binary_file(iterator->path())),
        });
    }
    return objects;
}

bool looks_like_url(const std::string& value) {
    return value.find("://") != std::string::npos;
}

void write_binary_file(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void restore_selected_branch(
    const Config& config,
    const Refs& refs,
    const ObjectStore& object_store,
    const Index& index,
    const WorkingTree& working_tree,
    const std::string& branch
) {
    refs.set_head_branch(branch);
    const fs::path branch_path = config.heads_dir / branch;
    const std::string commit = fs::exists(branch_path) ? trim_copy(read_text_file(branch_path)) : "";

    if (commit.empty()) {
        working_tree.clear();
        index.write_entries({});
        return;
    }

    const std::string tree = get_tree_hash(object_store, commit);
    working_tree.clear();
    working_tree.restore_tree(tree, config.working_dir);
    working_tree.rebuild_index();
}

void git_init(const Config& config) {
    ensure_repo_layout(config);
    std::cout << "Initialized empty Mini Git repository.\n";
}

void list_remotes(const RepoConfig& repo_config) {
    for (const auto& remote : repo_config.list_remotes()) {
        std::cout << remote.name << '\n';
    }
}

void add_remote(const RepoConfig& repo_config, const std::string& name, const std::string& url, const std::string& repo_name) {
    repo_config.add_remote({name, url, repo_name});
    std::cout << "Added remote " << name << '\n';
}

void show_remote(const RepoConfig& repo_config, const std::string& name) {
    const auto remote = repo_config.get_remote(name);
    if (!remote) {
        std::cout << "Remote not found: " << name << '\n';
        return;
    }

    std::cout << "name " << remote->name << '\n';
    std::cout << "url " << remote->url << '\n';
    std::cout << "repo " << remote->repo << '\n';
}

void add_directory(
    const Config& config,
    ObjectStore& object_store,
    const Index& index,
    const std::string& dir_path
) {
    const fs::path base = config.working_dir / dir_path;
    if (!fs::exists(base)) {
        return;
    }

    for (fs::recursive_directory_iterator iterator(base), end; iterator != end; ++iterator) {
        if (iterator->is_directory() && iterator->path().filename() == ".git") {
            iterator.disable_recursion_pending();
            continue;
        }

        if (!iterator->is_regular_file()) {
            continue;
        }

        const std::string rel_path = relative_repo_path(config.working_dir, iterator->path());
        const std::string blob = create_blob(object_store, iterator->path());
        index.update_entry(rel_path, blob);
        std::cout << "Staged: " << rel_path << '\n';
    }
}

void add_file(
    const Config& config,
    ObjectStore& object_store,
    const Index& index,
    const std::string& file_path
) {
    const fs::path full_path = config.working_dir / file_path;

    if (!fs::exists(full_path)) {
        std::cout << "File not found: " << file_path << '\n';
        return;
    }

    if (fs::is_directory(full_path)) {
        add_directory(config, object_store, index, file_path);
        return;
    }

    const std::string rel_path = relative_repo_path(config.working_dir, full_path);
    const std::string blob = create_blob(object_store, full_path);
    index.update_entry(rel_path, blob);
    std::cout << "Staged: " << rel_path << '\n';
}

void mini_git_add(
    const Config& config,
    ObjectStore& object_store,
    const Index& index,
    const std::vector<std::string>& paths
) {
    if (paths.empty() || (paths.size() == 1 && paths[0] == ".")) {
        add_directory(config, object_store, index, ".");
        return;
    }

    for (const auto& path : paths) {
        add_file(config, object_store, index, path);
    }
}

void create_commit(
    const Config& config,
    ObjectStore& object_store,
    const Index& index,
    const Refs& refs,
    const std::string& message
) {
    const auto entries = index.read_entries();
    const OrderedTreeNode structure = build_structure(entries);
    const std::string root_tree = write_tree(object_store, structure);

    const std::string ref = refs.read_head_ref();
    const std::string parent = read_parent_ref(config, ref);
    const auto timestamp = current_timestamp();

    std::vector<std::string> lines;
    lines.push_back("tree " + root_tree);
    if (!parent.empty()) {
        lines.push_back("parent " + parent);
    }
    lines.push_back("author You <you@example.com> " + std::to_string(timestamp));
    lines.push_back("committer You <you@example.com> " + std::to_string(timestamp));
    lines.push_back("");
    lines.push_back(message);

    std::ostringstream buffer;
    for (std::size_t index_value = 0; index_value < lines.size(); ++index_value) {
        if (index_value > 0) {
            buffer << '\n';
        }
        buffer << lines[index_value];
    }

    const std::string commit_hash = object_store.write_object(buffer.str(), "commit");
    write_text_exact(config.git_dir / ref, commit_hash);
    std::cout << "Committed: " << commit_hash << '\n';
}

void git_status(
    const Refs& refs,
    const ObjectStore& object_store,
    const Index& index,
    const WorkingTree& working_tree
) {
    const auto index_entries = index.read_entries();
    const auto index_lookup = to_lookup(index_entries);
    const auto working_entries = working_tree.scan_files();

    std::unordered_map<std::string, std::string> head_tree;
    try {
        const std::string head_commit = refs.read_ref_value(refs.read_head_ref());
        const std::string tree_hash = get_tree_hash(object_store, head_commit);
        head_tree = to_lookup(read_tree(object_store, tree_hash, ""));
    } catch (...) {
    }

    std::cout << "=== Staged ===\n";
    for (const auto& entry : index_entries) {
        const auto iterator = head_tree.find(entry.path);
        if (iterator == head_tree.end() || iterator->second != entry.sha1) {
            std::cout << entry.path << '\n';
        }
    }

    std::cout << "\n=== Modified ===\n";
    for (const auto& entry : working_entries) {
        const auto iterator = index_lookup.find(entry.path);
        if (iterator != index_lookup.end() && iterator->second != entry.sha1) {
            std::cout << entry.path << '\n';
        }
    }

    std::cout << "\n=== Untracked ===\n";
    for (const auto& entry : working_entries) {
        if (index_lookup.find(entry.path) == index_lookup.end()) {
            std::cout << entry.path << '\n';
        }
    }
}

void git_log(const Refs& refs, const ObjectStore& object_store) {
    std::string commit = refs.read_ref_value(refs.read_head_ref());
    while (!commit.empty()) {
        const auto [parent, message] = parse_commit_for_log(object_store, commit);
        std::cout << "commit " << commit << '\n';
        std::cout << "     " << message << "\n\n";
        commit = parent;
    }
}

void list_branches(const Refs& refs, const Config& config) {
    const std::string current = refs.current_branch();
    for (const auto& entry : fs::directory_iterator(config.heads_dir)) {
        const std::string name = entry.path().filename().string();
        const std::string prefix = name == current ? "* " : "  ";
        std::cout << prefix << name << '\n';
    }
}

void create_branch(const Refs& refs, const std::string& name) {
    if (refs.branch_exists(name)) {
        std::cout << "Branch already exists\n";
        return;
    }

    const std::string commit = refs.read_branch(refs.current_branch());
    refs.write_branch(name, commit);
    std::cout << "Created branch " << name << '\n';
}

void checkout_branch(
    const Config& config,
    const Refs& refs,
    const ObjectStore& object_store,
    const WorkingTree& working_tree,
    const std::string& branch
) {
    if (!refs.branch_exists(branch)) {
        std::cout << "Branch does not exist\n";
        return;
    }

    refs.set_head_branch(branch);
    const std::string commit = refs.read_branch(branch);
    const std::string tree = get_tree_hash(object_store, commit);
    working_tree.clear();
    working_tree.restore_tree(tree, config.working_dir);
    working_tree.rebuild_index();
    std::cout << "Switched to " << branch << '\n';
}

void reset_commit(
    const Config& config,
    const Refs& refs,
    const ObjectStore& object_store,
    const WorkingTree& working_tree,
    const std::string& commit,
    const std::string& mode
) {
    refs.write_branch(refs.current_branch(), commit);

    if (mode == "mixed" || mode == "hard") {
        const std::string tree = get_tree_hash(object_store, commit);
        working_tree.clear();
        working_tree.restore_tree(tree, config.working_dir);
        working_tree.rebuild_index();
    }

    std::cout << "Reset " << mode << " to " << commit << '\n';
}

void merge_branch(
    const Refs& refs,
    ObjectStore& object_store,
    const std::string& branch_to_merge
) {
    const std::string current = refs.current_branch();
    const std::string parent1 = refs.read_branch(current);
    const std::string parent2 = refs.read_branch(branch_to_merge);
    const std::string tree = get_tree_hash(object_store, parent1);

    const std::string commit_data =
        "tree " + tree + "\n"
        "parent " + parent1 + "\n"
        "parent " + parent2 + "\n\n"
        "Merge branch '" + branch_to_merge + "'\n";

    const std::string sha1 = object_store.write_object(commit_data, "commit");
    refs.write_branch(current, sha1);
    std::cout << "Merged " << branch_to_merge << " into " << current << '\n';
}

void rebase_branch(
    const Refs& refs,
    ObjectStore& object_store,
    const std::string& target_branch
) {
    const std::string current = refs.current_branch();
    const std::string head_commit = refs.read_branch(current);
    const std::string target_commit = refs.read_branch(target_branch);
    const auto commits_to_replay = collect_commits_until(object_store, target_commit, head_commit);

    std::string new_parent = target_commit;
    for (const auto& old_commit : commits_to_replay) {
        const std::string tree = get_tree_hash(object_store, old_commit);
        const std::string message = read_commit_message_body(object_store, old_commit);
        const std::string commit_data =
            "tree " + tree + "\n"
            "parent " + new_parent + "\n\n" +
            message + "\n";
        new_parent = object_store.write_object(commit_data, "commit");
    }

    refs.write_branch(current, new_parent);
    std::cout << "Rebased " << current << " onto " << target_branch << '\n';
}

void stash_push(
    const Config& config,
    const Refs& refs,
    ObjectStore& object_store,
    const Index& index,
    const WorkingTree& working_tree
) {
    const auto entries = index.read_entries();
    const OrderedTreeNode structure = build_structure(entries);
    const std::string root_tree = write_tree(object_store, structure);

    const std::string ref = refs.read_head_ref();
    std::string head;
    if (refs.ref_exists(ref)) {
        head = refs.read_ref_value(ref);
    }

    const auto timestamp = current_timestamp();
    std::vector<std::string> lines;
    lines.push_back("tree " + root_tree);
    if (!head.empty()) {
        lines.push_back("parent " + head);
    }
    lines.push_back("author stash <stash@example.com> " + std::to_string(timestamp));
    lines.push_back("");
    lines.push_back("WIP Stash");

    std::ostringstream buffer;
    for (std::size_t index_value = 0; index_value < lines.size(); ++index_value) {
        if (index_value > 0) {
            buffer << '\n';
        }
        buffer << lines[index_value];
    }

    const std::string stash_commit = object_store.write_object(buffer.str(), "commit");
    write_text_exact(config.stash_file, stash_commit);

    if (!head.empty()) {
        const std::string tree = get_tree_hash(object_store, head);
        working_tree.clear();
        working_tree.restore_tree(tree, config.working_dir);
        working_tree.rebuild_index();
    }

    std::cout << "Saved working directory to stash: " << stash_commit << '\n';
}

void stash_apply(const Config& config, const ObjectStore& object_store, const WorkingTree& working_tree) {
    if (!fs::exists(config.stash_file)) {
        std::cout << "No stash found\n";
        return;
    }

    const std::string stash_commit = trim_copy(read_text_file(config.stash_file));
    const std::string tree = get_tree_hash(object_store, stash_commit);
    working_tree.clear();
    working_tree.restore_tree(tree, config.working_dir);
    working_tree.rebuild_index();
    std::cout << "Applied stash: " << stash_commit << '\n';
}

void stash_list(const Config& config) {
    if (!fs::exists(config.stash_file)) {
        std::cout << "No stash entries\n";
        return;
    }

    const std::string commit = trim_copy(read_text_file(config.stash_file));
    std::cout << "stash@{0}: " << commit << '\n';
}

void push_remote(const Config& config, const Refs& refs, const std::string& remote_url, const std::string& repo_name) {
    RemoteSnapshot snapshot;
    snapshot.current_branch = refs.current_branch();
    snapshot.refs = collect_branch_refs(config);
    snapshot.objects = collect_objects(config);
    if (fs::exists(config.stash_file)) {
        snapshot.stash = trim_copy(read_text_file(config.stash_file));
    }

    const std::string url = remote_url + "/api/repos/" + url_encode(repo_name) + "/push";
    http_post_text(url, serialize_remote_snapshot(snapshot));

    std::cout
        << "Pushed " << snapshot.objects.size()
        << " objects and " << snapshot.refs.size()
        << " branches to " << repo_name << '\n';
}

void pull_remote(
    const Config& config,
    const Refs& refs,
    const ObjectStore& object_store,
    const Index& index,
    const WorkingTree& working_tree,
    const std::string& remote_url,
    const std::string& repo_name,
    const std::string& requested_branch
) {
    ensure_repo_layout(config);

    const std::string url = remote_url + "/api/repos/" + url_encode(repo_name) + "/pull";
    const RemoteSnapshot snapshot = parse_remote_snapshot(http_get_text(url));

    for (const auto& [relative_path, content] : snapshot.objects) {
        write_binary_file(config.objects_dir / relative_path, hex_decode(content));
    }

    for (const auto& [branch_name, commit] : snapshot.refs) {
        refs.write_branch(branch_name, commit);
    }

    if (snapshot.stash.empty()) {
        fs::remove(config.stash_file);
    } else {
        write_text_exact(config.stash_file, snapshot.stash);
    }

    const std::string branch = requested_branch.empty() ? snapshot.current_branch : requested_branch;
    if (!branch.empty()) {
        const bool branch_exists = std::any_of(
            snapshot.refs.begin(),
            snapshot.refs.end(),
            [&](const auto& entry) { return entry.first == branch; }
        );

        if (!branch_exists) {
            throw std::runtime_error("Remote branch not found: " + branch);
        }

        restore_selected_branch(config, refs, object_store, index, working_tree, branch);
    }

    std::cout
        << "Pulled " << snapshot.objects.size()
        << " objects and " << snapshot.refs.size()
        << " branches from " << repo_name;
    if (!branch.empty()) {
        std::cout << " into " << branch;
    }
    std::cout << '\n';
}

RemoteConfig resolve_push_target(const RepoConfig& repo_config, const std::vector<std::string>& args) {
    if (args.size() == 1 && !looks_like_url(args[0])) {
        const auto remote = repo_config.get_remote(args[0]);
        if (!remote) {
            throw std::runtime_error("Remote not found: " + args[0]);
        }
        return *remote;
    }

    if (args.size() == 2 && looks_like_url(args[0])) {
        return {"", args[0], args[1]};
    }

    throw std::runtime_error("Usage: push <remote> | push <url> <repo>");
}

std::pair<RemoteConfig, std::string> resolve_pull_target(const RepoConfig& repo_config, const std::vector<std::string>& args) {
    if (!args.empty() && !looks_like_url(args[0])) {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("Usage: pull <remote> [branch]");
        }

        const auto remote = repo_config.get_remote(args[0]);
        if (!remote) {
            throw std::runtime_error("Remote not found: " + args[0]);
        }
        return {*remote, args.size() == 2 ? args[1] : ""};
    }

    if (!args.empty() && looks_like_url(args[0]) && (args.size() == 2 || args.size() == 3)) {
        return {{"", args[0], args[1]}, args.size() == 3 ? args[2] : ""};
    }

    throw std::runtime_error("Usage: pull <remote> [branch] | pull <url> <repo> [branch]");
}

}  // namespace

void print_help() {
    std::cout << R"(
Mini Git - Commands

  init
      Initialize repository

  add <file>|.
      Stage file(s) or entire directory

  commit
      Create a commit

  status
      Show working tree status

  log
      Show commit history

  branch [name]
      List branches or create branch

  checkout <branch>
      Switch branch

  reset --soft|--mixed|--hard <commit>
      Reset current branch

  merge <branch>
      Merge branch into current

  rebase <branch>
      Rebase current branch

  stash push|apply|list
      Stash operations

  remote
      List remotes

  remote add <name> <url> <repo>
      Add a named remote

  remote show <name>
      Show remote details

  push <remote>
      Push objects and refs to a named remote

  push <url> <repo>
      Push objects and refs to remote server

  pull <remote> [branch]
      Pull objects and refs from a named remote

  pull <url> <repo> [branch]
      Pull objects and refs from remote server

  help
      Show this help message
)";
}

int run_cli(const Config& config, int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    ObjectStore object_store(config);
    Index index(config);
    Refs refs(config);
    RepoConfig repo_config(config);
    WorkingTree working_tree(config, object_store, index);

    const std::string command = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);

    try {
        if (command == "init") {
            git_init(config);
        } else if (command == "add") {
            mini_git_add(config, object_store, index, args);
        } else if (command == "commit") {
            std::cout << "Commit message: ";
            std::cout.flush();
            std::string message;
            std::getline(std::cin, message);
            create_commit(config, object_store, index, refs, message);
        } else if (command == "status") {
            git_status(refs, object_store, index, working_tree);
        } else if (command == "log") {
            git_log(refs, object_store);
        } else if (command == "branch") {
            if (args.empty()) {
                list_branches(refs, config);
            } else {
                create_branch(refs, args[0]);
            }
        } else if (command == "checkout") {
            if (args.empty()) {
                std::cout << "Usage: checkout <branch>\n";
                return 0;
            }
            checkout_branch(config, refs, object_store, working_tree, args[0]);
        } else if (command == "reset") {
            if (args.size() != 2) {
                std::cout << "Usage: reset --soft|--mixed|--hard <commit>\n";
                return 0;
            }

            const std::string mode = args[0];
            if (mode != "--soft" && mode != "--mixed" && mode != "--hard") {
                std::cout << "Invalid reset mode\n";
                return 0;
            }

            reset_commit(config, refs, object_store, working_tree, args[1], mode.substr(2));
        } else if (command == "merge") {
            if (args.empty()) {
                std::cout << "Usage: merge <branch>\n";
                return 0;
            }
            merge_branch(refs, object_store, args[0]);
        } else if (command == "rebase") {
            if (args.empty()) {
                std::cout << "Usage: rebase <branch>\n";
                return 0;
            }
            rebase_branch(refs, object_store, args[0]);
        } else if (command == "stash") {
            if (args.empty()) {
                std::cout << "Usage: stash push|apply|list\n";
                return 0;
            }

            if (args[0] == "push") {
                stash_push(config, refs, object_store, index, working_tree);
            } else if (args[0] == "apply") {
                stash_apply(config, object_store, working_tree);
            } else if (args[0] == "list") {
                stash_list(config);
            } else {
                std::cout << "Unknown stash command\n";
            }
        } else if (command == "remote") {
            if (args.empty() || args[0] == "list") {
                list_remotes(repo_config);
            } else if (args[0] == "add") {
                if (args.size() != 4) {
                    std::cout << "Usage: remote add <name> <url> <repo>\n";
                    return 0;
                }
                add_remote(repo_config, args[1], args[2], args[3]);
            } else if (args[0] == "show") {
                if (args.size() != 2) {
                    std::cout << "Usage: remote show <name>\n";
                    return 0;
                }
                show_remote(repo_config, args[1]);
            } else {
                std::cout << "Unknown remote command\n";
            }
        } else if (command == "push") {
            const auto remote = resolve_push_target(repo_config, args);
            push_remote(config, refs, remote.url, remote.repo);
        } else if (command == "pull") {
            const auto [remote, branch] = resolve_pull_target(repo_config, args);
            pull_remote(
                config,
                refs,
                object_store,
                index,
                working_tree,
                remote.url,
                remote.repo,
                branch
            );
        } else if (command == "help") {
            print_help();
        } else {
            std::cout << "Unknown command: " << command << '\n';
            print_help();
        }
    } catch (const std::exception& error) {
        std::cout << "Error: " << error.what() << '\n';
    }

    return 0;
}

}  // namespace mini_git

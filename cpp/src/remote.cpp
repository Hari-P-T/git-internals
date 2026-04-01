#include "remote.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace mini_git {

namespace {

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

fs::path make_temp_path(const std::string& prefix) {
    const auto now = static_cast<long long>(std::time(nullptr));
    return fs::temp_directory_path() /
           (prefix + "-" + std::to_string(getpid()) + "-" + std::to_string(now) + ".tmp");
}

std::string read_text_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_text_file(const fs::path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string execute_curl(const std::string& command, const fs::path& response_path) {
    std::array<char, 128> buffer{};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("Could not start curl");
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int status = pclose(pipe);
    const std::string response_body = fs::exists(response_path) ? read_text_file(response_path) : "";

    if (status != 0) {
        throw std::runtime_error("curl request failed: " + response_body);
    }

    const std::string http_code = output;
    if (http_code.size() < 3) {
        throw std::runtime_error("curl did not return an HTTP status code");
    }

    if (http_code[0] != '2') {
        throw std::runtime_error("HTTP " + http_code + ": " + response_body);
    }

    return response_body;
}

std::string value_or_dash(const std::string& value) {
    return value.empty() ? "-" : value;
}

std::string dash_to_empty(const std::string& value) {
    return value == "-" ? "" : value;
}

}  // namespace

std::string hex_encode(const std::string& data) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char ch : data) {
        stream << std::setw(2) << static_cast<int>(ch);
    }
    return stream.str();
}

std::string hex_decode(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Invalid hex payload length");
    }

    std::string decoded;
    decoded.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const std::string chunk = hex.substr(index, 2);
        decoded.push_back(static_cast<char>(std::stoi(chunk, nullptr, 16)));
    }
    return decoded;
}

std::string url_encode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

std::string serialize_remote_snapshot(const RemoteSnapshot& snapshot) {
    std::vector<std::pair<std::string, std::string>> refs = snapshot.refs;
    std::vector<std::pair<std::string, std::string>> objects = snapshot.objects;

    std::sort(refs.begin(), refs.end());
    std::sort(objects.begin(), objects.end());

    std::ostringstream payload;
    payload << "MINIGIT_REMOTE_V1\n";
    payload << "CURRENT_BRANCH " << value_or_dash(snapshot.current_branch) << '\n';
    payload << "STASH " << value_or_dash(snapshot.stash) << '\n';

    for (const auto& [name, commit] : refs) {
        payload << "REF " << name << ' ' << value_or_dash(commit) << '\n';
    }

    for (const auto& [path, content] : objects) {
        payload << "OBJECT " << path << ' ' << content << '\n';
    }

    payload << "END\n";
    return payload.str();
}

RemoteSnapshot parse_remote_snapshot(const std::string& payload) {
    std::istringstream stream(payload);
    std::string line;
    if (!std::getline(stream, line) || line != "MINIGIT_REMOTE_V1") {
        throw std::runtime_error("Invalid remote snapshot header");
    }

    RemoteSnapshot snapshot;
    while (std::getline(stream, line)) {
        if (line == "END") {
            return snapshot;
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream line_stream(line);
        std::string kind;
        line_stream >> kind;

        if (kind == "CURRENT_BRANCH") {
            std::string branch;
            line_stream >> branch;
            snapshot.current_branch = dash_to_empty(branch);
        } else if (kind == "STASH") {
            std::string stash;
            line_stream >> stash;
            snapshot.stash = dash_to_empty(stash);
        } else if (kind == "REF") {
            std::string name;
            std::string commit;
            line_stream >> name >> commit;
            snapshot.refs.push_back({name, dash_to_empty(commit)});
        } else if (kind == "OBJECT") {
            std::string path;
            std::string content;
            line_stream >> path >> content;
            snapshot.objects.push_back({path, content});
        } else {
            throw std::runtime_error("Unknown remote snapshot record: " + kind);
        }
    }

    throw std::runtime_error("Remote snapshot terminated unexpectedly");
}

std::string http_get_text(const std::string& url) {
    const fs::path response_path = make_temp_path("mini-git-get");
    const std::string command =
        "curl -sS -o " + shell_quote(response_path.string()) +
        " -w '%{http_code}' " + shell_quote(url);

    try {
        const std::string response = execute_curl(command, response_path);
        fs::remove(response_path);
        return response;
    } catch (...) {
        fs::remove(response_path);
        throw;
    }
}

std::string http_post_text(const std::string& url, const std::string& body) {
    const fs::path payload_path = make_temp_path("mini-git-post-body");
    const fs::path response_path = make_temp_path("mini-git-post-response");
    write_text_file(payload_path, body);

    const std::string command =
        "curl -sS -o " + shell_quote(response_path.string()) +
        " -w '%{http_code}' -X POST -H 'Content-Type: text/plain' --data-binary @" +
        shell_quote(payload_path.string()) + ' ' + shell_quote(url);

    try {
        const std::string response = execute_curl(command, response_path);
        fs::remove(payload_path);
        fs::remove(response_path);
        return response;
    } catch (...) {
        fs::remove(payload_path);
        fs::remove(response_path);
        throw;
    }
}

}  // namespace mini_git

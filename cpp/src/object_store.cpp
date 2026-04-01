#include "object_store.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <zlib.h>

#include "sha1.hpp"

namespace fs = std::filesystem;

namespace mini_git {

namespace {

std::string compress_string(const std::string& input) {
    uLongf compressed_length = compressBound(input.size());
    std::string compressed(compressed_length, '\0');

    const int result = compress2(
        reinterpret_cast<Bytef*>(&compressed[0]),
        &compressed_length,
        reinterpret_cast<const Bytef*>(input.data()),
        input.size(),
        Z_DEFAULT_COMPRESSION
    );

    if (result != Z_OK) {
        throw std::runtime_error("zlib compression failed");
    }

    compressed.resize(compressed_length);
    return compressed;
}

std::string decompress_string(const std::string& input) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());

    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed");
    }

    std::string output;
    std::vector<char> buffer(4096);
    int result = Z_OK;

    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&stream, Z_NO_FLUSH);
        output.append(buffer.data(), buffer.size() - stream.avail_out);
    }

    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("zlib inflate failed");
    }

    return output;
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

void write_binary_file(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}

}  // namespace

ObjectStore::ObjectStore(const Config& config) : config_(config) {}

std::string ObjectStore::write_object(const std::string& data, const std::string& type) const {
    const std::string header = type + " " + std::to_string(data.size()) + '\0';
    const std::string store = header + data;
    const std::string sha1 = Sha1::hex_digest(store);

    const fs::path object_dir = config_.objects_dir / sha1.substr(0, 2);
    const fs::path object_file = object_dir / sha1.substr(2);
    fs::create_directories(object_dir);
    write_binary_file(object_file, compress_string(store));
    return sha1;
}

std::string ObjectStore::read_object_body(const std::string& sha1) const {
    const std::string full = read_object_full(sha1);
    const std::size_t separator = full.find('\0');
    if (separator == std::string::npos) {
        throw std::runtime_error("Invalid object header for " + sha1);
    }
    return full.substr(separator + 1);
}

std::string ObjectStore::read_object_text(const std::string& sha1) const {
    return read_object_body(sha1);
}

std::vector<TreeObjectEntry> ObjectStore::read_tree_entries(const std::string& sha1) const {
    const std::string body = read_object_body(sha1);
    std::vector<TreeObjectEntry> entries;

    std::size_t index = 0;
    while (index < body.size()) {
        const std::size_t separator = body.find('\0', index);
        if (separator == std::string::npos || separator + 21 > body.size()) {
            throw std::runtime_error("Invalid tree object: " + sha1);
        }

        const std::string header = body.substr(index, separator - index);
        const std::size_t space = header.find(' ');
        if (space == std::string::npos) {
            throw std::runtime_error("Invalid tree entry header");
        }

        TreeObjectEntry entry;
        entry.mode = header.substr(0, space);
        entry.name = header.substr(space + 1);
        entry.sha1 = Sha1::to_hex(
            reinterpret_cast<const std::uint8_t*>(body.data() + separator + 1),
            20
        );
        entries.push_back(entry);
        index = separator + 21;
    }

    return entries;
}

std::string ObjectStore::object_path_text(const std::string& sha1) const {
    return (config_.objects_dir / sha1.substr(0, 2) / sha1.substr(2)).string();
}

std::string ObjectStore::read_object_full(const std::string& sha1) const {
    return decompress_string(read_binary_file(object_path_text(sha1)));
}

}  // namespace mini_git

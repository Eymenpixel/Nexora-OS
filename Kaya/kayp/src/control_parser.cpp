#include "control_parser.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace kayp {

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::map<std::string, std::string> parse_control(const std::string& path) {
    std::map<std::string, std::string> fields;
    std::ifstream file(path);
    if (!file.is_open()) {
        return fields; // bos map - cagiran taraf kontrol etmeli
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t colon = trimmed.find(':');
        if (colon == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, colon));
        std::string value = trim(trimmed.substr(colon + 1));
        fields[key] = value;
    }
    return fields;
}

std::string get_field(const std::map<std::string, std::string>& fields,
                       const std::string& key,
                       const std::string& def) {
    auto it = fields.find(key);
    if (it == fields.end()) return def;
    return it->second;
}

} // namespace kayp

#include "db.hpp"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <iostream>

namespace kayp {

static void ensure_parent_dir(const std::string& file_path) {
    size_t pos = file_path.find_last_of('/');
    if (pos == std::string::npos) return;
    std::string dir = file_path.substr(0, pos);
    if (dir.empty()) return;

    // basit -p mkdir: yol boyunca her seviyeyi olustur
    std::string current;
    std::stringstream ss(dir);
    std::string part;
    if (dir[0] == '/') current = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        current += part + "/";
        mkdir(current.c_str(), 0755); // varsa hata verir, umursamiyoruz
    }
}

StatusDB::StatusDB(const std::string& db_path) : path_(db_path) {}

void StatusDB::load() {
    packages_.clear();
    std::ifstream file(path_);
    if (!file.is_open()) return; // henuz hic paket kurulmamis olabilir

    InstalledPackage current;
    bool have_current = false;
    std::string line;

    auto flush_current = [&]() {
        if (have_current && !current.name.empty()) {
            packages_[current.name] = current;
        }
        current = InstalledPackage{};
        have_current = false;
    };

    while (std::getline(file, line)) {
        if (line.empty()) {
            flush_current();
            continue;
        }
        if (line[0] == ' ' || line[0] == '\t') {
            // dosya listesi devami: "  /usr/bin/foo"
            std::string trimmed = line.substr(line.find_first_not_of(" \t"));
            if (!trimmed.empty()) current.installed_files.push_back(trimmed);
            continue;
        }

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);

        if (key == "Name") {
            flush_current();
            current.name = value;
            have_current = true;
        } else if (key == "Version") {
            current.version = value;
        } else if (key == "Description") {
            current.description = value;
        }
    }
    flush_current();
}

void StatusDB::save() const {
    ensure_parent_dir(path_);
    std::ofstream file(path_, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "kayp: uyari: status dosyasi yazilamadi: " << path_ << "\n";
        return;
    }

    for (const auto& [name, pkg] : packages_) {
        file << "Name: " << pkg.name << "\n";
        file << "Version: " << pkg.version << "\n";
        file << "Description: " << pkg.description << "\n";
        for (const auto& f : pkg.installed_files) {
            file << "  " << f << "\n";
        }
        file << "\n";
    }
}

bool StatusDB::is_installed(const std::string& name) const {
    return packages_.find(name) != packages_.end();
}

const InstalledPackage* StatusDB::get(const std::string& name) const {
    auto it = packages_.find(name);
    if (it == packages_.end()) return nullptr;
    return &it->second;
}

void StatusDB::add(const InstalledPackage& pkg) {
    packages_[pkg.name] = pkg;
}

void StatusDB::remove(const std::string& name) {
    packages_.erase(name);
}

std::vector<InstalledPackage> StatusDB::all() const {
    std::vector<InstalledPackage> result;
    result.reserve(packages_.size());
    for (const auto& [name, pkg] : packages_) result.push_back(pkg);
    return result;
}

} // namespace kayp

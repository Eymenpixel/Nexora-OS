// db.hpp
// Kurulu paketlerin durumunu tutan basit veritabani.
// dpkg'nin /var/lib/dpkg/status dosyasinin kayp esdegeri:
//   /var/lib/kayp/status
#pragma once

#include <map>
#include <string>
#include <vector>

namespace kayp {

struct InstalledPackage {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> installed_files; // kurulan dosyalarin tam yollari
};

class StatusDB {
public:
    // db_path: status dosyasinin yolu (ornegin <root>/var/lib/kayp/status)
    explicit StatusDB(const std::string& db_path);

    void load();
    void save() const;

    bool is_installed(const std::string& name) const;
    const InstalledPackage* get(const std::string& name) const;

    void add(const InstalledPackage& pkg);
    void remove(const std::string& name);

    std::vector<InstalledPackage> all() const;

private:
    std::string path_;
    std::map<std::string, InstalledPackage> packages_;
};

} // namespace kayp

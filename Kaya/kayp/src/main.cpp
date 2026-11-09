// kayp - Kaya OS Paket Yoneticisi
// dpkg'nin kayp/.kaya karsiligi. Bagimsiz, sifirdan C++ implementasyonu.
//
// Kullanim:
//   kayp install <paket.kaya> [--root <dizin>]
//   kayp remove  <paket-adi>  [--root <dizin>]
//   kayp list                 [--root <dizin>]
//   kayp info    <paket.kaya>
//
#include <iostream>
#include <string>
#include <vector>

#include "db.hpp"
#include "package.hpp"

static void print_usage() {
    std::cout <<
        "kayp - Kaya OS Paket Yoneticisi\n\n"
        "Kullanim:\n"
        "  kayp install <paket.kaya> [--root <dizin>]\n"
        "  kayp remove  <paket-adi>  [--root <dizin>]\n"
        "  kayp list                 [--root <dizin>]\n"
        "  kayp info    <paket.kaya>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::string root = "/";
    std::vector<std::string> positional;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root = argv[++i];
        } else {
            positional.push_back(arg);
        }
    }

    std::string db_path = root;
    if (!db_path.empty() && db_path.back() != '/') db_path += "/";
    db_path += "var/lib/kayp/status";

    if (command == "install") {
        if (positional.empty()) {
            std::cerr << "kayp: install icin bir .kaya dosyasi belirt\n";
            return 1;
        }
        kayp::StatusDB db(db_path);
        db.load();
        bool ok = kayp::install_package(positional[0], root, db);
        if (ok) db.save();
        return ok ? 0 : 1;
    }

    if (command == "remove") {
        if (positional.empty()) {
            std::cerr << "kayp: remove icin bir paket adi belirt\n";
            return 1;
        }
        kayp::StatusDB db(db_path);
        db.load();
        bool ok = kayp::remove_package(positional[0], root, db);
        if (ok) db.save();
        return ok ? 0 : 1;
    }

    if (command == "list") {
        kayp::StatusDB db(db_path);
        db.load();
        auto pkgs = db.all();
        if (pkgs.empty()) {
            std::cout << "kayp: kurulu paket yok\n";
            return 0;
        }
        for (const auto& pkg : pkgs) {
            std::cout << pkg.name << "\t" << pkg.version
                       << "\t" << pkg.description << "\n";
        }
        return 0;
    }

    if (command == "info") {
        if (positional.empty()) {
            std::cerr << "kayp: info icin bir .kaya dosyasi belirt\n";
            return 1;
        }
        auto info = kayp::read_package_info(positional[0]);
        if (info.name.empty()) {
            std::cerr << "kayp: paket bilgisi okunamadi\n";
            return 1;
        }
        std::cout << "Name: " << info.name << "\n";
        std::cout << "Version: " << info.version << "\n";
        std::cout << "Description: " << info.description << "\n";
        if (!info.depends.empty())
            std::cout << "Depends: " << info.depends << "\n";
        return 0;
    }

    print_usage();
    return 1;
}

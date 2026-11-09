#include "package.hpp"
#include "control_parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <array>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace kayp {

// Gecici bir calisma dizini olusturur, RAII ile temizler.
class TempDir {
public:
    TempDir() {
        char tmpl[] = "/tmp/kayp-XXXXXX";
        char* result = mkdtemp(tmpl);
        if (result) path_ = result;
    }
    ~TempDir() {
        if (!path_.empty()) {
            std::error_code ec;
            fs::remove_all(path_, ec);
        }
    }
    const std::string& path() const { return path_; }
    bool valid() const { return !path_.empty(); }

private:
    std::string path_;
};

// system() cagirir, komutu ve cikis kodunu doner. Kabuk enjeksiyonundan
// kacinmak icin argumanlar tek tek quote'lanir.
static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static int run(const std::string& cmd) {
    return system(cmd.c_str());
}

static std::vector<std::string> tar_list(const std::string& archive) {
    std::vector<std::string> files;
    std::string cmd = "tar -tf " + shell_quote(archive);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return files;
    std::array<char, 512> buf;
    while (fgets(buf.data(), buf.size(), pipe)) {
        std::string line(buf.data());
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (!line.empty()) files.push_back(line);
    }
    pclose(pipe);
    return files;
}

PackageInfo read_package_info(const std::string& kaya_file) {
    PackageInfo info;
    TempDir tmp;
    if (!tmp.valid()) return info;

    std::string cmd = "tar -xf " + shell_quote(kaya_file) +
                       " -C " + shell_quote(tmp.path()) + " control";
    if (run(cmd) != 0) return info;

    auto fields = parse_control(tmp.path() + "/control");
    info.name = get_field(fields, "Name");
    info.version = get_field(fields, "Version");
    info.description = get_field(fields, "Description");
    info.depends = get_field(fields, "Depends");
    return info;
}

bool install_package(const std::string& kaya_file, const std::string& root, StatusDB& db) {
    if (!fs::exists(kaya_file)) {
        std::cerr << "kayp: paket dosyasi bulunamadi: " << kaya_file << "\n";
        return false;
    }

    TempDir tmp;
    if (!tmp.valid()) {
        std::cerr << "kayp: gecici dizin olusturulamadi\n";
        return false;
    }

    // 1) Tum .kaya arsivini gecici dizine ac (control, data.tar.gz, script'ler)
    std::string extract_cmd = "tar -xf " + shell_quote(kaya_file) +
                               " -C " + shell_quote(tmp.path());
    if (run(extract_cmd) != 0) {
        std::cerr << "kayp: paket acilamadi (bozuk .kaya olabilir)\n";
        return false;
    }

    std::string control_path = tmp.path() + "/control";
    auto fields = parse_control(control_path);
    std::string name = get_field(fields, "Name");
    std::string version = get_field(fields, "Version");
    std::string description = get_field(fields, "Description");

    if (name.empty()) {
        std::cerr << "kayp: control dosyasinda 'Name' alani yok\n";
        return false;
    }

    if (db.is_installed(name)) {
        std::cout << "kayp: " << name << " zaten kurulu, uzerine yaziliyor\n";
    }

    // 2) preinst script'i varsa calistir
    std::string preinst = tmp.path() + "/preinst";
    if (fs::exists(preinst)) {
        fs::permissions(preinst, fs::perms::owner_exec, fs::perm_options::add);
        run(shell_quote(preinst) + " " + shell_quote(root));
    }

    // 3) data.tar.gz icindeki dosya listesini al (kaldirma icin lazim)
    std::string data_archive = tmp.path() + "/data.tar.gz";
    std::vector<std::string> file_list;
    if (fs::exists(data_archive)) {
        file_list = tar_list(data_archive);

        // 4) data.tar.gz'i hedef root'a ac
        fs::create_directories(root);
        std::string install_cmd = "tar -xzf " + shell_quote(data_archive) +
                                   " -C " + shell_quote(root);
        if (run(install_cmd) != 0) {
            std::cerr << "kayp: dosyalar kuruluyorken hata olustu\n";
            return false;
        }
    } else {
        std::cout << "kayp: uyari: data.tar.gz yok, sadece meta bilgi kaydediliyor\n";
    }

    // 5) postinst script'i varsa calistir
    std::string postinst = tmp.path() + "/postinst";
    if (fs::exists(postinst)) {
        fs::permissions(postinst, fs::perms::owner_exec, fs::perm_options::add);
        run(shell_quote(postinst) + " " + shell_quote(root));
    }

    // 6) durum veritabanina kaydet
    InstalledPackage pkg;
    pkg.name = name;
    pkg.version = version;
    pkg.description = description;
    for (auto& f : file_list) {
        // root'a gore tam yolu sakla, kaldirirken direkt silinebilsin
        std::string full = root;
        if (!full.empty() && full.back() != '/') full += "/";
        full += f;
        pkg.installed_files.push_back(full);
    }
    db.add(pkg);

    std::cout << "kayp: " << name << " (" << version << ") kuruldu\n";
    return true;
}

bool remove_package(const std::string& name, const std::string& root, StatusDB& db) {
    const InstalledPackage* pkg = db.get(name);
    if (!pkg) {
        std::cerr << "kayp: " << name << " kurulu degil\n";
        return false;
    }

    // dosyalari sondan basa dogru sil (once dosyalar, sonra bos kalan dizinler)
    for (auto it = pkg->installed_files.rbegin(); it != pkg->installed_files.rend(); ++it) {
        std::error_code ec;
        if (fs::is_directory(*it, ec)) {
            fs::remove(*it, ec); // sadece bossa siler, dolu ise hata verir ve devam eder
        } else {
            fs::remove(*it, ec);
        }
    }

    db.remove(name);
    std::cout << "kayp: " << name << " kaldirildi\n";
    return true;
}

} // namespace kayp

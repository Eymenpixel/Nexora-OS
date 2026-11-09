#include <cerrno>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char* kVersion = "0.1.0-dev";

bool is_project_root(const fs::path& path) {
    return fs::exists(path / "kernel" / "Makefile") &&
           fs::exists(path / "Kaya" / "kayp" / "Makefile");
}

fs::path find_project_root(const char* argv0) {
    std::error_code ec;
    fs::path executable = fs::weakly_canonical(argv0, ec);
    if (!ec) {
        for (fs::path candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
            if (is_project_root(candidate)) return candidate;
            if (candidate == candidate.root_path()) break;
        }
    }
    fs::path current = fs::current_path(ec);
    if (!ec) {
        for (fs::path candidate = current; !candidate.empty(); candidate = candidate.parent_path()) {
            if (is_project_root(candidate)) return candidate;
            if (candidate == candidate.root_path()) break;
        }
    }
    return {};
}

bool tokenize(const std::string& line, std::vector<std::string>& out) {
    std::string token;
    char quote = '\0';
    bool escaped = false;
    bool active = false;
    for (char c : line) {
        if (escaped) {
            token.push_back(c);
            escaped = false;
            active = true;
        } else if (c == '\\' && quote != '\'') {
            escaped = true;
            active = true;
        } else if (quote != '\0') {
            if (c == quote) quote = '\0';
            else token.push_back(c);
            active = true;
        } else if (c == '\'' || c == '"') {
            quote = c;
            active = true;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (active) {
                out.push_back(token);
                token.clear();
                active = false;
            }
        } else {
            token.push_back(c);
            active = true;
        }
    }
    if (escaped || quote != '\0') return false;
    if (active) out.push_back(token);
    return true;
}

int run_process(const std::vector<std::string>& args, const fs::path& cwd) {
    if (args.empty()) return 0;
    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "nexora: process could not start: " << std::strerror(errno) << '\n';
        return 1;
    }
    if (child == 0) {
        if (chdir(cwd.c_str()) != 0) _exit(126);
        std::vector<char*> raw;
        raw.reserve(args.size() + 1);
        for (const auto& arg : args) raw.push_back(const_cast<char*>(arg.c_str()));
        raw.push_back(nullptr);
        execv(raw[0], raw.data());
        std::cerr << "nexora: cannot run " << args[0] << ": " << std::strerror(errno) << '\n';
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::cerr << "nexora: waiting for child process failed\n";
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

fs::path default_kaya_root() {
    const char* home = std::getenv("HOME");
    if (home && *home) return fs::path(home) / ".local" / "share" / "nexora" / "root";
    return fs::current_path() / ".nexora-root";
}

bool confirm_action(const std::string& message) {
    std::cout << message << " [y/N] " << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;
    return answer == "y" || answer == "Y" || answer == "yes" || answer == "YES";
}

void print_help() {
    std::cout <<
        "Nexora OS developer CLI " << kVersion << "\n"
        "Komutlar:\n"
        "  help                         Bu yardimi goster\n"
        "  about                        Proje bilgisini goster\n"
        "  status                       Host ve kernel prototip durumunu goster\n"
        "  blackbit build|check|run     Kernel ISO'sunu derle, kontrol et veya QEMU'da baslat\n"
        "  blackbit serial              Kernel'i seri konsolda baslat\n"
        "  kaya list                    Nexora kullanici kokundeki paketleri listele\n"
        "  kaya info <dosya.kaya>       Paket bilgisini oku\n"
        "  kaya fetch <url> [UST opts]  Paketi UST ile indir\n"
        "  kaya install <dosya.kaya>    Paketi Nexora kullanici kokune kur\n"
        "  kaya remove <paket>          Kurulu paketi kaldir\n"
        "  getallow                    Kernel kabugunda Getallow komutlarini acikla\n"
        "  clear                        Ekrani temizle\n"
        "  exit                         CLI'dan cik\n\n"
        "Kaya paketleri kod calistirabilir. Yalnizca guvendigin .kaya arsivlerini kur.\n";
}

class NexoraCLI {
public:
    explicit NexoraCLI(fs::path root) : root_(std::move(root)), kaya_(root_ / "Kaya" / "kayp" / "kayp") {}

    int execute(const std::vector<std::string>& words, bool interactive) {
        if (words.empty()) return 0;
        const std::string& command = words[0];
        if (command == "help" || command == "--help" || command == "-h") {
            print_help();
            return 0;
        }
        if (command == "version" || command == "--version") {
            std::cout << "Nexora OS developer CLI " << kVersion << '\n';
            return 0;
        }
        if (command == "about") {
            std::cout << "Nexora OS - Eymen Akgun tarafindan gelistirilen erken prototip.\n"
                         "BlackBit x86 kernel tabani ve Kaya .kaya paket yoneticisi kaynaklarini birlestirir.\n"
                         "Bu CLI Linux/WSL hostunda calisir; kernel icinde genel user-space calistirma henuz yoktur.\n";
            return 0;
        }
        if (command == "status" || command == "system") return show_status();
        if (command == "clear") {
            if (isatty(STDOUT_FILENO)) std::cout << "\033[2J\033[H";
            return 0;
        }
        if (command == "exit" || command == "quit") return 2;
        if (command == "blackbit") return blackbit(words);
        if (command == "kaya") return kaya(words, interactive);
        if (command == "getallow") {
            std::cout << "Getallow, QEMU'da calisan kernel kabugunun komutudur.\n"
                         "Kernel'i 'blackbit run' ile baslat, ardindan 'getallow status' yaz.\n";
            return 0;
        }
        std::cerr << "nexora: bilinmeyen komut: " << command << " (help yaz)\n";
        return 2;
    }

private:
    int show_status() const {
        struct utsname host{};
        if (uname(&host) == 0)
            std::cout << "Host: " << host.sysname << ' ' << host.release << " (" << host.machine << ")\n";
        std::cout << "OS: Nexora OS " << kVersion << " / BlackBit 32-bit x86 kernel\n"
                  << "Kernel ISO: " << (fs::exists(root_ / "kernel" / "build" / "blackbit.iso") ? "hazir" : "henuz derlenmedi") << '\n'
                  << "Kaya Kayp: " << (fs::exists(kaya_) ? "derlenmis" : "kaynak mevcut, henuz derlenmedi") << '\n'
                  << "Kaya UST: " << (fs::exists(root_ / "build" / "ust" / "ust") ? "derlenmis" : "kaynak mevcut, henuz derlenmedi") << '\n'
                  << "Kaya kok dizini: " << default_kaya_root() << '\n';
        return 0;
    }

    int blackbit(const std::vector<std::string>& words) const {
        if (words.size() < 2) {
            std::cerr << "Kullanim: blackbit build|check|run|serial\n";
            return 2;
        }
        const std::string& action = words[1];
        if (action == "build") return run_process({"/usr/bin/make", "-C", (root_ / "kernel").string(), "iso"}, root_);
        if (action == "check") return run_process({"/usr/bin/make", "-C", (root_ / "kernel").string(), "check"}, root_);
        if (action == "run") return run_process({"/usr/bin/make", "-C", (root_ / "kernel").string(), "run"}, root_);
        if (action == "serial") return run_process({"/usr/bin/make", "-C", (root_ / "kernel").string(), "run-serial"}, root_);
        std::cerr << "nexora: bilinmeyen BlackBit eylemi: " << action << '\n';
        return 2;
    }

    int kaya(const std::vector<std::string>& words, bool interactive) const {
        if (!fs::exists(kaya_)) {
            std::cerr << "nexora: Kaya derlenmemis. Once 'make kaya' calistir.\n";
            return 1;
        }
        if (words.size() < 2 || words[1] == "help") {
            std::cout << "Kullanim: kaya list | info <dosya.kaya> | install <dosya.kaya> | remove <paket>\n";
            return 0;
        }
        const std::string& action = words[1];
        if (action == "fetch") {
            const fs::path ust = root_ / "build" / "ust" / "ust";
            if (!fs::exists(ust)) {
                std::cerr << "nexora: UST derlenmemis. Once 'make ust' calistir.\n";
                return 1;
            }
            if (words.size() < 3) {
                std::cerr << "Kullanim: kaya fetch [UST secenekleri] <url>\n";
                return 2;
            }
            std::vector<std::string> args{ust.string()};
            args.insert(args.end(), words.begin() + 2, words.end());
            return run_process(args, root_);
        }
        if (action != "list" && action != "info" && action != "install" && action != "remove") {
            std::cerr << "nexora: desteklenmeyen Kaya komutu: " << action << '\n';
            return 2;
        }
        if ((action == "info" || action == "install" || action == "remove") && words.size() < 3) {
            std::cerr << "nexora: Kaya komutu icin paket adi veya dosya yolu gerekli\n";
            return 2;
        }
        if (action == "install" || action == "remove") {
            const std::string subject = words.size() > 2 ? words[2] : "paket";
            const bool has_yes = std::find(words.begin() + 2, words.end(), "--yes") != words.end();
            if (action == "install" && !has_yes) {
                std::cerr << "Kaya paketleri kurulum betikleriyle sistem komutlari calistirabilir.\n";
                if (!interactive || !confirm_action("Bu pakete guveniyor ve kurmak istiyor musun?")) {
                    std::cerr << "nexora: kurulum iptal edildi; guvendigin paket icin --yes kullanabilirsin.\n";
                    return 1;
                }
            }
            if (action == "remove" && !has_yes) {
                if (!interactive) {
                    std::cerr << "nexora: otomatik kaldirma icin --yes gerekli.\n";
                    return 1;
                }
                if (!confirm_action(subject + " paketinin kayitli dosyalarini kaldirayim mi?")) {
                    std::cerr << "nexora: kaldirma iptal edildi.\n";
                    return 1;
                }
            }
        }
        fs::path install_root = default_kaya_root();
        std::error_code ec;
        if (action == "install") fs::create_directories(install_root, ec);
        if (ec) {
            std::cerr << "nexora: Kaya kullanici kok dizini olusturulamadi: " << ec.message() << '\n';
            return 1;
        }
        std::vector<std::string> args{kaya_.string()};
        for (size_t i = 1; i < words.size(); ++i) {
            if (words[i] == "--yes") continue;
            args.push_back(words[i]);
        }
        const bool has_root = std::find(args.begin(), args.end(), "--root") != args.end();
        if ((action == "list" || action == "install" || action == "remove") && !has_root) {
            args.push_back("--root");
            args.push_back(install_root.string());
        }
        return run_process(args, root_);
    }

    fs::path root_;
    fs::path kaya_;
};

} // namespace

int main(int argc, char** argv) {
    const fs::path root = find_project_root(argv[0]);
    if (root.empty()) {
        std::cerr << "nexora: proje kok dizini bulunamadi; Nexora deposunun icinden calistir.\n";
        return 1;
    }
    NexoraCLI cli(root);
    if (argc > 1) {
        std::vector<std::string> words;
        for (int i = 1; i < argc; ++i) words.emplace_back(argv[i]);
        const int result = cli.execute(words, false);
        return result == 2 && (words[0] == "exit" || words[0] == "quit") ? 0 : result;
    }

    std::cout << "NEXORA OS " << kVersion << " | BlackBit x86 kernel + Kaya package manager\n"
                 "Gelistirici CLI prototipi. Yardim icin help yaz.\n";
    std::string line;
    while (true) {
        std::cout << "nexora> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        std::vector<std::string> words;
        if (!tokenize(line, words)) {
            std::cerr << "nexora: tirnak veya ters bolu acik kalmis\n";
            continue;
        }
        const int result = cli.execute(words, true);
        if (result == 2 && !words.empty() && (words[0] == "exit" || words[0] == "quit")) break;
    }
    return 0;
}

#include <curl/curl.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace fs = std::filesystem;

struct Options {
    std::string url, output, method, user, user_agent;
    std::vector<std::string> headers, data, binary_data;
    bool method_explicit = false;
    bool remote_name = false, head = false, include_headers = false;
    bool follow = false, verbose = false, silent = false;
    bool fail = false, fail_with_body = false, help = false, version = false;
    double max_time = 100.0;
};

struct Transfer {
    std::ofstream file;
    bool to_file = false;
    bool include_headers = false;
    bool head = false;
    bool write_error = false;
};

static void help() {
    std::cout <<
        "UST 0.3.0 - Universal Source Tool (C++)\n"
        "Usage: ust [options] <url>\n"
        "  -o, --output FILE       Save body to FILE\n"
        "  -O, --remote-name       Save using URL file name\n"
        "  -I, --head              Send HEAD and print headers\n"
        "  -i, --include           Print response headers before body\n"
        "  -X, --request METHOD    Set HTTP method\n"
        "  -H, --header 'N: V'     Add a request header (repeatable)\n"
        "  -d, --data DATA         Send request body (repeatable; @FILE reads file)\n"
        "      --data-binary DATA  Send bytes (prefix @ to read a file)\n"
        "  -u, --user USER:PASS    HTTP Basic authentication\n"
        "  -A, --user-agent VALUE  Set User-Agent\n"
        "  -L, --location          Follow redirects\n"
        "  -v, --verbose           Show request/response details\n"
        "  -s, --silent            Suppress verbose details\n"
        "      --fail              Exit 22 for HTTP status 400+\n"
        "      --fail-with-body    Keep error body and exit 22\n"
        "      --max-time SECONDS  Timeout (default 100)\n"
        "  -h, --help              Show help\n"
        "      --version           Print version\n";
}

static bool take_value(int& i, int argc, char** argv, const std::string& opt,
                       std::string& out) {
    if (i + 1 >= argc) {
        std::cerr << "ust: missing value for " << opt << '\n';
        return false;
    }
    out = argv[++i];
    return true;
}

static bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i], value;
        if (a == "--") {
            if (i + 1 < argc && o.url.empty()) o.url = argv[++i];
            else if (i + 1 < argc) { std::cerr << "ust: only one URL is supported\n"; return false; }
            continue;
        }
        if (a == "--help" || a == "-h") { o.help = true; continue; }
        if (a == "--version") { o.version = true; continue; }
        if (a == "-O" || a == "--remote-name") { o.remote_name = true; continue; }
        if (a == "-I" || a == "--head") { o.head = true; o.include_headers = true; continue; }
        if (a == "-i" || a == "--include") { o.include_headers = true; continue; }
        if (a == "-L" || a == "--location") { o.follow = true; continue; }
        if (a == "-v" || a == "--verbose") { o.verbose = true; continue; }
        if (a == "-s" || a == "--silent") { o.silent = true; continue; }
        if (a == "--fail") { o.fail = true; continue; }
        if (a == "--fail-with-body") { o.fail = true; o.fail_with_body = true; continue; }
        if (a == "-o" || a == "--output") { if (!take_value(i, argc, argv, a, o.output)) return false; continue; }
        if (a == "-X" || a == "--request") { if (!take_value(i, argc, argv, a, o.method)) return false; o.method_explicit = true; continue; }
        if (a == "-H" || a == "--header") { if (!take_value(i, argc, argv, a, value)) return false; o.headers.push_back(value); continue; }
        if (a == "-d" || a == "--data") { if (!take_value(i, argc, argv, a, value)) return false; o.data.push_back(value); continue; }
        if (a == "--data-binary") { if (!take_value(i, argc, argv, a, value)) return false; o.binary_data.push_back(value); continue; }
        if (a == "-u" || a == "--user") { if (!take_value(i, argc, argv, a, o.user)) return false; continue; }
        if (a == "-A" || a == "--user-agent") { if (!take_value(i, argc, argv, a, o.user_agent)) return false; continue; }
        if (a == "--max-time") {
            if (!take_value(i, argc, argv, a, value)) return false;
            try { o.max_time = std::stod(value); } catch (...) { std::cerr << "ust: invalid --max-time\n"; return false; }
            if (!std::isfinite(o.max_time) || o.max_time <= 0.0 || o.max_time > static_cast<double>(std::numeric_limits<long>::max()) / 1000.0) {
                std::cerr << "ust: --max-time must be positive and within range\n"; return false;
            }
            continue;
        }
        if (a.size() > 2 && a[0] == '-' && a[1] != '-') {
            for (size_t j = 1; j < a.size(); ++j) {
                switch (a[j]) {
                    case 's': o.silent = true; break;
                    case 'L': o.follow = true; break;
                    case 'v': o.verbose = true; break;
                    case 'i': o.include_headers = true; break;
                    case 'I': o.head = true; o.include_headers = true; break;
                    case 'O': o.remote_name = true; break;
                    default: std::cerr << "ust: unsupported combined option " << a << '\n'; return false;
                }
            }
            continue;
        }
        if (!a.empty() && a[0] == '-') { std::cerr << "ust: unknown option: " << a << '\n'; return false; }
        if (!o.url.empty()) { std::cerr << "ust: only one URL is supported\n"; return false; }
        o.url = a;
    }
    if (!o.data.empty() && !o.binary_data.empty()) { std::cerr << "ust: do not mix --data and --data-binary\n"; return false; }
    return true;
}

static bool load_parts(const std::vector<std::string>& parts, bool join,
                       std::string& result) {
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        std::string chunk;
        if (!p.empty() && p[0] == '@') {
            std::ifstream in(p.substr(1), std::ios::binary);
            if (!in) { std::cerr << "ust: cannot read " << p.substr(1) << '\n'; return false; }
            std::ostringstream ss; ss << in.rdbuf(); chunk = ss.str();
        } else chunk = p;
        if (join && i) result.push_back('&');
        result += chunk;
    }
    return true;
}

static size_t write_body(char* data, size_t size, size_t count, void* context) {
    auto* t = static_cast<Transfer*>(context);
    const size_t n = size * count;
    if (t->head) return n;
    if (t->to_file) t->file.write(data, static_cast<std::streamsize>(n));
    else std::cout.write(data, static_cast<std::streamsize>(n));
    const bool okay = t->to_file ? t->file.good() : std::cout.good();
    if (!okay) t->write_error = true;
    return okay ? n : 0;
}

static size_t write_header(char* data, size_t size, size_t count, void* context) {
    auto* t = static_cast<Transfer*>(context);
    const size_t n = size * count;
    if (t->include_headers || t->head) std::cout.write(data, static_cast<std::streamsize>(n));
    return n;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    Options o;
    if (!parse_args(argc, argv, o)) return 2;
    if (o.help) { help(); return 0; }
    if (o.version) { std::cout << "UST 0.3.0\n"; return 0; }
    if (o.url.empty()) { help(); std::cerr << "ust: provide a URL\n"; return 2; }
    if (o.url.rfind("http://", 0) != 0 && o.url.rfind("https://", 0) != 0) {
        std::cerr << "ust: URL must use http:// or https://\n"; return 2;
    }

    std::string payload;
    if (!load_parts(o.data, true, payload) || !load_parts(o.binary_data, false, payload)) return 2;
    if (o.remote_name) {
        auto end = o.url.find_first_of("?#");
        std::string path = o.url.substr(0, end);
        auto slash = path.find_last_of('/');
        o.output = slash == std::string::npos ? "index.html" : path.substr(slash + 1);
        if (o.output.empty()) o.output = "index.html";
    }
    std::string method = o.head ? "HEAD" : o.method;
    if (method.empty()) method = (!o.data.empty() || !o.binary_data.empty()) ? "POST" : "GET";

    CURLcode init = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (init != CURLE_OK) { std::cerr << "ust: curl initialization failed\n"; return 7; }
    CURL* raw = curl_easy_init();
    if (!raw) { curl_global_cleanup(); std::cerr << "ust: curl initialization failed\n"; return 7; }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);

    struct curl_slist* list = nullptr;
    for (const auto& h : o.headers) list = curl_slist_append(list, h.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_list(list, curl_slist_free_all);

    Transfer transfer;
    transfer.include_headers = o.include_headers;
    transfer.head = o.head;
    fs::path temp_path;
    fs::path final_path;
    if (!o.output.empty() && !o.head) {
        final_path = fs::absolute(o.output);
        std::error_code ec;
        fs::create_directories(final_path.parent_path(), ec);
        if (ec) { std::cerr << "ust: cannot create output directory: " << ec.message() << '\n'; return 23; }
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        temp_path = final_path.parent_path() / (".ust-" + std::to_string(stamp) + ".tmp");
        transfer.file.open(temp_path, std::ios::binary | std::ios::trunc);
        if (!transfer.file) { std::cerr << "ust: cannot create temporary output file\n"; return 23; }
        transfer.to_file = true;
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, o.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, o.follow ? 1L : 0L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 20L);
    const long timeout_ms = static_cast<long>(std::ceil(o.max_time * 1000.0));
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &transfer);
    if (header_list) curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());
    if (o.method_explicit && !o.head) curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    if (o.head) curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
    if (o.user_agent.size()) curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, o.user_agent.c_str());
    if (!o.user.empty()) { curl_easy_setopt(curl.get(), CURLOPT_USERPWD, o.user.c_str()); curl_easy_setopt(curl.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC); }
    if (!o.data.empty() || !o.binary_data.empty()) {
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, payload.data());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    }
    if (o.verbose && !o.silent) curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1L);
    if (o.fail && !o.fail_with_body) curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);

    CURLcode result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (transfer.file.is_open()) {
        transfer.file.flush();
        if (!transfer.file.good()) transfer.write_error = true;
        transfer.file.close();
        if (transfer.file.fail()) transfer.write_error = true;
    }
    if (transfer.write_error) {
        std::error_code ignored;
        if (!temp_path.empty()) fs::remove(temp_path, ignored);
        std::cerr << "ust: failed to write output\n";
        return 23;
    }
    if (result != CURLE_OK) {
        std::error_code ignored;
        if (!temp_path.empty()) fs::remove(temp_path, ignored);
        if (result == CURLE_HTTP_RETURNED_ERROR && o.fail) { std::cerr << "ust: HTTP " << status << '\n'; return 22; }
        if (result == CURLE_OPERATION_TIMEDOUT) { std::cerr << "ust: request timed out\n"; return 28; }
        std::cerr << "ust: " << curl_easy_strerror(result) << '\n'; return 7;
    }
    if (!temp_path.empty()) {
        std::error_code ec;
        if (fs::exists(final_path, ec)) {
            fs::path backup = final_path; backup += ".ust-backup";
            fs::remove(backup, ec); ec.clear();
            fs::rename(final_path, backup, ec);
            if (ec) { fs::remove(temp_path); std::cerr << "ust: cannot replace destination: " << ec.message() << '\n'; return 23; }
            fs::rename(temp_path, final_path, ec);
            if (ec) { std::error_code restore; fs::rename(backup, final_path, restore); fs::remove(temp_path); std::cerr << "ust: cannot install downloaded file: " << ec.message() << '\n'; return 23; }
            fs::remove(backup, ec);
        } else {
            fs::rename(temp_path, final_path, ec);
            if (ec) { fs::remove(temp_path); std::cerr << "ust: cannot install downloaded file: " << ec.message() << '\n'; return 23; }
        }
    }
    if (o.fail && status >= 400) { std::cerr << "ust: HTTP " << status << '\n'; return 22; }
    return 0;
}

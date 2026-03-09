#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"crypt32.lib")

#define LOG_FILE "wrapper.log"
#define LOG_MAX_LINES 300

std::wstring getExePath() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    return buf;
}

std::wstring getDir(const std::wstring& p) {
    return p.substr(0, p.find_last_of(L"\\/"));
}

std::wstring getName(const std::wstring& p) {
    return p.substr(p.find_last_of(L"\\/") + 1);
}

bool exists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring quote(const std::wstring& s) {
    if (s.find(L' ') != std::wstring::npos)
        return L"\"" + s + L"\"";
    return s;
}

void log(const std::string& msg) {
    std::ifstream r(LOG_FILE);
    int lines = 0;
    std::string tmp;
    while (std::getline(r, tmp))
        lines++;
    r.close();
    if (lines >= LOG_MAX_LINES)
        std::ofstream(LOG_FILE, std::ios::trunc).close();
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::ofstream f(LOG_FILE, std::ios::app);
    f << "["
      << t.wYear << "-"
      << t.wMonth << "-"
      << t.wDay << " "
      << t.wHour << ":"
      << t.wMinute << ":"
      << t.wSecond
      << "] "
      << msg
      << "\n";
}

bool sha256(const std::wstring& path, std::string& out) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE)
        return false;
    HCRYPTPROV prov;
    HCRYPTHASH hash;
    CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash);
    BYTE buf[4096];
    DWORD read;
    while (ReadFile(file, buf, sizeof(buf), &read, NULL) && read)
        CryptHashData(hash, buf, read, 0);
    BYTE digest[32];
    DWORD len = 32;
    CryptGetHashParam(hash, HP_HASHVAL, digest, &len, 0);
    char hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(hex + i * 2, "%02x", digest[i]);
    out = hex;
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    CloseHandle(file);
    return true;
}

std::string httpGET(const std::wstring& host, const std::wstring& path) {
    std::string result;
    HINTERNET session = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36 Edg/145.0.0.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
    HINTERNET connect =
        WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request =
        WinHttpOpenRequest(connect, L"GET", path.c_str(),
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
    WinHttpAddRequestHeaders(
        request,
        L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36 Edg/145.0.0.0",
        -1,
        WINHTTP_ADDREQ_FLAG_ADD
    );
    WinHttpSendRequest(request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);
    WinHttpReceiveResponse(request, NULL);
    DWORD size = 0;
    do {
        WinHttpQueryDataAvailable(request, &size);
        if (!size)
            break;
        std::vector<char> buf(size);
        DWORD read;
        WinHttpReadData(request, buf.data(), size, &read);
        result.append(buf.data(), read);
    } while (size > 0);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

std::string latestVersion() {
    std::string json =
        httpGET(
            L"api.github.com",
            L"/repos/yt-dlp/yt-dlp/releases/latest"
        );
    auto p = json.find("\"tag_name\"");
    if (p == std::string::npos)
        return "";
    auto s = json.find("\"", p + 10) + 1;
    auto e = json.find("\"", s);
    return json.substr(s, e - s);
}

std::string loadVersion() {
    std::ifstream f("yt-dlp_version.txt");
    std::string v;
    f >> v;
    return v;
}

void saveVersion(const std::string& v) {
    std::ofstream f("yt-dlp_version.txt");
    f << v;
}

bool downloadFile(const std::wstring& url,
                  const std::wstring& path) {
    HINTERNET session =
        WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36 Edg/145.0.0.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
    HINTERNET connect =
        WinHttpConnect(session,
            L"github.com",
            INTERNET_DEFAULT_HTTPS_PORT,
            0);
    HINTERNET request =
        WinHttpOpenRequest(connect,
            L"GET",
            url.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
    WinHttpSendRequest(request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);
    WinHttpReceiveResponse(request, NULL);
    std::ofstream file(path, std::ios::binary);
    DWORD size = 0;
    do {
        WinHttpQueryDataAvailable(request, &size);
        if (!size)
            break;
        std::vector<char> buf(size);
        DWORD read;
        WinHttpReadData(request, buf.data(), size, &read);
        file.write(buf.data(), read);
    } while (size > 0);
    file.close();
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return true;
}

void updateYtdlp(const std::wstring& real) {
    log("Updating yt-dlp");
    DeleteFileW(real.c_str());
    downloadFile(
        L"/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
        real
    );
}

int runReal(const std::wstring& exe,
            bool cookie,
            int argc,
            wchar_t** argv) {
    std::wstring cmd = quote(exe);
    if (cookie)
        cmd += L" --cookies cookies.txt";
    for (int i = 1; i < argc; i++) {
        cmd += L" ";
        cmd += quote(argv[i]);
    }
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (!CreateProcessW(
        NULL,
        cmd.data(),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        log("CreateProcess failed");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

void setup(const std::wstring& self) {
    std::wstring dir = getDir(self);
    std::wstring ytdlp = dir + L"\\yt-dlp.exe";
    std::wstring real = dir + L"\\yt-dlp_real.exe";
    DeleteFileW(ytdlp.c_str());
    DeleteFileW(real.c_str());
    log("Downloading yt-dlp");
    downloadFile(
        L"/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
        ytdlp
    );
    MoveFileW(ytdlp.c_str(), real.c_str());
    CopyFileW(self.c_str(), ytdlp.c_str(), FALSE);
    saveVersion(latestVersion());
    MessageBoxW(NULL, L"Setup complete", L"VRCdlp", 0);
}

int wmain(int argc, wchar_t** argv) {
    std::wstring self = getExePath();
    std::wstring name = getName(self);
    std::wstring dir = getDir(self);
    std::wstring real = dir + L"\\yt-dlp_real.exe";
    if (name == L"VRCdlpsetup.exe") {
        setup(self);
        return 0;
    }
    if (!exists(real)) {
        MessageBoxW(NULL,
            L"yt-dlp_real.exe missing",
            L"Error",
            0);
        return 1;
    }
    std::string latest = latestVersion();
    std::string local = loadVersion();
    if (latest != "" && latest != local) {
        updateYtdlp(real);
        saveVersion(latest);
    }
    bool useCookie = exists(L"cookies.txt");
    return runReal(real, useCookie, argc, argv);
}

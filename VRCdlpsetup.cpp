#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"crypt32.lib")

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

std::string httpGET(const std::wstring& host, const std::wstring& path) {
    std::string result;
    HINTERNET session = WinHttpOpen(
        L"Mozilla/5.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) return result;
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return result; }
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return result; }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return result;
    }
    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return result;
    }
    DWORD size = 0;
    do {
        WinHttpQueryDataAvailable(request, &size);
        if (!size) break;
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
    std::string json = httpGET(L"api.github.com", L"/repos/yt-dlp/yt-dlp/releases/latest");
    auto p = json.find("\"tag_name\"");
    if (p == std::string::npos) return "";
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

bool downloadFile(const std::wstring& url, const std::wstring& path) {
    HINTERNET session = WinHttpOpen(
        L"Mozilla/5.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) return false;
    HINTERNET connect = WinHttpConnect(session, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", url.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return false;
    }
    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return false;
    }
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        NULL, &statusCode, &statusSize, NULL);
    if (statusCode != 200) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return false;
    }
    std::ofstream file(path, std::ios::binary);
    DWORD size = 0;
    do {
        WinHttpQueryDataAvailable(request, &size);
        if (!size) break;
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

bool isValidPE(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    BYTE header[2] = {};
    DWORD read = 0;
    ReadFile(f, header, 2, &read, NULL);
    CloseHandle(f);
    return (read == 2 && header[0] == 'M' && header[1] == 'Z');
}

void removeZoneIdentifier(const std::wstring& path) {
    DeleteFileW((path + L":Zone.Identifier").c_str());
}

bool moveFileRetry(const std::wstring& src, const std::wstring& dst, int maxRetries = 10) {
    for (int i = 0; i < maxRetries; i++) {
        if (MoveFileW(src.c_str(), dst.c_str()))
            return true;
        DWORD err = GetLastError();
        if (err != ERROR_SHARING_VIOLATION && err != ERROR_LOCK_VIOLATION)
            break;
        Sleep(500);
    }
    return false;
}

void updateYtdlp(const std::wstring& real) {
    std::wstring tmp = real + L".tmp";
    if (!downloadFile(L"/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", tmp)) {
        DeleteFileW(tmp.c_str());
        return;
    }
    if (!exists(tmp)) return;
    if (!isValidPE(tmp)) {
        DeleteFileW(tmp.c_str());
        return;
    }
    removeZoneIdentifier(tmp);
    DeleteFileW(real.c_str());
    if (!moveFileRetry(tmp, real))
        DeleteFileW(tmp.c_str());
}

int runReal(const std::wstring& exe, bool cookie) {
    std::wstring cmd = quote(exe);
    if (cookie)
        cmd += L" --cookies cookies.txt";
    std::wstring rawCmd = GetCommandLineW();
    std::wstring args;
    const wchar_t* p = rawCmd.c_str();
    bool startsWithExe = false;
    if (*p == L'"') {
        const wchar_t* end = wcschr(p + 1, L'"');
        if (end) {
            std::wstring first(p + 1, end);
            if (first.find(L'\\') != std::wstring::npos || first.find(L'/') != std::wstring::npos || first.find(L".exe") != std::wstring::npos)
                startsWithExe = true;
        }
    } else {
        const wchar_t* end = p;
        while (*end && *end != L' ') end++;
        std::wstring first(p, end);
        if (first.find(L'\\') != std::wstring::npos || first.find(L'/') != std::wstring::npos || first.find(L".exe") != std::wstring::npos)
            startsWithExe = true;
    }
    if (startsWithExe) {
        if (*p == L'"') {
            p = wcschr(p + 1, L'"');
            if (p) p++;
        } else {
            while (*p && *p != L' ') p++;
        }
        while (*p == L' ') p++;
        args = p;
    } else {
        args = rawCmd;
    }
    if (!args.empty()) {
        cmd += L" ";
        cmd += args;
    }
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdIn && hStdIn != INVALID_HANDLE_VALUE)
        SetHandleInformation(hStdIn, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (hStdOut && hStdOut != INVALID_HANDLE_VALUE)
        SetHandleInformation(hStdOut, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (hStdErr && hStdErr != INVALID_HANDLE_VALUE)
        SetHandleInformation(hStdErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdIn;
    si.hStdOutput = hStdOut;
    si.hStdError = hStdErr;
    if (!CreateProcessW(NULL, cmd.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        return 1;
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
    SetFileAttributesW(ytdlp.c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(ytdlp.c_str());
    DeleteFileW(real.c_str());
    if (!downloadFile(L"/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", real)) {
        MessageBoxW(NULL, L"Download failed", L"VRCdlp", MB_ICONERROR);
        return;
    }
    if (!exists(real) || !isValidPE(real)) {
        MessageBoxW(NULL, L"Download verification failed", L"VRCdlp", MB_ICONERROR);
        return;
    }
    removeZoneIdentifier(real);
    CopyFileW(self.c_str(), ytdlp.c_str(), FALSE);
    SetFileAttributesW(ytdlp.c_str(), FILE_ATTRIBUTE_READONLY);
    saveVersion(latestVersion());
    MessageBoxW(NULL, L"Setup complete", L"VRCdlp", 0);
}

int wmain(int argc, wchar_t** argv) {
    std::wstring self = getExePath();
    std::wstring name = getName(self);
    std::wstring dir = getDir(self);
    SetCurrentDirectoryW(dir.c_str());
    std::wstring tempDir = dir + L"\\tmp";
    CreateDirectoryW(tempDir.c_str(), NULL);
    SetEnvironmentVariableW(L"TEMP", tempDir.c_str());
    SetEnvironmentVariableW(L"TMP", tempDir.c_str());
    std::wstring real = dir + L"\\yt-dlp_real.exe";
    if (name == L"VRCdlpsetup.exe") {
        setup(self);
        return 0;
    }
    if (!exists(real)) {
        updateYtdlp(real);
        if (!exists(real))
            return 1;
    }
    WIN32_FILE_ATTRIBUTE_DATA verData;
    bool shouldUpdate = true;
    if (GetFileAttributesExW(L"yt-dlp_version.txt", GetFileExInfoStandard, &verData)) {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        ULARGE_INTEGER uNow, uFile;
        uNow.LowPart = now.dwLowDateTime;
        uNow.HighPart = now.dwHighDateTime;
        uFile.LowPart = verData.ftLastWriteTime.dwLowDateTime;
        uFile.HighPart = verData.ftLastWriteTime.dwHighDateTime;
        shouldUpdate = (uNow.QuadPart - uFile.QuadPart) > 864000000000ULL;
    }
    if (shouldUpdate) {
        std::string latest = latestVersion();
        std::string local = loadVersion();
        if (!latest.empty() && latest != local) {
            updateYtdlp(real);
            saveVersion(latest);
        }
    }
    return runReal(real, exists(L"cookies.txt"));
}

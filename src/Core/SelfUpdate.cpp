#include "SelfUpdate.h"

#include <winhttp.h>
#include <bcrypt.h>

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>
#include <initializer_list>

#include "../Utils/Logger.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace SelfUpdate
{
    // ------------------------------------------------------------------
    // Dedicated log — independent of the global Logger and its enable flag.
    // Appends to C:\Temp\rugir_selfupdate.log so the whole update flow is
    // traceable even when file logging is off elsewhere. Opened/closed per
    // line (cheap, low-frequency) and safe to call from the worker thread.
    // ------------------------------------------------------------------
    static void SuLog(const std::string& msg)
    {
#if defined(SELFUPDATE_ENABLE_LOG) && SELFUPDATE_ENABLE_LOG
        static const char* kPath = "C:\\Temp\\rugir_selfupdate.log";
        CreateDirectoryA("C:\\Temp", nullptr);

        HANDLE h = CreateFileA(kPath, FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return;
        SetFilePointer(h, 0, nullptr, FILE_END);

        SYSTEMTIME st;
        GetLocalTime(&st);
        char line[1024];
        int n = wsprintfA(line, "[%02u:%02u:%02u.%03u] %s\r\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg.c_str());
        if (n > 0)
        {
            DWORD written = 0;
            WriteFile(h, line, static_cast<DWORD>(n), &written, nullptr);
        }
        CloseHandle(h);
#else
        (void)msg; // logging disabled at compile time
#endif
    }

    // ------------------------------------------------------------------
    // Configuration.
    // Define SELFUPDATE_LOCAL_TEST=1 to point at a plain-HTTP localhost server
    // (no TLS, port 8080) for validating the swap+reload cycle before touching
    // the live Railway host. Prod path is unchanged when it's 0.
    // ------------------------------------------------------------------
#ifndef SELFUPDATE_LOCAL_TEST
#define SELFUPDATE_LOCAL_TEST 0
#endif

#if SELFUPDATE_LOCAL_TEST
    static const wchar_t* kUpdateHost   = L"127.0.0.1";
    static const INTERNET_PORT kUpdatePort = 8080;
    static const DWORD kSecureFlag = 0;                 // plain HTTP
#else
    static const wchar_t* kUpdateHost   = L"internalmhur1.up.railway.app";
    static const INTERNET_PORT kUpdatePort = INTERNET_DEFAULT_HTTPS_PORT;
    static const DWORD kSecureFlag = WINHTTP_FLAG_SECURE; // TLS
#endif

    static const wchar_t* kManifestPath = L"/api/update/manifest";
    static const wchar_t* kDownloadPath = L"/api/update/download"; // fallback if manifest has no "file"

    // Client version. Resolution order:
    //   1. SELFUPDATE_CLIENT_VERSION defined at compile time (manual/test builds,
    //      e.g. /DSELFUPDATE_CLIENT_VERSION#\"1.0.1\") wins — explicit override.
    //   2. Version.gen.h (RUGIR_AUTOVER), auto-generated & bumped on every Agency
    //      build by tools\bump_version.ps1 — the public build stamps itself.
    //   3. "1.0.0" fallback.
#if defined(SELFUPDATE_CLIENT_VERSION)
    static const char* kClientVersion = SELFUPDATE_CLIENT_VERSION;
#elif defined(__has_include)
#  if __has_include("Version.gen.h")
#    include "Version.gen.h"
     static const char* kClientVersion = RUGIR_AUTOVER;
#  else
     static const char* kClientVersion = "1.0.0";
#  endif
#else
    static const char* kClientVersion = "1.0.0";
#endif

    // Build/config tag sent to the server so it can resolve the right binary
    // (option B: multiple DLLs indexed by config + game build). Each vcxproj
    // configuration defines its own; Agency (the public build) is the default.
#ifndef RUGIR_UPDATE_CONFIG
#define RUGIR_UPDATE_CONFIG "agency"
#endif
    static const char* kUpdateConfig = RUGIR_UPDATE_CONFIG;

    static HMODULE   g_self    = nullptr;
    static CleanupFn g_cleanup = nullptr;
    static std::atomic_bool g_busy{ false };

    // Auto (game-update) mode: set when the game was patched so the DLL runs
    // render-only and must silently pull the game-matched build. Holds the game
    // build id (PE TimeDateStamp) used to key the server request, and arms a
    // background retry when no compatible build is online yet.
    static std::atomic<uint32_t> g_autoGameBuild{ 0 };
    static std::atomic_bool      g_autoMode{ false };
    static std::atomic_bool      g_retryArmed{ false };

    // ------------------------------------------------------------------
    // Shared UI state. Written by the worker threads, read by the render
    // thread every frame. Guarded by g_stateMutex; the Progress copy handed
    // to the UI is a value snapshot so the render thread never holds the lock
    // while drawing.
    // ------------------------------------------------------------------
    static std::mutex g_stateMutex;
    static Progress   g_progress;               // guarded
    static std::vector<char> g_stagedDll;       // downloaded bytes, guarded
    static std::string       g_stagedHash;      // manifest hash to verify, guarded
    static std::wstring      g_stagedFilePath;  // download path from manifest, guarded

    static void SetState(State s)
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_progress.state = s;
    }
    static void SetError(const std::string& msg)
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_progress.state = State::Error;
        g_progress.errorText = msg;
    }

    // ------------------------------------------------------------------
    // Small helpers
    // ------------------------------------------------------------------
    static std::string ToHexLower(const unsigned char* data, size_t len)
    {
        static const char* hx = "0123456789abcdef";
        std::string out;
        out.resize(len * 2);
        for (size_t i = 0; i < len; ++i)
        {
            out[i * 2]     = hx[(data[i] >> 4) & 0xF];
            out[i * 2 + 1] = hx[data[i] & 0xF];
        }
        return out;
    }

    static std::string Sha256Hex(const void* data, size_t len)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
            return std::string();

        std::string result;
        BCRYPT_HASH_HANDLE hash = nullptr;
        do
        {
            if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0)
                break;
            if (BCryptHashData(hash,
                    reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
                    static_cast<ULONG>(len), 0) != 0)
                break;

            unsigned char digest[32] = {};
            if (BCryptFinishHash(hash, digest, sizeof(digest), 0) != 0)
                break;

            result = ToHexLower(digest, sizeof(digest));
        } while (false);

        if (hash) BCryptDestroyHash(hash);
        if (alg)  BCryptCloseAlgorithmProvider(alg, 0);
        return result;
    }

    // Minimal JSON string field scrape: "key":"value"  (value has no escaped quotes here).
    static std::string JsonString(const std::string& j, const char* key)
    {
        std::string needle = std::string("\"") + key + "\"";
        size_t k = j.find(needle);
        if (k == std::string::npos) return std::string();
        size_t colon = j.find(':', k + needle.size());
        if (colon == std::string::npos) return std::string();
        size_t q1 = j.find('"', colon + 1);
        if (q1 == std::string::npos) return std::string();
        size_t q2 = j.find('"', q1 + 1);
        if (q2 == std::string::npos) return std::string();
        return j.substr(q1 + 1, q2 - q1 - 1);
    }

    static std::wstring Utf8ToUtf16(const std::string& s)
    {
        if (s.empty()) return std::wstring();
        int need = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (need <= 0) return std::wstring();
        std::wstring out(need, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], need);
        return out;
    }

    // Append the server-resolution query (?config=&client=&game=0x<TDS>) to a
    // base path. gameBuild==0 => omit the game key (normal cheat-update path).
    // The server uses config+game to pick the matching binary (option B); the
    // client key lets it compare versions for the interactive path.
    static std::wstring BuildQueryPath(const wchar_t* basePath, uint32_t gameBuild)
    {
        wchar_t buf[256];
        if (gameBuild)
            wsprintfW(buf, L"%s?config=%S&client=%S&game=0x%08X",
                      basePath, kUpdateConfig, kClientVersion, gameBuild);
        else
            wsprintfW(buf, L"%s?config=%S&client=%S",
                      basePath, kUpdateConfig, kClientVersion);
        return std::wstring(buf);
    }

    // ------------------------------------------------------------------
    // HTTP GET (binary-safe) against kUpdateHost over TLS.
    // outStatus (optional) receives the HTTP status code so callers can tell a
    // 204 (no compatible build) apart from a network failure.
    // ------------------------------------------------------------------
    static bool HttpGet(const wchar_t* path, std::vector<char>& out, DWORD* outStatus = nullptr)
    {
        out.clear();
        if (outStatus) *outStatus = 0;

        HINTERNET session = WinHttpOpen(L"RUGIR-UPD/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;

        WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);

        bool ok = false;
        HINTERNET connect = nullptr;
        HINTERNET request = nullptr;
        do
        {
            connect = WinHttpConnect(session, kUpdateHost, kUpdatePort, 0);
            if (!connect) break;

            request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, kSecureFlag);
            if (!request) break;

            if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
                break;

            if (!WinHttpReceiveResponse(request, nullptr))
                break;

            // Only accept 200.
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
            if (outStatus) *outStatus = status;
            if (status && status != 200)
                break;

            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0)
                    break;
                size_t base = out.size();
                out.resize(base + avail);
                DWORD read = 0;
                if (!WinHttpReadData(request, out.data() + base, avail, &read) || read == 0)
                {
                    out.resize(base);
                    break;
                }
                out.resize(base + read);
                if (out.size() > 64ull * 1024 * 1024) // 64 MB sanity cap
                    break;
            }
            ok = !out.empty();
        } while (false);

        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return ok;
    }

    // ------------------------------------------------------------------
    // Streaming GET that reports live progress into g_progress (bytes, total,
    // speed, fraction). Used for the DLL download so the UI can draw a bar and
    // a speed readout. Returns the body in `out`.
    // ------------------------------------------------------------------
    static bool HttpGetProgress(const wchar_t* path, std::vector<char>& out)
    {
        out.clear();

        HINTERNET session = WinHttpOpen(L"RUGIR-UPD/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;

        WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);

        bool ok = false;
        HINTERNET connect = nullptr;
        HINTERNET request = nullptr;
        do
        {
            connect = WinHttpConnect(session, kUpdateHost, kUpdatePort, 0);
            if (!connect) break;

            request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, kSecureFlag);
            if (!request) break;

            if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
                break;

            if (!WinHttpReceiveResponse(request, nullptr))
                break;

            DWORD status = 0, sz = sizeof(status);
            if (WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX)
                && status != 200)
                break;

            // Content-Length (may be absent on chunked responses).
            unsigned long long total = 0;
            DWORD clen = 0, clenSz = sizeof(clen);
            if (WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &clen, &clenSz, WINHTTP_NO_HEADER_INDEX))
                total = clen;

            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.bytesReceived = 0;
                g_progress.bytesTotal = total;
                g_progress.speedBytesPerSec = 0;
                g_progress.fraction = 0.0;
            }

            unsigned long long received = 0;
            ULONGLONG t0 = GetTickCount64();
            ULONGLONG lastTick = t0;
            unsigned long long lastBytes = 0;
            double smoothedSpeed = 0.0;

            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0)
                    break;
                size_t base = out.size();
                out.resize(base + avail);
                DWORD read = 0;
                if (!WinHttpReadData(request, out.data() + base, avail, &read) || read == 0)
                {
                    out.resize(base);
                    break;
                }
                out.resize(base + read);
                received += read;

                // Update speed at most ~10x/sec with an EMA so the readout is stable.
                ULONGLONG now = GetTickCount64();
                ULONGLONG dt = now - lastTick;
                if (dt >= 100)
                {
                    double inst = (double)(received - lastBytes) * 1000.0 / (double)dt;
                    smoothedSpeed = smoothedSpeed > 0 ? (smoothedSpeed * 0.7 + inst * 0.3) : inst;
                    lastTick = now;
                    lastBytes = received;

                    std::lock_guard<std::mutex> lk(g_stateMutex);
                    g_progress.bytesReceived = received;
                    g_progress.speedBytesPerSec = smoothedSpeed;
                    g_progress.fraction = total ? (double)received / (double)total : 0.0;
                }

                if (out.size() > 64ull * 1024 * 1024) // 64 MB sanity cap
                    break;
            }

            {
                // Final snapshot: full bar, average speed over the whole transfer.
                ULONGLONG elapsed = GetTickCount64() - t0;
                double avg = elapsed ? (double)received * 1000.0 / (double)elapsed : 0.0;
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.bytesReceived = received;
                if (!g_progress.bytesTotal) g_progress.bytesTotal = received;
                g_progress.speedBytesPerSec = avg;
                g_progress.fraction = 1.0;
            }

            ok = !out.empty();
        } while (false);

        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return ok;
    }

    // ------------------------------------------------------------------
    // Disk swap. Renaming a mapped module's file IS allowed; deleting is not.
    // So: old -> old.old (backup), new -> old (official name). Both atomic
    // renames, no SHARING_VIOLATION even with the old module still mapped.
    // ------------------------------------------------------------------
    static bool SwapOnDisk(const std::wstring& oldPath, const std::wstring& newPath)
    {
        std::wstring backup = oldPath + L".old";

        // Drop any stale backup first (best effort; may be pending until reboot).
        DeleteFileW(backup.c_str());

        if (!MoveFileExW(oldPath.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            SuLog("[SelfUpdate] backup rename failed err=" + std::to_string(GetLastError()));
            return false;
        }

        if (!MoveFileExW(newPath.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            SuLog("[SelfUpdate] promote rename failed err=" + std::to_string(GetLastError()));
            // Roll back so we don't leave the process without its DLL on disk.
            MoveFileExW(backup.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING);
            return false;
        }

        // Backup file is still mapped by the running module; delete it at reboot
        // if the fresh instance doesn't sweep it first.
        MoveFileExW(backup.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return true;
    }

    // ------------------------------------------------------------------
    // Reload trampoline. Position-independent x64 shellcode living in RWX
    // memory OUTSIDE the module, so it survives the unmap. It:
    //   WaitForSingleObject(hUnloadThread, INFINITE)  // old module fully gone
    //   Sleep(400)                                    // settle
    //   LoadLibraryW(path)                            // load the fresh bytes
    //   ExitThread(0)
    // All API addresses are resolved from kernel32 (stays mapped) and baked in;
    // the path string is copied into the same allocation. Nothing points into
    // the dying module.
    // ------------------------------------------------------------------
    static bool SpawnReloadTrampoline(const std::wstring& modulePath, HANDLE hUnloadThread)
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32) { SuLog("[SelfUpdate] tramp: no kernel32"); return false; }

        uint64_t pWait = (uint64_t)GetProcAddress(k32, "WaitForSingleObject");
        uint64_t pSleep = (uint64_t)GetProcAddress(k32, "Sleep");
        uint64_t pLoad = (uint64_t)GetProcAddress(k32, "LoadLibraryW");
        uint64_t pExit = (uint64_t)GetProcAddress(k32, "ExitThread");
        if (!pWait || !pSleep || !pLoad || !pExit)
        {
            SuLog("[SelfUpdate] tramp: GetProcAddress failed");
            return false;
        }

        // Build the shellcode programmatically so imm64 patch offsets are never
        // hand-counted. Each mov reg,imm64 records where its 8-byte immediate
        // lands; we backpatch the path pointer once the buffer is placed.
        std::vector<uint8_t> code;
        auto emit = [&](std::initializer_list<uint8_t> bs) { for (uint8_t b : bs) code.push_back(b); };
        auto emitImm64 = [&](uint64_t v) { for (int i = 0; i < 8; ++i) code.push_back((uint8_t)((v >> (i * 8)) & 0xFF)); };
        auto movRcx = [&](uint64_t v) { emit({ 0x48, 0xB9 }); emitImm64(v); };  // mov rcx, imm64
        auto movRdx = [&](uint64_t v) { emit({ 0x48, 0xBA }); emitImm64(v); };  // mov rdx, imm64
        auto movRax = [&](uint64_t v) { emit({ 0x48, 0xB8 }); emitImm64(v); };  // mov rax, imm64
        auto callRax = [&]() { emit({ 0xFF, 0xD0 }); };

        emit({ 0x48, 0x83, 0xEC, 0x38 });          // sub rsp, 38h  (align + shadow space)

        // WaitForSingleObject(hUnloadThread, INFINITE)
        movRcx(0);                                  // rcx = thread handle  (patched below)
        size_t handleOff = code.size() - 8;
        movRdx((uint64_t)(int64_t)-1);              // rdx = INFINITE
        movRax(pWait);
        callRax();

        // Sleep(400)
        movRcx(400);
        movRax(pSleep);
        callRax();

        // LoadLibraryW(path)
        movRcx(0);                                  // rcx = path pointer   (patched below)
        size_t pathOff = code.size() - 8;
        movRax(pLoad);
        callRax();

        // ExitThread(0)
        emit({ 0x48, 0x31, 0xC9 });                 // xor rcx, rcx
        movRax(pExit);
        callRax();

        const size_t codeLen = code.size();
        const size_t pathBytes = (modulePath.size() + 1) * sizeof(wchar_t);

        BYTE* mem = (BYTE*)VirtualAlloc(nullptr, codeLen + pathBytes,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!mem) { SuLog("[SelfUpdate] tramp: VirtualAlloc failed"); return false; }

        BYTE* pathDst = mem + codeLen;

        // Duplicate the unload-thread handle so the trampoline owns a valid one.
        HANDLE hDup = nullptr;
        DuplicateHandle(GetCurrentProcess(), hUnloadThread,
            GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS);
        uint64_t hWaitOn = (uint64_t)(hDup ? hDup : hUnloadThread);

        // Backpatch the two runtime pointers into the recorded immediate slots.
        memcpy(code.data() + handleOff, &hWaitOn, 8);
        uint64_t pathPtr = (uint64_t)pathDst;
        memcpy(code.data() + pathOff, &pathPtr, 8);

        memcpy(mem, code.data(), codeLen);
        memcpy(pathDst, modulePath.c_str(), pathBytes);

        FlushInstructionCache(GetCurrentProcess(), mem, codeLen);

        HANDLE hT = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)mem, nullptr, 0, nullptr);
        if (!hT)
        {
            SuLog("[SelfUpdate] tramp: CreateThread failed");
            if (hDup) CloseHandle(hDup);
            VirtualFree(mem, 0, MEM_RELEASE);
            return false;
        }
        CloseHandle(hT);
        SuLog("[SelfUpdate] tramp: armed");
        return true;
    }

    // Thread that performs the clean teardown, then frees the module. Its exit
    // is what the trampoline waits on before reloading.
    static DWORD WINAPI UnloadThenExit(LPVOID)
    {
        SuLog("[SelfUpdate] unload thread: cleanup begin");
        if (g_cleanup)
            g_cleanup();
        SuLog("[SelfUpdate] unload thread: cleanup done, FreeLibraryAndExitThread");

        FreeLibraryAndExitThread(g_self, 0);
        return 0; // unreachable
    }

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------
    void CleanupLeftovers()
    {
        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(g_self, path, MAX_PATH) == 0)
            return;
        std::wstring backup = std::wstring(path) + L".old";
        DeleteFileW(backup.c_str());
        std::wstring stale = std::wstring(path) + L".new";
        DeleteFileW(stale.c_str());
    }

    void Configure(HMODULE self, CleanupFn cleanup)
    {
        g_self = self;
        g_cleanup = cleanup;
        SuLog("[SelfUpdate] Configure called");
        CleanupLeftovers();
    }

    const char* ClientVersion()
    {
        return kClientVersion;
    }

    // ------------------------------------------------------------------
    // State machine workers
    // ------------------------------------------------------------------

    // Manifest check. Idle -> Checking -> Available (update) / Idle (current) / Error.
    static void CheckWorker()
    {
        if (!g_self) { SetError("module handle null"); return; }

        std::vector<char> manifestBytes;
        if (!HttpGet(kManifestPath, manifestBytes))
        {
            SuLog("[SelfUpdate] manifest fetch failed");
            SetError("Could not reach the server.");
            return;
        }
        std::string manifest(manifestBytes.begin(), manifestBytes.end());

        std::string version = JsonString(manifest, "version");
        std::string wantHash = JsonString(manifest, "hash");
        std::string file = JsonString(manifest, "file");
        if (version.empty() || wantHash.empty())
        {
            SuLog("[SelfUpdate] manifest malformed");
            SetError("Invalid server manifest.");
            return;
        }

        if (version == kClientVersion)
        {
            SuLog(std::string("[SelfUpdate] up to date (") + kClientVersion + ")");
            SetState(State::Idle);
            return;
        }

        SuLog(std::string("[SelfUpdate] update available: ") + kClientVersion + " -> " + version);
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_progress.latestVersion = version;
        g_stagedHash = wantHash;
        g_stagedFilePath = file.empty() ? std::wstring(kDownloadPath) : Utf8ToUtf16(file);
        g_stagedDll.clear();
        g_progress.state = State::Available;
    }

    // Download + integrity + stage on disk. Available -> Downloading -> Ready / Error.
    static void DownloadWorker()
    {
        std::wstring dlPath, wantHashW;
        std::string wantHash;
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            dlPath = g_stagedFilePath;
            wantHash = g_stagedHash;
        }

        SuLog("[SelfUpdate] download begin");
        std::vector<char> dll;
        if (!HttpGetProgress(dlPath.c_str(), dll) || dll.empty())
        {
            SuLog("[SelfUpdate] download failed");
            SetError("Download failed.");
            return;
        }

        std::string gotHash = Sha256Hex(dll.data(), dll.size());
        if (_stricmp(gotHash.c_str(), wantHash.c_str()) != 0)
        {
            SuLog("[SelfUpdate] hash mismatch got=" + gotHash + " want=" + wantHash);
            SetError("Integrity check failed.");
            return;
        }

        SuLog("[SelfUpdate] download ok, hash verified, staged in memory");
        std::lock_guard<std::mutex> lk(g_stateMutex);
        g_stagedDll = std::move(dll);
        g_progress.state = State::Ready;
    }

    // Swap + unload + reload. Ready -> Applying -> (module dies). This is the
    // validated apply path, unchanged from the original CheckAndApply.
    static void ApplyWorker()
    {
        if (!g_self) { SetError("module handle null"); return; }

        wchar_t modBuf[MAX_PATH] = {};
        if (GetModuleFileNameW(g_self, modBuf, MAX_PATH) == 0)
        {
            SetError("Module path not found.");
            return;
        }
        std::wstring oldPath = modBuf;

        std::vector<char> dll;
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            dll = std::move(g_stagedDll);
        }
        if (dll.empty()) { SetError("No binary pending."); return; }

        // Stage on disk (next to the module).
        std::wstring newPath = oldPath + L".new";
        {
            HANDLE hf = CreateFileW(newPath.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf == INVALID_HANDLE_VALUE)
            {
                SuLog("[SelfUpdate] cannot create staged file err=" + std::to_string(GetLastError()));
                SetError("Cannot write to disk.");
                return;
            }
            DWORD written = 0;
            BOOL wok = WriteFile(hf, dll.data(), (DWORD)dll.size(), &written, nullptr);
            CloseHandle(hf);
            if (!wok || written != dll.size())
            {
                DeleteFileW(newPath.c_str());
                SetError("Incomplete disk write.");
                return;
            }
        }

        // Swap names while still mapped (rename allowed on mapped module).
        if (!SwapOnDisk(oldPath, newPath))
        {
            SetError("File replacement failed.");
            return;
        }

        SuLog("[SelfUpdate] swapped on disk, unloading + reload trampoline");

        // Spawn the unload thread, hand its handle to the reload trampoline.
        HANDLE hUnload = CreateThread(nullptr, 0, UnloadThenExit, nullptr, CREATE_SUSPENDED, nullptr);
        if (!hUnload)
        {
            SuLog("[SelfUpdate] unload thread create failed");
            SetError("Unload thread creation failed.");
            return;
        }

        if (!SpawnReloadTrampoline(oldPath, hUnload))
        {
            TerminateThread(hUnload, 0);
            CloseHandle(hUnload);
            SuLog("[SelfUpdate] trampoline arm failed, aborted unload");
            SetError("Reload arming failed.");
            return;
        }

        ResumeThread(hUnload);   // teardown + FreeLibraryAndExitThread now runs
        CloseHandle(hUnload);    // trampoline holds its own duplicated handle
        // Module is on its way out; no further code runs reliably here.
    }

    // ------------------------------------------------------------------
    // Auto (game-update) path. Runs entirely without user interaction.
    // Queries the server for a build matching THIS game (config + gameBuild):
    //   - 200 with a "file" => download (progress -> toast) + hash verify, then
    //     apply immediately (no Ready/OK gate).
    //   - 204 / no binary   => WaitingCompatible + arm a background retry that
    //     re-runs this worker after a delay until a compatible build appears.
    // Reuses HttpGetProgress / ApplyWorker so the swap+reload path is identical.
    // ------------------------------------------------------------------
    static void ArmAutoRetry();   // fwd

    static void AutoWorker()
    {
        if (!g_self) { SetError("module handle null"); return; }

        uint32_t gb = g_autoGameBuild.load();
        std::wstring manifestPath = BuildQueryPath(kManifestPath, gb);

        std::vector<char> manifestBytes;
        DWORD status = 0;
        bool got = HttpGet(manifestPath.c_str(), manifestBytes, &status);

        if (status == 204)
        {
            // Server reachable but no compatible build published yet.
            SuLog("[SelfUpdate] auto: 204 no compatible build, waiting + retry");
            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.state = State::WaitingCompatible;
                g_progress.autoMode = true;
                g_progress.gameIncompatible = true;
                g_progress.errorText.clear();
            }
            ArmAutoRetry();
            return;
        }

        if (!got || manifestBytes.empty())
        {
            // Network failure — stay in waiting mode and retry rather than error.
            SuLog("[SelfUpdate] auto: manifest fetch failed, waiting + retry");
            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.state = State::WaitingCompatible;
                g_progress.autoMode = true;
                g_progress.gameIncompatible = true;
            }
            ArmAutoRetry();
            return;
        }

        std::string manifest(manifestBytes.begin(), manifestBytes.end());
        std::string version  = JsonString(manifest, "version");
        std::string wantHash = JsonString(manifest, "hash");
        std::string file     = JsonString(manifest, "file");
        if (version.empty() || wantHash.empty())
        {
            SuLog("[SelfUpdate] auto: manifest malformed, waiting + retry");
            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.state = State::WaitingCompatible;
                g_progress.autoMode = true;
                g_progress.gameIncompatible = true;
            }
            ArmAutoRetry();
            return;
        }

        // Compatible build online. Go straight to AutoUpdating + download.
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            g_progress.latestVersion   = version;
            g_progress.autoMode        = true;
            g_progress.gameIncompatible = true;
            g_stagedHash    = wantHash;
            g_stagedFilePath = file.empty()
                ? BuildQueryPath(kDownloadPath, gb)
                : Utf8ToUtf16(file);
            g_stagedDll.clear();
            g_progress.state = State::AutoUpdating;
        }
        SuLog(std::string("[SelfUpdate] auto: build found v") + version + ", downloading");

        std::vector<char> dll;
        if (!HttpGetProgress(g_stagedFilePath.c_str(), dll) || dll.empty())
        {
            SuLog("[SelfUpdate] auto: download failed, waiting + retry");
            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.state = State::WaitingCompatible;
            }
            ArmAutoRetry();
            return;
        }

        std::string gotHash = Sha256Hex(dll.data(), dll.size());
        if (_stricmp(gotHash.c_str(), wantHash.c_str()) != 0)
        {
            SuLog("[SelfUpdate] auto: hash mismatch, waiting + retry");
            {
                std::lock_guard<std::mutex> lk(g_stateMutex);
                g_progress.state = State::WaitingCompatible;
            }
            ArmAutoRetry();
            return;
        }

        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            g_stagedDll = std::move(dll);
        }
        SuLog("[SelfUpdate] auto: verified, applying automatically");
        // Apply immediately — no user gate in auto mode. ApplyWorker performs the
        // swap + unload + reload; the module starts dying right after.
        ApplyWorker();
    }

    // Background retry timer for the auto path. Sleeps ~2 min then re-dispatches
    // AutoWorker. Only one retry may be armed at a time.
    static void ArmAutoRetry()
    {
        bool expected = false;
        if (!g_retryArmed.compare_exchange_strong(expected, true))
            return; // already armed
        std::thread([]() {
            Sleep(120000); // ~2 min
            g_retryArmed.store(false);
            SuLog("[SelfUpdate] auto: retry firing");
            AutoWorker();
        }).detach();
    }

    // ------------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------------
    void CheckAsync()
    {
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            // Don't restart a check while one is in flight or mid-download/apply.
            if (g_progress.state == State::Checking ||
                g_progress.state == State::Downloading ||
                g_progress.state == State::Applying)
                return;
            g_progress.state = State::Checking;
            g_progress.errorText.clear();
        }
        SuLog("[SelfUpdate] CheckAsync dispatched");
        std::thread([]() { CheckWorker(); }).detach();
    }

    void CheckAsyncAuto(unsigned int gameBuild)
    {
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            // Don't restart while already downloading/applying in auto mode.
            if (g_progress.state == State::AutoUpdating ||
                g_progress.state == State::Applying)
                return;
            g_progress.autoMode = true;
            g_progress.gameIncompatible = true;
            g_progress.errorText.clear();
        }
        g_autoGameBuild.store(gameBuild);
        g_autoMode.store(true);
        {
            char b[32]; wsprintfA(b, "0x%08X", gameBuild);
            SuLog(std::string("[SelfUpdate] CheckAsyncAuto dispatched game=") + b);
        }
        std::thread([]() { AutoWorker(); }).detach();
    }

    void AcceptDownload()
    {
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            if (g_progress.state != State::Available) return;
            g_progress.state = State::Downloading;
            g_progress.errorText.clear();
        }
        SuLog("[SelfUpdate] user accepted download");
        std::thread([]() { DownloadWorker(); }).detach();
    }

    void DeclineDownload()
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        if (g_progress.state == State::Available)
            g_progress.state = State::Idle;
    }

    void ApplyUpdate()
    {
        {
            std::lock_guard<std::mutex> lk(g_stateMutex);
            if (g_progress.state != State::Ready) return;
            g_progress.state = State::Applying;
        }
        SuLog("[SelfUpdate] user confirmed apply");
        std::thread([]() { ApplyWorker(); }).detach();
    }

    void DismissError()
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        if (g_progress.state == State::Error)
        {
            g_progress.state = State::Idle;
            g_progress.errorText.clear();
        }
    }

    Progress GetProgress()
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        return g_progress;
    }

    bool HasPendingUI()
    {
        std::lock_guard<std::mutex> lk(g_stateMutex);
        switch (g_progress.state)
        {
        case State::Available:
        case State::Downloading:
        case State::Ready:
        case State::Applying:
        case State::Error:
        case State::AutoUpdating:
        case State::WaitingCompatible:
            return true;
        default:
            return false;
        }
    }
}

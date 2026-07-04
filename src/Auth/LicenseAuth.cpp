#include "LicenseAuth.h"

#include <Windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <shlobj.h>
#include <intrin.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "../Utils/Logger.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace Auth
{
    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    // Auth server host, no scheme, no trailing slash. TLS is forced.
    static const wchar_t* kAuthHost = L"internalmhur1.up.railway.app";
    static const wchar_t* kVerifyPath = L"/api/auth/verify";

    static const char* kClientVersion = "1.0.0";

    // Session stays valid this long after the last successful verify.
    // The heartbeat renews it well before it lapses; a killed network only
    // buys the user a short grace period, then the gate closes.
    static const unsigned long long kSessionValidMs = 20ull * 60ull * 1000ull;
    static const unsigned long long kHeartbeatEveryMs = 10ull * 60ull * 1000ull;

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    struct VerifyResult
    {
        bool ok = false;
        std::string error;   // server error code, or "network"
        std::string token;
        std::string tier;
        std::string expiresAt;   // ISO8601 from server, empty if none
    };

    static std::mutex g_mutex;
    static State g_state = State::Idle;
    static std::string g_key;            // current license key (guarded)
    static std::string g_tier;           // guarded
    static std::string g_expiresAt;      // formatted expiration (guarded)
    static std::string g_statusText = "Aucune cle.";
    static unsigned long long g_sessionExpiryMs = 0;   // GetTickCount64 deadline
    static unsigned long long g_lastHeartbeatMs = 0;
    static std::atomic<bool> g_requestInFlight{ false };

    // ------------------------------------------------------------------
    // Small helpers
    // ------------------------------------------------------------------

    static std::string ToHexLower(const unsigned char* data, size_t len)
    {
        static const char* digits = "0123456789abcdef";
        std::string out;
        out.resize(len * 2);
        for (size_t i = 0; i < len; ++i)
        {
            out[i * 2] = digits[(data[i] >> 4) & 0xF];
            out[i * 2 + 1] = digits[data[i] & 0xF];
        }
        return out;
    }

    static std::string Sha256Hex(const std::string& input)
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
                    reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                    static_cast<ULONG>(input.size()), 0) != 0)
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

    static std::string Utf16ToUtf8(const wchar_t* w)
    {
        if (!w) return std::string();
        int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (need <= 1) return std::string();
        std::string out(static_cast<size_t>(need - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], need, nullptr, nullptr);
        return out;
    }

    // ------------------------------------------------------------------
    // HWID
    // ------------------------------------------------------------------

    static std::string ReadMachineGuid()
    {
        wchar_t buffer[128] = {};
        DWORD size = sizeof(buffer);
        // 64-bit view so we read the real value from a WOW64 process too.
        LSTATUS st = RegGetValueW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid",
            RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buffer, &size);
        if (st != ERROR_SUCCESS)
            return std::string();
        return Utf16ToUtf8(buffer);
    }

    static std::string ReadSystemVolumeSerial()
    {
        wchar_t systemDir[MAX_PATH] = {};
        if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0)
            return std::string();

        // Keep only the root ("C:\\").
        wchar_t root[4] = { systemDir[0], L':', L'\\', 0 };
        DWORD serial = 0;
        if (!GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
            return std::string();

        char tmp[16] = {};
        wsprintfA(tmp, "%08lX", serial);
        return std::string(tmp);
    }

    static std::string ReadCpuId()
    {
        int regs[4] = {};
        __cpuid(regs, 0);
        char vendor[13] = {};
        *reinterpret_cast<int*>(vendor + 0) = regs[1];
        *reinterpret_cast<int*>(vendor + 4) = regs[3];
        *reinterpret_cast<int*>(vendor + 8) = regs[2];

        int info[4] = {};
        __cpuid(info, 1);

        char out[64] = {};
        wsprintfA(out, "%s-%08X", vendor, info[0]);
        return std::string(out);
    }

    const std::string& GetHwid()
    {
        static std::string cached;
        static bool built = false;
        if (built) return cached;

        std::string raw;
        raw += "mg:" + ReadMachineGuid();
        raw += "|vs:" + ReadSystemVolumeSerial();
        raw += "|cpu:" + ReadCpuId();

        cached = Sha256Hex(raw);
        if (cached.empty())
            cached = Sha256Hex("rugir-fallback|" + raw); // never empty in practice
        built = true;
        return cached;
    }

    std::string GetHwidShort()
    {
        const std::string& h = GetHwid();
        return h.size() >= 8 ? h.substr(0, 8) : h;
    }

    // ------------------------------------------------------------------
    // Minimal JSON scraping (no library; input is our own server's shape)
    // ------------------------------------------------------------------

    static bool JsonBool(const std::string& body, const char* key)
    {
        std::string needle = std::string("\"") + key + "\"";
        size_t p = body.find(needle);
        if (p == std::string::npos) return false;
        p = body.find(':', p + needle.size());
        if (p == std::string::npos) return false;
        return body.find("true", p) == body.find_first_not_of(" \t", p + 1);
    }

    static std::string JsonString(const std::string& body, const char* key)
    {
        std::string needle = std::string("\"") + key + "\"";
        size_t p = body.find(needle);
        if (p == std::string::npos) return std::string();
        p = body.find(':', p + needle.size());
        if (p == std::string::npos) return std::string();
        size_t q = body.find('"', p);
        if (q == std::string::npos) return std::string();
        size_t r = body.find('"', q + 1);
        if (r == std::string::npos) return std::string();
        return body.substr(q + 1, r - q - 1);
    }

    static std::string JsonEscape(const std::string& in)
    {
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in)
        {
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) { /* drop control chars */ }
                else out += c;
            }
        }
        return out;
    }

    // ------------------------------------------------------------------
    // HTTP verify
    // ------------------------------------------------------------------

    static VerifyResult VerifyOnce(const std::string& key)
    {
        VerifyResult vr;
        vr.error = "network";

        std::string body = "{\"key\":\"" + JsonEscape(key) +
                           "\",\"hwid\":\"" + GetHwid() +
                           "\",\"version\":\"" + kClientVersion + "\"}";

        HINTERNET session = WinHttpOpen(L"RUGIR/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return vr;

        WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

        HINTERNET connect = nullptr;
        HINTERNET request = nullptr;
        do
        {
            connect = WinHttpConnect(session, kAuthHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connect) break;

            request = WinHttpOpenRequest(connect, L"POST", kVerifyPath, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!request) break;

            const wchar_t* headers = L"Content-Type: application/json\r\n";
            if (!WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                    const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                    static_cast<DWORD>(body.size()), 0))
                break;

            if (!WinHttpReceiveResponse(request, nullptr))
                break;

            std::string response;
            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0)
                    break;
                std::vector<char> chunk(avail);
                DWORD read = 0;
                if (!WinHttpReadData(request, chunk.data(), avail, &read) || read == 0)
                    break;
                response.append(chunk.data(), read);
                if (response.size() > 64 * 1024) // sanity cap
                    break;
            }

            // We got a well-formed reply: parse it.
            vr.ok = JsonBool(response, "ok");
            if (vr.ok)
            {
                vr.error.clear();
                vr.token = JsonString(response, "token");
                vr.tier = JsonString(response, "tier");
                vr.expiresAt = JsonString(response, "expiresAt");
            }
            else
            {
                std::string err = JsonString(response, "error");
                vr.error = err.empty() ? "invalid_key" : err;
            }
        } while (false);

        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return vr;
    }

    // ------------------------------------------------------------------
    // Persistence (DPAPI: blob bound to this Windows user + machine)
    // ------------------------------------------------------------------

    static std::wstring KeyFilePath()
    {
        wchar_t appdata[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata) != S_OK)
            return std::wstring();
        std::wstring dir = std::wstring(appdata) + L"\\RUGIR";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\lic.bin";
    }

    static void SaveKeyEncrypted(const std::string& key)
    {
        std::wstring path = KeyFilePath();
        if (path.empty()) return;

        DATA_BLOB in{ static_cast<DWORD>(key.size()),
                      reinterpret_cast<BYTE*>(const_cast<char*>(key.data())) };
        DATA_BLOB out{};
        if (!CryptProtectData(&in, L"rugir-license", nullptr, nullptr, nullptr,
                CRYPTPROTECT_UI_FORBIDDEN, &out))
            return;

        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(h, out.pbData, out.cbData, &written, nullptr);
            CloseHandle(h);
        }
        if (out.pbData) LocalFree(out.pbData);
    }

    static std::string LoadKeyDecrypted()
    {
        std::wstring path = KeyFilePath();
        if (path.empty()) return std::string();

        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return std::string();

        std::vector<BYTE> raw;
        DWORD size = GetFileSize(h, nullptr);
        if (size > 0 && size < 64 * 1024)
        {
            raw.resize(size);
            DWORD readBytes = 0;
            if (!ReadFile(h, raw.data(), size, &readBytes, nullptr) || readBytes != size)
                raw.clear();
        }
        CloseHandle(h);
        if (raw.empty()) return std::string();

        DATA_BLOB in{ static_cast<DWORD>(raw.size()), raw.data() };
        DATA_BLOB out{};
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                CRYPTPROTECT_UI_FORBIDDEN, &out))
            return std::string();

        std::string key(reinterpret_cast<char*>(out.pbData), out.cbData);
        if (out.pbData) LocalFree(out.pbData);
        return key;
    }

    static void DeleteSavedKey()
    {
        std::wstring path = KeyFilePath();
        if (!path.empty())
            DeleteFileW(path.c_str());
    }

    // ------------------------------------------------------------------
    // State transitions
    // ------------------------------------------------------------------

    // "2026-08-03T23:07:46.903Z" -> "2026-08-03 23:07 UTC". Empty stays empty.
    static std::string FormatExpiry(const std::string& iso)
    {
        if (iso.size() < 16)
            return std::string();
        return iso.substr(0, 10) + " " + iso.substr(11, 5) + " UTC";
    }

    static std::string ErrorToFrench(const std::string& code)
    {
        if (code == "invalid_key")          return "Cle invalide.";
        if (code == "hwid_limit_reached")   return "Cle deja liee a un autre PC.";
        if (code == "license_expired")      return "Cle expiree.";
        if (code == "license_revoked")      return "Cle revoquee.";
        if (code == "missing_key_or_hwid")  return "Cle ou HWID manquant.";
        if (code == "network")              return "Serveur injoignable.";
        return "Refus: " + code;
    }

    // Runs on a worker thread. `save` = persist the key on success.
    static void DoVerify(std::string key, bool save)
    {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_state = State::Checking;
            g_statusText = "Verification...";
        }

        VerifyResult vr = VerifyOnce(key);

        std::lock_guard<std::mutex> lock(g_mutex);
        if (vr.ok)
        {
            g_key = key;
            g_tier = vr.tier;
            g_expiresAt = FormatExpiry(vr.expiresAt);
            g_state = State::Authorized;
            g_sessionExpiryMs = GetTickCount64() + kSessionValidMs;
            g_lastHeartbeatMs = GetTickCount64();
            g_statusText = "Active" + (vr.tier.empty() ? std::string() : " (" + vr.tier + ")") + ".";
            Logger::LogInfo("[Auth] Authorized, tier=" + vr.tier);
            if (save)
                SaveKeyEncrypted(key);
        }
        else
        {
            // A hard rejection kills any saved key; a network blip does not.
            if (vr.error != "network")
            {
                g_state = State::Denied;
                g_sessionExpiryMs = 0;
                DeleteSavedKey();
                g_key.clear();
            }
            else if (g_state != State::Authorized)
            {
                g_state = State::Denied;
            }
            g_statusText = ErrorToFrench(vr.error);
            Logger::LogWarning("[Auth] Verify failed: " + vr.error);
        }
        g_requestInFlight = false;
    }

    void ActivateAsync(const std::string& key)
    {
        std::string trimmed = key;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\r' ||
                                    trimmed.back() == '\n' || trimmed.back() == '\t'))
            trimmed.pop_back();
        if (trimmed.empty())
            return;
        bool expected = false;
        if (!g_requestInFlight.compare_exchange_strong(expected, true))
            return; // already checking

        std::thread(DoVerify, trimmed, true).detach();
    }

    std::string GetSavedKey()
    {
        // Only pre-fills the input field. Never connects on its own.
        return LoadKeyDecrypted();
    }

    void HeartbeatTick()
    {
        std::string key;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_state != State::Authorized || g_key.empty())
                return;
            if (GetTickCount64() - g_lastHeartbeatMs < kHeartbeatEveryMs)
                return;
            g_lastHeartbeatMs = GetTickCount64();
            key = g_key;
        }
        bool expected = false;
        if (!g_requestInFlight.compare_exchange_strong(expected, true))
            return;
        std::thread(DoVerify, key, false).detach();
    }

    bool IsAuthorized()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_state == State::Authorized && GetTickCount64() < g_sessionExpiryMs;
    }

    State GetState()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_state;
    }

    std::string GetStatusText()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_statusText;
    }

    std::string GetTier()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_tier;
    }

    std::string GetExpiresAt()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_expiresAt;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state = State::Idle;
        g_key.clear();
        g_tier.clear();
        g_expiresAt.clear();
        g_sessionExpiryMs = 0;
        g_statusText = "Aucune cle.";
        DeleteSavedKey();
    }
}

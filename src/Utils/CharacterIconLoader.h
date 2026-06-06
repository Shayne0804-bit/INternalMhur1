#pragma once
#include <d3d11.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <windows.h>
#include <cstring>
#include <wincodec.h>

namespace IconLoader
{
    class CharacterIconCache
    {
    private:
        static std::unordered_map<int, ID3D11ShaderResourceView*> s_iconCache;
        static ID3D11Device* s_device;
        static ID3D11DeviceContext* s_deviceContext;

    public:
        static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            s_device = device;
            s_deviceContext = context;
        }

        static void Shutdown()
        {
            for (auto& pair : s_iconCache)
            {
                if (pair.second)
                    pair.second->Release();
            }
            s_iconCache.clear();
            s_device = nullptr;
            s_deviceContext = nullptr;
        }

        static ID3D11ShaderResourceView* GetCharacterIcon(int characterId)
        {
            // Check cache first
            auto it = s_iconCache.find(characterId);
            if (it != s_iconCache.end())
                return it->second;

            ID3D11ShaderResourceView* srv = nullptr;
            for (const std::string& iconPath : GetIconFilePaths(characterId))
            {
                srv = LoadPNGFromFile(iconPath.c_str());
                if (srv)
                    break;
            }
            
            if (srv)
            {
                s_iconCache[characterId] = srv;
            }
            else
            {
                // Cache misses too; otherwise the menu retries disk/WIC lookups every frame.
                s_iconCache[characterId] = nullptr;
            }
            
            return srv;
        }

        /**
         * Public method to load PNG from arbitrary file path
         * Used for loading logos and other assets
         */
        static ID3D11ShaderResourceView* LoadPNGFromFileExternal(const char* filePath)
        {
            return LoadPNGFromFile(filePath);
        }

        static std::string GetProjectRootPath()
        {
            char dllPath[MAX_PATH] = { 0 };
            HMODULE hModule = NULL;

            if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&GetProjectRootPath,
                &hModule))
            {
                return {};
            }

            if (!GetModuleFileNameA(hModule, dllPath, MAX_PATH))
                return {};

            char* lastSlash = strrchr(dllPath, '\\');
            if (!lastSlash)
                return {};

            *lastSlash = '\0'; // DLL directory, usually bin\Release or bin\Debug.

            std::string moduleDir = dllPath;
            const std::string releaseSuffix = "\\bin\\Release";
            const std::string debugSuffix = "\\bin\\Debug";

            if (moduleDir.size() >= releaseSuffix.size() &&
                moduleDir.compare(moduleDir.size() - releaseSuffix.size(), releaseSuffix.size(), releaseSuffix) == 0)
            {
                moduleDir.resize(moduleDir.size() - releaseSuffix.size());
            }
            else if (moduleDir.size() >= debugSuffix.size() &&
                moduleDir.compare(moduleDir.size() - debugSuffix.size(), debugSuffix.size(), debugSuffix) == 0)
            {
                moduleDir.resize(moduleDir.size() - debugSuffix.size());
            }

            return moduleDir;
        }

    private:
        static std::string GetDllDirectoryPath()
        {
            char dllPath[MAX_PATH] = { 0 };
            HMODULE hModule = NULL;

            if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&GetDllDirectoryPath,
                &hModule))
            {
                return {};
            }

            if (!GetModuleFileNameA(hModule, dllPath, MAX_PATH))
                return {};

            char* lastSlash = strrchr(dllPath, '\\');
            if (!lastSlash)
                return {};

            *lastSlash = '\0';
            return dllPath;
        }

        static std::string JoinPath(const std::string& base, const std::string& relativePath)
        {
            if (base.empty())
                return relativePath;

            std::string result = base;
            if (!result.empty() && result.back() != '\\' && !relativePath.empty() && relativePath.front() != '\\')
                result += "\\";
            result += relativePath;
            return result;
        }

        static std::string GetIconFileName(int characterId)
        {
            return std::to_string(characterId) + ".png";
        }

        static std::vector<std::string> GetIconFilePaths(int characterId)
        {
            std::vector<std::string> paths;
            std::string fileName = GetIconFileName(characterId);
            std::string dllDir = GetDllDirectoryPath();
            std::string projectRoot = GetProjectRootPath();

            if (!dllDir.empty())
            {
                paths.push_back(JoinPath(JoinPath(dllDir, "Icons"), fileName));
                paths.push_back(JoinPath(dllDir, fileName));
            }

            if (!projectRoot.empty())
                paths.push_back(JoinPath(JoinPath(projectRoot, "src\\Icons"), fileName));

            paths.push_back(JoinPath("src\\Icons", fileName));
            return paths;
        }
        static ID3D11ShaderResourceView* LoadPNGFromFile(const char* filePath)
        {
            if (!s_device || !s_deviceContext || !filePath || !filePath[0])
                return nullptr;

            DWORD attributes = GetFileAttributesA(filePath);
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
                return nullptr;

            HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            bool shouldUninitializeCom = SUCCEEDED(comHr);

            IWICImagingFactory* factory = nullptr;
            IWICBitmapDecoder* decoder = nullptr;
            IWICBitmapFrameDecode* frame = nullptr;
            IWICFormatConverter* converter = nullptr;
            ID3D11Texture2D* texture = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;

            HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                __uuidof(IWICImagingFactory),
                (LPVOID*)&factory);

            if (FAILED(hr) || !factory)
            {
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Convert path to wide string
            int len = MultiByteToWideChar(CP_ACP, 0, filePath, -1, NULL, 0);
            if (len <= 0)
            {
                factory->Release();
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            wchar_t* widePath = new wchar_t[len];
            MultiByteToWideChar(CP_ACP, 0, filePath, -1, widePath, len);

            // Create decoder
            hr = factory->CreateDecoderFromFilename(
                widePath,
                NULL,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder);

            if (FAILED(hr) || !decoder)
            {
                delete[] widePath;
                factory->Release();
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Get first frame
            hr = decoder->GetFrame(0, &frame);
            if (FAILED(hr) || !frame)
            {
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Create format converter
            hr = factory->CreateFormatConverter(&converter);
            if (FAILED(hr) || !converter)
            {
                frame->Release();
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Convert to RGBA format
            hr = converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                NULL,
                0.0,
                WICBitmapPaletteTypeCustom);

            if (FAILED(hr))
            {
                converter->Release();
                frame->Release();
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Get image dimensions
            UINT width = 0, height = 0;
            converter->GetSize(&width, &height);
            if (width == 0 || height == 0)
            {
                converter->Release();
                frame->Release();
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Read pixel data
            UINT stride = width * 4;  // 32 bpp RGBA
            UINT dataSize = stride * height;
            BYTE* pixelData = new BYTE[dataSize];

            hr = converter->CopyPixels(NULL, stride, dataSize, pixelData);
            if (FAILED(hr))
            {
                delete[] pixelData;
                converter->Release();
                frame->Release();
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Create D3D11 texture
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = width;
            texDesc.Height = height;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_IMMUTABLE;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = pixelData;
            initData.SysMemPitch = stride;

            hr = s_device->CreateTexture2D(&texDesc, &initData, &texture);
            if (FAILED(hr) || !texture)
            {
                delete[] pixelData;
                converter->Release();
                frame->Release();
                decoder->Release();
                factory->Release();
                delete[] widePath;
                if (shouldUninitializeCom)
                    CoUninitialize();
                return nullptr;
            }

            // Create shader resource view
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = texDesc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            hr = s_device->CreateShaderResourceView(texture, &srvDesc, &srv);
            texture->Release();

            delete[] pixelData;
            converter->Release();
            frame->Release();
            decoder->Release();
            factory->Release();
            delete[] widePath;
            if (shouldUninitializeCom)
                CoUninitialize();

            return srv;
        }
    };

    // Static member definitions
    std::unordered_map<int, ID3D11ShaderResourceView*> CharacterIconCache::s_iconCache;
    ID3D11Device* CharacterIconCache::s_device = nullptr;
    ID3D11DeviceContext* CharacterIconCache::s_deviceContext = nullptr;
}

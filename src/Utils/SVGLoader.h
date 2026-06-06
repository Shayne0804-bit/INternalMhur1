#pragma once
#include <d3d11.h>
#include <windows.h>
#include <string>

namespace SVGLoader
{
    class SVGTextureGenerator
    {
    private:
        static ID3D11Device* s_device;
        static ID3D11DeviceContext* s_deviceContext;
        static ID3D11Texture2D* s_texture;
        static ID3D11ShaderResourceView* s_srv;
        static constexpr int TEXTURE_SIZE = 512;

    public:
        static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            if (!device || !context) return;

            s_device = device;
            s_deviceContext = context;

            // Create render target texture for dynamic logo
            D3D11_TEXTURE2D_DESC texDesc{};
            texDesc.Width = TEXTURE_SIZE;
            texDesc.Height = TEXTURE_SIZE;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DYNAMIC;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &s_texture);
            if (SUCCEEDED(hr))
            {
                // Create gradient pixels
                uint32_t* pixels = new uint32_t[TEXTURE_SIZE * TEXTURE_SIZE];
                
                // DARK_PURPLE #5B22B8 to PRIMARY #8B3DFF gradient
                uint32_t color1 = 0xFFB8225B; // DARK_PURPLE (BGRA)
                uint32_t color2 = 0xFFFF3D8B; // PRIMARY (BGRA)

                for (int y = 0; y < TEXTURE_SIZE; y++)
                {
                    float t = (float)y / TEXTURE_SIZE;
                    uint32_t r = (uint32_t)(0xB8 + (0xFF - 0xB8) * t);
                    uint32_t g = (uint32_t)(0x22 + (0x3D - 0x22) * t);
                    uint32_t b = (uint32_t)(0x5B + (0x8B - 0x5B) * t);
                    uint32_t color = 0xFF000000 | (b << 16) | (g << 8) | r;

                    for (int x = 0; x < TEXTURE_SIZE; x++)
                    {
                        pixels[y * TEXTURE_SIZE + x] = color;
                    }
                }

                // Fill texture with gradient
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (SUCCEEDED(context->Map(s_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    memcpy(mapped.pData, pixels, TEXTURE_SIZE * TEXTURE_SIZE * 4);
                    context->Unmap(s_texture, 0);
                }

                delete[] pixels;

                // Create SRV
                hr = device->CreateShaderResourceView(s_texture, nullptr, &s_srv);
            }
        }

        static void Shutdown()
        {
            if (s_srv) { s_srv->Release(); s_srv = nullptr; }
            if (s_texture) { s_texture->Release(); s_texture = nullptr; }
        }

        static ID3D11ShaderResourceView* LoadSVG(const char* filePath, int width = TEXTURE_SIZE, int height = TEXTURE_SIZE)
        {
            // For now, return the pre-generated gradient texture
            // Full SVG parsing would require external library (RapidSVG, nanosvg, etc.)
            return s_srv;
        }

        static ID3D11ShaderResourceView* GetSRV() { return s_srv; }
        static int GetTextureSize() { return TEXTURE_SIZE; }
    };

    // Static member initialization
    ID3D11Device* SVGTextureGenerator::s_device = nullptr;
    ID3D11DeviceContext* SVGTextureGenerator::s_deviceContext = nullptr;
    ID3D11Texture2D* SVGTextureGenerator::s_texture = nullptr;
    ID3D11ShaderResourceView* SVGTextureGenerator::s_srv = nullptr;
}

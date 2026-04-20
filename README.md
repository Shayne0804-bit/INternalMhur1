# RUGIR-INTERNAL

Internal C++ cheat/ESP tool for game exploration with ImGui overlay menu and D3D11 hook.

## Features

- 🎮 DLL Injection (x64)
- 🎨 ImGui Overlay Menu
- 🔗 D3D11 Hook for rendering
- 📊 Game object ESP
- 🔧 Modular architecture

## Project Structure

```
src/
├── Core/        - Main initialization
├── Hooks/       - D3D11 rendering hooks
├── Menu/        - ImGui UI components
└── Utils/       - Logging and helpers
```

## Requirements

- Visual Studio 2022 (C++20)
- Windows 10/11 SDK
- DirectX 11 SDK

## Building

```bash
msbuild RUGIR-INTERNAL.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Usage

Inject the compiled DLL into the target game process.

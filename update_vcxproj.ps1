# Modify .vcxproj to include all SDK files
$projFile = "RUGIR-INTERNAL.vcxproj"
$includeFile = "sdk_includes.txt"

# Read the content
$projContent = Get-Content $projFile -Raw
$sdkIncludes = Get-Content $includeFile -Raw

# Find the ClCompile ItemGroup and replace it with new one that includes SDK files
$pattern = '  <ItemGroup>\s*<ClCompile Include="DllEntry\.cpp".*?</ItemGroup>'

# Create new ItemGroup with all includes
$newItemGroup = @"
  <ItemGroup>
    <ClCompile Include="DllEntry.cpp" />
    <ClCompile Include="src\Core\Main.cpp" />
    <ClCompile Include="src\Hooks\D3D11Hook.cpp" />
    <ClCompile Include="src\Menu\ImGuiMenu.cpp" />
    <ClCompile Include="src\Utils\Logger.cpp" />
    <ClCompile Include="src\SDK_Test.cpp" />
$sdkIncludes    <ClCompile Include="src\dependencies\imgui\imgui.cpp" />
    <ClCompile Include="src\dependencies\imgui\imgui_draw.cpp" />
    <ClCompile Include="src\dependencies\imgui\imgui_widgets.cpp" />
    <ClCompile Include="src\dependencies\imgui\imgui_tables.cpp" />
    <ClCompile Include="src\dependencies\imgui\imgui_impl_dx11.cpp" />
    <ClCompile Include="src\dependencies\imgui\imgui_impl_win32.cpp" />
    <ClCompile Include="src\dependencies\imgui\custom.cpp" />
  </ItemGroup>
"@

# Replace
$newContent = $projContent -replace $pattern, $newItemGroup

# Write back
$newContent | Out-File $projFile -Encoding UTF8 -NoNewline

Write-Host "Modified RUGIR-INTERNAL.vcxproj with 627 SDK files"

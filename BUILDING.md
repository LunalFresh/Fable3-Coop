# Source/build provenance

This beta includes the matching modified runtime source in `source/xlivelessness` and module source in `source/xlln-modules-fable3`. The distributed DLLs are the gameplay-tested build; packaging does not rebuild them. Original source READMEs describe upstream features and are not the compatibility guide for this beta. Use the root README and manifest for supported game details.

The validated runtime build used Visual Studio 2022 Build Tools, v143, Win32 Debug, Windows SDK 10.0.26100.0, CMake, and the June 2010 DirectX SDK. xlive's supplied CMake files manage Opus 1.5.2 and RapidXML dependencies and use the static debug CRT. Its resulting DLL does not require the separately installed Visual Studio debug runtime.

For xlive, from a developer shell:

```powershell
cmake -S source/xlivelessness -B build/xlive -G "Visual Studio 17 2022" -A Win32
cmake --build build/xlive --config Debug
cmake -S source/xlivelessness/tests -B build/tests -G "Visual Studio 17 2022" -A Win32
cmake --build build/tests --config Debug
ctest --test-dir build/tests -C Debug --output-on-failure
```

For the Fable module, build its existing `xlln-modules/xlln-fable-3/xlln-fable-3.vcxproj` with MSBuild using `Configuration=Debug`, `Platform=Win32`, `PlatformToolset=v143`, and `WindowsTargetPlatformVersion=10.0.26100.0`. Pass `SolutionDir` as the absolute module source root with a trailing backslash (containing bin/TitleBinary.props), and set isolated OutDir and IntDir paths. No import from a private backend or another user's profile is required.

If MSBuild reports duplicate environment dictionary keys, normalize inherited environment variable names to uppercase before invoking CMake/MSBuild; the validated build used a Python subprocess with `{k.upper(): v for k, v in os.environ.items()}`.

The package manifest describes the distributed binaries. Rebuilt binaries can have different hashes due to build metadata; update your own package manifest after verifying the rebuilt pair. Keep all original exports and the versioned bridge compatible. Rebuilding/repackaging is separate from installing this already-built ZIP.

The network-ID fix links bcrypt from the Windows SDK. Run the six runtime checks with the game closed so the isolated D3D9 test can create its device. The Opus build enables AVX2; support for CPUs without AVX2 has not been verified.

## Installer validation

With the game closed, run this from the package root in Windows PowerShell 5.1. Supply a local supported installation for the two fixture input files and a separate writable directory for test output:

```powershell
.\tests\Test-Installer.ps1 -GameSource "C:\Games\Fable 3" -FixtureRoot "C:\Temp\FableCoopTests"
```

The test copies Fable3.exe and FableLauncher.exe into a new fixture folder; it does not launch them or install into GameSource. The optional `-UpgradeDll` argument selects an older mod DLL for the upgrade/restore check. Otherwise a sentinel file represents the previous shim. Test fixtures stay in FixtureRoot for inspection and must never be included in a release.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Xunit;

namespace Godot.NET.Sdk.Tests;

public class SdkRootingTests
{
    private const int MsBuildTimeoutMilliseconds = 120_000;

    private static readonly string SdkSourceDir = Path.Combine(
        TestPaths.RepoRoot,
        "modules", "mono", "editor", "Godot.NET.Sdk", "Godot.NET.Sdk", "Sdk");

    private static readonly string FixturesDir = Path.Combine(
        TestPaths.RepoRoot,
        "modules", "mono", "editor", "Godot.NET.Sdk", "Godot.NET.Sdk.Tests", "Fixtures");

    // Generated into modules/mono/ by the mono build; imported unconditionally by
    // modules/mono/Directory.Build.props. Only linked into the Sdk/ folder when packing.
    private static readonly string SdkPackageVersionsPath = Path.Combine(
        TestPaths.RepoRoot, "modules", "mono", "SdkPackageVersions.props");

    private sealed record RootItem(string Identity, string DefiningProjectName);

    private static List<RootItem> GetTrimmerRoots(params string[] extraProperties)
        => RunFixture("SdkRootsFixture.csproj", SdkSourceDir, extraProperties);

    private static List<RootItem> RunFixture(string fixtureName, string sdkSourceDir, params string[] extraProperties)
    {
        string workDir = Path.Combine(
            Path.GetTempPath(), "godot-sdk-rooting-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(workDir);
        try
        {
            string fixturePath = Path.Combine(workDir, fixtureName);
            File.Copy(Path.Combine(FixturesDir, fixtureName), fixturePath);

            var arguments = new List<string>
            {
                "msbuild", fixturePath,
                "-getItem:TrimmerRootAssembly",
                "-nologo",
                // Do not leave reusable MSBuild nodes behind; they can hold locks on
                // the copied fixture files and leak processes on CI.
                "-nodeReuse:false",
                $"-p:GodotSdkSourceDir={sdkSourceDir}",
            };
            arguments.AddRange(extraProperties);

            (int exitCode, string stdout, string stderr) =
                ProcessRunner.Run("dotnet", arguments, MsBuildTimeoutMilliseconds);

            Assert.True(exitCode == 0,
                $"dotnet msbuild failed ({exitCode}).\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}");

            int jsonStart = stdout.IndexOf('{');
            Assert.True(jsonStart >= 0,
                $"No JSON output from dotnet msbuild.\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}");

            using JsonDocument document = JsonDocument.Parse(stdout.Substring(jsonStart));
            var roots = new List<RootItem>();
            if (document.RootElement.TryGetProperty("Items", out JsonElement items) &&
                items.TryGetProperty("TrimmerRootAssembly", out JsonElement array))
            {
                foreach (JsonElement element in array.EnumerateArray())
                {
                    roots.Add(new RootItem(
                        element.GetProperty("Identity").GetString()!,
                        element.GetProperty("DefiningProjectName").GetString()!));
                }
            }
            return roots;
        }
        finally
        {
            Directory.Delete(workDir, recursive: true);
        }
    }

    // The Sdk.props fixture needs SdkPackageVersions.props next to the imported
    // Sdk.props. That file is a build artifact, so stage a self-contained copy of
    // the Sdk source folder instead of writing into the repository.
    private static List<RootItem> GetTrimmerRootsThroughSdkProps(params string[] extraProperties)
    {
        Assert.True(File.Exists(SdkPackageVersionsPath),
            $"'{SdkPackageVersionsPath}' is required to evaluate Sdk.props. " +
            "Run the mono build (or generate_sdk_package_versions) first.");

        string stagedSdkDir = Path.Combine(
            Path.GetTempPath(), "godot-sdk-rooting-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stagedSdkDir);
        try
        {
            foreach (string file in Directory.GetFiles(SdkSourceDir))
            {
                File.Copy(file, Path.Combine(stagedSdkDir, Path.GetFileName(file)));
            }
            File.Copy(SdkPackageVersionsPath, Path.Combine(stagedSdkDir, "SdkPackageVersions.props"));
            return RunFixture("SdkPropsFixture.csproj", stagedSdkDir, extraProperties);
        }
        finally
        {
            Directory.Delete(stagedSdkDir, recursive: true);
        }
    }

    [Fact]
    public void DesktopAot_RootsGodotSharpAndGameAssembly()
    {
        var roots = GetTrimmerRoots("-p:PublishAot=true");
        Assert.Equal(
            new[] { "GodotSharp", "MyGame" },
            roots.Select(r => r.Identity).OrderBy(x => x, StringComparer.Ordinal).ToArray());
        Assert.All(roots, r => Assert.Equal("Sdk", r.DefiningProjectName));
    }

    [Fact]
    public void PreviewAot_RootsGodotBindingsAndGameAssembly()
    {
        var roots = GetTrimmerRoots("-p:PublishAot=true", "-p:EnableGodotDotNetPreview=true");
        Assert.Equal(
            new[] { "Godot.Bindings", "MyGame" },
            roots.Select(r => r.Identity).OrderBy(x => x, StringComparer.Ordinal).ToArray());
    }

    [Fact]
    public void NoAot_HasNoTrimmerRoots()
    {
        Assert.Empty(GetTrimmerRoots("-p:PublishAot=false"));
    }

    [Fact]
    public void DisableImplicitGodotSharpReferences_DoesNotRootGodotSharp()
    {
        var roots = GetTrimmerRoots("-p:PublishAot=true", "-p:DisableImplicitGodotSharpReferences=true");
        Assert.Equal(new[] { "MyGame" }, roots.Select(r => r.Identity).ToArray());
    }

    [Fact]
    public void IosAot_RootsAreDefinedBySdkTargets()
    {
        var roots = GetTrimmerRoots("-p:PublishAot=true", "-p:GodotTargetPlatform=ios");
        Assert.Equal(
            new[] { "GodotSharp", "MyGame" },
            roots.Select(r => r.Identity).OrderBy(x => x, StringComparer.Ordinal).ToArray());
        // 修复后 iOS 的 root 也必须由 Sdk.targets 定义（不再由 iOS.targets 定义）。
        Assert.All(roots, r => Assert.Equal("Sdk", r.DefiningProjectName));
    }

    [Fact]
    public void IosWithoutExplicitPublishAot_RootsThroughIosProps()
    {
        // iOS.props sets PublishAot=true on its own, so naming the target platform
        // must be enough. This covers the real trigger chain the SDK change relies
        // on, which the Sdk.targets-only fixture cannot observe because it never
        // imports iOS.props.
        var roots = GetTrimmerRootsThroughSdkProps("-p:GodotTargetPlatform=ios");
        Assert.Equal(
            new[] { "GodotSharp", "MyGame" },
            roots.Select(r => r.Identity).OrderBy(x => x, StringComparer.Ordinal).ToArray());
        Assert.All(roots, r => Assert.Equal("Sdk", r.DefiningProjectName));
    }
}

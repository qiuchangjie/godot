using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using Xunit;

namespace Godot.NET.Sdk.Tests;

public class SdkRootingTests
{
    private static readonly string SdkSourceDir = Path.Combine(
        TestPaths.RepoRoot,
        "modules", "mono", "editor", "Godot.NET.Sdk", "Godot.NET.Sdk", "Sdk");

    private static readonly string FixtureSourcePath = Path.Combine(
        TestPaths.RepoRoot,
        "modules", "mono", "editor", "Godot.NET.Sdk", "Godot.NET.Sdk.Tests", "Fixtures", "SdkRootsFixture.csproj");

    private sealed record RootItem(string Identity, string DefiningProjectName);

    private static List<RootItem> GetTrimmerRoots(params string[] extraProperties)
    {
        string workDir = Path.Combine(
            Path.GetTempPath(), "godot-sdk-rooting-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(workDir);
        try
        {
            string fixturePath = Path.Combine(workDir, "SdkRootsFixture.csproj");
            File.Copy(FixtureSourcePath, fixturePath);

            var arguments = new List<string>
            {
                "msbuild", fixturePath,
                "-getItem:TrimmerRootAssembly",
                "-nologo",
                $"-p:GodotSdkSourceDir={SdkSourceDir}",
            };
            arguments.AddRange(extraProperties);

            var startInfo = new ProcessStartInfo("dotnet")
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
            };
            foreach (string argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            using var process = Process.Start(startInfo)!;
            string stdout = process.StandardOutput.ReadToEnd();
            string stderr = process.StandardError.ReadToEnd();
            process.WaitForExit();

            Assert.True(process.ExitCode == 0,
                $"dotnet msbuild failed ({process.ExitCode}).\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}");

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
}

using System;
using System.IO;

namespace Godot.NET.Sdk.Tests;

internal static class TestPaths
{
    public static string RepoRoot { get; } = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            string sentinel = Path.Combine(
                directory.FullName,
                "modules", "mono", "editor", "Godot.NET.Sdk", "Godot.NET.Sdk", "Sdk", "Sdk.targets");
            if (File.Exists(sentinel))
            {
                return directory.FullName;
            }
            directory = directory.Parent;
        }
        throw new InvalidOperationException(
            $"Unable to locate the Godot repository root from '{AppContext.BaseDirectory}'.");
    }
}

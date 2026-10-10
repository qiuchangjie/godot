using System;
using System.Diagnostics;
using Xunit;

namespace Godot.NET.Sdk.Tests;

public class ProcessRunnerTests
{
    [Fact]
    public void Run_KillsChildThatExceedsTimeout()
    {
        (string fileName, string[] arguments) = SleepCommand(seconds: 30);
        var stopwatch = Stopwatch.StartNew();

        Assert.Throws<TimeoutException>(
            () => ProcessRunner.Run(fileName, arguments, timeoutMilliseconds: 1_000));

        stopwatch.Stop();
        Assert.True(stopwatch.ElapsedMilliseconds < 20_000,
            $"Timeout handling returned after {stopwatch.ElapsedMilliseconds} ms, expected it to kill the child promptly.");
    }

    [Fact]
    public void Run_DrainsBothPipesConcurrently()
    {
        // Each stream gets far more than a pipe buffer (~4 KB). A sequential reader
        // deadlocks here; a concurrent reader completes and captures both markers.
        (string fileName, string[] arguments) = LargeDualOutputCommand();

        (int exitCode, string stdout, string stderr) =
            ProcessRunner.Run(fileName, arguments, timeoutMilliseconds: 60_000);

        Assert.Equal(0, exitCode);
        Assert.Contains("OUTPUT_MARKER", stdout);
        Assert.Contains("ERROR_MARKER", stderr);
    }

    private static (string FileName, string[] Arguments) SleepCommand(int seconds)
    {
        if (OperatingSystem.IsWindows())
        {
            // ping waits roughly N-1 seconds on Windows.
            return ("cmd", new[] { "/c", $"ping -n {seconds + 1} 127.0.0.1 >NUL" });
        }
        return ("sleep", new[] { seconds.ToString() });
    }

    private static (string FileName, string[] Arguments) LargeDualOutputCommand()
    {
        if (OperatingSystem.IsWindows())
        {
            return ("cmd", new[]
            {
                "/c",
                "(for /L %i in (1,1,5000) do @echo OUTPUT_MARKER%i) & (for /L %i in (1,1,5000) do @echo ERROR_MARKER%i 1>&2)",
            });
        }
        return ("sh", new[]
        {
            "-c",
            "i=0; while [ $i -lt 5000 ]; do echo OUTPUT_MARKER$i; echo ERROR_MARKER$i 1>&2; i=$((i+1)); done",
        });
    }
}

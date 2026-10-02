using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading.Tasks;

namespace Godot.NET.Sdk.Tests;

internal static class ProcessRunner
{
    public static (int ExitCode, string StandardOutput, string StandardError) Run(
        string fileName, IReadOnlyList<string> arguments, int timeoutMilliseconds)
    {
        var startInfo = new ProcessStartInfo(fileName)
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

        // Read both pipes concurrently. Reading them sequentially deadlocks when the
        // child fills one buffer while we are still blocked on the other, and stderr
        // is exactly what the caller wants in its failure message.
        Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
        Task<string> stderrTask = process.StandardError.ReadToEndAsync();

        if (!process.WaitForExit(timeoutMilliseconds))
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch (InvalidOperationException)
            {
                // The child exited between the wait timing out and the kill.
            }

            throw new TimeoutException(
                $"Process '{fileName}' did not exit within {timeoutMilliseconds} ms.");
        }

        string stdout = stdoutTask.GetAwaiter().GetResult();
        string stderr = stderrTask.GetAwaiter().GetResult();

        return (process.ExitCode, stdout, stderr);
    }
}

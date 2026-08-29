using System.Threading;
using AbCatDC.Kernel;

namespace AbCatDC.RazorLib.Services;

/// <summary>
/// Runs quiver-diagram canonicalization on its own dedicated background
/// thread, with progress reporting and cooperative cancellation — the .NET
/// analogue of a QThread subclass (System.Threading.Thread is sealed, so the
/// thread is wrapped rather than inherited).
/// </summary>
public sealed class CanonicalFormThread
{
    private readonly Thread thread;
    private readonly CancellationTokenSource cts = new();

    public CanonicalFormThread(
        string quiverJson,
        string title,
        Action<int, int> onProgress,
        Action<QuiverImport.CanonicalOutcome> onCompleted)
    {
        thread = new Thread(() =>
        {
            var outcome = QuiverImport.run(quiverJson, title, onProgress, cts.Token);
            onCompleted(outcome);
        })
        {
            IsBackground = true,
            Name = "CanonicalFormThread",
        };
    }

    public bool IsAlive => thread.IsAlive;

    public void Start() => thread.Start();

    /// <summary>Request cooperative cancellation; the worker exits at the next check.</summary>
    public void Cancel() => cts.Cancel();
}

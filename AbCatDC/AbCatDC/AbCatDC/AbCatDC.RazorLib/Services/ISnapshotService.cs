namespace AbCatDC.RazorLib.Services;

/// <summary>Captures the host web view as a PNG (base64), for diagram
/// snapshots. Provided by the MAUI app on platforms that can; absent
/// elsewhere, in which case the snapshot button is not offered.</summary>
public interface ISnapshotService
{
    Task<string?> CapturePngBase64Async();
}

using System.Text.Json;
using Microsoft.JSInterop;

namespace AbCatDC.RazorLib.Services;

/// <summary>
/// App-wide settings, persisted in the webview's localStorage.
/// </summary>
public class AppSettingsService(IJSRuntime js)
{
    private class Model
    {
        public int DelayMs { get; set; } = 600;
        public bool Dark { get; set; }
    }

    private Task? loadTask;

    /// <summary>Idle time before the completion popup opens (Ctrl+Space is instant).</summary>
    public int CompletionDelayMs { get; set; } = 600;

    public bool DarkMode { get; set; }

    public event Action? Changed;

    // cache the task, not a bool, so concurrent callers await the in-flight load
    public Task EnsureLoadedAsync() => loadTask ??= LoadAsync();

    private async Task LoadAsync()
    {
        try
        {
            var json = await js.InvokeAsync<string?>("abEditor.loadSettings");
            if (!string.IsNullOrEmpty(json))
            {
                var m = JsonSerializer.Deserialize<Model>(json);
                if (m != null)
                {
                    CompletionDelayMs = Math.Clamp(m.DelayMs, 100, 5000);
                    DarkMode = m.Dark;
                }
            }
        }
        catch
        {
            // localStorage unavailable — keep defaults
        }
        await ApplyThemeAsync();
    }

    public async Task SaveAsync()
    {
        try
        {
            var json = JsonSerializer.Serialize(new Model { DelayMs = CompletionDelayMs, Dark = DarkMode });
            await js.InvokeVoidAsync("abEditor.saveSettings", json);
        }
        catch
        {
            // best effort
        }
        await ApplyThemeAsync();
        Changed?.Invoke();
    }

    public async Task ApplyThemeAsync()
    {
        try { await js.InvokeVoidAsync("abEditor.theme", DarkMode); } catch { }
    }
}

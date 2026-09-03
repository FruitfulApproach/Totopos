using System.Text.Json;
using Microsoft.JSInterop;

namespace AbCatDC.RazorLib.Shared;

// The persisted shape of a formal system — shared by the Rules page and the
// prover. Serialized with DEFAULT System.Text.Json options (PascalCase,
// case-sensitive): existing stored systems depend on it.

public class RuleDto
{
    public string Name { get; set; } = "";
    public string Kind { get; set; } = "Introduction";
    public string LongName { get; set; } = "";
    public string Description { get; set; } = "";
    public List<string> Premises { get; set; } = new();
    public List<string> Conclusions { get; set; } = new();
}

public class SketchDto
{
    public string Name { get; set; } = "";
    public string Json { get; set; } = "";
}

public class SystemDto
{
    public string Name { get; set; } = "";
    /// <summary>Rigid names of this system (Ring, Mod, …): parsed as constant
    /// atoms, never metavariables, and function-like ("Mod R").</summary>
    public List<string> Constants { get; set; } = new();
    public List<RuleDto> Rules { get; set; } = new();
    public List<SketchDto> Sketches { get; set; } = new();
}

public static class SystemStore
{
    public const string Key = "abcatdc-rules";

    public static readonly string[] Kinds =
        { "Introduction", "Elimination", "Construction", "Computational", "Structural", "Theorem" };

    public static async Task<SystemDto> LoadAsync(IJSRuntime js)
    {
        try
        {
            var json = await js.InvokeAsync<string?>("abEditor.loadItem", Key);
            if (!string.IsNullOrEmpty(json))
            {
                var dto = JsonSerializer.Deserialize<SystemDto>(json);
                if (dto != null)
                {
                    dto.Rules ??= new();
                    dto.Sketches ??= new();
                    dto.Constants ??= new();
                    return dto;
                }
            }
        }
        catch
        {
            // corrupted or unavailable store — start fresh
        }
        return new SystemDto();
    }

    public static async Task SaveAsync(IJSRuntime js, SystemDto dto)
    {
        try { await js.InvokeVoidAsync("abEditor.saveItem", Key, JsonSerializer.Serialize(dto)); }
        catch { }
    }
}

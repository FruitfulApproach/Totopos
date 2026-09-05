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
    /// <summary>A rule recorded in the proof assistant: it also appears as a button
    /// under the diagram whenever it applies.</summary>
    public bool IsButton { get; set; }
    public string Description { get; set; } = "";
    public List<string> Premises { get; set; } = new();
    public List<string> Conclusions { get; set; } = new();
}

public class SketchDto
{
    public string Name { get; set; } = "";
    public string Json { get; set; } = "";
    /// <summary>A PNG data URL taken in the diagram editor; previews show it
    /// instead of loading a live quiver frame.</summary>
    public string? Snapshot { get; set; }
}

public class SystemDto
{
    public string Name { get; set; } = "";
    /// <summary>Which meaning "[D] commutes" carries. 0/1 (absent in stored
    /// systems) = the opaque, name-matched token; 2 = the built-in ∀∃ reading
    /// (diagrams match by shape, contribute their typings and equations, and
    /// dashed arrows are existential witnesses). New systems start at 2.</summary>
    public int SemanticsVersion { get; set; } = 0;
    public bool DiagramSemantics => SemanticsVersion >= 2;
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
        // a brand-new system gets the built-in diagram semantics
        return new SystemDto { SemanticsVersion = 2 };
    }

    public static async Task SaveAsync(IJSRuntime js, SystemDto dto)
    {
        try { await js.InvokeVoidAsync("abEditor.saveItem", Key, JsonSerializer.Serialize(dto)); }
        catch { }
    }
}

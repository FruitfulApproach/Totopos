namespace AbCatDC.RazorLib.Shared;

/// <summary>
/// The shared shape of a prebuilt inference rule, used by the example
/// palettes. (The old 700-rule searchable library was removed.)
/// </summary>
public static class RulePalette
{
    /// <summary>Name is the short (often formula-style) label, e.g. "A × (B ∪ C) = ?";
    /// LongName is the descriptive phrase, e.g. "× distributes over set ∪ (union) op".</summary>
    public record Entry(string Kind, string Name, string[] Premises, string[] Conclusions, string Description, string LongName = "");
}

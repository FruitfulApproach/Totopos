using System.Text.Json.Serialization;

namespace AbCatDC.RazorLib.Shared;

// The persisted shape of a proof tree (Proof tree page). Default
// System.Text.Json options (PascalCase), like the other stores.

public class TreeNodeDto
{
    public int Id { get; set; }
    public bool IsRule { get; set; }
    /// <summary>For statements: "given" (established by assumption), "goal"
    /// (to be proven), or "lemma" (an intermediate statement). Rules: "rule".</summary>
    public string Role { get; set; } = "lemma";
    public string RuleName { get; set; } = "";
    public string RuleKind { get; set; } = "";
    public List<string> Premises { get; set; } = new();
    public List<string> Conclusions { get; set; } = new();
    public double X { get; set; }
    public double Y { get; set; }
    [JsonIgnore] public string? Error { get; set; }

    public bool IsGiven => !IsRule && Role == "given";
    public bool IsGoal => !IsRule && Role == "goal";
}

public class TreeWireDto
{
    public int Id { get; set; }
    public int Src { get; set; }
    public int SrcIdx { get; set; }
    public int Dst { get; set; }
    public int DstIdx { get; set; }
}

public class TreeDto
{
    public string Name { get; set; } = "";
    public string SystemName { get; set; } = "";
    public string Description { get; set; } = "";
    public int NextId { get; set; } = 1;
    public List<TreeNodeDto> Nodes { get; set; } = new();
    public List<TreeWireDto> Wires { get; set; } = new();
    /// <summary>Diagrams this tree brings along (an example's situation
    /// picture; instantiated witnesses), registered with the system's.</summary>
    public List<SketchDto> Sketches { get; set; } = new();
}

public class TreesBlob
{
    public string Active { get; set; } = "";
    public List<TreeDto> Trees { get; set; } = new();
}

namespace AbCatDC.RazorLib.Shared;

/// <summary>
/// LaTeX-style editor commands (typed as \command) and the unicode symbol each
/// produces. A command is applied as soon as its typed prefix is unique.
/// </summary>
public static class SymbolCommands
{
    public record Entry(string Command, string Symbol);

    public static readonly Entry[] All =
    {
        new("forall", "∀"),
        new("exists", "∃"),
        new("mu", "μ"),
        new("cap", "∩"),
        new("cup", "∪"),
        new("bigcap", "⋂"),
        new("bigcup", "⋃"),
        new("times", "×"),
        new("to", "→"),
        new("rightarrow", "→"),
        new("langle", "⟨"),
        new("rangle", "⟩"),
        new("alpha", "α"),
        new("beta", "β"),
        new("gamma", "γ"),
        new("delta", "δ"),
        new("rho", "ρ"),
        new("sigma", "σ"),
        new("tau", "τ"),
        new("phi", "φ"),
        new("psi", "ψ"),
        new("omega", "ω"),
        new("lambda", "λ"),
        new("nat", "ℕ"),
        new("int", "ℤ"),
        new("rat", "ℚ"),
        new("real", "ℝ"),
        new("complex", "ℂ"),
        new("vdash", "⊢"),
        new("entails", "⊢"),
        new("Gamma", "Γ"),
        new("Delta", "Δ"),
        new("Theta", "Θ"),
        new("Xi", "Ξ"),
        // single-letter blackboard-bold aliases (case-sensitive): \N, \Z, ...
        new("N", "ℕ"),
        new("Z", "ℤ"),
        new("Q", "ℚ"),
        new("R", "ℝ"),
        new("C", "ℂ"),
    };

    private static readonly Dictionary<char, string> Blackboard = new()
    {
        ['N'] = "ℕ", ['Z'] = "ℤ", ['Q'] = "ℚ", ['R'] = "ℝ", ['C'] = "ℂ",
    };

    // Unicode subscript forms. Capitals have no subscript codepoints, so an
    // uppercase letter falls back to the lowercase subscript (X → ₓ); letters
    // with no subscript form at all (b, c, d, f, g, q, w, y, z) don't expand.
    private static readonly Dictionary<char, char> Subscripts = new()
    {
        ['0'] = '₀', ['1'] = '₁', ['2'] = '₂', ['3'] = '₃', ['4'] = '₄',
        ['5'] = '₅', ['6'] = '₆', ['7'] = '₇', ['8'] = '₈', ['9'] = '₉',
        ['a'] = 'ₐ', ['e'] = 'ₑ', ['h'] = 'ₕ', ['i'] = 'ᵢ', ['j'] = 'ⱼ',
        ['k'] = 'ₖ', ['l'] = 'ₗ', ['m'] = 'ₘ', ['n'] = 'ₙ', ['o'] = 'ₒ',
        ['p'] = 'ₚ', ['r'] = 'ᵣ', ['s'] = 'ₛ', ['t'] = 'ₜ', ['u'] = 'ᵤ',
        ['v'] = 'ᵥ', ['x'] = 'ₓ',
        ['+'] = '₊', ['-'] = '₋', ['='] = '₌',
    };

    private static bool TrySub(char c, out char sub) =>
        Subscripts.TryGetValue(char.ToLowerInvariant(c), out sub);

    /// <summary>
    /// The completed subscript escape ending at <paramref name="cursor"/>:
    /// \_c (single character) or \_{chars} (on typing the closing brace).
    /// Returns the start index of the escape and the subscript replacement.
    /// </summary>
    public static (int Start, string Replacement)? FindSubscript(string text, int cursor)
    {
        if (cursor > text.Length) cursor = text.Length;
        // brace form \_{...} — expand when '}' is typed
        if (cursor >= 5 && text[cursor - 1] == '}')
        {
            var i = cursor - 2;
            var chars = new List<char>();
            while (i >= 0 && text[i] != '{' && text[i] != '}') { chars.Add(text[i]); i--; }
            if (i >= 2 && text[i] == '{' && text[i - 1] == '_' && text[i - 2] == '\\' && chars.Count > 0)
            {
                chars.Reverse();
                var sb = new System.Text.StringBuilder();
                foreach (var ch in chars)
                {
                    if (!TrySub(ch, out var s)) return null;
                    sb.Append(s);
                }
                return (i - 2, sb.ToString());
            }
        }
        // single form \_c
        if (cursor >= 3 && text[cursor - 3] == '\\' && text[cursor - 2] == '_' && text[cursor - 1] != '{'
            && TrySub(text[cursor - 1], out var single))
            return (cursor - 3, single.ToString());
        return null;
    }

    /// <summary>
    /// Expand every complete LaTeX-style command in the text, wherever it
    /// occurs: \Bbb{N} / \mathbb{N} / \Bbb N forms, every exact \command from
    /// the table, and unicode superscripts normalized to ^n. Used to normalize
    /// search queries so "cast-\Bbb{N}^2" matches "cast-ℕ^2→ℤ^2".
    /// </summary>
    public static string ExpandAll(string text)
    {
        if (string.IsNullOrEmpty(text)) return text;
        text = System.Text.RegularExpressions.Regex.Replace(
            text, @"\\(?:Bbb|mathbb)\s*\{?\s*([NZQRC])\s*\}?",
            m => Blackboard[m.Groups[1].Value[0]]);
        text = System.Text.RegularExpressions.Regex.Replace(
            text, @"\\_(?:\{([^}]*)\}|(.))",
            m =>
            {
                var payload = m.Groups[1].Success ? m.Groups[1].Value : m.Groups[2].Value;
                var sb = new System.Text.StringBuilder();
                foreach (var ch in payload)
                {
                    if (!TrySub(ch, out var s)) return m.Value;
                    sb.Append(s);
                }
                return sb.ToString();
            });
        text = System.Text.RegularExpressions.Regex.Replace(
            text, @"\\([A-Za-z]+)",
            m =>
            {
                var cmd = m.Groups[1].Value;
                var hit = All.FirstOrDefault(e => e.Command == cmd);
                return hit is null ? m.Value : hit.Symbol;
            });
        const string sup = "⁰¹²³⁴⁵⁶⁷⁸⁹";
        text = System.Text.RegularExpressions.Regex.Replace(
            text, "[⁰¹²³⁴⁵⁶⁷⁸⁹]+",
            m => "^" + string.Concat(m.Value.Select(c => (char)('0' + sup.IndexOf(c)))));
        return text;
    }

    public static List<Entry> Matches(string prefix) =>
        All.Where(c => c.Command.StartsWith(prefix, StringComparison.Ordinal)).ToList();

    /// <summary>
    /// The \command context ending at <paramref name="cursor"/>: returns the
    /// index of the backslash and the typed prefix after it, or (-1, "").
    /// </summary>
    public static (int Start, string Prefix) FindContext(string text, int cursor)
    {
        if (cursor > text.Length) cursor = text.Length;
        var i = cursor - 1;
        while (i >= 0 && char.IsLetter(text[i])) i--;
        if (i >= 0 && text[i] == '\\')
            return (i, text[(i + 1)..cursor]);
        return (-1, "");
    }
}

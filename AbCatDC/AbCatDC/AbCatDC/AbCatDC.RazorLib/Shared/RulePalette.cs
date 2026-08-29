namespace AbCatDC.RazorLib.Shared;

/// <summary>
/// A library of common inference rules, expressed in the kernel's syntax,
/// ready to load into the rule builder or add to the current formal system.
/// </summary>
public static class RulePalette
{
    /// <summary>Name is the short (often formula-style) label, e.g. "A × (B ∪ C) = ?";
    /// LongName is the descriptive phrase, e.g. "× distributes over set ∪ (union) op".</summary>
    public record Entry(string Kind, string Name, string[] Premises, string[] Conclusions, string Description, string LongName = "");

    /// <summary>The full library: the curated rules plus the generated families.
    /// Lazy so it never races the initialization of <see cref="All"/>.</summary>
    public static IReadOnlyList<Entry> Library => library.Value;

    private static readonly Lazy<IReadOnlyList<Entry>> library = new(() =>
    {
        var l = new List<Entry>(All!);
        l.AddRange(RulePaletteGenerator.Generate());
        return l;
    });

    public static readonly Entry[] All =
    {
        // ---- Introduction ----------------------------------------------------
        new("Introduction", "→-intro", new[] { "τ" }, new[] { "σ → τ" },
            "Implication / function introduction (abstraction): if τ is derivable under the assumption σ, discharge the assumption and conclude σ → τ. Curry–Howard: λ-abstraction."),
        new("Introduction", "×-intro", new[] { "σ", "τ" }, new[] { "σ × τ" },
            "Pairing: proofs of both components combine into a proof of the product. Curry–Howard: the pair (M, N)."),
        new("Introduction", "+-intro₁", new[] { "σ" }, new[] { "σ + τ" },
            "Left injection ι₁: a proof of σ already proves the sum σ + τ."),
        new("Introduction", "+-intro₂", new[] { "τ" }, new[] { "σ + τ" },
            "Right injection ι₂: a proof of τ already proves the sum σ + τ."),
        new("Introduction", "∩-intro", new[] { "σ", "τ" }, new[] { "σ ∩ τ" },
            "Intersection introduction: one and the same term must inhabit both σ and τ; then it inhabits σ ∩ τ."),
        new("Introduction", "∀-intro", new[] { "τ" }, new[] { "∀α. τ" },
            "Generalization: if τ holds with α completely arbitrary (α not free in any open assumption), conclude ∀α. τ."),
        new("Introduction", "∃-intro", new[] { "τ" }, new[] { "∃α. τ" },
            "Witnessing: from τ[α := σ] for some concrete witness type σ, conclude ∃α. τ."),
        new("Introduction", "⟨⟩-intro", new[] { "τ" }, new[] { "⟨x : τ⟩" },
            "Record introduction: package a proven member under the label x."),

        // ---- Elimination -----------------------------------------------------
        new("Elimination", "→-elim", new[] { "σ → τ", "σ" }, new[] { "τ" },
            "Modus ponens / application: a function from σ to τ applied to an argument of σ yields a τ."),
        new("Elimination", "×-elim₁", new[] { "σ × τ" }, new[] { "σ" },
            "First projection π₁: from a pair extract its first component."),
        new("Elimination", "×-elim₂", new[] { "σ × τ" }, new[] { "τ" },
            "Second projection π₂: from a pair extract its second component."),
        new("Elimination", "+-elim", new[] { "σ + τ", "σ → ρ", "τ → ρ" }, new[] { "ρ" },
            "Case analysis: to use a sum, handle both injections and land in a common result ρ."),
        new("Elimination", "∩-elim", new[] { "σ ∩ τ" }, new[] { "σ" },
            "Intersection elimination: an inhabitant of σ ∩ τ inhabits σ (and symmetrically τ)."),
        new("Elimination", "∀-elim", new[] { "∀α. τ" }, new[] { "τ" },
            "Instantiation: specialize the universally quantified α to any type σ, yielding τ[α := σ]."),
        new("Elimination", "∃-elim", new[] { "∃α. τ", "∀α. τ → ρ" }, new[] { "ρ" },
            "Unpacking: use the hidden witness uniformly (α must not occur free in ρ)."),
        new("Elimination", "⟨⟩-elim", new[] { "⟨x : τ⟩" }, new[] { "τ" },
            "Member access M.x: a record with member x : τ yields that member."),
        new("Elimination", "⋂-elim", new[] { "⋂(x:σ) τ", "σ" }, new[] { "τ" },
            "Familial instantiation: a member of the familial intersection specializes at any term N : σ, giving τ[x := N]."),

        // ---- Construction ----------------------------------------------------
        new("Construction", "0-construct", System.Array.Empty<string>(), new[] { "0" },
            "The natural number zero — a ground constant of ℕ, available with no premises."),
        new("Construction", "succ-construct", new[] { "n" }, new[] { "n + 1" },
            "Successor: from an integer n construct n + 1 (read computationally in ℤ)."),
        new("Construction", "×-construct", new[] { "A", "B" }, new[] { "A × B" },
            "Cartesian product of two sets of integers: {(a, b) : a ∈ A, b ∈ B} ⊆ ℤ²."),
        new("Construction", "²-construct", new[] { "A" }, new[] { "A^2" },
            "Diagonal power: the cartesian square A² = {(a, b) : a, b ∈ A} ⊆ ℤ²."),
        new("Construction", "juxt-construct", new[] { "A", "B" }, new[] { "AB" },
            "Juxtaposition: the elementwise product {ab : a ∈ A, b ∈ B} ⊆ ℤ."),
        new("Construction", "∪-construct", new[] { "A", "B" }, new[] { "A ∪ B" },
            "Union: combine two sets of integers into one."),

        // ---- Computational ---------------------------------------------------
        new("Computational", "β-compute", new[] { "(x : σ) → τ", "σ" }, new[] { "τ" },
            "β-reduction: applying a (dependent) abstraction substitutes the argument into the body — the result has type τ[x := N]."),
        new("Computational", "π-compute", new[] { "σ × τ" }, new[] { "σ" },
            "Projection reduction: π₁(M, N) ↝ M — projecting a constructed pair computes to the component itself."),
        new("Computational", "unfold-compute", new[] { "μα. τ" }, new[] { "τ" },
            "Unfolding: one step of the recursive type, μα. τ ↝ τ[α := μα. τ]."),
        new("Computational", "pow-compute", new[] { "A^2" }, new[] { "A × A" },
            "Powers compute to iterated products — the definitional equality A² ≡ A × A the canonicalizer uses."),
        new("Computational", "ℤ-compute", new[] { "1 + 2" }, new[] { "3" },
            "Numerals compute inside ℤ: closed arithmetic on literals reduces to a literal."),

        // ---- Structural (sequent bookkeeping) --------------------------------
        new("Structural", "axiom", System.Array.Empty<string>(), new[] { "Γ, x : σ ⊢ x : σ" },
            "Identity / variable rule: a hypothesis proves itself — the leaves of every derivation."),
        new("Structural", "cut", new[] { "Γ ⊢ M : σ", "Δ, x : σ ⊢ N : ρ" }, new[] { "Γ, Δ ⊢ N : ρ" },
            "Cut: a proved lemma may be used as a hypothesis; eliminating cuts is substitution N[x := M]."),
        new("Structural", "weakening", new[] { "Γ ⊢ M : ρ" }, new[] { "Γ, σ ⊢ M : ρ" },
            "Weakening: an unused extra hypothesis is harmless. (Rejected by linear logic.)"),
        new("Structural", "contraction", new[] { "Γ, x : σ, y : σ ⊢ M : ρ" }, new[] { "Γ, x : σ ⊢ M : ρ" },
            "Contraction: two copies of a hypothesis collapse into one. (Rejected by linear logic.)"),
        new("Structural", "exchange", new[] { "Γ, x : σ, y : τ ⊢ M : ρ" }, new[] { "Γ, y : τ, x : σ ⊢ M : ρ" },
            "Exchange: independent hypotheses commute (valid when neither type mentions the other's variable)."),
    };
}

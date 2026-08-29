namespace AbCatDC.RazorLib.Shared;

/// <summary>
/// Ready-made example palettes: small curated rule systems the user can load
/// into the rule editor as a starting point. Schematic where the kernel's
/// syntax lacks a native notion (noted in the descriptions).
/// </summary>
public static class ExampleSystems
{
    public record ExampleSystem(string Name, string Description, RulePalette.Entry[] Rules);

    static RulePalette.Entry R(string kind, string name, string[] prem, string[] concl, string desc) =>
        new(kind, name, prem, concl, desc);

    static readonly string[] None = System.Array.Empty<string>();

    public static readonly ExampleSystem[] All =
    {
        new("Simply typed λ-calculus",
            "The minimal functional core in sequent style, with typing judgments t : T throughout.",
            new[]
            {
                R("Structural", "var", None, new[] { "Γ, x : σ ⊢ x : σ" },
                    "Variable rule: a hypothesis proves itself."),
                R("Introduction", "→-intro", new[] { "Γ, x : σ ⊢ M : τ" }, new[] { "Γ ⊢ λxM : σ → τ" },
                    "Abstraction: discharge the hypothesis x : σ; λxM stands for λx. M."),
                R("Elimination", "→-elim", new[] { "Γ ⊢ f : σ → τ", "Γ ⊢ a : σ" }, new[] { "Γ ⊢ fa : τ" },
                    "Application: fa stands for f a."),
                R("Computational", "β-compute", new[] { "Γ, x : σ ⊢ M : τ", "Γ ⊢ a : σ" }, new[] { "Γ ⊢ Mxa : τ" },
                    "β-reduction: (λx. M) a ↝ M[x := a]; Mxa stands for the substituted body."),
                R("Introduction", "×-intro", new[] { "Γ ⊢ M : σ", "Γ ⊢ N : τ" }, new[] { "Γ ⊢ MN : σ × τ" },
                    "Pairing: MN stands for the pair (M, N)."),
                R("Elimination", "×-proj1", new[] { "Γ ⊢ p : σ × τ" }, new[] { "Γ ⊢ p1 : σ" },
                    "First projection: p1 stands for π₁ p."),
                R("Elimination", "×-proj2", new[] { "Γ ⊢ p : σ × τ" }, new[] { "Γ ⊢ p2 : τ" },
                    "Second projection: p2 stands for π₂ p."),
            }),

        new("System F",
            "Girard/Reynolds polymorphism in sequent style with typing judgments.",
            new[]
            {
                R("Structural", "var", None, new[] { "Γ, x : σ ⊢ x : σ" }, "Variable rule."),
                R("Introduction", "→-intro", new[] { "Γ, x : σ ⊢ M : τ" }, new[] { "Γ ⊢ λxM : σ → τ" },
                    "Term abstraction, discharging x : σ."),
                R("Elimination", "→-elim", new[] { "Γ ⊢ f : σ → τ", "Γ ⊢ a : σ" }, new[] { "Γ ⊢ fa : τ" },
                    "Term application."),
                R("Introduction", "∀-intro", new[] { "Γ ⊢ M : τ" }, new[] { "Γ ⊢ M : ∀α. τ" },
                    "Type abstraction Λα. M — side condition: α not free in Γ."),
                R("Elimination", "∀-elim", new[] { "Γ ⊢ M : ∀α. τ" }, new[] { "Γ ⊢ M : τ" },
                    "Type application: the conclusion is at τ[α := σ] for any chosen σ."),
                R("Computational", "Λβ-compute", new[] { "Γ ⊢ M : ∀α. τ" }, new[] { "Γ ⊢ M : τ" },
                    "Type-level β: (Λα. M)[σ] ↝ M[α := σ]."),
                R("Introduction", "church-two", None, new[] { "⊢ c2 : ∀α. (α → α) → α → α" },
                    "A closed inhabitant: the Church numeral 2 = λf. λx. f (f x), typed in the empty context."),
            }),

        new("Martin-Löf type theory",
            "Dependent Π and Σ over ℕ with the induction principle.",
            new[]
            {
                R("Structural", "var", None, new[] { "Γ, x : σ ⊢ x : σ" }, "Variable rule."),
                R("Introduction", "Π-intro", new[] { "Γ, x : σ ⊢ M : τ" }, new[] { "Γ ⊢ λxM : (x : σ) → τ" },
                    "Dependent abstraction over the hypothesis x : σ."),
                R("Elimination", "Π-elim", new[] { "Γ ⊢ f : (x : σ) → τ", "Γ ⊢ a : σ" }, new[] { "Γ ⊢ fa : τ" },
                    "Dependent application: the result type is τ[x := a]."),
                R("Introduction", "Σ-intro", new[] { "Γ ⊢ a : σ", "Γ ⊢ b : τ" }, new[] { "Γ ⊢ ab : (x : σ) × τ" },
                    "Dependent pairing: witness a with proof b of τ[x := a]."),
                R("Elimination", "Σ-proj1", new[] { "Γ ⊢ p : (x : σ) × τ" }, new[] { "Γ ⊢ p1 : σ" },
                    "First projection: the witness."),
                R("Elimination", "Σ-proj2", new[] { "Γ ⊢ p : (x : σ) × τ" }, new[] { "Γ ⊢ p2 : τ" },
                    "Second projection, at type τ[x := π₁ p]."),
                R("Construction", "ℕ-zero", None, new[] { "⊢ 0 : ℕ" }, "Zero, in the empty context."),
                R("Construction", "ℕ-succ", new[] { "Γ ⊢ n : ℕ" }, new[] { "Γ ⊢ n + 1 : ℕ" }, "Successor."),
                R("Elimination", "ℕ-ind",
                    new[] { "Γ ⊢ z : ρ", "Γ ⊢ s : (n : ℕ) → ρ → ρ", "Γ ⊢ m : ℕ" }, new[] { "Γ ⊢ r : ρ" },
                    "Induction / recursion: r stands for ind(z, s, m) — base case z, step s, applied to m."),
            }),

        new("Cubical type theory",
            "Schematic cubical primitives: the interval I, paths as functions out of it, transport and composition.",
            new[]
            {
                R("Construction", "I-0", None, new[] { "0" },
                    "The left endpoint of the interval I (rendered as the numeral 0)."),
                R("Construction", "I-1", None, new[] { "1" },
                    "The right endpoint of the interval I."),
                R("Introduction", "Path-intro", new[] { "τ" }, new[] { "(i : I) → τ" },
                    "A path is a function out of the interval: λi. M, with M possibly mentioning i."),
                R("Elimination", "Path-app", new[] { "(i : I) → τ", "I" }, new[] { "τ" },
                    "Evaluate a path at an interval point; the endpoints give the two sides of the equality."),
                R("Construction", "refl", new[] { "σ" }, new[] { "(i : I) → σ" },
                    "The constant path at a point — reflexivity."),
                R("Computational", "transp", new[] { "(i : I) → τ" }, new[] { "τ → τ" },
                    "Transport along a line of types: coerce the fiber at 0 into the fiber at 1."),
                R("Computational", "hcomp", new[] { "σ", "(i : I) → σ" }, new[] { "σ" },
                    "Homogeneous composition: cap an open box in σ."),
                R("Computational", "funext", new[] { "(x : σ) → (i : I) → τ" }, new[] { "(i : I) → (x : σ) → τ" },
                    "Function extensionality: a family of paths swaps into a path of functions."),
                R("Computational", "conn-meet", new[] { "I", "I" }, new[] { "I" },
                    "Connection i ∧ j: the meet operation on the interval."),
                R("Computational", "conn-join", new[] { "I", "I" }, new[] { "I" },
                    "Connection i ∨ j: the join operation on the interval."),
                R("Computational", "ua", new[] { "σ → τ", "τ → σ" }, new[] { "(i : I) → σ ∪ τ" },
                    "Univalence, schematically: an equivalence between σ and τ yields a line of types between them."),
            }),

        new("Peano arithmetic",
            "ℤ-flavored arithmetic: zero, successor, the defining equations of addition, and induction.",
            new[]
            {
                R("Construction", "zero", None, new[] { "0" }, "The numeral 0 : ℕ."),
                R("Construction", "succ", new[] { "n" }, new[] { "n + 1" },
                    "Successor: n ↦ n + 1, computing in ℤ."),
                R("Computational", "add-zero", new[] { "n + 0" }, new[] { "n" },
                    "Defining equation: n + 0 = n."),
                R("Computational", "add-succ", new[] { "n + (m + 1)" }, new[] { "(n + m) + 1" },
                    "Defining equation: n + s(m) = s(n + m)."),
                R("Computational", "ℤ-add-2+2", new[] { "2 + 2" }, new[] { "4" },
                    "A closed instance: 2 + 2 computes to 4 in ℤ."),
                R("Elimination", "ℕ-ind", new[] { "ρ", "(n : ℕ) → ρ → ρ", "ℕ" }, new[] { "ρ" },
                    "Induction: ρ(0) and ∀n. ρ(n) → ρ(n+1) give ρ for every natural number."),
            }),

        new("Intersection types",
            "The intersection type discipline — the system whose typable terms are exactly the strongly normalizing ones.",
            new[]
            {
                R("Introduction", "∩-intro", new[] { "σ", "τ" }, new[] { "σ ∩ τ" },
                    "One and the same term must inhabit both types."),
                R("Elimination", "∩-elim1", new[] { "σ ∩ τ" }, new[] { "σ" }, "Left component."),
                R("Elimination", "∩-elim2", new[] { "σ ∩ τ" }, new[] { "τ" }, "Right component."),
                R("Computational", "∩-idem", new[] { "σ ∩ σ" }, new[] { "σ" }, "Idempotence."),
                R("Computational", "∩-comm", new[] { "σ ∩ τ" }, new[] { "τ ∩ σ" }, "Commutativity."),
                R("Elimination", "∩-mono", new[] { "A → B" }, new[] { "(A ∩ X) → (B ∩ X)" },
                    "Monotonicity of intersection in a subtyping reading."),
                R("Elimination", "→-elim", new[] { "σ → τ", "σ" }, new[] { "τ" }, "Application."),
            }),

        new("Linear logic (fragment)",
            "Resource-sensitive sequent calculus: × read as ⊗, + as ⊕, ∩ as & — multiplicative rules SPLIT the context (Γ, Δ), additive rules SHARE it; no weakening or contraction.",
            new[]
            {
                R("Structural", "axiom", None, new[] { "x : σ ⊢ x : σ" },
                    "Identity: exactly one hypothesis, used exactly once — the whole context is the single resource."),
                R("Introduction", "⊸-intro", new[] { "Γ, x : σ ⊢ M : τ" }, new[] { "Γ ⊢ λxM : σ → τ" },
                    "Linear abstraction: the body must use x exactly once."),
                R("Elimination", "⊸-elim", new[] { "Γ ⊢ f : σ → τ", "Δ ⊢ a : σ" }, new[] { "Γ, Δ ⊢ fa : τ" },
                    "Linear application — multiplicative: the function and argument consume disjoint resources Γ and Δ."),
                R("Introduction", "⊗-intro", new[] { "Γ ⊢ M : σ", "Δ ⊢ N : τ" }, new[] { "Γ, Δ ⊢ MN : σ × τ" },
                    "Tensor — multiplicative: the pair's components draw on disjoint contexts, both consumed."),
                R("Elimination", "⊗-elim", new[] { "Γ ⊢ p : σ × τ", "Δ, x : σ, y : τ ⊢ N : ρ" }, new[] { "Γ, Δ ⊢ M : ρ" },
                    "Tensor elimination: let (x, y) = p in N — both components must be used."),
                R("Introduction", "&-intro", new[] { "Γ ⊢ M : σ", "Γ ⊢ N : τ" }, new[] { "Γ ⊢ MN : σ ∩ τ" },
                    "With — additive: BOTH alternatives are offered from the SAME resources Γ; the consumer will pick one."),
                R("Elimination", "&-elim1", new[] { "Γ ⊢ p : σ ∩ τ" }, new[] { "Γ ⊢ p1 : σ" },
                    "Choose the left alternative."),
                R("Elimination", "&-elim2", new[] { "Γ ⊢ p : σ ∩ τ" }, new[] { "Γ ⊢ p2 : τ" },
                    "Choose the right alternative."),
                R("Introduction", "⊕-intro1", new[] { "Γ ⊢ M : σ" }, new[] { "Γ ⊢ M : σ + τ" },
                    "Left injection into the plus."),
                R("Introduction", "⊕-intro2", new[] { "Γ ⊢ M : τ" }, new[] { "Γ ⊢ M : σ + τ" },
                    "Right injection into the plus."),
                R("Elimination", "⊕-elim",
                    new[] { "Γ ⊢ M : σ + τ", "Δ, x : σ ⊢ N1 : ρ", "Δ, y : τ ⊢ N2 : ρ" }, new[] { "Γ, Δ ⊢ N : ρ" },
                    "Case analysis: whichever branch fires consumes Δ plus its injected component exactly once."),
            }),
    };
}

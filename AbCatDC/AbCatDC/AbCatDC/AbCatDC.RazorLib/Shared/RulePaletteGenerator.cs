namespace AbCatDC.RazorLib.Shared;

/// <summary>
/// Systematically generates the large rule library: families of inference
/// rules parameterized by connective, arity, index, and numeric value. Every
/// expression is valid kernel syntax.
/// </summary>
public static class RulePaletteGenerator
{
    const string Intro = "Introduction";
    const string Elim = "Elimination";
    const string Constr = "Construction";
    const string Comp = "Computational";

    static string Chain(string op, IEnumerable<string> xs) => string.Join($" {op} ", xs);
    static string[] Vars(string stem, int n) => Enumerable.Range(1, n).Select(i => $"{stem}{i}").ToArray();
    static string Sup(int n) => string.Concat(n.ToString().Select(c => "⁰¹²³⁴⁵⁶⁷⁸⁹"[c - '0']));

    public static List<RulePalette.Entry> Generate()
    {
        var L = new List<RulePalette.Entry>();
        void Add(string kind, string name, string[] prem, string[] concl, string desc, string longName = "") =>
            L.Add(new RulePalette.Entry(kind, name, prem, concl, desc, longName));

        // ---- numeric coercion tower ℕ ⊆ ℤ ⊆ ℚ ⊆ ℝ ⊆ ℂ ------------------
        string[] tower = { "ℕ", "ℤ", "ℚ", "ℝ", "ℂ" };
        for (var i = 0; i < tower.Length; i++)
            for (var j = i + 1; j < tower.Length; j++)
            {
                Add(Comp, $"{tower[i]}→{tower[j]}", new[] { tower[i] }, new[] { tower[j] },
                    $"Numeric coercion (cast) along the tower ℕ ⊆ ℤ ⊆ ℚ ⊆ ℝ ⊆ ℂ: every element of {tower[i]} is an element of {tower[j]}.",
                    $"cast {tower[i]} into {tower[j]}");
                for (var n = 2; n <= 8; n++)
                    Add(Comp, $"{tower[i]}{Sup(n)}→{tower[j]}{Sup(n)}", new[] { $"{tower[i]}^{n}" }, new[] { $"{tower[j]}^{n}" },
                        $"Componentwise coercion (cast) of {n}-tuples: {tower[i]}^{n} embeds into {tower[j]}^{n} because {tower[i]} ⊆ {tower[j]}.",
                        $"cast {tower[i]}{Sup(n)} into {tower[j]}{Sup(n)} componentwise");
            }

        // ---- n-ary product / sum / intersection / union families ----------
        for (var n = 2; n <= 13; n++)
        {
            var vs = Vars("σ", n);
            var prod = Chain("×", vs);
            var sum = Chain("+", vs);
            var inter = Chain("∩", vs);
            var union = Chain("∪", vs);

            Add(Intro, $"×-intro{n}", vs, new[] { prod },
                $"{n}-ary pairing: proofs of all {n} components combine into the {n}-tuple of type {prod}.");
            Add(Intro, $"∩-intro{n}", vs, new[] { inter },
                $"{n}-ary intersection introduction: one and the same term must inhabit each of the {n} types.");
            Add(Elim, $"+-case{n}", new[] { sum }.Concat(vs.Select(v => $"{v} → ρ")).ToArray(), new[] { "ρ" },
                $"{n}-way case analysis: to use the sum, handle each of its {n} injections and land in a common ρ.");
            Add(Elim, $"∪-case{n}", new[] { union }.Concat(vs.Select(v => $"{v} → ρ")).ToArray(), new[] { "ρ" },
                $"{n}-way union elimination: a member of the union is a member of one branch — handle all {n}.");

            for (var i = 1; i <= n; i++)
            {
                Add(Elim, $"×-proj-{i}of{n}", new[] { prod }, new[] { $"σ{i}" },
                    $"Projection π{i}: extract component {i} from the {n}-ary product {prod}.");
                Add(Intro, $"+-inj-{i}of{n}", new[] { $"σ{i}" }, new[] { sum },
                    $"Injection ι{i}: a proof of σ{i} proves the {n}-ary sum {sum}.");
                Add(Elim, $"∩-proj-{i}of{n}", new[] { inter }, new[] { $"σ{i}" },
                    $"Intersection elimination at position {i}: an inhabitant of {inter} inhabits σ{i}.");
                Add(Intro, $"∪-inj-{i}of{n}", new[] { $"σ{i}" }, new[] { union },
                    $"Union introduction at position {i}: a member of σ{i} is a member of {union}.");
            }
        }

        // ---- distribution / factoring laws ---------------------------------
        (string Op, string Word)[] conns = { ("×", "product"), ("+", "sum"), ("∩", "intersection"), ("∪", "union") };
        foreach (var (c, cw) in conns)
            foreach (var (d, dw) in conns)
            {
                if (c == d) continue;
                Add(Comp, $"A {c} (B {d} C) = ?",
                    new[] { $"A {c} (B {d} C)" }, new[] { $"(A {c} B) {d} (A {c} C)" },
                    $"Left distribution: A {c} (B {d} C) rewrites to (A {c} B) {d} (A {c} C).",
                    $"{c} distributes over {d} ({dw}) op — left");
                Add(Comp, $"(A {c} B) {d} (A {c} C) = ?",
                    new[] { $"(A {c} B) {d} (A {c} C)" }, new[] { $"A {c} (B {d} C)" },
                    $"Left factoring, the inverse rewrite: pull the common factor A out of a {dw} of {cw}s.",
                    $"{d} ({dw}) of {c}s factors out A — left");
                Add(Comp, $"(B {d} C) {c} A = ?",
                    new[] { $"(B {d} C) {c} A" }, new[] { $"(B {c} A) {d} (C {c} A)" },
                    $"Right distribution: (B {d} C) {c} A rewrites to (B {c} A) {d} (C {c} A).",
                    $"{c} distributes over {d} ({dw}) op — right");
                Add(Comp, $"(B {c} A) {d} (C {c} A) = ?",
                    new[] { $"(B {c} A) {d} (C {c} A)" }, new[] { $"(B {d} C) {c} A" },
                    $"Right factoring, the inverse rewrite: pull the common factor A out on the right.",
                    $"{d} ({dw}) of {c}s factors out A — right");
            }
        foreach (var (d, dw) in conns)
        {
            Add(Comp, $"σ → (τ {d} ρ) = ?", new[] { $"σ → (τ {d} ρ)" }, new[] { $"(σ → τ) {d} (σ → ρ)" },
                $"Distribute the function arrow over the {dw} in its codomain.",
                $"→ distributes over {d} ({dw}) in the codomain");
            Add(Comp, $"(σ → τ) {d} (σ → ρ) = ?", new[] { $"(σ → τ) {d} (σ → ρ)" }, new[] { $"σ → (τ {d} ρ)" },
                $"Factor a {dw} of functions sharing a domain into one function into the {dw}.",
                $"{d} ({dw}) of arrows factors through a shared domain");
        }

        // ---- commutativity / associativity / idempotence / absorption ------
        foreach (var (c, cw) in conns)
        {
            Add(Comp, $"σ {c} τ = τ {c} σ", new[] { $"σ {c} τ" }, new[] { $"τ {c} σ" },
                c == "×"
                    ? "Swap isomorphism: the product is commutative up to the isomorphism (a, b) ↦ (b, a)."
                    : $"Commutativity of the {cw}: the operands may be exchanged.",
                $"{c} ({cw}) is commutative");
            Add(Comp, $"σ {c} (τ {c} ρ) = (σ {c} τ) {c} ρ", new[] { $"σ {c} (τ {c} ρ)" }, new[] { $"(σ {c} τ) {c} ρ" },
                $"Associativity of the {cw}, reassociating to the left.",
                $"{c} ({cw}) reassociates left");
            Add(Comp, $"(σ {c} τ) {c} ρ = σ {c} (τ {c} ρ)", new[] { $"(σ {c} τ) {c} ρ" }, new[] { $"σ {c} (τ {c} ρ)" },
                $"Associativity of the {cw}, reassociating to the right.",
                $"{c} ({cw}) reassociates right");
        }
        Add(Comp, "σ ∩ σ = σ", new[] { "σ ∩ σ" }, new[] { "σ" },
            "Idempotence: intersecting a type with itself changes nothing.", "∩ (intersection) is idempotent");
        Add(Comp, "σ ∪ σ = σ", new[] { "σ ∪ σ" }, new[] { "σ" },
            "Idempotence: the union of a type with itself changes nothing.", "∪ (union) is idempotent");
        Add(Comp, "A ∩ (A ∪ B) = A", new[] { "A ∩ (A ∪ B)" }, new[] { "A" },
            "Absorption: A ∩ (A ∪ B) collapses to A.", "∩ absorbs ∪");
        Add(Comp, "A ∪ (A ∩ B) = A", new[] { "A ∪ (A ∩ B)" }, new[] { "A" },
            "Absorption: A ∪ (A ∩ B) collapses to A.", "∪ absorbs ∩");
        Add(Intro, "A = A ∩ (A ∪ B)", new[] { "A" }, new[] { "A ∩ (A ∪ B)" },
            "Reverse absorption: A already inhabits A ∩ (A ∪ B).", "reverse absorption into ∩");
        Add(Intro, "A = A ∪ (A ∩ B)", new[] { "A" }, new[] { "A ∪ (A ∩ B)" },
            "Reverse absorption: A already inhabits A ∪ (A ∩ B).", "reverse absorption into ∪");

        // ---- quantifier laws ------------------------------------------------
        (string Q, string Word)[] quants = { ("∀", "universal"), ("∃", "existential") };
        foreach (var (q, qw) in quants)
        {
            foreach (var (c, cw) in conns)
            {
                Add(Comp, $"{q}-dist-{c}", new[] { $"{q}α. σ {c} τ" }, new[] { $"({q}α. σ) {c} ({q}α. τ)" },
                    $"Distribute the {qw} quantifier over the {cw} (sound in this direction for schematic bodies).");
                Add(Comp, $"{q}-factor-{c}", new[] { $"({q}α. σ) {c} ({q}α. τ)" }, new[] { $"{q}α. σ {c} τ" },
                    $"Factor a {cw} of {qw}s over the same variable into a single quantifier.");
            }
            Add(Comp, $"{q}-vacuous-elim", new[] { $"{q}α. σ" }, new[] { "σ" },
                $"Vacuous quantifier elimination: α does not occur in σ, so the {qw} quantifier is dropped.");
            Add(Intro, $"{q}-vacuous-intro", new[] { "σ" }, new[] { $"{q}α. σ" },
                $"Vacuous quantifier introduction: quantify a variable that does not occur in σ.");
        }
        Add(Comp, "∀∀-swap", new[] { "∀α. ∀β. τ" }, new[] { "∀β. ∀α. τ" }, "Adjacent universal quantifiers commute.");
        Add(Comp, "∃∃-swap", new[] { "∃α. ∃β. τ" }, new[] { "∃β. ∃α. τ" }, "Adjacent existential quantifiers commute.");
        Add(Comp, "∃∀-swap", new[] { "∃α. ∀β. τ" }, new[] { "∀β. ∃α. τ" },
            "∃∀ entails ∀∃: a uniform witness specializes pointwise (the converse is invalid).");
        Add(Comp, "μ-unroll", new[] { "μα. τ" }, new[] { "τ" }, "Unfold the recursive type one step: μα. τ ↝ τ[α := μα. τ].");
        Add(Intro, "μ-roll", new[] { "τ" }, new[] { "μα. τ" }, "Fold one step of recursion: from the unfolding τ[α := μα. τ] conclude μα. τ.");
        Add(Comp, "μ-swap", new[] { "μα. μβ. τ" }, new[] { "μβ. μα. τ" }, "Nested recursive binders commute for schematic bodies.");

        // ---- monotonicity ---------------------------------------------------
        foreach (var (c, cw) in conns)
        {
            Add(Elim, $"{c}-monoL", new[] { "A → B" }, new[] { $"(A {c} X) → (B {c} X)" },
                $"Left monotonicity: a map A → B lifts to the {cw} with a fixed right operand X.");
            Add(Elim, $"{c}-monoR", new[] { "A → B" }, new[] { $"(X {c} A) → (X {c} B)" },
                $"Right monotonicity: a map A → B lifts to the {cw} with a fixed left operand X.");
        }
        Add(Elim, "→-mono-cod", new[] { "A → B" }, new[] { "(X → A) → (X → B)" },
            "Covariance in the codomain: post-compose with A → B.");
        Add(Elim, "→-mono-dom", new[] { "A → B" }, new[] { "(B → X) → (A → X)" },
            "Contravariance in the domain: pre-compose with A → B, reversing the arrow.");
        Add(Elim, "∀-mono", new[] { "∀α. σ → τ" }, new[] { "(∀α. σ) → (∀α. τ)" },
            "Monotonicity under ∀: a uniform map of bodies maps the quantified types.");
        Add(Elim, "∃-mono", new[] { "∀α. σ → τ" }, new[] { "(∃α. σ) → (∃α. τ)" },
            "Monotonicity under ∃: a uniform map of bodies maps the packages.");
        for (var n = 2; n <= 8; n++)
            Add(Elim, $"^-mono-{n}", new[] { "A → B" }, new[] { $"A^{n} → B^{n}" },
                $"Componentwise action on {n}-tuples: a map A → B acts on each coordinate of A^{n}.");

        // ---- combinators ----------------------------------------------------
        Add(Intro, "I-combinator", System.Array.Empty<string>(), new[] { "σ → σ" },
            "Identity: λx. x — every type maps to itself.");
        Add(Intro, "K-combinator", System.Array.Empty<string>(), new[] { "σ → τ → σ" },
            "Constant: λx. λy. x — discard the second argument.");
        Add(Intro, "S-combinator", System.Array.Empty<string>(), new[] { "(σ → τ → ρ) → (σ → τ) → σ → ρ" },
            "Substitution: λf. λg. λx. f x (g x) — the applicative combinator.");
        Add(Intro, "B-combinator", System.Array.Empty<string>(), new[] { "(τ → ρ) → (σ → τ) → σ → ρ" },
            "Composition: λf. λg. λx. f (g x).");
        Add(Intro, "C-combinator", System.Array.Empty<string>(), new[] { "(σ → τ → ρ) → τ → σ → ρ" },
            "Flip: λf. λy. λx. f x y — exchange the argument order.");
        Add(Intro, "W-combinator", System.Array.Empty<string>(), new[] { "(σ → σ → ρ) → σ → ρ" },
            "Duplication: λf. λx. f x x — use the argument twice.");
        Add(Elim, "apply", new[] { "(σ → τ) × σ" }, new[] { "τ" },
            "Uncurried application: a function paired with its argument yields the result.");
        Add(Comp, "curry", new[] { "(σ × τ) → ρ" }, new[] { "σ → τ → ρ" },
            "Currying: a function of a pair becomes a function returning a function.");
        Add(Comp, "uncurry", new[] { "σ → τ → ρ" }, new[] { "(σ × τ) → ρ" },
            "Uncurrying: collapse two arguments into one pair argument.");
        Add(Elim, "diag", new[] { "σ" }, new[] { "σ × σ" },
            "Diagonal: duplicate a value into the pair (x, x).");
        for (var n = 2; n <= 8; n++)
            Add(Elim, $"compose-{n}",
                Enumerable.Range(1, n).Select(i => $"σ{i} → σ{i + 1}").ToArray(),
                new[] { $"σ1 → σ{n + 1}" },
                $"Chain composition of {n} arrows: σ1 → σ2 → ⋯ → σ{n + 1} composes end to end.");

        // ---- records --------------------------------------------------------
        string[] labels = { "x", "y", "z", "w" };
        for (var n = 1; n <= 4; n++)
        {
            var ms = Enumerable.Range(0, n).Select(i => $"{labels[i]} : σ{i + 1}").ToList();
            var rec = "⟨" + string.Join(", ", ms) + "⟩";
            Add(Intro, $"⟨⟩-intro{n}", Vars("σ", n), new[] { rec },
                $"Record introduction with {n} member{(n > 1 ? "s" : "")}: package the components under their labels.");
            for (var i = 0; i < n; i++)
                Add(Elim, $"⟨⟩-get-{labels[i]}of{n}", new[] { rec }, new[] { $"σ{i + 1}" },
                    $"Member access M.{labels[i]}: read member {labels[i]} out of the {n}-member record.");
            if (n >= 2)
                for (var i = 0; i < n; i++)
                {
                    var dropped = "⟨" + string.Join(", ", ms.Where((_, j) => j != i)) + "⟩";
                    Add(Elim, $"⟨⟩-drop-{labels[i]}of{n}", new[] { rec }, new[] { dropped },
                        $"Width subtyping: forget member {labels[i]}; a record with more members is one with fewer.");
                }
        }
        Add(Comp, "⟨⟩-perm2", new[] { "⟨x : σ1, y : σ2⟩" }, new[] { "⟨y : σ2, x : σ1⟩" },
            "Permutation: record member order is irrelevant (the canonicalizer sorts by label).");
        Add(Comp, "⟨⟩-perm3", new[] { "⟨x : σ1, y : σ2, z : σ3⟩" }, new[] { "⟨z : σ3, x : σ1, y : σ2⟩" },
            "Permutation of a three-member record: any reordering denotes the same record type.");

        // ---- power laws -----------------------------------------------------
        for (var n = 2; n <= 10; n++)
        {
            var chain = Chain("×", Enumerable.Repeat("A", n));
            Add(Comp, $"^-expand-{n}", new[] { $"A^{n}" }, new[] { chain },
                $"Expand the diagonal power: A^{n} computes to the {n}-fold product {chain}.");
            Add(Comp, $"^-contract-{n}", new[] { chain }, new[] { $"A^{n}" },
                $"Contract an {n}-fold product of the same set back into the power A^{n}.");
        }
        for (var m = 2; m <= 5; m++)
            for (var n = 2; n <= 5; n++)
            {
                Add(Comp, $"^-split-{m}+{n}", new[] { $"A^{m + n}" }, new[] { $"A^{m} × A^{n}" },
                    $"Split a power along {m} + {n} = {m + n}: A^{m + n} is A^{m} × A^{n} up to reassociation.");
                Add(Comp, $"^-tower-{m}^{n}", new[] { $"(A^{m})^{n}" }, new[] { $"A^{m * n}" },
                    $"Power of a power: ({m}-tuples){n}-tupled flattens to {m}·{n} = {m * n} coordinates.");
            }
        Add(Comp, "^-one", new[] { "A^1" }, new[] { "A" }, "A first power is the set itself.");
        for (var n = 2; n <= 6; n++)
            Add(Constr, $"^-construct-{n}", new[] { "A" }, new[] { $"A^{n}" },
                $"Construct the {n}-fold diagonal power: tuples of length {n} with every coordinate drawn from A ⊆ ℤ.");

        // ---- arithmetic in ℤ ------------------------------------------------
        for (var m = 0; m <= 10; m++)
            for (var n = 0; n <= 10; n++)
                Add(Comp, $"ℤ-add-{m}+{n}", new[] { $"{m} + {n}" }, new[] { $"{m + n}" },
                    $"Closed arithmetic computes in ℤ: the sum {m} + {n} reduces to the literal {m + n}.");
        for (var n = 0; n <= 40; n++)
            Add(Constr, $"ℕ-succ-{n}", new[] { $"{n}" }, new[] { $"{n} + 1" },
                $"Successor: from the numeral {n} construct {n} + 1 (computing to {n + 1} in ℤ).");
        for (var n = 0; n <= 40; n++)
            Add(Constr, $"ℕ-lit-{n}", System.Array.Empty<string>(), new[] { $"{n}" },
                $"The numeral {n}: a constant of type ℕ whose computational value is {n} ∈ ℤ.");
        for (var n = 1; n <= 15; n++)
            Add(Comp, $"ℤ-double-{n}", new[] { $"{n} + {n}" }, new[] { $"{2 * n}" },
                $"Doubling computes in ℤ: {n} + {n} reduces to {2 * n}.");

        // ---- juxtaposition (elementwise integer products) -------------------
        var setNames = "ABCDEF";
        foreach (var a in setNames)
            foreach (var b in setNames)
            {
                if (a == b) continue;
                Add(Constr, $"juxt-construct-{a}{b}", new[] { a.ToString(), b.ToString() }, new[] { $"{a}{b}" },
                    $"Juxtaposition: the elementwise product {a}{b} = {{{char.ToLower(a)}{char.ToLower(b)} : {char.ToLower(a)} ∈ {a}, {char.ToLower(b)} ∈ {b}}} ⊆ ℤ.");
                if (a < b)
                    Add(Comp, $"juxt-comm-{a}{b}", new[] { $"{a}{b}" }, new[] { $"{b}{a}" },
                        $"Elementwise products of integer sets commute: {a}{b} = {b}{a} because multiplication in ℤ does.");
            }
        foreach (var a in setNames)
            Add(Constr, $"juxt-square-{a}", new[] { a.ToString() }, new[] { $"{a}{a}" },
                $"Self-product: {a}{a} = {{{char.ToLower(a)}1·{char.ToLower(a)}2 : both factors from {a}}} — note this is not the diagonal power {a}².");

        // ---- familial forms -------------------------------------------------
        string[] domains = { "ℕ", "ℤ", "A", "σ" };
        foreach (var d in domains)
        {
            Add(Intro, $"⋂-intro-{d}", new[] { "τ" }, new[] { $"⋂(x:{d}) τ" },
                $"Familial intersection introduction: if τ holds for an arbitrary x : {d}, it holds for the whole family.");
            Add(Elim, $"⋂-elim-{d}", new[] { $"⋂(x:{d}) τ", d }, new[] { "τ" },
                $"Familial instantiation: specialize the family at any term N : {d}, giving τ[x := N].");
            Add(Intro, $"⋃-intro-{d}", new[] { "τ", d }, new[] { $"⋃(x:{d}) τ" },
                $"Familial union introduction: exhibit an index N : {d} at which τ[x := N] holds.");
            Add(Elim, $"⋃-elim-{d}", new[] { $"⋃(x:{d}) τ", $"(x : {d}) → τ → ρ" }, new[] { "ρ" },
                $"Familial union elimination: use the member uniformly in the index (x not free in ρ).");
        }

        // ---- dependent forms ------------------------------------------------
        foreach (var d in domains)
        {
            Add(Intro, $"Π-intro-{d}", new[] { "τ" }, new[] { $"(x : {d}) → τ" },
                $"Dependent function introduction over {d}: from a body τ valid for arbitrary x : {d}, abstract λx. M.");
            Add(Elim, $"Π-elim-{d}", new[] { $"(x : {d}) → τ", d }, new[] { "τ" },
                $"Dependent application: apply to any N : {d}; the result type is τ[x := N].");
            Add(Comp, $"Π-β-{d}", new[] { $"(x : {d}) → τ", d }, new[] { "τ" },
                $"β-reduction for the dependent function over {d}: (λx. M) N ↝ M[x := N].");
            Add(Intro, $"Σ-intro-{d}", new[] { d, "τ" }, new[] { $"(x : {d}) × τ" },
                $"Dependent pair introduction over {d}: pick a witness N : {d} and a proof of τ[x := N].");
            Add(Elim, $"Σ-proj1-{d}", new[] { $"(x : {d}) × τ" }, new[] { d },
                $"First projection of the dependent pair: the witness in {d}.");
            Add(Elim, $"Σ-proj2-{d}", new[] { $"(x : {d}) × τ" }, new[] { "τ" },
                $"Second projection: the proof component, at type τ[x := π₁ M].");
            Add(Intro, $"∩dep-intro-{d}", new[] { d, "τ" }, new[] { $"(x : {d}) ∩ τ" },
                $"Dependent intersection introduction over {d}: one term inhabits {d} and τ[x := itself].");
            Add(Elim, $"∩dep-elim-{d}", new[] { $"(x : {d}) ∩ τ" }, new[] { d },
                $"Dependent intersection elimination: the inhabitant is in particular a member of {d}.");
        }

        return L;
    }
}

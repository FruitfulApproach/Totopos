namespace AbCatDC.RazorLib.Shared;

/// <summary>Example goals for the Proof tree page, each with a proof in
/// progress. They are written against the "Chases in left R-modules" palette:
/// the rule nodes carry that palette's premises and conclusions, and each tree
/// brings along the pictures it mentions, so an example loads even under
/// another system (the automated search then only finds the current system's
/// rules).</summary>
public static class ExampleTrees
{
    public record Example(string Name, string Description, Func<TreeDto> Build);

    static ExampleSystems.ExampleSystem Rmod =>
        ExampleSystems.All.First(s => s.Name == "Chases in left R-modules");

    static RulePalette.Entry Rule(string name) => Rmod.Rules.First(r => r.Name == name);

    static string Sketch(string name) => Rmod.Sketches!.First(s => s.Name == name).Json;

    /// <summary>A small builder so the examples read like the proofs they are.</summary>
    sealed class B
    {
        public readonly TreeDto T;
        public B(string name, string description)
        {
            T = new TreeDto { Name = name, Description = description, SystemName = "Chases in left R-modules" };
        }

        public int Stmt(string text, string role, double x, double y)
        {
            var id = T.NextId++;
            T.Nodes.Add(new TreeNodeDto { Id = id, IsRule = false, Role = role, Premises = new() { text }, Conclusions = new() { text }, X = x, Y = y });
            return id;
        }

        public int Given(string text, double x, double y) => Stmt(text, "given", x, y);
        public int Goal(string text, double x, double y) => Stmt(text, "goal", x, y);

        public int RuleNode(string name, double x, double y)
        {
            var r = Rule(name);
            var id = T.NextId++;
            T.Nodes.Add(new TreeNodeDto
            {
                Id = id, IsRule = true, Role = "rule", RuleName = r.Name, RuleKind = r.Kind,
                Premises = r.Premises.ToList(), Conclusions = r.Conclusions.ToList(), X = x, Y = y
            });
            return id;
        }

        public void Wire(int src, int srcIdx, int dst, int dstIdx)
        {
            T.Wires.Add(new TreeWireDto { Id = T.NextId++, Src = src, SrcIdx = srcIdx, Dst = dst, DstIdx = dstIdx });
        }

        public void Sk(string name) => T.Sketches.Add(new SketchDto { Name = name, Json = Sketch(name) });
        public void Sk(string name, string json) => T.Sketches.Add(new SketchDto { Name = name, Json = json });
    }

    public static readonly Example[] All =
    {
        new("Snake lemma — connecting homomorphism exists",
            "In the snake picture with an element y of B drawn in (z = qy, w = by), if c kills z then w lifts along s: the connecting homomorphism is well defined. The rule snake-connect is on the canvas with two of its three premises wired; finish the third and wire its conclusion into the goal, or let the search do it.",
            () =>
            {
                var b = new B("Snake lemma — connecting homomorphism exists", "");
                foreach (var s in new[] { "SnakeEl", "RowST", "KillZ", "ImS" }) b.Sk(s);
                var g1 = b.Given("Γ ⊢ [SnakeEl] commutes in Mod(R)", 40, 40);
                var g2 = b.Given("Γ ⊢ [RowST] has exact rows", 520, 40);
                var g3 = b.Given("Γ ⊢ [KillZ] commutes", 880, 40);
                var r = b.RuleNode("snake-connect", 300, 300);
                var goal = b.Goal("Γ ⊢ [ImS] commutes in Mod(R)", 420, 620);
                b.Wire(g1, 0, r, 0);
                b.Wire(g2, 0, r, 1);
                b.T.Description = "";
                return b.T;
            }),
        new("Four lemma (mono)",
            "The ladder [Four] with exact rows: a surjective and b, d injective force c injective. The rule is placed with its two picture premises wired; the three property premises and the goal remain.",
            () =>
            {
                var b = new B("Four lemma (mono)", "");
                b.Sk("Four");
                var g1 = b.Given("Γ ⊢ [Four] commutes in Mod(R)", 40, 40);
                var g2 = b.Given("Γ ⊢ [Four] has exact rows", 520, 40);
                var g3 = b.Given("Γ ⊢ a : surjective", 860, 40);
                var g4 = b.Given("Γ ⊢ b : injective", 1060, 40);
                var g5 = b.Given("Γ ⊢ d : injective", 1240, 40);
                var r = b.RuleNode("four-lemma-mono", 320, 300);
                var goal = b.Goal("Γ ⊢ c : injective", 560, 600);
                b.Wire(g1, 0, r, 0);
                b.Wire(g2, 0, r, 1);
                return b.T;
            }),
        new("Lift along an exact row",
            "One picture of the situation — an exact row A → B → C, an element y of B, and g kills y — and the goal that y lifts along f (the dashed x). Nothing is wired yet: a good first run for the automated search, which works backward from the goal to exact-lift and closes its premises against the givens.",
            () =>
            {
                var b = new B("Lift along an exact row", "");
                b.Sk("G", "[0, 4, [1,0,\"R\"], [0,1,\"A\"], [1,1,\"B\"], [2,1,\"C\"], [0,1,\"p\"], [1,2,\"f\"], [2,3,\"g\"], [0,2,\"y\"], [0,3,\"0\"]]");
                b.Sk("Lift", "[0, 3, [1,0,\"R\"], [0,1,\"A\"], [1,1,\"B\"], [0,1,\"x\",0,{\"style\":{\"body\":{\"name\":\"dashed\"}}}], [1,2,\"f\"], [0,2,\"y\"]]");
                b.Given("Γ ⊢ [G] commutes in Mod(R)", 40, 40);
                b.Given("Γ ⊢ [G] has exact rows in Mod(R)", 480, 40);
                b.Goal("Γ ⊢ [Lift] commutes in Mod(R)", 300, 520);
                return b.T;
            }),
        new("Composite of injectives",
            "A two-arrow row with both arrows injective; the goal is that the composite is. One rule closes it.",
            () =>
            {
                var b = new B("Composite of injectives", "");
                b.Sk("Row");
                b.Given("Γ ⊢ [Row] in Mod(R)", 40, 40);
                b.Given("Γ ⊢ f : injective", 380, 40);
                b.Given("Γ ⊢ g : injective", 600, 40);
                b.Goal("Γ ⊢ gf : injective", 300, 460);
                return b.T;
            }),
        new("Chasing a square",
            "A commuting square with an element pushed around it. The three pictures are given; the goal is the equation rm = sn. The search applies sq-chase forward — the glued picture asserts the equation — and closes the goal through what that picture contributes.",
            () =>
            {
                var b = new B("Chasing a square", "");
                b.Sk("MySq", "[0, 4, [0,0,\"X\"], [1,0,\"Y\"], [0,1,\"Z\"], [1,1,\"W\"], [0,1,\"p\"], [0,2,\"q\"], [1,3,\"r\"], [2,3,\"s\"]]");
                b.Sk("T1", "[0, 3, [1,0,\"R\"], [0,1,\"X\"], [1,1,\"Y\"], [0,1,\"e\"], [1,2,\"p\"], [0,2,\"m\"]]");
                b.Sk("T2", "[0, 3, [1,0,\"R\"], [0,1,\"X\"], [1,1,\"Z\"], [0,1,\"e\"], [1,2,\"q\"], [0,2,\"n\"]]");
                b.Given("Γ ⊢ [MySq] commutes", 40, 40);
                b.Given("Γ ⊢ [T1] commutes", 360, 40);
                b.Given("Γ ⊢ [T2] commutes", 640, 40);
                b.Goal("Γ ⊢ rm = sn", 340, 480);
                return b.T;
            }),
        new("Short exact sequence, unpacked",
            "A drawn short exact sequence 0 → L → M → N → 0 with exact rows; the goals are i injective and j surjective.",
            () =>
            {
                var b = new B("Short exact sequence, unpacked", "");
                b.Sk("MySES", "[0, 5, [0,0,\"0\"], [1,0,\"L\"], [2,0,\"M\"], [3,0,\"N\"], [4,0,\"0\"], [0,1,\"0\"], [1,2,\"i\"], [2,3,\"j\"], [3,4,\"0\"]]");
                b.Given("Γ ⊢ [MySES] has exact rows in Mod(R)", 200, 40);
                b.Goal("Γ ⊢ i : injective", 80, 460);
                b.Goal("Γ ⊢ j : surjective", 480, 460);
                return b.T;
            }),
    };
}

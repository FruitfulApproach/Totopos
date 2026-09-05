namespace AbCatDC.Kernel

/// C#-friendly result for the frontend test screen.
type RunResult =
    { Success: bool
      Formatted: string
      Canonical: string
      /// "canonical ← original" lines for bound and free variables
      VariableMap: string
      /// inferred typing judgments for the expression's leaves
      Inference: string
      /// the expression after elaboration (powers expanded to products);
      /// empty when elaboration changes nothing
      Elaborated: string
      Error: string }

/// Result of instantiating a schematic expression with metavariable bindings.
type InstantiateOutcome =
    { Ok: bool
      Formatted: string
      Canonical: string
      Error: string }

/// Result of the automatic rule-application engine.
type ApplyOutcome =
    { Ok: bool
      /// metavariable name -> chosen instantiation (formatted)
      Bindings: (string * string)[]
      /// for each premise in order, the 0-based index of the matched step
      UsedSteps: int[]
      /// instantiated conclusions, formatted (round-trip-safe)
      NewConclusions: string[]
      /// per premise: "" when it matched the step itself, else the judgment a
      /// diagram step contributed that it matched instead
      UsedVia: string[]
      /// diagrams the conclusions introduced, instantiated by the match:
      /// (sketch name, quiver JSON) — the proof should adopt them
      NewSketches: (string * string)[]
      Error: string }

module Api =

    let private canonicalText (t: Ty) : string =
        Ty.format (Canonical.canonicalize (Ty.expandPowers t)).Ty

    /// Declare the active system's rigid constant names (replaces the previous
    /// set). Call before parsing that system's rules; blank entries ignored.
    let SetConstants (names: string[]) : unit =
        Ty.userConstants <-
            names
            |> Array.map (fun n -> (n : string).Trim())
            |> Array.filter (fun n -> n.Length > 0)
            |> Set.ofArray

    /// Register the active system's diagrams (name, quiver JSON) — the Chase
    /// page passes the system's sketches plus the proof's own. Replaces the
    /// previous set; call before any parsing/matching batch.
    let SetSketches (entries: (string * string)[]) : unit =
        Ty.userSketches <- entries |> Array.filter (fun (nm, _) -> nm <> "") |> Map.ofArray

    /// Whether "[D] commutes" carries its built-in ∀∃ meaning (false = the
    /// opaque, name-matched token of systems stored before that semantics).
    let SetDiagramSemantics (on: bool) : unit = Ty.diagramSemantics <- on

    /// Sketch names mentioned anywhere in an expression, first occurrence first.
    let rec sketchesIn (t: Ty) : string list =
        match t with
        | Ty.Sketch nm -> [ nm ]
        | Ty.Atom _ | Ty.Var _ | Ty.Lit _ -> []
        | Ty.Power (b, _) | Ty.Commutes b | Ty.Exact (b, _) -> sketchesIn b
        | Ty.InCategory (a, b) | Ty.HasType (a, b) | Ty.Eq (a, b)
        | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
        | Ty.Intersection (a, b) | Ty.Union (a, b) -> sketchesIn a @ sketchesIn b
        | Ty.Record ms -> ms |> List.collect (fun (_, mt) -> sketchesIn mt)
        | Ty.Polymorphic (_, b) | Ty.Existential (_, b) | Ty.Recursive (_, b) -> sketchesIn b
        | Ty.DependentFunction (_, a, b) | Ty.DependentPair (_, a, b)
        | Ty.DependentIntersection (_, a, b)
        | Ty.FamilialIntersection (_, a, b) | Ty.FamilialUnion (_, a, b) -> sketchesIn a @ sketchesIn b
        | Ty.Entails (ctx, goal) ->
            (ctx |> List.collect (function CtxVar _ -> [] | Hyp (_, h) -> sketchesIn h | Anon a -> sketchesIn a))
            @ sketchesIn goal

    /// Rename sketch references throughout an expression.
    let rec mapSketches (f: string -> string) (t: Ty) : Ty =
        let go = mapSketches f
        match t with
        | Ty.Sketch nm -> Ty.Sketch (f nm)
        | Ty.Atom _ | Ty.Var _ | Ty.Lit _ -> t
        | Ty.Power (b, k) -> Ty.Power (go b, k)
        | Ty.Commutes b -> Ty.Commutes (go b)
        | Ty.Exact (b, ax) -> Ty.Exact (go b, ax)
        | Ty.InCategory (a, b) -> Ty.InCategory (go a, go b)
        | Ty.HasType (a, b) -> Ty.HasType (go a, go b)
        | Ty.Eq (a, b) -> Ty.Eq (go a, go b)
        | Ty.Function (a, b) -> Ty.Function (go a, go b)
        | Ty.Product (a, b) -> Ty.Product (go a, go b)
        | Ty.Sum (a, b) -> Ty.Sum (go a, go b)
        | Ty.Intersection (a, b) -> Ty.Intersection (go a, go b)
        | Ty.Union (a, b) -> Ty.Union (go a, go b)
        | Ty.Record ms -> Ty.Record (ms |> List.map (fun (nm, mt) -> nm, go mt))
        | Ty.Polymorphic (a, b) -> Ty.Polymorphic (a, go b)
        | Ty.Existential (a, b) -> Ty.Existential (a, go b)
        | Ty.Recursive (a, b) -> Ty.Recursive (a, go b)
        | Ty.DependentFunction (x, a, b) -> Ty.DependentFunction (x, go a, go b)
        | Ty.DependentPair (x, a, b) -> Ty.DependentPair (x, go a, go b)
        | Ty.DependentIntersection (x, a, b) -> Ty.DependentIntersection (x, go a, go b)
        | Ty.FamilialIntersection (x, a, b) -> Ty.FamilialIntersection (x, go a, go b)
        | Ty.FamilialUnion (x, a, b) -> Ty.FamilialUnion (x, go a, go b)
        | Ty.Entails (ctx, goal) ->
            let ctx' = ctx |> List.map (function CtxVar v -> CtxVar v | Hyp (x, h) -> Hyp (x, go h) | Anon a -> Anon (go a))
            Ty.Entails (ctx', go goal)

    /// The labels of a registered diagram that act as NAMES of a rule that
    /// mentions it: every non-generated, non-rigid object and arrow label.
    let private sketchLabels (name: string) : string list =
        if not Ty.diagramSemantics then [] else
        match QuiverImport.LookupSketch name with
        | None -> []
        | Some g ->
            let el = QuiverImport.elaborate g "" QuiverImport.ArrowStyle Set.empty
            [ for it in el.Items do
                if not it.Generated && it.Quant <> QuiverImport.Bound then yield it.Name ]
            |> List.distinct

    /// Free (never binder-bound) variables of an expression, in order.
    /// Empty on parse failure — callers should surface parse errors separately.
    let FreeVars (input: string) : string[] =
        match Parser.parse input with
        | Error _ -> [||]
        | Ok t -> Canonical.freeVariables t

    /// The names a judgment DECLARES: the subject of a typing judgment ("A : C"
    /// declares A; "Γ ⊢ m : A → B" declares m), or, for "[D] in C", every
    /// object and arrow of the diagram. Empty when nothing is declared (an
    /// equation; a bare "[D] commutes" only USES its labels).
    let DeclaredNames (input: string) : string[] =
        let rec subjects t =
            match t with
            | Ty.HasType (Ty.Var nm, _) -> [ nm ]
            | Ty.InCategory (Ty.Sketch nm, _)
            | Ty.Commutes (Ty.InCategory (Ty.Sketch nm, _))
            | Ty.InCategory (Ty.Commutes (Ty.Sketch nm), _)
            | Ty.InCategory (Ty.Exact (Ty.Sketch nm, _), _)
            | Ty.Exact (Ty.InCategory (Ty.Sketch nm, _), _) -> sketchLabels nm
            | Ty.Entails (_, goal) -> subjects goal
            | _ -> []
        match Parser.parse input with
        | Error _ -> [||]
        | Ok t -> Array.ofList (subjects t)

    /// The first declared name, or "" — see DeclaredNames.
    let DeclaredName (input: string) : string =
        match DeclaredNames input with
        | [||] -> ""
        | ns -> ns.[0]

    /// Every name an expression mentions: its free variables plus, for a
    /// juxtaposition composite, the units it composes (mg mentions m and g).
    /// Used to order premises so a name is declared before it is used.
    let MentionedNames (input: string) : string[] =
        let seen = System.Collections.Generic.HashSet<string>()
        let labels =
            match Parser.parse input with
            | Ok t -> sketchesIn t |> List.collect sketchLabels |> Array.ofList
            | Error _ -> [||]
        [| for v in Array.append (FreeVars input) labels do
            if seen.Add v then yield v
            if Ty.isCompositeName v then
                for u in Ty.nameUnits v do
                    if seen.Add u then yield u |]

    /// Every name a binder OWNS in this expression: ∀/∃/μ variables, dependent
    /// and familial binder variables, and sequent hypothesis names. Distinct
    /// from "not free": a word like Ring is neither free-as-a-unit nor bound.
    let private boundNamesOf (t: Ty) : Set<Name> =
        let acc = System.Collections.Generic.HashSet<Name>()
        let rec go t =
            match t with
            | Ty.Atom _ | Ty.Var _ | Ty.Lit _ | Ty.Sketch _ -> ()
            | Ty.Power (b, _) | Ty.Commutes b | Ty.Exact (b, _) -> go b
            | Ty.InCategory (a, b) | Ty.HasType (a, b) | Ty.Eq (a, b)
            | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
            | Ty.Intersection (a, b) | Ty.Union (a, b) -> go a; go b
            | Ty.Record ms -> ms |> List.iter (fun (_, mt) -> go mt)
            | Ty.Polymorphic (a, b) | Ty.Existential (a, b) | Ty.Recursive (a, b) ->
                acc.Add a |> ignore; go b
            | Ty.DependentFunction (x, a, b) | Ty.DependentPair (x, a, b)
            | Ty.DependentIntersection (x, a, b)
            | Ty.FamilialIntersection (x, a, b) | Ty.FamilialUnion (x, a, b) ->
                acc.Add x |> ignore; go a; go b
            | Ty.Entails (ctx, goal) ->
                for it in ctx do
                    match it with
                    | CtxVar _ -> ()
                    | Hyp (x, ht) -> acc.Add x |> ignore; go ht
                    | Anon at -> go at
                go goal
        go t
        Set.ofSeq acc

    /// Names a binder owns in an expression (sequent hypotheses, ∀/∃/μ and
    /// dependent binders) — what a diagram's labels are "bound" against.
    let BoundNames (input: string) : string[] =
        match Parser.parse input with
        | Ok t -> boundNamesOf t |> Set.toArray
        | Error _ -> [||]

    /// The metavariables of a rule: ordered union of free variables over its
    /// expressions, minus constants, context metavariables, and "Type".
    let MetaVars (exprs: string[]) : string[] =
        let seen = System.Collections.Generic.HashSet<string>()
        let raw =
            [| for e in exprs do
                // a diagram's unbound labels are metavariables of the rule
                let labels =
                    match Parser.parse e with
                    | Ok t -> sketchesIn t |> List.collect sketchLabels |> Array.ofList
                    | Error _ -> [||]
                for v in Array.append (FreeVars e) labels do
                    if not (Set.contains v Ty.constantAtoms)
                       && not (Set.contains v Ty.contextVars)
                       && v <> "Type"
                       && seen.Add v then
                        yield v |]
        // a composite like gf (or i₁p₁) is determined by its units when those
        // are metavariables themselves — don't offer it as its own slot.
        // Digit-led units (the 0 in F0) are rigid parts, never slots.
        let atomic = raw |> Array.filter (fun v -> not (Ty.isCompositeName v)) |> Set.ofArray
        let bound =
            exprs
            |> Array.fold (fun acc e ->
                match Parser.parse e with
                | Ok t -> Set.union acc (boundNamesOf t)
                | Error _ -> acc) Set.empty
        // A composite is determined by its units when each one is an atomic
        // free name, a digit, or a name a BINDER OWNS (the x of
        // "⋂(x:σ) (fx = gx)", a hypothesis name). Such a composite must not be
        // its own metavariable: as one it would match any name whatsoever,
        // with no tie back to f. An undeclared WORD (Ring) has units that are
        // neither, so it stays schematic — declare it to make it rigid.
        let determined u =
            Set.contains u atomic || System.Char.IsDigit u.[0] || Set.contains u bound
        raw
        |> Array.filter (fun v ->
            not (Ty.isCompositeName v)
            || not (Ty.nameUnits v |> List.forall determined))

    /// Name-preserving semantic equality of two expressions (see Canonical.sameTy).
    let SameExpr (a: string) (b: string) : bool =
        match Parser.parse a, Parser.parse b with
        | Ok ta, Ok tb -> Canonical.sameTy ta tb
        | _ -> false

    let private parseBindings (bindings: (string * string)[]) : Result<Map<Name, Ty>, string> =
        let mutable err = None
        let mutable m = Map.empty
        for (name, repl) in bindings do
            if err.IsNone then
                let r = (repl : string).Trim()
                // blank = leave the variable to the engine; "f := f" pins it to the proof's own f
                if r <> "" then
                    match Parser.parse r with
                    | Ok t -> m <- Map.add name t m
                    | Error e -> err <- Some $"binding {name} := {r}: {e}"
        match err with Some e -> Error e | None -> Ok m

    /// Substitute, then verify the result survives its own surface syntax
    /// (format → parse → same canonical form) so it can be stored as text.
    let private instantiateTy (m: Map<Name, Ty>) (t: Ty) : Result<string * string, string> =
        let t' = Ty.substTyVars m t
        let text = Ty.format t'
        match Parser.parse text with
        | Error e -> Error $"substitution leaves the surface syntax ({e})"
        | Ok back ->
            if Canonical.sameTy back t' then Ok (text, canonicalText t')
            else Error "substitution leaves the surface syntax (round-trip mismatch)"

    /// Instantiate one schematic expression with explicit bindings.
    let Instantiate (input: string) (bindings: (string * string)[]) : InstantiateOutcome =
        match Parser.parse input with
        | Error e -> { Ok = false; Formatted = ""; Canonical = ""; Error = e }
        | Ok t ->
            match parseBindings bindings with
            | Error e -> { Ok = false; Formatted = ""; Canonical = ""; Error = e }
            | Ok m ->
                match instantiateTy m t with
                | Error e -> { Ok = false; Formatted = ""; Canonical = ""; Error = e }
                | Ok (text, canon) -> { Ok = true; Formatted = text; Canonical = canon; Error = "" }

    let private failApply e : ApplyOutcome =
        { Ok = false; Bindings = [||]; UsedSteps = [||]; NewConclusions = [||]; UsedVia = [||]; NewSketches = [||]; Error = e }

    /// The judgments a proven diagram step makes available for free, each in
    /// the step's own context. Steps are GROUND, so nothing is quantified:
    ///   [D] commutes          — its commuting equations
    ///   [D] in C              — its object and arrow typings
    ///   [D] commutes in C     — both
    /// The exact(f, g) judgments "[D] has exact rows/columns" stands for, each
    /// re-wrapped in the given context.
    let private exactJudgments (wrap: Ty -> Ty) (name: string) (ax: Axis) : Ty list =
        match Map.tryFind name Ty.userSketches with
        | None -> []
        | Some json ->
            let stmts, _ = QuiverImport.ExactStatements json (ax = Axis.Rows)
            [ for txt in stmts do
                match Parser.parse txt with
                | Ok t -> yield wrap t
                | Error _ -> () ]

    let private derivedFacts (step: Ty) : Ty list =
        if not Ty.diagramSemantics then [] else
        let ctx, goal = match step with Ty.Entails (c, g) -> c, g | t -> [], t
        let wrap (t: Ty) = if List.isEmpty ctx then t else Ty.Entails (ctx, t)
        let facts (name: string) (category: string) (typings: bool) (equations: bool) =
            match QuiverImport.LookupSketch name with
            | None -> []
            | Some g ->
                let el = QuiverImport.elaborate g category (QuiverImport.HomStyleForActiveSystem ()) Set.empty
                let texts =
                    Array.concat [ (if typings then Array.append el.ObjectTypings el.ArrowTypings else [||])
                                   (if equations then el.Equations else [||]) ]
                [ for txt in texts do
                    match Parser.parse txt with
                    | Ok t -> yield wrap t
                    | Error _ -> () ]
        // "[D] commutes in C" parses with `in` outermost; "[D] in C commutes"
        // with `commutes` outermost — both mean the same thing
        // a step "[D] commutes in C" also satisfies the weaker premises
        // "[D] commutes" and "[D] in C", whichever spelling a rule uses
        match goal with
        | Ty.Commutes (Ty.Sketch nm) -> facts nm "" false true
        | Ty.Commutes (Ty.InCategory (Ty.Sketch nm, c))
        | Ty.InCategory (Ty.Commutes (Ty.Sketch nm), c) ->
            wrap (Ty.Commutes (Ty.Sketch nm)) :: wrap (Ty.InCategory (Ty.Sketch nm, c)) :: facts nm (Ty.format c) true true
        | Ty.InCategory (Ty.Sketch nm, c) -> facts nm (Ty.format c) true false
        | Ty.Exact (Ty.Sketch nm, ax) -> exactJudgments wrap nm ax
        | Ty.InCategory (Ty.Exact (Ty.Sketch nm, ax), c)
        | Ty.Exact (Ty.InCategory (Ty.Sketch nm, c), ax) ->
            wrap (Ty.Exact (Ty.Sketch nm, ax)) :: wrap (Ty.InCategory (Ty.Sketch nm, c))
            :: facts nm (Ty.format c) true false @ exactJudgments wrap nm ax
        | _ -> []

    /// The ∀∃ reading of every diagram assertion in an expression, for display:
    /// (heading such as "[D]" or "[D] has exact rows", meaning), with the
    /// expression's own binder-owned names treated as bound.
    let DiagramMeanings (input: string) : (string * QuiverImport.MeaningOutcome)[] =
        match Parser.parse input with
        | Error _ -> [||]
        | Ok t ->
            let bound = boundNamesOf t |> Set.toArray
            let rec find t =
                match t with
                | Ty.Commutes (Ty.Sketch nm) -> [ nm, "", None ]
                | Ty.Commutes (Ty.InCategory (Ty.Sketch nm, c))
                | Ty.InCategory (Ty.Commutes (Ty.Sketch nm), c)
                | Ty.InCategory (Ty.Sketch nm, c) -> [ nm, Ty.format c, None ]
                | Ty.Exact (Ty.Sketch nm, ax) -> [ nm, "", Some ax ]
                | Ty.InCategory (Ty.Exact (Ty.Sketch nm, ax), c)
                | Ty.Exact (Ty.InCategory (Ty.Sketch nm, c), ax) -> [ nm, Ty.format c, None; nm, "", Some ax ]
                | Ty.Entails (ctx, goal) ->
                    (ctx |> List.collect (function Hyp (_, h) -> find h | Anon a -> find a | CtxVar _ -> [])) @ find goal
                | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Eq (a, b) | Ty.HasType (a, b) -> find a @ find b
                | _ -> []
            [| for (nm, cat, ax) in find t |> List.distinct do
                match Map.tryFind nm Ty.userSketches with
                | Some json ->
                    match ax with
                    | None ->
                        let mo = QuiverImport.TryExpandCommutes json cat bound
                        if mo.Ok then yield $"[{nm}]", mo
                    | Some a ->
                        let mo = QuiverImport.TryExpandExact json (a = Axis.Rows)
                        if mo.Ok then yield Ty.format (Ty.Exact (Ty.Sketch nm, a)), mo
                | None -> () |]

    /// What a step's diagram contributes, as terms (the proof tree wires a
    /// premise to a diagram port through these).
    let DerivedFactsOf (step: Ty) : Ty list = derivedFacts step

    /// Public view for the UI: what a step's diagram contributes.
    let DerivedFacts (step: string) : string[] =
        match Parser.parse step with
        | Ok t -> derivedFacts t |> List.map Ty.format |> Array.ofList
        | Error _ -> [||]

    /// The matching pool: every step plus what its diagram contributes, each
    /// tagged with the step's index.
    let private poolOf (indexed: (int * Ty) list) : (Ty * int)[] =
        [| for (i, t) in indexed do
            yield t, i
            for d in derivedFacts t -> d, i |]

    /// Names already in play in a proof: the steps' free variables plus every
    /// label of every diagram a step mentions.
    let private namesInUse (stepTys: Ty list) : Set<string> =
        [ for t in stepTys do
            yield! Canonical.freeVariables t
            for nm in sketchesIn t do
                match QuiverImport.LookupSketch nm with
                | Some g ->
                    for it in (QuiverImport.elaborate g "" QuiverImport.ArrowStyle Set.empty).Items do yield it.Name
                | None -> () ]
        |> Set.ofList

    let private finishApply (metaOrder: string[]) (subst0: Map<Name, Ty>) (used: (int * Ty) list) (stepAt: int -> Ty option)
                            (conclusionTys0: Ty list) (usedNames0: Set<string>) (introduces: bool) : ApplyOutcome =
        // ---- diagrams in the conclusions are instantiated by the match:
        // M(H) — every label rewritten through the substitution, existential
        // witnesses (and, for a Construction rule, every still-unbound label)
        // given FRESH names. The result is a new sketch the proof adopts, and
        // the conclusion refers to it — a bare "[H]" is never emitted with
        // labels the match changed.
        let mutable subst = subst0
        let mutable usedNames = usedNames0
        let newSketches = ResizeArray<string * string>()
        let renamed = System.Collections.Generic.Dictionary<string, string>()
        let mutable sketchErr : string option = None
        let freshName (baseName: string) =
            let mutable k = 0
            let mutable cand = baseName
            while usedNames.Contains cand do
                k <- k + 1
                cand <- baseName + QuiverImport.SubscriptDigits k
            usedNames <- Set.add cand usedNames
            cand
        if Ty.diagramSemantics then
            for c in conclusionTys0 do
                for nm in sketchesIn c |> List.distinct do
                    if sketchErr.IsNone && not (renamed.ContainsKey nm) then
                        match Map.tryFind nm Ty.userSketches, QuiverImport.LookupSketch nm with
                        | Some json, Some g ->
                            let el = QuiverImport.elaborate g "" QuiverImport.ArrowStyle Set.empty
                            let rename = ResizeArray<string * string>()
                            let mutable changed = false
                            // a diagram with witnesses is ALWAYS copied into the
                            // proof: the dashed arrow belongs to the proof (it turns
                            // solid on use), never to the rule's own picture
                            let mutable hasWitness = false
                            let seenLabel = System.Collections.Generic.HashSet<string>()
                            for it in el.Items do
                                if sketchErr.IsNone && not it.Generated && it.Quant <> QuiverImport.Bound && seenLabel.Add it.Name then
                                    match Map.tryFind it.Name subst with
                                    | Some rep ->
                                        match Ty.simpleName rep with
                                        | Some target ->
                                            if target <> it.Name then changed <- true
                                            rename.Add (it.Name, target)
                                        | None ->
                                            sketchErr <- Some $"conclusion diagram [{nm}] cannot be relabelled: '{it.Name}' is bound to the compound term '{Ty.format rep}' — bind it to a plain name instead"
                                    | None ->
                                        let existential =
                                            it.Quant = QuiverImport.Existential || it.Quant = QuiverImport.UniqueExistential
                                        if existential then hasWitness <- true
                                        if existential || introduces then
                                            let f = freshName it.Name
                                            if f <> it.Name then changed <- true
                                            rename.Add (it.Name, f)
                                            subst <- Map.add it.Name (Ty.Var f) subst
                                        else rename.Add (it.Name, it.Name)
                            if sketchErr.IsNone && (changed || hasWitness) then
                                let newName =
                                    let mutable k = 1
                                    let mutable cand = nm + QuiverImport.SubscriptDigits k
                                    while Map.containsKey cand Ty.userSketches || newSketches |> Seq.exists (fun (x, _) -> x = cand) do
                                        k <- k + 1
                                        cand <- nm + QuiverImport.SubscriptDigits k
                                    cand
                                newSketches.Add (newName, QuiverImport.Relabel json (rename.ToArray()))
                                renamed.[nm] <- newName
                        | _ -> ()
        match sketchErr with
        | Some e -> failApply e
        | None ->
        let conclusionTys =
            conclusionTys0 |> List.map (mapSketches (fun nm -> match renamed.TryGetValue nm with | true, r -> r | _ -> nm))
        // eigenvariable condition: a witness-introducing (Construction) rule's
        // conclusion-only metavariables are FRESH names — refuse to emit a
        // witness whose name already occurs in the proof, else two
        // applications would silently conflate their witnesses
        let unbound = metaOrder |> Array.filter (fun v -> not (Map.containsKey v subst))
        let clash = if introduces then unbound |> Array.tryFind usedNames.Contains else None
        match clash with
        | Some v ->
            failApply $"this rule introduces '{v}' as a fresh name, but '{v}' already occurs in the proof — use Manual substitution to pick an unused name for it"
        | None ->
        // composite-corruption guard: a conclusion composite whose unit is
        // bound to a compound term cannot be rebuilt as a name — refuse
        // rather than emit a half-substituted equation
        let mixed =
            conclusionTys
            |> List.tryPick (fun c ->
                Canonical.freeVariables c
                |> Array.tryPick (fun v ->
                    let units = Ty.nameUnits v
                    if units.Length < 2 then None
                    else
                        units
                        |> List.tryPick (fun u ->
                            match Map.tryFind u subst with
                            | Some rep when (Ty.simpleName rep).IsNone -> Some (v, u, Ty.format rep)
                            | _ -> None)))
        match mixed with
        | Some (v, u, repText) ->
            failApply $"conclusion composite '{v}' cannot be instantiated: its component '{u}' is bound to the compound term '{repText}' — bind it to a plain name instead"
        | None ->
        let mutable err = None
        let outs =
            [| for c in conclusionTys do
                if err.IsNone then
                    match instantiateTy subst c with
                    | Ok (text, _) -> yield text
                    | Error e -> err <- Some $"conclusion '{Ty.format c}': {e}" |]
        match err with
        | Some e -> failApply e
        | None ->
            let bindings =
                [| for v in metaOrder do
                    match Map.tryFind v subst with
                    | Some t -> yield v, Ty.format t
                    | None -> () |]
            let usedSteps = used |> List.map fst |> Array.ofList
            let usedVia =
                used
                |> List.map (fun (i, t) ->
                    match stepAt i with
                    | Some st when Canonical.sameTy st t -> ""
                    | _ -> Ty.format t)
                |> Array.ofList
            // the proof adopts the new diagrams; make them resolvable at once
            lock Ty.sketchLock (fun () ->
                for (nm, json) in newSketches do Ty.userSketches <- Map.add nm json Ty.userSketches)
            { Ok = true; Bindings = bindings; UsedSteps = usedSteps; NewConclusions = outs
              UsedVia = usedVia; NewSketches = newSketches.ToArray(); Error = "" }

    /// A premise "[D] has exact rows" is matched through what it MEANS: the
    /// exact(f, g) judgments of the rule's own picture (schematic in its
    /// labels), each looked up in the pool like any other premise. Matching
    /// the picture by shape would not do — an embedding need not carry rows
    /// to rows. A picture that asserts nothing keeps the premise literal.
    let private expandExactPremise (p: Ty) : Ty list =
        if not Ty.diagramSemantics then [ p ] else
        let ctx, goal = match p with Ty.Entails (c, g) -> c, g | t -> [], t
        let wrap (t: Ty) = if List.isEmpty ctx then t else Ty.Entails (ctx, t)
        match goal with
        | Ty.Exact (Ty.Sketch nm, ax) ->
            match exactJudgments wrap nm ax with
            | [] -> [ p ]
            | facts -> facts
        | Ty.InCategory (Ty.Exact (Ty.Sketch nm, ax), c)
        | Ty.Exact (Ty.InCategory (Ty.Sketch nm, c), ax) ->
            wrap (Ty.InCategory (Ty.Sketch nm, c)) :: exactJudgments wrap nm ax
        | _ -> [ p ]

    /// How many pool entries each premise becomes after unfolding (a
    /// "[D] has exact rows" premise is several): lets a caller walk UsedSteps
    /// premise by premise.
    let PremiseExpansion (premises: string[]) : int[] =
        [| for p in premises do
            match Parser.parse p with
            | Ok t -> yield (expandExactPremise t).Length
            | Error _ -> yield 1 |]

    /// Automatic rule application: pattern-match the premises against the
    /// proven steps (backtracking, first solution) and instantiate the
    /// conclusions. UsedSteps holds, per premise, the index of the matched step.
    let TryApplyRule (premises: string[]) (conclusions: string[]) (steps: string[]) (introduces: bool) : ApplyOutcome =
        let parseAll (label: string) (xs: string[]) =
            let mutable err = None
            let ts =
                [ for i in 0 .. xs.Length - 1 do
                    if err.IsNone then
                        match Parser.parse xs.[i] with
                        | Ok t -> yield t
                        | Error e -> err <- Some $"{label} {i + 1}: {e}" ]
            match err with Some e -> Error e | None -> Ok ts
        match parseAll "premise" premises, parseAll "conclusion" conclusions, parseAll "step" steps with
        | Error e, _, _ | _, Error e, _ | _, _, Error e -> failApply e
        | Ok prems, Ok concls, Ok stepTys ->
            let prems = prems |> List.collect expandExactPremise
            let metaOrder = MetaVars (Array.append premises conclusions)
            let metas = Set.ofArray metaOrder
            let usedNames = namesInUse stepTys
            let stepArr = Array.ofList stepTys
            let pool = poolOf (List.indexed stepTys)
            match Match.applyRule metas prems pool with
            | None -> failApply "no consistent match of the premises against the proven steps"
            | Some (subst, used) ->
                finishApply metaOrder subst used (fun i -> if i < stepArr.Length then Some stepArr.[i] else None) concls usedNames introduces

    /// Manual fallback: the user supplies the substitution; each instantiated
    /// premise must equal (name-preservingly) some proven step.
    let TryApplyRuleManual (premises: string[]) (conclusions: string[]) (steps: string[]) (bindings: (string * string)[]) (introduces: bool) : ApplyOutcome =
        match parseBindings bindings with
        | Error e -> failApply e
        | Ok m ->
            let stepTys =
                steps |> Array.map (fun s -> match Parser.parse s with Ok t -> Some t | Error _ -> None)
            let usedNames = namesInUse [ for t in stepTys do match t with Some t -> yield t | None -> () ]
            let pool =
                poolOf [ for i in 0 .. stepTys.Length - 1 do match stepTys.[i] with Some t -> yield i, t | None -> () ]
            // guard the rule's OWN names: a metavariable that occurs in no
            // premise is part of what the rule concludes, not a slot the user
            // may aim at existing objects — a Construction rule may name its
            // fresh witness, every other rule's conclusion names are literal
            let premMetas = Set.ofArray (MetaVars premises)
            let allMetas = Set.ofArray (MetaVars (Array.append premises conclusions))
            let mutable gerr = None
            for KeyValue (name, rep) in m do
                if gerr.IsNone && not (Set.contains name allMetas) then
                    gerr <- Some $"'{name}' is not a metavariable of this rule"
                elif gerr.IsNone && not (Set.contains name premMetas) then
                    if not introduces then
                        gerr <- Some $"'{name}' occurs only in the rule's conclusions — it denotes the literal name and cannot be instantiated manually"
                    else
                        match Ty.simpleName rep with
                        | Some n when n.Length > 0 && System.Char.IsLetter n.[0] && not (usedNames.Contains n) -> ()
                        | _ -> gerr <- Some $"'{name}' is introduced by this rule as a fresh witness — bind it to a new, unused name (not '{Ty.format rep}')"
            if gerr.IsSome then failApply gerr.Value else
            // the user's bindings SEED the ordinary engine: the matcher fills
            // in whatever they left open, diagrams still match by shape (a
            // rule's [Four] against the proof's renamed ladder), and the
            // premise search backtracks as in automatic mode
            let mutable err = None
            let prems =
                [ for i in 0 .. premises.Length - 1 do
                    if err.IsNone then
                        match Parser.parse premises.[i] with
                        | Ok p -> yield! expandExactPremise p
                        | Error e -> err <- Some $"premise {i + 1}: {e}" ]
            let concls =
                [ for i in 0 .. conclusions.Length - 1 do
                    if err.IsNone then
                        match Parser.parse conclusions.[i] with
                        | Ok t -> yield t
                        | Error e -> err <- Some $"conclusion {i + 1}: {e}" ]
            match err with
            | Some e -> failApply e
            | None ->
                let metaOrder = MetaVars (Array.append premises conclusions)
                let metas = Set.ofArray metaOrder
                // a binding must respect what the premises say: seed only the
                // rule's metavariables, then let the engine check consistency
                let seed = m |> Map.filter (fun k _ -> Set.contains k metas)
                match Match.applyRuleFrom seed metas prems pool with
                | None ->
                    let shown = seed |> Map.toList |> List.map (fun (k, v) -> $"{k} := {Ty.format v}") |> String.concat ", "
                    failApply $"no proven step (nor diagram-supplied judgment) matches the premises under {shown}"
                | Some (subst, used) ->
                    finishApply metaOrder subst used (fun i -> if i < stepTys.Length then stepTys.[i] else None) concls usedNames introduces

    /// Parse the input, format it, canonicalize it, and render the
    /// canonical-name -> original-name mapping — everything as plain strings.
    let Run (input: string) : RunResult =
        match Parser.parse input with
        | Error e ->
            { Success = false; Formatted = ""; Canonical = ""; VariableMap = ""; Inference = ""; Elaborated = ""; Error = e }
        | Ok ty ->
            // elaborate first (A^2 ⇒ A × A), so that A^2 and A × A share a
            // canonical form — they are definitionally equal
            let formatted = Ty.format ty
            let elaborated = Ty.expandPowers ty
            let elaboratedText = Ty.format elaborated
            let c = Canonical.canonicalize elaborated
            let mapLines =
                [ for canon, orig in c.BoundVars -> $"{canon} ← {orig}  (bound)"
                  for KeyValue (canon, orig) in c.FreeVars -> $"{canon} ← {orig}  (free)" ]
                |> String.concat "\n"
            let inferLines =
                Infer.infer ty
                |> List.map (fun j ->
                    let rel = if j.Type.StartsWith "⊆" then j.Type else ": " + j.Type
                    $"{j.Subject} {rel}  — {j.Note}")
                |> String.concat "\n"
            { Success = true
              Formatted = formatted
              Canonical = Ty.format c.Ty
              VariableMap = mapLines
              Inference = inferLines
              Elaborated = (if elaboratedText = formatted then "" else elaboratedText)
              Error = "" }

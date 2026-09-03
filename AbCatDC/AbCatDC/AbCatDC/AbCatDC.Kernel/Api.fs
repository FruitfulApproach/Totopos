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

    /// Free (never binder-bound) variables of an expression, in order.
    /// Empty on parse failure — callers should surface parse errors separately.
    let FreeVars (input: string) : string[] =
        match Parser.parse input with
        | Error _ -> [||]
        | Ok t -> Canonical.freeVariables t

    /// The name a judgment DECLARES: the subject of a typing judgment
    /// ("A : C" declares A; "Γ ⊢ m : A → B" declares m). "" when the
    /// expression is not a typing judgment (an equation declares nothing).
    let DeclaredName (input: string) : string =
        let rec subject t =
            match t with
            | Ty.HasType (Ty.Var n, _) -> n
            | Ty.Entails (_, goal) -> subject goal
            | _ -> ""
        match Parser.parse input with
        | Error _ -> ""
        | Ok t -> subject t

    /// Every name an expression mentions: its free variables plus, for a
    /// juxtaposition composite, the units it composes (mg mentions m and g).
    /// Used to order premises so a name is declared before it is used.
    let MentionedNames (input: string) : string[] =
        let seen = System.Collections.Generic.HashSet<string>()
        [| for v in FreeVars input do
            if seen.Add v then yield v
            if Ty.isCompositeName v then
                for u in Ty.nameUnits v do
                    if seen.Add u then yield u |]

    /// The metavariables of a rule: ordered union of free variables over its
    /// expressions, minus constants, context metavariables, and "Type".
    let MetaVars (exprs: string[]) : string[] =
        let seen = System.Collections.Generic.HashSet<string>()
        let raw =
            [| for e in exprs do
                for v in FreeVars e do
                    if not (Set.contains v Ty.constantAtoms)
                       && not (Set.contains v Ty.contextVars)
                       && v <> "Type"
                       && seen.Add v then
                        yield v |]
        // a composite like gf (or i₁p₁) is determined by its units when those
        // are metavariables themselves — don't offer it as its own slot.
        // Digit-led units (the 0 in F0) are rigid parts, never slots.
        let atomic = raw |> Array.filter (fun v -> not (Ty.isCompositeName v)) |> Set.ofArray
        raw
        |> Array.filter (fun v ->
            not (Ty.isCompositeName v)
            || not (Ty.nameUnits v
                    |> List.forall (fun u -> Set.contains u atomic || System.Char.IsDigit u.[0])))

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
                // blank or identity binding = leave the variable alone
                if r <> "" && r <> name then
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
        { Ok = false; Bindings = [||]; UsedSteps = [||]; NewConclusions = [||]; Error = e }

    let private finishApply (metaOrder: string[]) (subst: Map<Name, Ty>) (used: int list)
                            (conclusionTys: Ty list) (usedNames: Set<string>) (introduces: bool) : ApplyOutcome =
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
            { Ok = true; Bindings = bindings; UsedSteps = Array.ofList used; NewConclusions = outs; Error = "" }

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
            let metaOrder = MetaVars (Array.append premises conclusions)
            let metas = Set.ofArray metaOrder
            let usedNames =
                stepTys |> List.collect (Canonical.freeVariables >> List.ofArray) |> Set.ofList
            match Match.applyRule metas prems (Array.ofList stepTys) with
            | None -> failApply "no consistent match of the premises against the proven steps"
            | Some (subst, used) -> finishApply metaOrder subst used concls usedNames introduces

    /// Manual fallback: the user supplies the substitution; each instantiated
    /// premise must equal (name-preservingly) some proven step.
    let TryApplyRuleManual (premises: string[]) (conclusions: string[]) (steps: string[]) (bindings: (string * string)[]) (introduces: bool) : ApplyOutcome =
        match parseBindings bindings with
        | Error e -> failApply e
        | Ok m ->
            let stepTys =
                steps |> Array.map (fun s -> match Parser.parse s with Ok t -> Some t | Error _ -> None)
            let usedNames =
                stepTys
                |> Array.collect (function Some t -> Canonical.freeVariables t | None -> [||])
                |> Set.ofArray
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
            let mutable err = None
            let mutable used = []
            for i in 0 .. premises.Length - 1 do
                if err.IsNone then
                    match Parser.parse premises.[i] with
                    | Error e -> err <- Some $"premise {i + 1}: {e}"
                    | Ok p ->
                        let inst = Ty.substTyVars m p
                        let hit =
                            stepTys
                            |> Array.tryFindIndex (function
                                | Some s -> Canonical.sameTy s inst
                                | None -> false)
                        match hit with
                        | Some idx -> used <- idx :: used
                        | None -> err <- Some $"premise {i + 1} ({Ty.format inst}) is not among the proven steps"
            match err with
            | Some e -> failApply e
            | None ->
                let mutable cerr = None
                let concls =
                    [ for i in 0 .. conclusions.Length - 1 do
                        if cerr.IsNone then
                            match Parser.parse conclusions.[i] with
                            | Ok t -> yield t
                            | Error e -> cerr <- Some $"conclusion {i + 1}: {e}" ]
                match cerr with
                | Some e -> failApply e
                | None ->
                    let metaOrder = MetaVars (Array.append premises conclusions)
                    finishApply metaOrder m (List.rev used) concls usedNames introduces

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

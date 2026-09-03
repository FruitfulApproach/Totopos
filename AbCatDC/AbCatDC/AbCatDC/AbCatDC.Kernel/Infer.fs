module AbCatDC.Kernel.Infer

open AbCatDC.Kernel

/// One inferred typing judgment for a leaf of the expression.
type Judgment =
    { Subject: string
      Type: string
      Note: string }

let private atomDescriptions =
    dict [ "ℕ", "the natural numbers"
           "ℤ", "the integers"
           "ℚ", "the rationals"
           "ℝ", "the reals"
           "ℂ", "the complex numbers"
           "Mono", "the class of monomorphisms — m : Mono is the judgment 'm is monic'"
           "Epi", "the class of epimorphisms — e : Epi is the judgment 'e is epic'"
           "Ker", "kernel marker — k : Ker f is the judgment 'k is a kernel of f'"
           "Coker", "cokernel marker — c : Coker f is the judgment 'c is a cokernel of f'"
           "Functor", "the class of functors — F : Functor is the judgment 'F is functorial'"
           "Additive", "the class of additive functors — F : Additive says F acts on hom-groups as a homomorphism" ]

/// Infer typings for the leaves of a type expression.
///
/// The rules:
///  - An integer literal that is not a registered symbol is a *constant* of
///    type ℕ; computationally it lives inside ℤ and is equivalent to its value
///    (3 : ℕ, computes as 3 ∈ ℤ).
///  - Well-known constant atoms (ℕ, ℤ, …) are type constants.
///  - A variable bound by ∀/∃/μ is a type variable; one bound by a dependent
///    or familial binder has the binder's domain as its type.
///  - An UNDECLARED symbol defaults to something integer-related:
///      · lowercase (a)  ⇒  a : ℕ (an integer, computing in ℤ)
///      · uppercase (A)  ⇒  A ⊆ ℤ (a set of integers)
///      · juxtaposition (AB)  ⇒  the elementwise product {ab : a ∈ A, b ∈ B}
///      · A^n  ⇒  the diagonal power A × ⋯ × A ⊆ ℤ^n; a^n computes in ℤ.
let infer (ty: Ty) : Judgment list =
    let acc = ResizeArray<Judgment>()
    let seen = System.Collections.Generic.HashSet<string>()
    let add subject typ note =
        if seen.Add subject then acc.Add { Subject = subject; Type = typ; Note = note }

    // Greek letters are schematic metavariables (rule templates range over all
    // types); Latin letters get the ℤ-related defaults.
    let isGreekChar (c: char) = (c >= 'α' && c <= 'ω') || (c >= 'Α' && c <= 'Ω')

    // default typing for an undeclared symbol
    let rec defaultVar (v: string) =
        if v.Length > 0 && v |> Seq.forall isGreekChar then
            add v "Type" "schematic metavariable — ranges over arbitrary types; instantiate uniformly (implicitly ∀-quantified at the meta level)"
        elif v.Length >= 2 && v |> Seq.forall System.Char.IsLetter && not (Ty.isCompositeName v) then
            // a multi-letter WORD (monomorphism, BigCat) — never a composition
            add v (if System.Char.IsUpper v.[0] then "Type" else "Term")
                "undeclared NAME — read as a single identifier, and schematic: as a rule it matches ANY expression. To make it a fixed constant, add it to the system's 'Rigid constants' box (Rules page)."
        elif v.Length >= 2 && v |> Seq.forall System.Char.IsLetter then
            // juxtaposition: AB is the elementwise product {ab : a ∈ A, b ∈ B}
            let parts = [ for ch in v -> string ch ]
            let dup = (parts |> List.distinct |> List.length) < parts.Length
            let pieces =
                parts
                |> List.mapi (fun i p ->
                    if System.Char.IsUpper p.[0] then
                        let e = if dup then $"{p.ToLowerInvariant()}{i + 1}" else p.ToLowerInvariant()
                        e, Some $"{e} ∈ {p}"
                    else p, None)
            let isSet = v |> Seq.exists System.Char.IsUpper
            if isSet then
                let product = pieces |> List.map fst |> String.concat ""
                let clauses = pieces |> List.choose snd |> String.concat ", "
                add v "⊆ ℤ" $"undeclared — juxtaposition, the elementwise product {{{product} : {clauses}}} (if this is one name, declare it in 'Rigid constants')"
            else
                let prod = String.concat "·" parts
                add v "ℕ" $"undeclared — juxtaposition, the integer product {prod} in ℤ (if this is one name, declare it in 'Rigid constants')"
            parts |> List.iter defaultVar
        elif v.Length > 0 && System.Char.IsUpper v.[0] then
            add v "⊆ ℤ" "undeclared — defaulted to a set of integers"
        else
            add v "ℕ" "undeclared — defaulted to an integer, computing in ℤ"

    let rec go (env: Map<Name, string>) t =
        match t with
        | Ty.Lit n ->
            add (string n) "ℕ" $"constant — computes inside ℤ, equivalent to the value {n}"
        | Ty.Atom a when a.Length > 0 && System.Char.IsDigit a.[0] ->
            add a "Mor" "identity notation — 1ₓ names the identity morphism on the object X (rigid, never a metavariable)"
        | Ty.Atom a when a.Contains "_" ->
            add a "Type" "user-defined rigid tag — a constant, never a metavariable; following atoms are its arguments (Mono_C (A → B))"
        | Ty.Atom a when Set.contains a Ty.userConstants ->
            add a "Type" "declared rigid constant of this system — never a metavariable; function-like (Mod R applies it to R)"
        | Ty.Atom a ->
            let note = if atomDescriptions.ContainsKey a then atomDescriptions.[a] else "type constant"
            add a "Type" note
        | Ty.Var v ->
            match Map.tryFind v env with
            | Some declared -> add v declared "bound variable"
            | None ->
                // a composite whose every unit is a declared arrow is a
                // COMPOSITION, not an integer product: mg is m∘g, and its type
                // is read off the ends of the chain (rightmost applied first)
                let units = if Ty.isCompositeName v then Ty.nameUnits v else []
                let arrows =
                    units |> List.map (fun u ->
                        match Map.tryFind u env with
                        | Some t when (t: string).Contains "→" ->
                            match t.Split([| " → " |], System.StringSplitOptions.None) with
                            | [| d; c |] -> Some (u, d, c)
                            | _ -> None
                        | _ -> None)
                if not units.IsEmpty && arrows |> List.forall Option.isSome then
                    let parts = arrows |> List.map Option.get
                    let (_, _, cod) = List.head parts        // leftmost applies last
                    let (_, dom, _) = List.last parts        // rightmost applies first
                    let chain = parts |> List.map (fun (u, _, _) -> u) |> String.concat "∘"
                    add v $"{dom} → {cod}" $"composition — juxtaposition is composition, so {v} is {chain} (rightmost applied first)"
                else defaultVar v
        | Ty.Sketch n ->
            add $"[{n}]" "Diagram" "a named quiver sketch — its identity is the diagram's canonical form"
        | Ty.Commutes b ->
            go env b
            add (Ty.format (Ty.Commutes b)) "Prop"
                "commutativity assertion: every pair of parallel composites of arrows in the diagram is equal"
        | Ty.InCategory (d, c) ->
            go env c
            go env d
            add (Ty.format (Ty.InCategory (d, c)))
                "Ctx"
                "diagram-in-category: shorthand for the group of judgments declaring each object (X : C) and each morphism (f : X → Y) of the sketch — expandable to text with ⇄"
        | Ty.Eq (a, b) ->
            go env a
            go env b
            add (Ty.format (Ty.Eq (a, b))) "Prop"
                "equation: both sides denote the same element/morphism (symmetric — canonicalization may orient it)"
        | Ty.HasType (subj, t) ->
            go env t
            (match subj with
             | Ty.Var m when not (Map.containsKey m env) ->
                 add m (Ty.format t) "term — typed by this judgment"
             | _ -> go env subj)
            add (Ty.format (Ty.HasType (subj, t))) "Judgment"
                "typing judgment: the term on the left inhabits the type on the right"
        | Ty.Entails (ctx, goal) ->
            let mutable env2 = env
            for it in ctx do
                match it with
                | CtxVar n ->
                    add n "Ctx" "context metavariable — stands for a whole list of hypotheses"
                | Anon t -> go env2 t
                | Hyp (x, t) ->
                    go env2 t
                    add x (Ty.format t) "hypothesis — assumed in the context, in scope to the right of it"
                    env2 <- Map.add x (Ty.format t) env2
            go env2 goal
            add (Ty.format (Ty.Entails (ctx, goal))) "Judgment"
                "entailment (⊢): the goal on the right is derivable from the hypotheses on the left"
        | Ty.Power (b, n) ->
            go env b
            let bs = Ty.format b
            let setLike =
                match b with
                | Ty.Var v -> v.Length > 0 && System.Char.IsUpper v.[0] && not (Map.containsKey v env)
                | Ty.Atom _ -> true
                | _ -> true
            if setLike then
                add $"{bs}^{n}" $"⊆ ℤ^{n}"
                    $"diagonal power — {bs}^{n} ≡ {bs} × ⋯ × {bs} = {{(x1, …, x{n}) : xi ∈ {bs}}}"
            else
                add $"{bs}^{n}" "ℕ" $"integer power — computes in ℤ as the product of {n} copies of {bs}"
        | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
        | Ty.Intersection (a, b) | Ty.Union (a, b) ->
            go env a
            go env b
        | Ty.Record ms -> ms |> List.iter (fun (_, mt) -> go env mt)
        | Ty.Polymorphic (a, body) ->
            add a "Type" "universally quantified type variable (∀)"
            go (Map.add a "Type" env) body
        | Ty.Existential (a, body) ->
            add a "Type" "existentially quantified type variable (∃)"
            go (Map.add a "Type" env) body
        | Ty.Recursive (a, body) ->
            add a "Type" "recursive type variable (μ)"
            go (Map.add a "Type" env) body
        | Ty.DependentFunction (x, dom, cod) ->
            go env dom
            let d = Ty.format dom
            add x d "bound by the dependent function (x : σ) → τ"
            go (Map.add x d env) cod
        | Ty.DependentPair (x, dom, snd) ->
            go env dom
            let d = Ty.format dom
            add x d "bound by the dependent pair (x : σ) × τ"
            go (Map.add x d env) snd
        | Ty.DependentIntersection (x, dom, snd) ->
            go env dom
            let d = Ty.format dom
            add x d "bound by the dependent intersection (x : σ) ∩ τ"
            go (Map.add x d env) snd
        | Ty.FamilialIntersection (x, dom, body) ->
            go env dom
            let d = Ty.format dom
            add x d "indexes the familial intersection ⋂(x:σ)"
            go (Map.add x d env) body
        | Ty.FamilialUnion (x, dom, body) ->
            go env dom
            let d = Ty.format dom
            add x d "indexes the familial union ⋃(x:σ)"
            go (Map.add x d env) body
    go Map.empty ty
    List.ofSeq acc

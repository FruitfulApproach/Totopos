module AbCatDC.Kernel.Match

open AbCatDC.Kernel

/// Pattern matching of rule premises (schematic in their metavariables)
/// against proven steps, and backtracking assignment of steps to premises.
///
/// v1 semantics (deliberate):
///  - structural matching up to α (binder names correspond, including sequent
///    hypothesis names);
///  - a context metavariable Γ matches only the literal token Γ; contexts are
///    matched item-by-item in order (no weakening, no permutation);
///  - Eq is tried in both orientations;
///  - a metavariable may not capture a bound variable of the surrounding
///    binders (scope check).

/// The names a term mentions outside its own internal binders.
let private mentionedVars (t: Ty) : Set<Name> =
    let acc = System.Collections.Generic.HashSet<Name>()
    let rec go (shadow: Set<Name>) t =
        match t with
        | Ty.Var v -> if not (shadow.Contains v) then acc.Add v |> ignore
        | Ty.Atom _ | Ty.Lit _ | Ty.Sketch _ -> ()
        | Ty.Power (b, _) | Ty.Commutes b -> go shadow b
        | Ty.InCategory (a, b) | Ty.HasType (a, b) | Ty.Eq (a, b)
        | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
        | Ty.Intersection (a, b) | Ty.Union (a, b) -> go shadow a; go shadow b
        | Ty.Record ms -> ms |> List.iter (fun (_, mt) -> go shadow mt)
        | Ty.Polymorphic (a, b) | Ty.Existential (a, b) | Ty.Recursive (a, b) ->
            go (Set.add a shadow) b
        | Ty.DependentFunction (x, a, b) | Ty.DependentPair (x, a, b)
        | Ty.DependentIntersection (x, a, b)
        | Ty.FamilialIntersection (x, a, b) | Ty.FamilialUnion (x, a, b) ->
            go shadow a
            go (Set.add x shadow) b
        | Ty.Entails (ctx, goal) ->
            let mutable sh = shadow
            for it in ctx do
                match it with
                | CtxVar _ -> ()
                | Hyp (x, ht) -> go sh ht; sh <- Set.add x sh
                | Anon at -> go sh at
            go sh goal
    go Set.empty t
    Set.ofSeq acc

/// One matching attempt: pattern (schematic in `metas`) against term, seeded
/// with an existing substitution. Returns the extended substitution.
let matchPattern (metas: Set<Name>) (initial: Map<Name, Ty>) (pattern: Ty) (term: Ty) : Map<Name, Ty> option =
    // p2t / t2p: bound-name correspondence between the two sides
    let rec go (p2t: Map<Name, Name>) (t2p: Map<Name, Name>) (subst: Map<Name, Ty>) p t : Map<Name, Ty> option =
        match p, t with
        | Ty.Var v, _ when metas.Contains v && not (p2t.ContainsKey v) ->
            // scope check: the bound names of the surrounding term-side
            // binders may not leak into a metavariable binding
            let termBound = t2p |> Map.toSeq |> Seq.map fst |> Set.ofSeq
            if not (Set.isEmpty (Set.intersect termBound (mentionedVars t))) then None
            else
                match Map.tryFind v subst with
                | Some prev -> if Canonical.sameTy prev t then Some subst else None
                | None -> Some (Map.add v t subst)
        | Ty.Var pv, Ty.Var tv when
                not (metas.Contains pv) && not (p2t.ContainsKey pv)
                && (let pu = Ty.nameUnits pv
                    let tu = Ty.nameUnits tv
                    Ty.isCompositeName pv && Ty.isCompositeName tv && pu.Length = tu.Length
                    && pu |> List.forall metas.Contains
                    && tu |> List.forall (fun u -> System.Char.IsLetter u.[0])) ->
            // composite of metavariable units (e.g. mu read m∘u, i₁p₁ read
            // i₁∘p₁): decompose unit-wise so each component binds consistently
            // with the other premises — an opaque match of the literal name
            // could mix a bound m with an unrelated morphism named m.
            // v1: units must correspond one-to-one with the term's units;
            // anything richer falls through to the manual path.
            (Some subst, List.zip (Ty.nameUnits pv) (Ty.nameUnits tv))
            ||> List.fold (fun acc (pc, tc) ->
                acc |> Option.bind (fun s -> go p2t t2p s (Ty.Var pc) (Ty.Var tc)))
        | Ty.Var pv, Ty.Var tv ->
            (match Map.tryFind pv p2t with
             | Some corr -> if corr = tv then Some subst else None
             | None -> if pv = tv && not (t2p.ContainsKey tv) then Some subst else None)
        | Ty.Atom a, Ty.Atom b when a = b -> Some subst
        | Ty.Lit a, Ty.Lit b when a = b -> Some subst
        | Ty.Sketch a, Ty.Sketch b when a = b -> Some subst
        | Ty.Power (a, n), Ty.Power (b, m) when n = m -> go p2t t2p subst a b
        | Ty.Commutes a, Ty.Commutes b -> go p2t t2p subst a b
        | Ty.InCategory (a, b), Ty.InCategory (c, d)
        | Ty.HasType (a, b), Ty.HasType (c, d)
        | Ty.Function (a, b), Ty.Function (c, d)
        | Ty.Product (a, b), Ty.Product (c, d)
        | Ty.Sum (a, b), Ty.Sum (c, d)
        | Ty.Intersection (a, b), Ty.Intersection (c, d)
        | Ty.Union (a, b), Ty.Union (c, d) ->
            go p2t t2p subst a c |> Option.bind (fun s -> go p2t t2p s b d)
        | Ty.Eq (a, b), Ty.Eq (c, d) ->
            // symmetric: try both orientations
            match go p2t t2p subst a c |> Option.bind (fun s -> go p2t t2p s b d) with
            | Some s -> Some s
            | None -> go p2t t2p subst a d |> Option.bind (fun s -> go p2t t2p s b c)
        | Ty.Record ms1, Ty.Record ms2 when List.length ms1 = List.length ms2 ->
            (Some subst, List.zip ms1 ms2)
            ||> List.fold (fun acc ((n1, t1), (n2, t2)) ->
                acc |> Option.bind (fun s -> if n1 = n2 then go p2t t2p s t1 t2 else None))
        | Ty.Polymorphic (a, b1), Ty.Polymorphic (c, b2)
        | Ty.Existential (a, b1), Ty.Existential (c, b2)
        | Ty.Recursive (a, b1), Ty.Recursive (c, b2) ->
            go (Map.add a c p2t) (Map.add c a t2p) subst b1 b2
        | Ty.DependentFunction (x, d1, c1), Ty.DependentFunction (y, d2, c2)
        | Ty.DependentPair (x, d1, c1), Ty.DependentPair (y, d2, c2)
        | Ty.DependentIntersection (x, d1, c1), Ty.DependentIntersection (y, d2, c2)
        | Ty.FamilialIntersection (x, d1, c1), Ty.FamilialIntersection (y, d2, c2)
        | Ty.FamilialUnion (x, d1, c1), Ty.FamilialUnion (y, d2, c2) ->
            go p2t t2p subst d1 d2
            |> Option.bind (fun s -> go (Map.add x y p2t) (Map.add y x t2p) s c1 c2)
        | Ty.Entails (ctx1, g1), Ty.Entails (ctx2, g2) when List.length ctx1 = List.length ctx2 ->
            let folded =
                (Some (p2t, t2p, subst), List.zip ctx1 ctx2)
                ||> List.fold (fun acc (i1, i2) ->
                    acc |> Option.bind (fun (p2t, t2p, s) ->
                        match i1, i2 with
                        | CtxVar a, CtxVar b when a = b -> Some (p2t, t2p, s)
                        | Hyp (x, t1), Hyp (y, t2) ->
                            go p2t t2p s t1 t2
                            |> Option.map (fun s' -> (Map.add x y p2t, Map.add y x t2p, s'))
                        | Anon t1, Anon t2 -> go p2t t2p s t1 t2 |> Option.map (fun s' -> (p2t, t2p, s'))
                        | _ -> None))
            folded |> Option.bind (fun (p2t, t2p, s) -> go p2t t2p s g1 g2)
        | _ -> None
    go Map.empty Map.empty initial pattern term

/// Backtracking assignment of proven steps to premises. Returns the combined
/// substitution and, for each premise in order, the index (into `steps`) of
/// the step it matched.
let applyRule (metas: Set<Name>) (premises: Ty list) (steps: Ty[]) : (Map<Name, Ty> * int list) option =
    let rec search subst used prems =
        match prems with
        | [] -> Some (subst, List.rev used)
        | p :: rest ->
            steps
            |> Seq.indexed
            |> Seq.tryPick (fun (i, s) ->
                match matchPattern metas subst p s with
                | Some subst' -> search subst' (i :: used) rest
                | None -> None)
    search Map.empty [] premises

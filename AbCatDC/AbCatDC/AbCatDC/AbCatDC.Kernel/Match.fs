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
        | Ty.Power (b, _) | Ty.Commutes b | Ty.Exact (b, _) -> go shadow b
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

/// Whether a term mentions a diagram anywhere.
let rec mentionsSketch (t: Ty) : bool =
    match t with
    | Ty.Sketch _ -> true
    | Ty.Atom _ | Ty.Var _ | Ty.Lit _ -> false
    | Ty.Power (b, _) | Ty.Commutes b | Ty.Exact (b, _) -> mentionsSketch b
    | Ty.InCategory (a, b) | Ty.HasType (a, b) | Ty.Eq (a, b)
    | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
    | Ty.Intersection (a, b) | Ty.Union (a, b) -> mentionsSketch a || mentionsSketch b
    | Ty.Record ms -> ms |> List.exists (fun (_, mt) -> mentionsSketch mt)
    | Ty.Polymorphic (_, b) | Ty.Existential (_, b) | Ty.Recursive (_, b) -> mentionsSketch b
    | Ty.DependentFunction (_, a, b) | Ty.DependentPair (_, a, b)
    | Ty.DependentIntersection (_, a, b)
    | Ty.FamilialIntersection (_, a, b) | Ty.FamilialUnion (_, a, b) -> mentionsSketch a || mentionsSketch b
    | Ty.Entails (ctx, goal) ->
        mentionsSketch goal
        || ctx |> List.exists (function CtxVar _ -> false | Hyp (_, h) -> mentionsSketch h | Anon a -> mentionsSketch a)

/// One matching attempt: pattern (schematic in `metas`) against term, seeded
/// with an existing substitution. Returns the extended substitution. A
/// picture in the pattern may embed into the term's picture in several
/// ways; `nth` selects which consistent embedding to take (0 = the first),
/// so a caller can enumerate them when a later premise rules the first out.
let matchPatternNth (nth: int) (metas: Set<Name>) (initial: Map<Name, Ty>) (pattern: Ty) (term: Ty) : Map<Name, Ty> option =
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
                    // each unit is either a metavariable to bind, or a name a
                    // surrounding binder owns (matched through p2t) — so
                    // "fx" inside ⋂(x:σ) really does tie f to the term's f
                    && pu |> List.forall (fun u -> metas.Contains u || p2t.ContainsKey u)
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
        | Ty.Sketch a, Ty.Sketch b ->
            // A diagram is matched by SHAPE: any embedding of the pattern's
            // diagram into the term's, with each pattern label bound (through
            // the ordinary metavariable/bound-name logic of `go`) to the
            // label it lands on — so "[Sq] commutes" fires on any commuting
            // square, and "[Four]" binds p, q, r, … for the conclusion. Names
            // the registry does not know fall back to the opaque token.
            if not Ty.diagramSemantics then (if a = b then Some subst else None)
            else
            match QuiverImport.LookupSketch a, QuiverImport.LookupSketch b with
            | Some ga, Some gb ->
                QuiverImport.Embeddings ga gb
                |> Seq.choose (fun pairs ->
                    pairs
                    |> List.fold (fun acc (pl, tl) ->
                        acc |> Option.bind (fun s -> go p2t t2p s (Ty.Var pl) (Ty.Var tl))) (Some subst))
                |> Seq.tryItem nth
            | _ -> if a = b then Some subst else None
        | Ty.Power (a, n), Ty.Power (b, m) when n = m -> go p2t t2p subst a b
        | Ty.Commutes a, Ty.Commutes b -> go p2t t2p subst a b
        | Ty.Exact (a, ax), Ty.Exact (b, bx) when ax = bx -> go p2t t2p subst a b
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

let matchPattern (metas: Set<Name>) (initial: Map<Name, Ty>) (pattern: Ty) (term: Ty) : Map<Name, Ty> option =
    matchPatternNth 0 metas initial pattern term

/// A coarse discriminator so a premise only tries pool entries of its own
/// shape: (is a sequent, kind of the goal). Kind 0 is a bare metavariable,
/// which may match any goal.
let shapeKey (t: Ty) : bool * int =
    let goal, entails = match t with Ty.Entails (_, g) -> g, true | g -> g, false
    let kind =
        match goal with
        | Ty.Var _ -> 0
        | Ty.HasType _ -> 1
        | Ty.Eq _ -> 2
        | Ty.Commutes _ -> 3
        | Ty.Exact _ -> 7
        | Ty.InCategory _ -> 4
        | Ty.Function _ -> 5
        | _ -> 6
    entails, kind

/// Backtracking assignment of pool entries to premises. The pool is every
/// proven step plus the judgments a diagram step contributes, each tagged
/// with the index of the step it came from. Returns the combined
/// substitution and, per premise in order, (step index, the entry matched).
let applyRuleFrom (initial: Map<Name, Ty>) (metas: Set<Name>) (premises: Ty list) (pool: (Ty * int)[]) : (Map<Name, Ty> * (int * Ty) list) option =
    let keyed = pool |> Array.map (fun (t, i) -> shapeKey t, t, i)
    let rec search subst used prems =
        match prems with
        | [] -> Some (subst, List.rev used)
        | p :: rest ->
            let (pe, pk) = shapeKey p
            // prefer a step no earlier premise used: two premises MAY share
            // a step, but "u : T → A, v : T → A" should not both land on the
            // same arrow when a second one is available
            let usedIdx = used |> List.map fst |> Set.ofList
            keyed
            |> Seq.filter (fun ((te, tk), _, _) -> te = pe && (pk = 0 || tk = pk))
            |> Seq.sortBy (fun (_, _, i) -> if usedIdx.Contains i then 1 else 0)
            |> Seq.tryPick (fun (_, t, i) ->
                // a picture may embed into the step's picture in several ways:
                // try each consistent embedding in turn, so a later premise
                // (an equation, a typing) can rule the first one out
                let attempts = if mentionsSketch p then Seq.initInfinite id else Seq.singleton 0
                attempts
                |> Seq.map (fun k -> matchPatternNth k metas subst p t)
                |> Seq.takeWhile Option.isSome
                |> Seq.tryPick (fun s ->
                    match s with
                    | Some subst' -> search subst' ((i, t) :: used) rest
                    | None -> None))
    search initial [] premises

let applyRule (metas: Set<Name>) (premises: Ty list) (pool: (Ty * int)[]) : (Map<Name, Ty> * (int * Ty) list) option =
    applyRuleFrom Map.empty metas premises pool

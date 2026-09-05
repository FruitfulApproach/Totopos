module AbCatDC.Kernel.Canonical

open AbCatDC.Kernel

/// The canonical type together with the mapping from every canonical variable
/// name back to the name used in the source expression.
type Canonicalized =
    { Ty: Ty
      /// Canonical bound name "@level" -> original binder name, in binder
      /// order. A list, not a map: de Bruijn levels repeat (two sibling
      /// binders at the same depth both canonicalize to the same "@n").
      BoundVars: (Name * Name) list
      /// Canonical free name "#index" -> original variable name.
      FreeVars: Map<Name, Name> }

let private levelName d : Name = sprintf "@%d" d
let private freeName i : Name = sprintf "#%d" i
let private isBoundName (n: Name) = n.StartsWith "@"

// ---- pass 1: rename bound variables to de Bruijn levels -------------------
// The outermost binder becomes @0, the next one in @1, and so on; occurrences
// refer to their binder's level. Free variables keep their names for now.

let private alphaRename (ty: Ty) : Ty * (Name * Name) list =
    let log = ResizeArray()
    let rec go env depth ty =
        let bind (orig: Name) =
            let n = levelName depth
            log.Add (n, orig)
            Map.add orig n env, n
        match ty with
        | Ty.Atom _ | Ty.Lit _ | Ty.Sketch _ -> ty
        | Ty.Var a -> (match Map.tryFind a env with Some b -> Ty.Var b | None -> ty)
        | Ty.Power (b, n) -> Ty.Power (go env depth b, n)
        | Ty.Commutes b -> Ty.Commutes (go env depth b)
        | Ty.Exact (b, ax) -> Ty.Exact (go env depth b, ax)
        | Ty.InCategory (d, c) -> Ty.InCategory (go env depth d, go env depth c)
        | Ty.HasType (subj, t) -> Ty.HasType (go env depth subj, go env depth t)
        | Ty.Eq (a, b) -> Ty.Eq (go env depth a, go env depth b)
        | Ty.Entails (ctx, goal) ->
            // hypotheses bind their names in later entries and the goal
            let mutable env2 = env
            let mutable d = depth
            let items =
                ctx |> List.map (fun it ->
                    match it with
                    | CtxVar n -> CtxVar n
                    | Anon t -> Anon (go env2 d t)
                    | Hyp (x, t) ->
                        let t' = go env2 d t
                        let nm = levelName d
                        log.Add (nm, x)
                        env2 <- Map.add x nm env2
                        d <- d + 1
                        Hyp (nm, t'))
            Ty.Entails (items, go env2 d goal)
        | Ty.Function (a, b) -> Ty.Function (go env depth a, go env depth b)
        | Ty.Product (a, b) -> Ty.Product (go env depth a, go env depth b)
        | Ty.Sum (a, b) -> Ty.Sum (go env depth a, go env depth b)
        | Ty.Intersection (a, b) -> Ty.Intersection (go env depth a, go env depth b)
        | Ty.Union (a, b) -> Ty.Union (go env depth a, go env depth b)
        | Ty.Record ms -> Ty.Record [ for n, t in ms -> n, go env depth t ]
        | Ty.Polymorphic (a, body) ->
            let env', n = bind a in Ty.Polymorphic (n, go env' (depth + 1) body)
        | Ty.Existential (a, body) ->
            let env', n = bind a in Ty.Existential (n, go env' (depth + 1) body)
        | Ty.Recursive (a, body) ->
            let env', n = bind a in Ty.Recursive (n, go env' (depth + 1) body)
        | Ty.DependentFunction (x, d, c) ->
            let env', n = bind x in Ty.DependentFunction (n, go env depth d, go env' (depth + 1) c)
        | Ty.DependentPair (x, a, b) ->
            let env', n = bind x in Ty.DependentPair (n, go env depth a, go env' (depth + 1) b)
        | Ty.DependentIntersection (x, a, b) ->
            let env', n = bind x in Ty.DependentIntersection (n, go env depth a, go env' (depth + 1) b)
        | Ty.FamilialIntersection (x, a, b) ->
            let env', n = bind x in Ty.FamilialIntersection (n, go env depth a, go env' (depth + 1) b)
        | Ty.FamilialUnion (x, a, b) ->
            let env', n = bind x in Ty.FamilialUnion (n, go env depth a, go env' (depth + 1) b)
    let t = go Map.empty 0 ty
    t, List.ofSeq log

// ---- helpers ---------------------------------------------------------------

let rec private mapVars (f: Name -> Name) ty =
    match ty with
    | Ty.Atom _ | Ty.Lit _ | Ty.Sketch _ -> ty
    | Ty.Var a -> Ty.Var (f a)
    | Ty.Power (b, n) -> Ty.Power (mapVars f b, n)
    | Ty.Commutes b -> Ty.Commutes (mapVars f b)
    | Ty.Exact (b, ax) -> Ty.Exact (mapVars f b, ax)
    | Ty.InCategory (d, c) -> Ty.InCategory (mapVars f d, mapVars f c)
    | Ty.HasType (subj, t) -> Ty.HasType (mapVars f subj, mapVars f t)
    | Ty.Eq (a, b) -> Ty.Eq (mapVars f a, mapVars f b)
    | Ty.Entails (ctx, goal) ->
        let item = function
            | CtxVar n -> CtxVar n
            | Hyp (x, t) -> Hyp (x, mapVars f t)
            | Anon t -> Anon (mapVars f t)
        Ty.Entails (List.map item ctx, mapVars f goal)
    | Ty.Function (a, b) -> Ty.Function (mapVars f a, mapVars f b)
    | Ty.Product (a, b) -> Ty.Product (mapVars f a, mapVars f b)
    | Ty.Sum (a, b) -> Ty.Sum (mapVars f a, mapVars f b)
    | Ty.Intersection (a, b) -> Ty.Intersection (mapVars f a, mapVars f b)
    | Ty.Union (a, b) -> Ty.Union (mapVars f a, mapVars f b)
    | Ty.Record ms -> Ty.Record [ for n, t in ms -> n, mapVars f t ]
    | Ty.Polymorphic (a, b) -> Ty.Polymorphic (a, mapVars f b)
    | Ty.Existential (a, b) -> Ty.Existential (a, mapVars f b)
    | Ty.Recursive (a, b) -> Ty.Recursive (a, mapVars f b)
    | Ty.DependentFunction (x, a, b) -> Ty.DependentFunction (x, mapVars f a, mapVars f b)
    | Ty.DependentPair (x, a, b) -> Ty.DependentPair (x, mapVars f a, mapVars f b)
    | Ty.DependentIntersection (x, a, b) -> Ty.DependentIntersection (x, mapVars f a, mapVars f b)
    | Ty.FamilialIntersection (x, a, b) -> Ty.FamilialIntersection (x, mapVars f a, mapVars f b)
    | Ty.FamilialUnion (x, a, b) -> Ty.FamilialUnion (x, mapVars f a, mapVars f b)

/// Free variables hidden behind a placeholder, so that sorting the operands of
/// ∩/∪ does not depend on the (not yet assigned) free-variable numbering.
let private masked ty =
    mapVars (fun a -> if isBoundName a then a else "•") ty

/// Distinct free variables in left-to-right traversal order.
let private freeVarsInOrder (ty: Ty) : Name list =
    let acc = ResizeArray()
    let rec go ty =
        match ty with
        | Ty.Var a when not (isBoundName a) -> if not (acc.Contains a) then acc.Add a
        | Ty.Var _ | Ty.Atom _ | Ty.Lit _ | Ty.Sketch _ -> ()
        | Ty.Power (b, _) | Ty.Commutes b | Ty.Exact (b, _) -> go b
        | Ty.InCategory (a, b) -> go a; go b
        | Ty.HasType (subj, t) -> go subj; go t
        | Ty.Eq (a, b) -> go a; go b
        | Ty.Entails (ctx, goal) ->
            ctx |> List.iter (function CtxVar _ -> () | Hyp (_, t) | Anon t -> go t)
            go goal
        | Ty.Function (a, b) | Ty.Product (a, b) | Ty.Sum (a, b)
        | Ty.Intersection (a, b) | Ty.Union (a, b) -> go a; go b
        | Ty.Record ms -> ms |> List.iter (snd >> go)
        | Ty.Polymorphic (_, b) | Ty.Existential (_, b) | Ty.Recursive (_, b) -> go b
        | Ty.DependentFunction (_, a, b) | Ty.DependentPair (_, a, b)
        | Ty.DependentIntersection (_, a, b)
        | Ty.FamilialIntersection (_, a, b) | Ty.FamilialUnion (_, a, b) -> go a; go b
    go ty
    List.ofSeq acc

/// Number the free variables #0, #1, ... by first occurrence.
let private renumberFrees (ty: Ty) : Ty * Map<Name, Name> =
    let order = freeVarsInOrder ty
    let toCanon = order |> List.mapi (fun i a -> a, freeName i) |> Map.ofList
    let t = mapVars (fun a -> if isBoundName a then a else toCanon.[a]) ty
    t, (order |> List.mapi (fun i a -> freeName i, a) |> Map.ofList)

// ---- pass 2: candidate orderings for commutative operators ----------------
// ∩ and ∪ are commutative and associative, so their operand chains are
// flattened, canonicalized, and sorted. Operands whose masked forms tie are
// disambiguated by trying their permutations (the worst case) and keeping
// whichever ordering yields the least formatted result overall.

let private maxVariants = 512

let rec private permutations (xs: 'a list) : 'a list list =
    match xs with
    | [] | [ _ ] -> [ xs ]
    | _ ->
        [ for i in 0 .. xs.Length - 1 do
            let x = xs.[i]
            let rest = xs |> List.mapi (fun j y -> j, y) |> List.choose (fun (j, y) -> if j = i then None else Some y)
            for p in permutations rest -> x :: p ]

let private cartesian (lists: 'a list list) : 'a list list =
    lists
    |> List.fold (fun acc xs -> [ for a in acc do for x in xs -> a @ [ x ] ] |> List.truncate maxVariants) [ [] ]

let rec private variants (ty: Ty) : Ty list =
    let via rebuild parts =
        cartesian (List.map variants parts)
        |> List.map rebuild
        |> List.truncate maxVariants
    match ty with
    | Ty.Atom _ | Ty.Var _ | Ty.Lit _ | Ty.Sketch _ -> [ ty ]
    | Ty.Power (b, n) -> via (fun l -> Ty.Power (l.[0], n)) [ b ]
    | Ty.Commutes b -> via (fun l -> Ty.Commutes l.[0]) [ b ]
    | Ty.Exact (b, ax) -> via (fun l -> Ty.Exact (l.[0], ax)) [ b ]
    | Ty.InCategory (d, c) -> via (fun l -> Ty.InCategory (l.[0], l.[1])) [ d; c ]
    | Ty.HasType (subj, t) -> via (fun l -> Ty.HasType (l.[0], l.[1])) [ subj; t ]
    | Ty.Eq (a, b) ->
        // = is symmetric: offer both orientations and let the least form win
        [ for av in variants a do
            for bv in variants b do
                yield Ty.Eq (av, bv)
                yield Ty.Eq (bv, av) ]
        |> List.truncate maxVariants
    | Ty.Entails (ctx, goal) ->
        // hypothesis order can carry dependencies, so the context is not permuted
        let choices =
            ctx |> List.map (function
                | CtxVar n -> [ CtxVar n ]
                | Hyp (x, t) -> variants t |> List.map (fun v -> Hyp (x, v))
                | Anon t -> variants t |> List.map Anon)
        [ for combo in cartesian choices do
            for gv in variants goal do
                yield Ty.Entails (combo, gv) ]
        |> List.truncate maxVariants
    | Ty.Intersection _ ->
        commVariants Ty.Intersection (function Ty.Intersection (a, b) -> Some (a, b) | _ -> None) ty
    | Ty.Union _ ->
        commVariants Ty.Union (function Ty.Union (a, b) -> Some (a, b) | _ -> None) ty
    | Ty.Function (a, b) -> via (fun l -> Ty.Function (l.[0], l.[1])) [ a; b ]
    | Ty.Product (a, b) -> via (fun l -> Ty.Product (l.[0], l.[1])) [ a; b ]
    | Ty.Sum (a, b) -> via (fun l -> Ty.Sum (l.[0], l.[1])) [ a; b ]
    | Ty.Record ms ->
        // member names are semantic, so a canonical record just sorts by name
        let sorted = ms |> List.sortBy fst
        via (fun l -> Ty.Record (List.zip (List.map fst sorted) l)) (List.map snd sorted)
    | Ty.Polymorphic (a, b) -> via (fun l -> Ty.Polymorphic (a, l.[0])) [ b ]
    | Ty.Existential (a, b) -> via (fun l -> Ty.Existential (a, l.[0])) [ b ]
    | Ty.Recursive (a, b) -> via (fun l -> Ty.Recursive (a, l.[0])) [ b ]
    | Ty.DependentFunction (x, a, b) -> via (fun l -> Ty.DependentFunction (x, l.[0], l.[1])) [ a; b ]
    | Ty.DependentPair (x, a, b) -> via (fun l -> Ty.DependentPair (x, l.[0], l.[1])) [ a; b ]
    | Ty.DependentIntersection (x, a, b) -> via (fun l -> Ty.DependentIntersection (x, l.[0], l.[1])) [ a; b ]
    | Ty.FamilialIntersection (x, a, b) -> via (fun l -> Ty.FamilialIntersection (x, l.[0], l.[1])) [ a; b ]
    | Ty.FamilialUnion (x, a, b) -> via (fun l -> Ty.FamilialUnion (x, l.[0], l.[1])) [ a; b ]

and private commVariants mk dec ty =
    let rec flat t =
        match dec t with
        | Some (a, b) -> flat a @ flat b
        | None -> [ t ]
    let parts = flat ty
    [ for combo in cartesian (List.map variants parts) do
        let sorted = combo |> List.sortBy (fun t -> Ty.format (masked t))
        let groups = sorted |> List.groupBy (fun t -> Ty.format (masked t)) |> List.map snd
        let choices =
            groups
            |> List.map (fun g -> if g.Length > 1 && g.Length <= 4 then permutations g else [ g ])
        for pick in cartesian choices do
            yield pick |> List.concat |> List.reduceBack (fun a b -> mk (a, b)) ]
    |> List.truncate maxVariants

// ---- entry point -----------------------------------------------------------

/// Canonicalize a type:
///  1. bound variables become de Bruijn levels @0, @1, ...
///  2. operands of ∩ / ∪ are flattened and sorted (permuting tied operands in
///     the worst case), record members are sorted by name
///  3. free variables become #0, #1, ... by first occurrence
/// The least formatted candidate wins, and the canonical-name -> original-name
/// mappings are returned alongside the type.
/// Free variables (never binder-bound names) in first-occurrence order.
let freeVariables (ty: Ty) : Name[] =
    let renamed, _ = alphaRename ty
    freeVarsInOrder renamed |> Array.ofList

let canonicalize (ty: Ty) : Canonicalized =
    let renamed, boundLog = alphaRename ty
    let bestTy, freeMap =
        variants renamed
        |> List.map renumberFrees
        |> List.minBy (fun (t, _) -> Ty.format t)
    { Ty = bestTy; BoundVars = boundLog; FreeVars = freeMap }

let private canonicalText (t: Ty) : string =
    Ty.format (canonicalize (Ty.expandPowers t)).Ty

/// Name-preserving semantic equality. Canonical strings alone identify
/// expressions only up to renaming of FREE variables (A → B ≡ C → D), which
/// is too loose for proof checking. The pairing trick fixes it: a ≡ b iff
/// canonical ⟨a, b⟩ = canonical ⟨a, a⟩ — the shared free-variable renumbering
/// across the pair forces the names to correspond, while α-equivalence,
/// ∩/∪ sorting, and Eq orientation are still absorbed.
let sameTy (a: Ty) (b: Ty) : bool =
    // cheap pre-filter: different canonical shapes can never be equal
    if canonicalText a <> canonicalText b then false
    else canonicalText (Ty.Product (a, b)) = canonicalText (Ty.Product (a, a))

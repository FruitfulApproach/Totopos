namespace AbCatDC.Kernel

open System.Text.RegularExpressions

/// One judgment (a premise or conclusion port) of a node on the proof-tree
/// canvas, as the current wiring determines it.
type TreeJudgment =
    { Node: int
      Index: int
      IsPremise: bool
      /// the judgment with the tree's substitution applied, instance tags stripped
      Text: string
      /// colour class: judgments that are the same up to metavariable
      /// substitution (in either direction) share a class
      Class: int
      /// no metavariable of the rule is left unbound
      Ground: bool }

/// A metavariable of a rule instance and what the wiring bound it to.
type TreeBinding = { Node: int; Meta: string; Value: string }

type TreePort = { Node: int; Index: int; Text: string }

type TreeWireIssue = { Wire: int; Message: string }

type TreeReport =
    { Judgments: TreeJudgment[]
      Bindings: TreeBinding[]
      /// premise ports no wire discharges (a given's own top port excepted):
      /// the tree's remaining obligations
      Open: TreePort[]
      /// conclusion ports no wire consumes — what the tree establishes
      Roots: TreePort[]
      /// conclusion ports that are PROVEN: a given's, or a node's whose every
      /// premise is discharged by an established port
      Established: TreePort[]
      /// statements (goals, lemmas) whose top port is discharged by an
      /// established port
      Proven: TreePort[]
      /// nodes and wires lying on a cycle (a proof must be a DAG)
      CycleNodes: int[]
      CycleWires: int[]
      /// wires whose two ends do not unify under the rest of the tree
      BadWires: TreeWireIssue[]
      ClassCount: int }

type internal TreeNode =
    { Id: int
      IsRule: bool
      /// a given: established by assumption, its top port is no obligation
      IsGiven: bool
      Premises: Ty[]
      Conclusions: Ty[]
      /// this instance's own copies of the rule's metavariables
      Metas: Set<Name> }

type internal TreeWire = { Id: int; Src: int; SrcIdx: int; Dst: int; DstIdx: int }

/// A proof tree: rule instances — each with its OWN copy of the rule's
/// metavariables, so two uses of "compose" never share an f — and statements
/// (givens, goals, lemmas), wired conclusion → premise. Every wire is a
/// constraint "this conclusion IS that premise"; the tree's substitution is
/// re-solved from scratch on each query by replaying the wires in id order,
/// so removing a wire simply removes its constraint.
type ProofTree() =
    let nodes = System.Collections.Generic.SortedDictionary<int, TreeNode>()
    let wires = System.Collections.Generic.SortedDictionary<int, TreeWire>()

    /// instance tag: "₌" + subscript digits — one identifier unit for the
    /// lexer, so a composite gf becomes g₌₃f₌₃ and still decomposes unit-wise
    static let tagOf (id: int) = "₌" + QuiverImport.SubscriptDigits id
    static let tagRx = Regex("₌[₀-₉]+", RegexOptions.Compiled)
    static let strip (n: Name) = tagRx.Replace(n, "")

    /// a judgment for the eye: instance tags stripped from every name
    let display (t: Ty) : string =
        let ren =
            Canonical.freeVariables t
            |> Array.filter (fun v -> v.Contains "₌")
            |> Array.map (fun v -> v, Ty.Var (strip v))
            |> Map.ofArray
        let t = if Map.isEmpty ren then t else Ty.substTyVars ren t
        Ty.format (Api.mapSketches strip t)

    /// apply the substitution until nothing changes (bindings may mention
    /// other instances' metavariables bound later)
    let resolve (σ: Map<Name, Ty>) (t: Ty) : Ty =
        let mutable cur = t
        let mutable i = 0
        let mutable go = not (Map.isEmpty σ)
        while go && i < 20 do
            let next = Ty.substTyVars σ cur
            if next = cur then go <- false else cur <- next
            i <- i + 1
        cur

    let allMetas () = nodes.Values |> Seq.collect (fun n -> n.Metas) |> Set.ofSeq

    /// two-sided: either judgment may be the schematic one; and a diagram
    /// port discharges a premise through what the diagram contributes
    /// (typings, equations, exactness), as on the Chase page
    let unify (metas: Set<Name>) (σ: Map<Name, Ty>) (p: Ty) (c: Ty) =
        let direct =
            match Match.matchPattern metas σ p c with
            | Some s -> Some s
            | None -> Match.matchPattern metas σ c p
        match direct with
        | Some s -> Some s
        | None ->
            Api.DerivedFactsOf c
            |> List.tryPick (fun d ->
                let d = resolve σ d
                match Match.matchPattern metas σ p d with
                | Some s -> Some s
                | None -> Match.matchPattern metas σ d p)

    /// replay the wires: the substitution they determine, and the wires that
    /// do not fit
    let solve () : Map<Name, Ty> * TreeWireIssue[] =
        let metas = allMetas ()
        let mutable σ = Map.empty
        let bad = ResizeArray<TreeWireIssue>()
        for w in wires.Values do
            match nodes.TryGetValue w.Src, nodes.TryGetValue w.Dst with
            | (true, src), (true, dst) when w.SrcIdx < src.Conclusions.Length && w.DstIdx < dst.Premises.Length ->
                let c = resolve σ src.Conclusions.[w.SrcIdx]
                let p = resolve σ dst.Premises.[w.DstIdx]
                match unify metas σ p c with
                | Some s ->
                    let cyclic =
                        s |> Map.toSeq
                        |> Seq.tryFind (fun (k, v) -> not (Map.containsKey k σ) && Array.contains k (Canonical.freeVariables v))
                    match cyclic with
                    | Some (k, _) -> bad.Add { Wire = w.Id; Message = $"'{strip k}' would be bound to a term containing itself" }
                    | None -> σ <- s
                | None ->
                    bad.Add { Wire = w.Id; Message = $"'{display c}' does not match '{display p}' under the rest of the tree" }
            | _ -> bad.Add { Wire = w.Id; Message = "dangling wire" }
        σ, bad.ToArray()

    /// union-find over "same up to metavariable substitution"
    let classes (metas: Set<Name>) (js: Ty[]) : int[] =
        // identical judgments are one representative; the quadratic pass runs
        // over representatives only
        let repOf = System.Collections.Generic.Dictionary<string, int>()
        let reps = ResizeArray<Ty>()
        let repIdx =
            js |> Array.map (fun t ->
                let k = Ty.format t
                match repOf.TryGetValue k with
                | true, r -> r
                | _ ->
                    let r = reps.Count
                    reps.Add t
                    repOf.[k] <- r
                    r)
        let n = reps.Count
        let parent = Array.init n id
        let rec find i = if parent.[i] = i then i else (let r = find parent.[i] in parent.[i] <- r; r)
        let equiv a b =
            (Match.matchPattern metas Map.empty a b).IsSome || (Match.matchPattern metas Map.empty b a).IsSome
        for i in 0 .. n - 1 do
            for j in i + 1 .. n - 1 do
                let ri, rj = find i, find j
                if ri <> rj && equiv reps.[i] reps.[j] then parent.[max ri rj] <- min ri rj
        let ids = System.Collections.Generic.Dictionary<int, int>()
        let classOfRep =
            [| for i in 0 .. n - 1 do
                let r = find i
                match ids.TryGetValue r with
                | true, c -> yield c
                | _ ->
                    let c = ids.Count
                    ids.[r] <- c
                    yield c |]
        repIdx |> Array.map (fun r -> classOfRep.[r])

    /// Tarjan: nodes in a non-trivial strongly connected component (or with
    /// a self-loop) lie on a cycle; so do the wires inside such a component
    let cycles () : int[] * int[] =
        let index = System.Collections.Generic.Dictionary<int, int>()
        let low = System.Collections.Generic.Dictionary<int, int>()
        let onStack = System.Collections.Generic.HashSet<int>()
        let stack = System.Collections.Generic.Stack<int>()
        let sccOf = System.Collections.Generic.Dictionary<int, int>()
        let mutable counter = 0
        let mutable sccCount = 0
        let succ v = [ for w in wires.Values do if w.Src = v && nodes.ContainsKey w.Dst then yield w.Dst ]
        let rec strong v =
            index.[v] <- counter
            low.[v] <- counter
            counter <- counter + 1
            stack.Push v
            onStack.Add v |> ignore
            for u in succ v do
                if not (index.ContainsKey u) then
                    strong u
                    low.[v] <- min low.[v] low.[u]
                elif onStack.Contains u then
                    low.[v] <- min low.[v] index.[u]
            if low.[v] = index.[v] then
                let mutable go = true
                while go do
                    let u = stack.Pop()
                    onStack.Remove u |> ignore
                    sccOf.[u] <- sccCount
                    if u = v then go <- false
                sccCount <- sccCount + 1
        for v in nodes.Keys do
            if not (index.ContainsKey v) then strong v
        let size = sccOf.Values |> Seq.countBy id |> Map.ofSeq
        let selfLoop v = wires.Values |> Seq.exists (fun w -> w.Src = v && w.Dst = v)
        let cyclic =
            nodes.Keys |> Seq.filter (fun v -> size.[sccOf.[v]] > 1 || selfLoop v) |> Set.ofSeq
        let cycleWires =
            [| for w in wires.Values do
                if cyclic.Contains w.Src && cyclic.Contains w.Dst && sccOf.[w.Src] = sccOf.[w.Dst] then yield w.Id |]
        Set.toArray cyclic, cycleWires

    /// Add a rule instance. Returns "" or a parse error.
    member _.AddRule (id: int, premises: string[], conclusions: string[]) : string =
        let parseAll (label: string) (xs: string[]) =
            let mutable err = None
            let ts =
                [| for i in 0 .. xs.Length - 1 do
                    if err.IsNone then
                        match Parser.parse xs.[i] with
                        | Ok t -> yield t
                        | Error e -> err <- Some $"{label} {i + 1}: {e}" |]
            match err with Some e -> Error e | None -> Ok ts
        match parseAll "premise" premises, parseAll "conclusion" conclusions with
        | Error e, _ | _, Error e -> e
        | Ok ps, Ok cs ->
            let metas = Api.MetaVars (Array.append premises conclusions)
            let tag = tagOf id
            let ren = metas |> Array.map (fun m -> m, Ty.Var (m + tag)) |> Map.ofArray
            // the pictures this instance mentions get their own copies, labels
            // tagged like the text, so shape matching binds THIS instance's f
            let sketchRen =
                Array.append ps cs
                |> Array.toList
                |> List.collect Api.sketchesIn
                |> List.distinct
                |> List.choose (fun nm ->
                    match Map.tryFind nm Ty.userSketches with
                    | Some json ->
                        let pairs = metas |> Array.map (fun m -> m, m + tag)
                        let copy = nm + tag
                        let relabelled = QuiverImport.Relabel json pairs
                        lock Ty.sketchLock (fun () -> Ty.userSketches <- Map.add copy relabelled Ty.userSketches)
                        Some (nm, copy)
                    | None -> None)
                |> Map.ofList
            let rn (t: Ty) =
                let t = if Map.isEmpty ren then t else Ty.substTyVars ren t
                if Map.isEmpty sketchRen then t
                else Api.mapSketches (fun nm -> match Map.tryFind nm sketchRen with Some c -> c | None -> nm) t
            nodes.[id] <-
                { Id = id; IsRule = true; IsGiven = false
                  Premises = Array.map rn ps; Conclusions = Array.map rn cs
                  Metas = metas |> Array.map (fun m -> m + tag) |> Set.ofArray }
            ""

    /// Add a statement: its one judgment is both a premise port (to be
    /// established) and a conclusion port (to be used). A GIVEN is established
    /// by assumption; a goal or lemma must be discharged by a wire into its
    /// top port. Names in a statement are the proof's own — never metavariables.
    member _.AddStatement (id: int, text: string, isGiven: bool) : string =
        match Parser.parse text with
        | Error e -> e
        | Ok t ->
            nodes.[id] <- { Id = id; IsRule = false; IsGiven = isGiven; Premises = [| t |]; Conclusions = [| t |]; Metas = Set.empty }
            ""

    member this.AddStatement (id: int, text: string) : string = this.AddStatement (id, text, false)

    member internal _.NodesSnapshot = nodes.Values |> Array.ofSeq
    member internal _.WiresSnapshot = wires.Values |> Array.ofSeq

    /// An independent copy — a search thread works on its own tree.
    member this.Clone () : ProofTree =
        let t = ProofTree()
        t.Import (this.NodesSnapshot, this.WiresSnapshot)
        t

    member internal _.Import (ns: TreeNode[], ws: TreeWire[]) : unit =
        nodes.Clear()
        wires.Clear()
        for n in ns do nodes.[n.Id] <- n
        for w in ws do wires.[w.Id] <- w

    /// Whether a node is a given statement.
    member _.IsGiven (id: int) : bool =
        match nodes.TryGetValue id with
        | true, n -> not n.IsRule && n.IsGiven
        | _ -> false

    member _.HasNode (id: int) : bool = nodes.ContainsKey id

    member _.Remove (id: int) : unit =
        nodes.Remove id |> ignore
        for w in wires.Values |> Seq.filter (fun w -> w.Src = id || w.Dst = id) |> List.ofSeq do
            wires.Remove w.Id |> ignore

    /// Wire a conclusion port to a premise port (replacing any wire already
    /// into that premise). The wire is kept either way; the result is "" when
    /// the two judgments unify under the rest of the tree, else the reason.
    member _.Connect (wireId: int, src: int, srcIdx: int, dst: int, dstIdx: int) : string =
        for w in wires.Values |> Seq.filter (fun w -> w.Dst = dst && w.DstIdx = dstIdx) |> List.ofSeq do
            wires.Remove w.Id |> ignore
        wires.[wireId] <- { Id = wireId; Src = src; SrcIdx = srcIdx; Dst = dst; DstIdx = dstIdx }
        let _, bad = solve ()
        match bad |> Array.tryFind (fun b -> b.Wire = wireId) with
        | Some b -> b.Message
        | None -> ""

    member _.Disconnect (wireId: int) : unit = wires.Remove wireId |> ignore

    member _.Clear () : unit =
        nodes.Clear()
        wires.Clear()

    member _.Report () : TreeReport =
        let metas = allMetas ()
        let σ, bad = solve ()
        let js = ResizeArray<int * int * bool * Ty>()
        for n in nodes.Values do
            n.Premises |> Array.iteri (fun i t -> js.Add (n.Id, i, true, resolve σ t))
            n.Conclusions |> Array.iteri (fun i t -> js.Add (n.Id, i, false, resolve σ t))
        let tys = js |> Seq.map (fun (_, _, _, t) -> t) |> Array.ofSeq
        let cls = classes metas tys
        let judgments =
            [| for k in 0 .. js.Count - 1 do
                let (nid, i, isP, t) = js.[k]
                let ground = Canonical.freeVariables t |> Array.forall (fun v -> not (metas.Contains v))
                yield { Node = nid; Index = i; IsPremise = isP; Text = display t; Class = cls.[k]; Ground = ground } |]
        let bindings =
            [| for n in nodes.Values do
                for m in n.Metas do
                    match Map.tryFind m σ with
                    | Some v -> yield { Node = n.Id; Meta = strip m; Value = display (resolve σ v) }
                    | None -> () |]
        let wired = wires.Values |> Seq.map (fun w -> w.Dst, w.DstIdx) |> Set.ofSeq
        let consumed = wires.Values |> Seq.map (fun w -> w.Src, w.SrcIdx) |> Set.ofSeq
        let port (j: TreeJudgment) = { Node = j.Node; Index = j.Index; Text = j.Text }
        let givenTop (nid: int) = match nodes.TryGetValue nid with | (true, n) -> not n.IsRule && n.IsGiven | _ -> false
        let opens =
            [| for j in judgments do
                if j.IsPremise && not (wired.Contains (j.Node, j.Index)) && not (givenTop j.Node) then yield port j |]
        let roots = [| for j in judgments do if not j.IsPremise && not (consumed.Contains (j.Node, j.Index)) then yield port j |]
        // established: by assumption (a given), or every premise discharged by
        // an established port through a sound wire — cycles never count
        let badIds = bad |> Array.map (fun b -> b.Wire) |> Set.ofArray
        let into = wires.Values |> Seq.filter (fun w -> not (badIds.Contains w.Id)) |> Seq.map (fun w -> (w.Dst, w.DstIdx), w) |> Map.ofSeq
        let memo = System.Collections.Generic.Dictionary<int, bool>()
        let rec est (visiting: Set<int>) (n: TreeNode) : bool =
            match memo.TryGetValue n.Id with
            | true, v -> v
            | _ ->
                if visiting.Contains n.Id then false
                else
                    let v =
                        if not n.IsRule && n.IsGiven then true
                        else
                            [ 0 .. n.Premises.Length - 1 ]
                            |> List.forall (fun i ->
                                match Map.tryFind (n.Id, i) into with
                                | Some w ->
                                    match nodes.TryGetValue w.Src with
                                    | true, src -> est (visiting.Add n.Id) src
                                    | _ -> false
                                | None -> false)
                    memo.[n.Id] <- v
                    v
        let established =
            [| for j in judgments do
                if not j.IsPremise then
                    match nodes.TryGetValue j.Node with
                    | true, n when est Set.empty n -> yield port j
                    | _ -> () |]
        let proven =
            [| for j in judgments do
                if not j.IsPremise then
                    match nodes.TryGetValue j.Node with
                    | true, n when not n.IsRule && not n.IsGiven && est Set.empty n -> yield port j
                    | _ -> () |]
        let cn, cw = cycles ()
        { Judgments = judgments; Bindings = bindings; Open = opens; Roots = roots
          Established = established; Proven = proven
          CycleNodes = cn; CycleWires = cw; BadWires = bad
          ClassCount = (if cls.Length = 0 then 0 else Array.max cls + 1) }

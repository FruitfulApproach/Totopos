namespace AbCatDC.Kernel

/// A rule the search may instantiate.
type SearchRule =
    { Name: string
      Kind: string
      Premises: string[]
      Conclusions: string[] }

/// One wire the search adds: (wire id, source node, source port, target node, target port).
type SearchWire = { Wire: int; Src: int; SrcIdx: int; Dst: int; DstIdx: int }

/// One move of the search — already applied to the search's own tree; the
/// page mirrors it onto the canvas.
type SearchMove =
    { /// "close" (a wire between existing ports), "backward" (a rule instance
      /// concluding an obligation) or "forward" (a rule instance whose
      /// premises are all established)
      Kind: string
      /// the node added, or -1
      NewNode: int
      RuleName: string
      RuleKind: string
      Premises: string[]
      Conclusions: string[]
      Wires: SearchWire[]
      /// place the new node relative to this node: above it (backward) or
      /// below it (forward)
      Anchor: int
      Above: bool
      Note: string }

type SearchStatus =
    { Solved: bool
      Exhausted: bool
      Steps: int
      Nodes: int
      Note: string }

/// Automated proof search over a proof tree: alternately CLOSES open
/// obligations against established conclusions, works BACKWARD from the most
/// recent obligation (a rule whose conclusion fits it), and works FORWARD
/// from what is established (a rule whose premises all fit). Every move is
/// checked on the tree — a wire whose ends do not unify, or that closes a
/// cycle, is undone — so the tree stays a sound DAG throughout.
type ProofSearch(tree: ProofTree, rules: SearchRule[], firstId: int, maxNodes: int, maxDepth: int) =
    let mutable nextId = firstId
    let mutable steps = 0
    let mutable exhausted = false
    let tried = System.Collections.Generic.HashSet<string>()
    /// distance from a goal, for backward moves
    let depthOf = System.Collections.Generic.Dictionary<int, int>()
    let noMove = None

    let fresh () =
        let id = nextId
        nextId <- nextId + 1
        id

    let parse (s: string) = match Parser.parse s with Ok t -> Some t | Error _ -> None

    let sameText (a: string) (b: string) = Api.SameExpr a b

    let depth (nid: int) = match depthOf.TryGetValue nid with | true, d -> d | _ -> 0

    /// try a wire; keep it only if it unifies and closes no cycle
    let tryWire (wid: int) (src: int) (srcIdx: int) (dst: int) (dstIdx: int) : bool =
        let msg = tree.Connect (wid, src, srcIdx, dst, dstIdx)
        if msg.Length > 0 then
            tree.Disconnect wid
            false
        else
            let r = tree.Report ()
            if r.CycleNodes.Length > 0 then
                tree.Disconnect wid
                false
            else true

    // ---- close: an open obligation against an established conclusion --------
    let closeMove (r: TreeReport) : SearchMove option =
        let classOf = r.Judgments |> Array.map (fun j -> (j.Node, j.Index, j.IsPremise), j.Class) |> Map.ofArray
        let est = r.Established
        r.Open
        |> Array.rev   // most recent obligations first
        |> Array.tryPick (fun p ->
            let pc = classOf.[(p.Node, p.Index, true)]
            est
            |> Array.filter (fun e -> e.Node <> p.Node)
            // same colour class first (a cheap necessary condition for a
            // direct match); diagram ports may still discharge through
            // derived facts, so try them all
            |> Array.sortBy (fun e -> if classOf.[(e.Node, e.Index, false)] = pc then 0 else 1)
            |> Array.tryPick (fun e ->
                let key = $"close:{p.Node}:{p.Index}:{e.Node}:{e.Index}"
                if tried.Contains key then None
                else
                    tried.Add key |> ignore
                    let wid = fresh ()
                    if tryWire wid e.Node e.Index p.Node p.Index then
                        Some { Kind = "close"; NewNode = -1; RuleName = ""; RuleKind = ""; Premises = [||]; Conclusions = [||]
                               Wires = [| { Wire = wid; Src = e.Node; SrcIdx = e.Index; Dst = p.Node; DstIdx = p.Index } |]
                               Anchor = p.Node; Above = true
                               Note = $"'{e.Text}' discharges '{p.Text}'" }
                    else None))

    // ---- backward: a rule whose conclusion fits an obligation --------------
    let backwardMove (r: TreeReport) : SearchMove option =
        let estTexts = r.Established |> Array.map (fun e -> e.Text)
        r.Open
        |> Array.rev
        |> Array.filter (fun p -> depth p.Node < maxDepth)
        |> Array.tryPick (fun p ->
            match parse p.Text with
            | None -> None
            | Some pty ->
                // candidate rules: a conclusion that matches the obligation as a pattern
                let cands =
                    [ for rule in rules do
                        let metas = Api.MetaVars (Array.append rule.Premises rule.Conclusions) |> Set.ofArray
                        for j in 0 .. rule.Conclusions.Length - 1 do
                            match parse rule.Conclusions.[j] with
                            | Some cty when (Match.matchPattern metas Map.empty cty pty).IsSome ->
                                // score: premises that already look established are cheap
                                let score =
                                    rule.Premises
                                    |> Array.sumBy (fun pr ->
                                        match parse pr with
                                        | Some prt when estTexts |> Array.exists (fun t -> match parse t with Some tt -> (Match.matchPattern metas Map.empty prt tt).IsSome | None -> false) -> 0
                                        | _ -> 1)
                                yield ((score, rule.Premises.Length, (if rule.Kind = "Construction" then 1 else 0)), rule, j)
                            | _ -> () ]
                    |> List.sortBy (fun (k, _, _) -> k)
                cands
                |> List.tryPick (fun (_, rule, j) ->
                    let key = $"back:{rule.Name}:{j}:{p.Text}"
                    if tried.Contains key then None
                    else
                        tried.Add key |> ignore
                        let id = fresh ()
                        if (tree.AddRule (id, rule.Premises, rule.Conclusions)).Length > 0 then None
                        else
                            let wid = fresh ()
                            if tryWire wid id j p.Node p.Index then
                                depthOf.[id] <- depth p.Node + 1
                                Some { Kind = "backward"; NewNode = id; RuleName = rule.Name; RuleKind = rule.Kind
                                       Premises = rule.Premises; Conclusions = rule.Conclusions
                                       Wires = [| { Wire = wid; Src = id; SrcIdx = j; Dst = p.Node; DstIdx = p.Index } |]
                                       Anchor = p.Node; Above = true
                                       Note = $"{rule.Name} concludes '{p.Text}'" }
                            else
                                tree.Remove id
                                None))

    // ---- forward: a rule whose premises are all established ---------------

    /// the port each ORIGINAL premise was satisfied by (an unfolded premise
    /// is several pool entries; take its first)
    let premisePorts (rule: SearchRule) (used: int[]) : int[] =
        let counts = Api.PremiseExpansion rule.Premises
        let starts = counts |> Array.scan (fun acc c -> acc + max 1 c) 0
        rule.Premises
        |> Array.mapi (fun i _ ->
            let k = starts.[i]
            if k < used.Length then used.[k] else -1)

    /// instantiate `rule` below the established ports `ports`; None if a wire fails
    let placeForward (rule: SearchRule) (est: TreePort[]) (ports: int[]) : SearchMove option =
        let id = fresh ()
        if (tree.AddRule (id, rule.Premises, rule.Conclusions)).Length > 0 then None
        else
            let wireIds = ports |> Array.map (fun _ -> fresh ())
            let ok =
                ports
                |> Array.mapi (fun i pi -> (i, pi))
                |> Array.forall (fun (i, pi) -> tryWire wireIds.[i] est.[pi].Node est.[pi].Index id i)
            if not ok then
                tree.Remove id
                None
            else
                let wiresMade =
                    ports |> Array.mapi (fun i pi -> { Wire = wireIds.[i]; Src = est.[pi].Node; SrcIdx = est.[pi].Index; Dst = id; DstIdx = i })
                Some { Kind = "forward"; NewNode = id; RuleName = rule.Name; RuleKind = rule.Kind
                       Premises = rule.Premises; Conclusions = rule.Conclusions
                       Wires = wiresMade
                       Anchor = est.[ports.[0]].Node; Above = false
                       Note = rule.Name + " applies to what is established" }

    let forwardFor (est: TreePort[]) (pool: string[]) (rule: SearchRule) : SearchMove option =
        if rule.Premises.Length = 0 then None
        else
            let outc = Api.TryApplyRule rule.Premises rule.Conclusions pool (rule.Kind = "Construction")
            let already (t: string) = pool |> Array.exists (fun e -> sameText e t)
            if not outc.Ok then None
            elif outc.NewConclusions |> Array.forall already then None
            else
                let ports = premisePorts rule outc.UsedSteps
                let outOfRange = ports |> Array.exists (fun i -> i < 0 || i >= est.Length)
                let key = "fwd:" + rule.Name + ":" + (ports |> Array.map string |> String.concat ",")
                if outOfRange || tried.Contains key then None
                else
                    tried.Add key |> ignore
                    placeForward rule est ports

    let forwardMove (r: TreeReport) : SearchMove option =
        let est = r.Established
        if est.Length = 0 then None
        else
            let pool = est |> Array.map (fun e -> e.Text)
            rules
            |> Array.sortBy (fun rule -> ((if rule.Kind = "Construction" then 1 else 0), rule.Premises.Length))
            |> Array.tryPick (forwardFor est pool)

    /// The goals: statements that are neither givens nor established.
    member _.Solved (r: TreeReport) =
        r.Roots
        |> Array.filter (fun p -> not (tree.IsGiven p.Node))
        |> Array.forall (fun p -> r.Proven |> Array.exists (fun q -> q.Node = p.Node))

    /// One move, or None when solved / exhausted / out of budget.
    member this.Step () : SearchMove option =
        let r = tree.Report ()
        if this.Solved r then noMove
        elif r.Judgments |> Array.map (fun j -> j.Node) |> Array.distinct |> Array.length >= maxNodes then
            exhausted <- true
            noMove
        else
            let mv =
                match closeMove r with
                | Some m -> Some m
                | None ->
                    match backwardMove r with
                    | Some m -> Some m
                    | None -> forwardMove r
            match mv with
            | Some m ->
                steps <- steps + 1
                Some m
            | None ->
                exhausted <- true
                noMove

    member _.NextId = nextId

    member this.Status : SearchStatus =
        let r = tree.Report ()
        let solved = this.Solved r
        { Solved = solved; Exhausted = exhausted && not solved; Steps = steps
          Nodes = r.Judgments |> Array.map (fun j -> j.Node) |> Array.distinct |> Array.length
          Note = (if solved then "every goal is established" elif exhausted then "no applicable move is left" else "searching") }

module AbCatDC.Kernel.QuiverImport

open System
open System.Collections.Generic
open System.Text.Json
open System.Threading

/// A quiver diagram reduced to its 1-skeleton.
type Graph =
    { VertexCount: int
      VertexLabels: string[]
      /// grid positions (x, y) per vertex, as drawn in quiver
      Positions: (int * int)[]
      /// (source, target) pairs indexing vertices; higher cells are filtered out upstream
      Edges: (int * int)[]
      /// labels parallel to Edges
      EdgeLabels: string[]
      SkippedHigherCells: int }

/// The canonicalized diagram in the internal language.
type CanonicalDiagram =
    { /// one line per edge: {@e}:{@s}\to{@t}
      Lines: string[]
      /// the ∀-prefix binding the unmentioned vertices, e.g. "∀ {@1} {@2}"
      ForallPrefix: string
      /// canonical free token -> original vertex label (mentioned in the title)
      FreeMap: (string * string)[]
      /// canonical bound vertex token -> original vertex label
      BoundMap: (string * string)[]
      SkippedHigherCells: int
      /// how many candidate orderings were examined
      Candidates: int }

type CanonicalOutcome =
    { Ok: bool
      Diagram: CanonicalDiagram
      Error: string }

let private emptyDiagram =
    { Lines = [||]; ForallPrefix = ""; FreeMap = [||]; BoundMap = [||]; SkippedHigherCells = 0; Candidates = 0 }

/// Parse quiver's export format: [version, vertexCount, ...cells], vertices
/// first as [x, y, label?, ...], then edges as [source, target, ...] indexing
/// the cells array. Accepts raw JSON or the base64 form used in share links.
let rec parse (input: string) : Result<Graph, string> =
    let text = input.Trim()
    if text.Length = 0 then Error "Empty input." else
    if not (text.StartsWith "[") then
        try
            let decoded = Text.Encoding.UTF8.GetString(Convert.FromBase64String text)
            if decoded.TrimStart().StartsWith "[" then parse decoded
            else Error "Input is neither a JSON array nor base64-encoded quiver data."
        with _ -> Error "Input is neither a JSON array nor valid base64."
    else
    try
        use doc = JsonDocument.Parse text
        let root = doc.RootElement
        if root.ValueKind <> JsonValueKind.Array then Error "Expected a JSON array (quiver export format)." else
        let items = [| for e in root.EnumerateArray() -> e.Clone() |]
        if items.Length < 2 then Error "Expected [version, vertexCount, ...cells]." else
        let version = items.[0].GetInt32()
        if version <> 0 then Error $"Unsupported quiver format version {version}." else
        let n = items.[1].GetInt32()
        if n < 0 || items.Length - 2 < n then Error "Vertex count does not match the cell list." else
        let labels =
            [| for i in 0 .. n - 1 ->
                let c = items.[2 + i]
                if c.GetArrayLength() >= 3 && c.[2].ValueKind = JsonValueKind.String
                then c.[2].GetString()
                else "" |]
        let positions =
            [| for i in 0 .. n - 1 ->
                let c = items.[2 + i]
                int (c.[0].GetDouble()), int (c.[1].GetDouble()) |]
        let all = ResizeArray()
        for idx in 2 + n .. items.Length - 1 do
            let c = items.[idx]
            let label =
                if c.GetArrayLength() >= 3 && c.[2].ValueKind = JsonValueKind.String
                then c.[2].GetString()
                else ""
            all.Add (c.[0].GetInt32(), c.[1].GetInt32(), label)
        let oneCells = all |> Seq.filter (fun (s, t, _) -> 0 <= s && s < n && 0 <= t && t < n) |> Array.ofSeq
        Ok { VertexCount = n
             VertexLabels = labels
             Positions = positions
             Edges = oneCells |> Array.map (fun (s, t, _) -> s, t)
             EdgeLabels = oneCells |> Array.map (fun (_, _, l) -> l)
             SkippedHigherCells = all.Count - oneCells.Length }
    with ex -> Error $"Could not parse quiver JSON: {ex.Message}"

/// C#-friendly parse wrapper for the preview renderer.
type ParseOutcome =
    { Ok: bool
      Graph: Graph
      Error: string }

let private emptyGraph =
    { VertexCount = 0; VertexLabels = [||]; Positions = [||]; Edges = [||]; EdgeLabels = [||]; SkippedHigherCells = 0 }

let TryParse (input: string) : ParseOutcome =
    match parse input with
    | Ok g -> { Ok = true; Graph = g; Error = "" }
    | Error e -> { Ok = false; Graph = emptyGraph; Error = e }

let rec private permutations (xs: 'a list) : 'a list list =
    match xs with
    | [] | [ _ ] -> [ xs ]
    | _ ->
        [ for i in 0 .. xs.Length - 1 do
            let x = xs.[i]
            let rest = xs |> List.mapi (fun j y -> j, y) |> List.choose (fun (j, y) -> if j = i then None else Some y)
            for p in permutations rest -> x :: p ]

let private maxCandidates = 20000

/// Canonicalize: choose the ordering of edges (permuting invariant-tied edges
/// in the worst case) whose de Bruijn rendering is lexicographically least.
/// Vertices mentioned in `title` stay free (#k); all others are ∀-bound (@k).
/// `report` receives (candidatesDone, candidatesTotal); `ct` cancels.
let run (json: string) (title: string) (report: Action<int, int>) (ct: CancellationToken) : CanonicalOutcome =
    match parse json with
    | Error e -> { Ok = false; Diagram = emptyDiagram; Error = e }
    | Ok g ->
        try
            let n = g.VertexCount
            let isFree v =
                let l = g.VertexLabels.[v]
                l <> "" && not (String.IsNullOrWhiteSpace title) && title.Contains l
            let outd = Array.zeroCreate n
            let ind = Array.zeroCreate n
            for (s, t) in g.Edges do
                outd.[s] <- outd.[s] + 1
                ind.[t] <- ind.[t] + 1

            // invariant used to pre-sort edges; only tied edges get permuted
            let key (s, t) =
                (not (isFree s), not (isFree t),
                 -outd.[s], -ind.[s], -outd.[t], -ind.[t], (s = t))
            let sorted = g.Edges |> Array.sortBy key |> List.ofArray
            let groups = sorted |> List.groupBy key |> List.map snd
            let choices = groups |> List.map (fun grp -> if grp.Length <= 5 then permutations grp else [ grp ])
            let total = choices |> List.fold (fun acc c -> min (acc * c.Length) maxCandidates) 1

            // render one candidate ordering; edge tokens are allocated before
            // their endpoints, matching {@0}:{@1}\to{@2}
            let render (ordering: (int * int) list) =
                let tokens = Dictionary<int, string>()
                let mutable nextBound = 0
                let mutable nextFree = 0
                let boundLog = ResizeArray()
                let freeLog = ResizeArray()
                let tok v =
                    match tokens.TryGetValue v with
                    | true, t -> t
                    | _ ->
                        let t =
                            if isFree v then
                                let t = $"#{nextFree}"
                                nextFree <- nextFree + 1
                                freeLog.Add(t, g.VertexLabels.[v])
                                t
                            else
                                let t = $"@{nextBound}"
                                nextBound <- nextBound + 1
                                boundLog.Add(t, (if g.VertexLabels.[v] = "" then "·" else g.VertexLabels.[v]))
                                t
                        tokens.[v] <- t
                        t
                let lines =
                    [| for (s, t) in ordering ->
                        let e = $"@{nextBound}"
                        nextBound <- nextBound + 1
                        let st = tok s
                        let tt = tok t
                        $"{{{e}}}:{{{st}}}\\to{{{tt}}}" |]
                // isolated vertices still need binders / free names
                for v in 0 .. n - 1 do
                    if not (tokens.ContainsKey v) then tok v |> ignore
                lines, boundLog.ToArray(), freeLog.ToArray()

            let cartesian (lists: (int * int) list list list) =
                lists |> List.fold (fun acc xs -> [ for a in acc do for x in xs -> a @ [ x ] ] |> List.truncate maxCandidates) [ [] ]

            let mutable best : (string * ((int * int) list)) option = None
            let mutable count = 0
            for combo in cartesian choices do
                ct.ThrowIfCancellationRequested()
                let ordering = List.concat combo
                let lines, _, _ = render ordering
                let k = String.Join("\n", lines)
                match best with
                | Some (bk, _) when String.CompareOrdinal(k, bk) >= 0 -> ()
                | _ -> best <- Some (k, ordering)
                count <- count + 1
                if count % 32 = 0 || count = total then report.Invoke(count, total)
            report.Invoke(total, total)

            let ordering = match best with Some (_, o) -> o | None -> []
            let lines, boundLog, freeLog = render ordering
            let prefix =
                if boundLog.Length = 0 then ""
                else "∀ " + String.Join(" ", boundLog |> Array.map (fun (t, _) -> $"{{{t}}}"))
            { Ok = true
              Diagram =
                { Lines = lines
                  ForallPrefix = prefix
                  FreeMap = freeLog
                  BoundMap = boundLog
                  SkippedHigherCells = g.SkippedHigherCells
                  Candidates = count }
              Error = "" }
        with
        | :? OperationCanceledException ->
            { Ok = false; Diagram = emptyDiagram; Error = "Cancelled." }
        | ex ->
            { Ok = false; Diagram = emptyDiagram; Error = ex.Message }

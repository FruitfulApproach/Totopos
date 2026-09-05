namespace AbCatDC

open System
open System.IO
open System.Threading.Tasks
open Microsoft.Maui.ApplicationModel
open Microsoft.Maui.Controls
open Microsoft.AspNetCore.Components.WebView.Maui
open AbCatDC.RazorLib.Services

/// Captures the Blazor web view as a PNG through WebView2 (Windows). The
/// page crops the capture to the diagram itself.
type SnapshotService() =
    /// the BlazorWebView somewhere in the page's visual tree
    let rec findWebView (v: obj) : BlazorWebView option =
        match v with
        | :? BlazorWebView as b -> Some b
        | :? ContentPage as p -> findWebView p.Content
        | :? Layout as l -> l.Children |> Seq.tryPick (fun c -> findWebView (box c))
        | :? ContentView as c -> findWebView c.Content
        | _ -> None

    interface ISnapshotService with
        member _.CapturePngBase64Async () : Task<string> =
#if WINDOWS
            MainThread.InvokeOnMainThreadAsync<string>(fun () ->
                task {
                    let page =
                        match Application.Current with
                        | null -> null
                        | app when app.Windows.Count > 0 -> app.Windows.[0].Page
                        | _ -> null
                    match findWebView (box page) with
                    | Some bwv when not (isNull bwv.Handler) ->
                        match bwv.Handler.PlatformView with
                        | :? Microsoft.UI.Xaml.Controls.WebView2 as wv when not (isNull wv.CoreWebView2) ->
                            use ms = new MemoryStream()
                            do! wv.CoreWebView2
                                    .CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png,
                                                         System.IO.WindowsRuntimeStreamExtensions.AsRandomAccessStream ms)
                                    .AsTask()
                            return Convert.ToBase64String(ms.ToArray())
                        | _ -> return null
                    | _ -> return null
                })
#else
            Task.FromResult<string>(null)
#endif

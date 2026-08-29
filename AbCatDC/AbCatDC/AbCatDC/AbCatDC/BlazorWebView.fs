namespace Fabulous.Maui.Blazor

open Microsoft.AspNetCore.Components.WebView
open Microsoft.AspNetCore.Components.WebView.Maui
open Fabulous
open Fabulous.Maui

type FabRootComponent = RootComponent

type IFabBlazorWebView =
    inherit IFabView

module BlazorWebView = 
    let WidgetKey = Widgets.register<BlazorWebView>()

    let HostPage = Attributes.defineSimpleScalarWithEquality<string> "BlazorWebView_HostPage" (fun _ newValueOpt node ->
        let bwv = node.Target :?> BlazorWebView
    
        match newValueOpt with
            | ValueNone -> ()
            | ValueSome hostPage -> bwv.HostPage <- hostPage)

    let RootComponents = Attributes.defineSimpleScalarWithEquality<RootComponent list> "BlazorWebView_RootComponents" (fun _ newValueOpt node ->
        let bwv = node.Target :?> BlazorWebView

        // Fresh RootComponent instances are created on every MVU render, so this
        // updater re-runs even when nothing changed; it must be idempotent —
        // blindly re-adding throws "There is already a root component with
        // selector '#app'" and crashes the app at startup.
        let clear () =
            while bwv.RootComponents.Count > 0 do
                bwv.RootComponents.RemoveAt(bwv.RootComponents.Count - 1)

        match newValueOpt with
            | ValueNone -> clear ()
            | ValueSome rootComponents ->
                let unchanged =
                    bwv.RootComponents.Count = List.length rootComponents
                    && Seq.forall2
                        (fun (a: RootComponent) (b: RootComponent) ->
                            a.Selector = b.Selector && a.ComponentType = b.ComponentType)
                        bwv.RootComponents rootComponents
                if not unchanged then
                    clear ()
                    rootComponents |> List.iter bwv.RootComponents.Add)
#if NET8_0_OR_GREATER
    let StartPath = Attributes.defineBindableWithEquality<string> BlazorWebView.StartPathProperty
#endif

    let BlazorWebViewInitializing = Fabulous.Attributes.Mvu.defineEvent<BlazorWebViewInitializingEventArgs> "BlazorWebView_BlazorWebViewInitializing" (fun target -> (target :?> BlazorWebView).BlazorWebViewInitializing)

    let BlazorWebViewInitialized = Fabulous.Attributes.Mvu.defineEvent<BlazorWebViewInitializedEventArgs> "BlazorWebView_BlazorWebViewInitialized" (fun target -> (target :?> BlazorWebView).BlazorWebViewInitialized)

    let UrlLoading = Fabulous.Attributes.Mvu.defineEvent<UrlLoadingEventArgs> "BlazorWebView_UrlLoading" (fun target -> (target :?> BlazorWebView).UrlLoading)

[<AutoOpen>]
module BlazorWebViewBuilders =
    type View with
        static member inline BlazorWebView<'msg when 'msg: equality>(hostPage: string, rootComponents: RootComponent list) =
            WidgetBuilder<'msg, IFabBlazorWebView>(
                BlazorWebView.WidgetKey,
                BlazorWebView.HostPage.WithValue(hostPage),
                BlazorWebView.RootComponents.WithValue(rootComponents)
            )
#if NET8_0_OR_GREATER
        static member inline BlazorWebView<'msg when 'msg: equality>(hostPage: string, startPath: string, rootComponents: RootComponent list) =
            WidgetBuilder<'msg, IFabBlazorWebView>(
                BlazorWebView.WidgetKey,
                BlazorWebView.HostPage.WithValue(hostPage),
                BlazorWebView.StartPath.WithValue(startPath),
                BlazorWebView.RootComponents.WithValue(rootComponents)
            )
#endif

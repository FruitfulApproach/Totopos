namespace AbCatDC

open Fabulous
open Fabulous.Maui
open Fabulous.Maui.Blazor
open Microsoft.Maui
open Microsoft.Maui.Graphics
open Microsoft.Maui.Hosting
open Microsoft.Maui.Accessibility
open Microsoft.Maui.Primitives
open System.Reflection
open AbCatDC.RazorLib

open type Fabulous.Maui.View

module App =
    let mauiVersion =
        let version = Assembly.GetAssembly(typeof<MauiApp>).GetCustomAttribute<AssemblyInformationalVersionAttribute>().InformationalVersion
        $".NET MAUI ver. {version[..version.IndexOf('+') - 1]}"

    let view _ =
        Application(
            ContentPage(
                Grid(coldefs = [ Star ], rowdefs = [ Star; Absolute 40. ]) {
                    (BlazorWebView("wwwroot/index.html", "/", [ new FabRootComponent( Selector = "#app", ComponentType = typeof<Main> ) ])).gridRow(0)

                    (Grid() {
                        Label(mauiVersion).center()
                    }).gridRow(1)
                }
            )
        )

    let program = Program.stateless view

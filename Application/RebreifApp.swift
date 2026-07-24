//
//  RebreifApp.swift
//  Rebreif
//
//  Created by Chernakov, Igor (148) on 24.07.26.
//

import SwiftUI

private struct ContentViewContainer: View {
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        ContentView(openPresenterWindow: { url in
            openWindow(id: "presenter", value: url)
        })
    }
}

@main
struct RebreifApp: App {
    var body: some Scene {
        // Welcome / launcher window
        WindowGroup("QuickSlides") {
            ContentViewContainer()
        }
        .defaultSize(width: 740, height: 480)
        .windowResizability(.contentMinSize)

        // Presenter window — one per open .slides file
        WindowGroup(id: "presenter", for: PresentationOpenRequest.self) { $request in
            if let request, let session = PresentationSession(request: request) {
                PresenterWindowView(session: session)
            } else {
                Text("Could not open presentation.")
                    .frame(width: 400, height: 200)
            }
        }
        .defaultSize(width: 900, height: 560)
        .windowResizability(.contentMinSize)
    }
}

//
//  RebreifApp.swift
//  Rebreif
//
//  Created by Chernakov, Igor (148) on 24.07.26.
//

import SwiftUI

@main
struct RebreifApp: App {
    var body: some Scene {
        // Welcome / launcher window
        WindowGroup("QuickSlides") {
            ContentView()
        }
        .defaultSize(width: 740, height: 480)
        .windowResizability(.contentMinSize)

        // Presenter window — one per open .slides file
        WindowGroup(id: "presenter", for: URL.self) { $url in
            if let url, let session = PresentationSession(slidesPath: url.path) {
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

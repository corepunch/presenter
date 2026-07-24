//
//  RebreifApp.swift
//  Rebreif
//
//  Created by Chernakov, Igor (148) on 24.07.26.
//

import SwiftUI

private struct ContentViewContainer: View {
    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss
    @State private var didHandleInitialLaunch = false

    var body: some View {
        ContentView(openPresenterWindow: { url in
            openWindow(id: "presenter", value: url)
        })
        .task {
            handleInitialLaunch()
        }
    }

    private func handleInitialLaunch() {
        guard !didHandleInitialLaunch else { return }
        didHandleInitialLaunch = true

        let requests = LaunchArguments.presentationRequests()
        guard !requests.isEmpty else { return }

        for request in requests {
            openWindow(id: "presenter", value: request)
        }
        dismiss()
    }
}

private enum LaunchArguments {
    static func presentationRequests(
        arguments: [String] = CommandLine.arguments
    ) -> [PresentationOpenRequest] {
        arguments
            .dropFirst()
            .filter { !$0.hasPrefix("-psn_") }
            .map { ($0 as NSString).expandingTildeInPath }
            .map(URL.init(fileURLWithPath:))
            .filter { $0.pathExtension == "slides" }
            .map { PresentationOpenRequest(url: $0, bookmarkData: nil) }
    }
}

@main
struct RebreifApp: App {
    @Environment(\.openWindow) private var openWindow

    var body: some Scene {
        WindowGroup("QuickSlides") {
            ContentViewContainer()
        }
        .defaultSize(width: 740, height: 480)
        .windowResizability(.contentMinSize)
        .onOpenURL { url in
            guard url.pathExtension == "slides" else { return }
            let request = PresentationOpenRequest(url: url, bookmarkData: nil)
            openWindow(id: "presenter", value: request)
        }

        WindowGroup(id: "presenter", for: PresentationOpenRequest.self) { $request in
            if let request, let session = PresentationSession(request: request) {
                PresenterWindowView(session: session)
            } else {
                Text("Could not open presentation.")
                    .frame(width: 400, height: 200)
            }
        }
        .defaultSize(width: 640, height: 800)
        .windowResizability(.contentMinSize)
    }
}

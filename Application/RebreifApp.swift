//
//  RebreifApp.swift
//  Rebreif
//
//  Created by Chernakov, Igor (148) on 24.07.26.
//

import SwiftUI
import AppKit

private struct ContentViewContainer: View {
    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss
    @State private var didHandleInitialLaunch = false

    var body: some View {
        ContentView(openPresenterWindow: { url in
            openWindow(id: "presenter", value: url)
        })
        .onOpenURL { url in
            openPresentation(url: url)
        }
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

    private func openPresentation(url: URL) {
        guard url.pathExtension == "slides" else { return }
        openWindow(
            id: "presenter",
            value: PresentationOpenRequest(url: url, bookmarkData: nil)
        )
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
    @NSApplicationDelegateAdaptor(QuickSlidesAppDelegate.self) var appDelegate

    var body: some Scene {
        WindowGroup("QuickSlides") {
            ContentViewContainer()
        }
        .defaultSize(width: 740, height: 480)
        .windowResizability(.contentMinSize)

        WindowGroup(id: "presenter", for: PresentationOpenRequest.self) { $request in
            PresenterWindow(request: request)
        }
        .defaultSize(width: 640, height: 800)
        .windowResizability(.contentMinSize)
    }
}

private final class QuickSlidesAppDelegate: NSObject, NSApplicationDelegate {
    func application(_ application: NSApplication,
                     shouldRestoreApplicationState coder: NSCoder) -> Bool { false }
}

private final class InvalidWindowCloser: NSView {
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if let window {
            DispatchQueue.main.async { window.close() }
        }
    }
}

private struct InvalidPresenterCleanup: NSViewRepresentable {
    func makeNSView(context: Context) -> InvalidWindowCloser {
        InvalidWindowCloser()
    }

    func updateNSView(_ nsView: InvalidWindowCloser, context: Context) {}
}

private struct PresenterWindow: View {
    let request: PresentationOpenRequest?

    var body: some View {
        if let request, let session = PresentationSession(request: request) {
            PresenterWindowView(session: session)
        } else {
            InvalidPresenterCleanup()
        }
    }
}

import SwiftUI

/// Drives the live presentation: navigation, notes, slide rendering.
/// Owned by the window that opens a .slides file.
@Observable
final class PresentationSession {

    // MARK: - Published state

    private(set) var slideIndex: Int = 0
    private(set) var slideCount: Int = 0
    private(set) var slideLabel: String = ""
    private(set) var notes: String = ""
    private(set) var title: String = ""
    private(set) var canGoNext: Bool = false
    private(set) var canGoPrev: Bool = false

    /// Bumped whenever a new frame should be rendered
    private(set) var renderToken: Int = 0

    // MARK: - Private

    private let bridge: PresenterBridge
    private let sourceURL: URL
    private let isAccessingSecurityScopedResource: Bool

    // MARK: - Init

    init?(request: PresentationOpenRequest) {
        var bookmarkIsStale = false
        let resolvedURL: URL
        if let bookmarkData = request.bookmarkData,
           let bookmarkedURL = try? URL(
                resolvingBookmarkData: bookmarkData,
                options: .withSecurityScope,
                relativeTo: nil,
                bookmarkDataIsStale: &bookmarkIsStale
           ) {
            resolvedURL = bookmarkedURL
        } else {
            resolvedURL = request.url
        }

        let didStartAccessing = resolvedURL.startAccessingSecurityScopedResource()
        guard let b = PresenterBridge(slidesPath: resolvedURL.path) else {
            if didStartAccessing {
                resolvedURL.stopAccessingSecurityScopedResource()
            }
            return nil
        }

        self.sourceURL = resolvedURL
        self.isAccessingSecurityScopedResource = didStartAccessing
        self.bridge = b
        sync()
    }

    deinit {
        if isAccessingSecurityScopedResource {
            sourceURL.stopAccessingSecurityScopedResource()
        }
    }

    // MARK: - Navigation

    func next() { bridge.next(); sync() }
    func prev() { bridge.prev(); sync() }
    func goTo(_ index: Int) { bridge.go(to: index); sync() }

    // MARK: - Rendering

    /// Fill `pixels` with the audience slide RGBA8888 frame.
    func renderSlide(into pixels: UnsafeMutablePointer<UInt8>,
                     width: Int, height: Int) -> Bool {
        bridge.renderSlide(intoPixels: pixels,
                           width: Int32(width),
                           height: Int32(height),
                           stride: Int32(width * 4))
    }

    /// Fill `pixels` with the presenter view RGBA8888 frame.
    func renderPresenterView(into pixels: UnsafeMutablePointer<UInt8>,
                             width: Int, height: Int) -> Bool {
        bridge.renderPresenterView(intoPixels: pixels,
                                   width: Int32(width),
                                   height: Int32(height),
                                   stride: Int32(width * 4))
    }

    // MARK: - Theme

    var themeCount: Int { bridge.themeCount }
    func themeName(at index: Int) -> String { bridge.themeName(at: index) }
    var currentTheme: Int {
        get { bridge.currentTheme }
        set { bridge.currentTheme = newValue; sync() }
    }

    // MARK: - Helpers

    private func sync() {
        let info = bridge.currentInfo()
        slideIndex = info.slideIndex
        slideCount = info.slideCount
        slideLabel = info.slideLabel
        notes      = info.notes
        title      = info.presentationTitle
        canGoNext  = info.canGoNext
        canGoPrev  = info.canGoPrev
        renderToken &+= 1
    }
}

import SwiftUI

/// Drives the live presentation: navigation, notes, slide rendering.
/// Owned by the window that opens a .slides file.
@Observable
final class PresentationSession {

    // MARK: - Published state (read from SwiftUI views)

    private(set) var slideIndex: Int = 0
    private(set) var slideCount: Int = 0
    private(set) var slideLabel: String = ""
    private(set) var notes: String = ""
    private(set) var title: String = ""
    private(set) var slideTitles: [String] = []
    private(set) var canGoNext: Bool = false
    private(set) var canGoPrev: Bool = false

    // Bumped whenever a new frame should be requested from the renderer
    private(set) var renderToken: Int = 0

    // MARK: - Private

    private let bridge: PresenterBridge

    // MARK: - Init

    init?(slidesPath: String) {
        guard let b = PresenterBridge(slidesPath: slidesPath) else { return nil }
        self.bridge = b
        sync()
    }

    // MARK: - Navigation

    func next() {
        bridge.next()
        sync()
    }

    func prev() {
        bridge.prev()
        sync()
    }

    func goTo(_ index: Int) {
        bridge.goTo(index)
        sync()
    }

    func slideTitle(at index: Int) -> String {
        guard slideTitles.indices.contains(index) else { return "Slide \(index + 1)" }
        return slideTitles[index]
    }

    // MARK: - Rendering

    /// Fill `pixels` with the audience slide RGBA8888 frame.
    /// Returns true on success.
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
        set {
            bridge.currentTheme = newValue
            sync()
        }
    }

    // MARK: - Private helpers

    private func sync() {
        let info = bridge.currentInfo()
        slideIndex = info.slideIndex
        slideCount = info.slideCount
        slideLabel = info.slideLabel
        notes      = info.notes
        title      = info.presentationTitle
        slideTitles = (0..<info.slideCount).map { index in
            let title = bridge.slideTitle(at: index).trimmingCharacters(in: .whitespacesAndNewlines)
            return title.isEmpty ? "Slide \(index + 1)" : title
        }
        canGoNext  = info.canGoNext
        canGoPrev  = info.canGoPrev
        renderToken &+= 1
    }
}

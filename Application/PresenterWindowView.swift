import AppKit
import SwiftUI

/// The presenter's control window: rendered slide thumbnail, notes,
/// navigation controls, slide counter, and theme picker.
struct PresenterWindowView: View {
    @State var session: PresentationSession
    @State private var audienceWindowController = AudienceWindowController()
    @State private var isAudienceWindowVisible = false
    @State private var isSidebarVisible = false

    var body: some View {
        HSplitView {
            if isSidebarVisible {
                slideSidebar
                    .frame(minWidth: 160, idealWidth: 200, maxWidth: 260)
            }

            VStack(spacing: 0) {
                slidePreview
                    .frame(minWidth: 520, minHeight: 340)

                Divider()

                notesPanel
                    .frame(minHeight: 140, idealHeight: 180, maxHeight: 260)
            }
        }
        .frame(minWidth: 640, idealWidth: 720, minHeight: 640, idealHeight: 720)
        .navigationTitle(session.title)
        .toolbar { toolbarItems }
        .onKeyPress(.rightArrow) { session.next(); return .handled }
        .onKeyPress(.leftArrow)  { session.prev(); return .handled }
        .onKeyPress(.space)      { session.next(); return .handled }
        .onDisappear {
            audienceWindowController.hide()
        }
    }

    // MARK: - Subviews

    private var slideSidebar: some View {
        List(selection: Binding<Int?>(
            get: { session.slideIndex },
            set: { index in
                if let index {
                    session.goTo(index)
                }
            }
        )) {
            ForEach(0..<session.slideCount, id: \.self) { index in
                Text(session.slideTitle(at: index))
                    .lineLimit(1)
                    .tag(index)
            }
        }
        .listStyle(.sidebar)
    }

    private var slidePreview: some View {
        slidePreviewContent
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var slidePreviewContent: some View {
        RenderedSlideView(session: session, presenterView: false)
            .aspectRatio(16.0 / 9.0, contentMode: .fit)
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .shadow(radius: 6)
            .padding(20)
    }

    private var notesPanel: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Speaker Notes")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(.secondary)
                .textCase(.uppercase)
                .tracking(0.5)
                .padding(.horizontal, 16)
                .padding(.vertical, 10)

            Divider()

            ScrollView {
                Text(session.notes.isEmpty ? "No notes for this slide." : session.notes)
                    .font(.system(size: 13))
                    .foregroundStyle(session.notes.isEmpty ? .tertiary : .primary)
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    .padding(16)
            }
        }
        .background(.background)
    }

    @ToolbarContentBuilder
    private var toolbarItems: some ToolbarContent {
        ToolbarItemGroup(placement: .primaryAction) {
            Button {
                isSidebarVisible.toggle()
            } label: {
                Label("Slides", systemImage: "sidebar.left")
                    .labelStyle(.iconOnly)
            }
            .help(isSidebarVisible ? "Hide slide list" : "Show slide list")

            ControlGroup {
                Button { session.prev() } label: {
                    Label("Previous", systemImage: "chevron.left")
                        .labelStyle(.iconOnly)
                }
                .help("Previous slide")
                .disabled(!session.canGoPrev)

                Button { session.next() } label: {
                    Label("Next", systemImage: "chevron.right")
                        .labelStyle(.iconOnly)
                }
                .help("Next slide")
                .disabled(!session.canGoNext)
            }

            Text(session.slideLabel)
                .font(.system(size: 13, weight: .medium).monospacedDigit())
                .foregroundStyle(.secondary)
                .frame(minWidth: 56)
        }

        ToolbarItem(placement: .automatic) {
            Button {
                toggleAudienceWindow()
            } label: {
                Label(
                    isAudienceWindowVisible ? "Hide Audience Window" : "Show Audience Window",
                    systemImage: isAudienceWindowVisible ? "rectangle.badge.minus" : "rectangle.badge.plus"
                )
            }
            .help(isAudienceWindowVisible ? "Hide audience window" : "Show audience window")
        }

        ToolbarItem(placement: .automatic) {
            Menu {
                ForEach(0..<session.themeCount, id: \.self) { i in
                    Button(session.themeName(at: i)) { session.currentTheme = i }
                }
            } label: {
                Label("Theme", systemImage: "paintpalette")
            }
        }
    }

    private func toggleAudienceWindow() {
        if isAudienceWindowVisible {
            audienceWindowController.hide()
            isAudienceWindowVisible = false
        } else {
            audienceWindowController.show(session: session, title: session.title) {
                isAudienceWindowVisible = false
            }
            isAudienceWindowVisible = true
        }
    }
}

// MARK: - Audience Window

private struct AudienceWindowView: View {
    var session: PresentationSession

    var body: some View {
        RenderedSlideView(session: session, presenterView: false)
            .aspectRatio(16.0 / 9.0, contentMode: .fit)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color.black)
    }
}

@Observable
private final class AudienceWindowController: NSObject, NSWindowDelegate {
    private var window: NSWindow?
    private var onClose: (() -> Void)?

    func show(session: PresentationSession, title: String, onClose: @escaping () -> Void) {
        self.onClose = onClose

        if let window {
            window.title = audienceTitle(for: title)
            window.contentView = NSHostingView(rootView: AudienceWindowView(session: session))
            window.makeKeyAndOrderFront(nil)
            return
        }

        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1280, height: 720),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = audienceTitle(for: title)
        window.contentView = NSHostingView(rootView: AudienceWindowView(session: session))
        window.delegate = self
        window.minSize = NSSize(width: 640, height: 360)
        window.center()
        window.makeKeyAndOrderFront(nil)
        self.window = window
    }

    func hide() {
        window?.close()
    }

    func windowWillClose(_ notification: Notification) {
        window = nil
        onClose?()
        onClose = nil
    }

    private func audienceTitle(for presentationTitle: String) -> String {
        presentationTitle.isEmpty ? "Audience Window" : "\(presentationTitle) - Audience"
    }
}

// MARK: - RenderedSlideView

/// Renders on the presentation's fixed 1280x720 virtual canvas. SwiftUI only
/// scales the finished bitmap to fit the preview.
struct RenderedSlideView: View {
    var session: PresentationSession
    var presenterView: Bool = false

    @State private var image: CGImage?

    var body: some View {
        Group {
            if let img = image {
                Image(img, scale: 1, label: Text("Slide"))
                    .resizable()
                    .interpolation(.medium)
                    .aspectRatio(
                        CGSize(width: img.width, height: img.height),
                        contentMode: .fit
                    )
            } else {
                Color.black
            }
        }
        .onChange(of: session.renderToken, initial: true) { _, _ in render() }
    }

    private func render() {
        let w = 1280
        let h = 720
        let stride = w * 4
        var pixels = [UInt8](repeating: 0, count: stride * h)
        let ok = pixels.withUnsafeMutableBytes { buf in
            let ptr = buf.bindMemory(to: UInt8.self).baseAddress!
            return presenterView
                ? session.renderPresenterView(into: ptr, width: w, height: h)
                : session.renderSlide(into: ptr, width: w, height: h)
        }
        guard ok else { return }
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        guard let provider = CGDataProvider(data: Data(pixels) as CFData) else { return }
        image = CGImage(
            width: w, height: h,
            bitsPerComponent: 8, bitsPerPixel: 32,
            bytesPerRow: stride,
            space: colorSpace,
            bitmapInfo: CGBitmapInfo(rawValue:
                CGImageAlphaInfo.premultipliedLast.rawValue |
                CGBitmapInfo.byteOrder32Big.rawValue),
            provider: provider,
            decode: nil,
            shouldInterpolate: true,
            intent: .defaultIntent
        )
    }
}

// MARK: - Preview

#Preview {
    let previewURL = Bundle.main.url(
        forResource: "Nature Portfolio",
        withExtension: "slides",
        subdirectory: "demo"
    ) ?? URL(fileURLWithPath: "demo/Nature Portfolio.slides")

    if let session = PresentationSession(
        request: PresentationOpenRequest(url: previewURL, bookmarkData: nil)
    ) {
        PresenterWindowView(session: session)
    } else {
        Text("Could not load preview presentation.")
            .frame(width: 900, height: 560)
    }
}

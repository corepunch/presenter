import SwiftUI

/// The presenter's control window: shows rendered slide thumbnail, notes,
/// navigation controls, and slide counter.
struct PresenterWindowView: View {
    @State var session: PresentationSession
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
        .onKeyPress(.rightArrow)  { session.next();    return .handled }
        .onKeyPress(.leftArrow)   { session.prev();    return .handled }
        .onKeyPress(.space)       { session.next();    return .handled }
        .onKeyPress(.escape)      { return .ignored }
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
        VStack(spacing: 12) {
            RenderedSlideView(session: session)
                .aspectRatio(16 / 9, contentMode: .fit)
                .clipShape(RoundedRectangle(cornerRadius: 8))
                .shadow(radius: 6)
                .padding(.horizontal, 20)
                .padding(.top, 20)

            Text(session.slideLabel)
                .font(.system(size: 13, weight: .medium).monospacedDigit())
                .foregroundStyle(.secondary)
                .padding(.bottom, 8)

            navigationBar
                .padding(.horizontal, 20)
                .padding(.bottom, 16)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
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

    private var navigationBar: some View {
        HStack(spacing: 12) {
            Button(action: { session.prev() }) {
                Image(systemName: "chevron.left")
                    .frame(width: 28, height: 28)
            }
            .buttonStyle(.bordered)
            .disabled(!session.canGoPrev)
            .keyboardShortcut(.leftArrow, modifiers: [])

            Spacer()

            Button(action: { session.next() }) {
                Image(systemName: "chevron.right")
                    .frame(width: 28, height: 28)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!session.canGoNext)
            .keyboardShortcut(.rightArrow, modifiers: [])
        }
    }

    @ToolbarContentBuilder
    private var toolbarItems: some ToolbarContent {
        ToolbarItem(placement: .automatic) {
            Button {
                isSidebarVisible.toggle()
            } label: {
                Label("Slides", systemImage: "sidebar.left")
            }
            .help(isSidebarVisible ? "Hide slide list" : "Show slide list")
        }

        ToolbarItem(placement: .automatic) {
            themeMenu
        }
    }

    private var themeMenu: some View {
        Menu {
            ForEach(0..<session.themeCount, id: \.self) { i in
                Button(session.themeName(at: i)) {
                    session.currentTheme = i
                }
            }
        } label: {
            Label("Theme", systemImage: "paintpalette")
        }
    }
}

// MARK: - RenderedSlideView

/// A SwiftUI view that asks PresentationSession to render a frame into a
/// CGImage whenever renderToken changes.
struct RenderedSlideView: View {
    var session: PresentationSession

    @State private var image: CGImage?

    var body: some View {
        Group {
            if let img = image {
                Image(img, scale: 1, label: Text("Slide"))
                    .resizable()
                    .aspectRatio(CGSize(width: img.width, height: img.height), contentMode: .fit)
                    .interpolation(.medium)
            } else {
                Color.black
            }
        }
        .onChange(of: session.renderToken, initial: true) { _, _ in
            render()
        }
    }

    private func render() {
        let w = 1280
        let h = 720
        let stride = w * 4
        var pixels = [UInt8](repeating: 0, count: stride * h)
        let ok = pixels.withUnsafeMutableBytes { buf in
            let ptr = buf.bindMemory(to: UInt8.self).baseAddress!
            return session.renderSlide(into: ptr, width: w, height: h)
        }
        guard ok else { return }
        let data = Data(pixels)
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        guard let provider = CGDataProvider(data: data as CFData) else { return }
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

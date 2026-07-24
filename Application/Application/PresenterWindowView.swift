import SwiftUI

/// The presenter's control window: shows rendered slide thumbnail, notes,
/// navigation controls, and slide counter.
struct PresenterWindowView: View {
    @State var session: PresentationSession
    @State private var isSidebarVisible = false
    @State private var isNotesBodyVisible = true

    var body: some View {
        ZStack(alignment: .leading) {
            mainContent

            if isSidebarVisible {
                dismissScrim
                    .zIndex(1)

                sidebarOverlay
                    .transition(.move(edge: .leading).combined(with: .opacity))
                    .zIndex(2)
            }
        }
        .animation(.easeOut(duration: 0.2), value: isSidebarVisible)
        .frame(
            minWidth: LayoutMetrics.windowSize.minimum.width,
            idealWidth: LayoutMetrics.windowSize.ideal.width,
            minHeight: LayoutMetrics.windowSize.minimum.height,
            idealHeight: LayoutMetrics.windowSize.ideal.height
        )
        .navigationTitle(session.title)
        .toolbar { toolbarItems }
        .onKeyPress(.rightArrow)  { session.next();    return .handled }
        .onKeyPress(.leftArrow)   { session.prev();    return .handled }
        .onKeyPress(.space)       { session.next();    return .handled }
        .onKeyPress(.escape)      { return .ignored }
    }

    // MARK: - Subviews

    @ViewBuilder
    private var mainContent: some View {
        VStack(spacing: 0) {
            presentationHeader

            Divider()

            VSplitView {
                slidePreview
                    .layoutPriority(1)

                notesPanel
                    .frame(
                        minHeight: isNotesBodyVisible
                            ? LayoutMetrics.notesHeight.minimum
                            : LayoutMetrics.notesHeaderHeight,
                        idealHeight: isNotesBodyVisible
                            ? LayoutMetrics.notesHeight.ideal
                            : LayoutMetrics.notesHeaderHeight,
                        maxHeight: isNotesBodyVisible
                            ? LayoutMetrics.notesHeight.maximum
                            : LayoutMetrics.notesHeaderHeight
                    )
            }
        }
        .frame(minWidth: 0, maxWidth: .infinity, minHeight: 0, maxHeight: .infinity)
    }

    private var presentationHeader: some View {
        HStack {
            Text(session.title.isEmpty ? "Presentation" : session.title)
                .font(.system(size: 14, weight: .semibold))
                .lineLimit(1)
                .truncationMode(.tail)

            Spacer(minLength: 12)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
    }

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
                    .font(.system(size: 13))
                    .lineLimit(2)
                    .truncationMode(.tail)
                    .padding(.vertical, 2)
                    .tag(index)
            }
        }
        .listStyle(.sidebar)
        .scrollContentBackground(.hidden)
    }

    private var dismissScrim: some View {
        Color.clear
            .contentShape(Rectangle())
            .onTapGesture {
                isSidebarVisible = false
            }
    }

    private var sidebarOverlay: some View {
        slideSidebar
            .frame(width: LayoutMetrics.sidebarWidth.ideal)
            .frame(maxHeight: .infinity)
            .background(VisualEffectView())
            .overlay(alignment: .trailing) {
                Divider()
            }
            .shadow(radius: 16, x: 4)
    }

    private var slidePreview: some View {
        VStack(spacing: 12) {
            slidePreviewContent

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

    private var slidePreviewContent: some View {
        GeometryReader { proxy in
            let inset = compactInset(for: proxy.size)

            RenderedSlideView(session: session)
                .aspectRatio(16 / 9, contentMode: .fit)
                .frame(
                    width: max(0, proxy.size.width - inset * 2),
                    height: max(0, proxy.size.height - inset * 2)
                )
                .background(Color.black)
                .clipShape(RoundedRectangle(
                    cornerRadius: LayoutMetrics.slidePreviewCornerRadius,
                    style: .continuous
                ))
                .overlay {
                    RoundedRectangle(
                        cornerRadius: LayoutMetrics.slidePreviewCornerRadius,
                        style: .continuous
                    )
                    .stroke(.separator.opacity(0.5), lineWidth: 1)
                }
                .shadow(radius: proxy.size.width < 360 ? 3 : 6)
                .frame(width: proxy.size.width, height: proxy.size.height, alignment: .center)
        }
        .frame(minHeight: 0)
    }

    private var notesPanel: some View {
        VStack(alignment: .leading, spacing: 0) {
            notesHeader

            if isNotesBodyVisible {
                Divider()

                ScrollView {
                    Text(session.notes.isEmpty ? "No notes for this slide." : session.notes)
                        .font(.system(size: 13))
                        .foregroundStyle(session.notes.isEmpty ? .tertiary : .primary)
                        .frame(maxWidth: .infinity, alignment: .topLeading)
                        .padding(16)
                }
            }
        }
        .background(.background)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }

    private var notesHeader: some View {
        HStack(spacing: 8) {
            Text("Speaker Notes")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(.secondary)
                .textCase(.uppercase)
                .tracking(0.5)

            Spacer(minLength: 8)

            Button {
                isNotesBodyVisible.toggle()
            } label: {
                Label(
                    isNotesBodyVisible ? "Collapse Notes" : "Expand Notes",
                    systemImage: isNotesBodyVisible ? "chevron.down" : "chevron.up"
                )
                    .labelStyle(.iconOnly)
            }
            .buttonStyle(.borderless)
            .help(isNotesBodyVisible ? "Collapse speaker notes" : "Expand speaker notes")
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
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
            Button {
                isNotesBodyVisible.toggle()
            } label: {
                Label(isNotesBodyVisible ? "Collapse Notes" : "Expand Notes",
                      systemImage: "rectangle.bottomthird.inset.filled")
            }
            .help(isNotesBodyVisible ? "Collapse speaker notes" : "Expand speaker notes")
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

    private func compactInset(for size: CGSize) -> CGFloat {
        size.width < 360 || size.height < 260 ? 12 : 20
    }
}

private enum LayoutMetrics {
    static let sidebarWidth = SizeRange(minimum: 220, ideal: 280, maximum: 380)
    static let windowSize = WindowSize(
        minimum: CGSize(width: 480, height: 480),
        ideal: CGSize(width: 640, height: 800)
    )
    static let slidePreviewCornerRadius: CGFloat = 14
    static let notesHeaderHeight: CGFloat = 40
    static let notesHeight = SizeRange(minimum: 240, ideal: 360, maximum: 520)
}

private struct WindowSize {
    let minimum: CGSize
    let ideal: CGSize
}

private struct SizeRange {
    let minimum: CGFloat
    let ideal: CGFloat
    let maximum: CGFloat
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

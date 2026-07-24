import SwiftUI

/// The presenter's control window: rendered slide thumbnail, notes,
/// navigation controls, slide counter, and theme picker.
struct PresenterWindowView: View {
    @State var session: PresentationSession

    var body: some View {
        HSplitView {
            slidePreview
                .frame(minWidth: 480)

            notesPanel
                .frame(minWidth: 200, maxWidth: 340)
        }
        .frame(minWidth: 700, minHeight: 420)
        .navigationTitle(session.title)
        .toolbar { toolbarItems }
        .onKeyPress(.rightArrow) { session.next(); return .handled }
        .onKeyPress(.leftArrow)  { session.prev(); return .handled }
        .onKeyPress(.space)      { session.next(); return .handled }
    }

    // MARK: - Subviews

    private var slidePreview: some View {
        VStack(spacing: 12) {
            RenderedSlideView(session: session, presenterView: false)
                .aspectRatio(16.0 / 9.0, contentMode: .fit)
                .clipShape(RoundedRectangle(cornerRadius: 8))
                .shadow(radius: 6)
                .padding(.horizontal, 20)
                .padding(.top, 20)

            Text(session.slideLabel)
                .font(.system(size: 13, weight: .medium).monospacedDigit())
                .foregroundStyle(.secondary)

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
            Button { session.prev() } label: {
                Image(systemName: "chevron.left").frame(width: 28, height: 28)
            }
            .buttonStyle(.bordered)
            .disabled(!session.canGoPrev)

            Spacer()

            Button { session.next() } label: {
                Image(systemName: "chevron.right").frame(width: 28, height: 28)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!session.canGoNext)
        }
    }

    @ToolbarContentBuilder
    private var toolbarItems: some ToolbarContent {
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
}

// MARK: - RenderedSlideView

/// Asks PresentationSession to render a frame into a CGImage whenever
/// renderToken or the view size changes, then displays it.
struct RenderedSlideView: View {
    var session: PresentationSession
    var presenterView: Bool = false

    @State private var image: CGImage?

    var body: some View {
        GeometryReader { geo in
            Group {
                if let img = image {
                    Image(img, scale: 1, label: Text("Slide"))
                        .resizable()
                        .interpolation(.medium)
                } else {
                    Color.black
                }
            }
            .onChange(of: session.renderToken, initial: true) { _, _ in render(size: geo.size) }
            .onChange(of: geo.size) { _, newSize in render(size: newSize) }
        }
    }

    private func render(size: CGSize) {
        let w = max(1, Int(size.width))
        let h = max(1, Int(size.height))
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

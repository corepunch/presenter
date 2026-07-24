import AppKit
import SwiftUI

/// The presenter's control window: rendered slide thumbnail, notes,
/// navigation controls, slide counter, and theme picker.
///
/// The slide list is an overlay, not a split-view column — toggling it
/// never resizes the preview. This mirrors Finder/Mail/Xcode, where the
/// sidebar is a distinct translucent surface, not a content squeeze.
struct PresenterWindowView: View {
  @State var session: PresentationSession
  @State private var audience = AudienceWindowController()
  @State private var isAudienceVisible = false
  @State private var isSidebarVisible = false
  @State private var isNotesExpanded = true
  
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
      minWidth: LayoutMetrics.window.minimum.width,
      idealWidth: LayoutMetrics.window.ideal.width,
      minHeight: LayoutMetrics.window.minimum.height,
      idealHeight: LayoutMetrics.window.ideal.height
    )
    .navigationTitle(session.title)
    .toolbar { toolbarItems }
    .onKeyPress(.rightArrow) { session.next(); return .handled }
    .onKeyPress(.leftArrow) { session.prev(); return .handled }
    .onKeyPress(.space) { session.next(); return .handled }
    .onDisappear { audience.hide() }
  }
  
  // MARK: - Main content
  
  private var mainContent: some View {
    VSplitView {
      SlidePreviewPane(session: session)
        .layoutPriority(1)
      
      NotesPanel(notes: session.notes, isExpanded: $isNotesExpanded)
        .frame(
          minHeight: notesHeight.minimum,
          idealHeight: notesHeight.ideal,
          maxHeight: notesHeight.maximum
        )
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity)
  }
  
  /// Collapsed notes only need header height — previously this still
  /// reserved up to 480pt even while collapsed.
  private var notesHeight: SizeRange {
    isNotesExpanded
    ? LayoutMetrics.notes
    : SizeRange(repeating: LayoutMetrics.notesHeaderHeight)
  }
  
  // MARK: - Sidebar overlay
  
  /// Invisible full-window layer that closes the sidebar on an outside
  /// tap, the same dismissal behavior as a popover.
  private var dismissScrim: some View {
    Color.clear
      .contentShape(Rectangle())
      .onTapGesture { isSidebarVisible = false }
  }
  
  private var sidebarOverlay: some View {
    SlideListSidebar(
      session: session,
      selection: Binding(
        get: { session.slideIndex },
        set: { if let index = $0 { session.goTo(index) } }
      )
    )
    .frame(width: LayoutMetrics.sidebarWidth)
    .background(VisualEffectView(material: .sidebar, blendingMode: .withinWindow))
    .overlay(alignment: .trailing) { Divider() }
    .shadow(radius: 16, x: 4)
  }
  
  // MARK: - Toolbar
  
  @ToolbarContentBuilder
  private var toolbarItems: some ToolbarContent {
    ToolbarItemGroup(placement: .primaryAction) {
      Button {
        withAnimation { isSidebarVisible.toggle() }
      } label: {
        Label("Slides", systemImage: "sidebar.left")
          .labelStyle(.iconOnly)
      }
      .help(isSidebarVisible ? "Hide slide list" : "Show slide list")
      
      Button {
        withAnimation { isNotesExpanded.toggle() }
      } label: {
        Label(
          isNotesExpanded ? "Collapse Notes" : "Expand Notes",
          systemImage: "rectangle.bottomthird.inset.filled"
        )
        .labelStyle(.iconOnly)
      }
      .help(isNotesExpanded ? "Collapse notes" : "Expand notes")
      
      NavigationControls(session: session)
    }
    
    ToolbarItem(placement: .automatic) {
      Button {
        toggleAudienceWindow()
      } label: {
        Label(
          isAudienceVisible
          ? "Hide Audience Window"
          : "Show Audience Window",
          systemImage: isAudienceVisible
          ? "rectangle.badge.minus"
          : "rectangle.badge.plus"
        )
      }
      .help(
        isAudienceVisible
        ? "Hide audience window"
        : "Show audience window"
      )
    }
    
    ToolbarItem(placement: .automatic) {
      ThemeMenu(session: session)
    }
  }
  
  private func toggleAudienceWindow() {
    if isAudienceVisible {
      audience.hide()
      isAudienceVisible = false
    } else {
      audience.show(session: session, title: session.title) {
        isAudienceVisible = false
      }
      isAudienceVisible = true
    }
  }
}

// MARK: - Slide preview

/// Slide thumbnail, scaled to fit while preserving the 16:9 canvas.
private struct SlidePreviewPane: View {
  var session: PresentationSession
  
  var body: some View {
    GeometryReader { proxy in
      let inset = inset(for: proxy.size)
      RenderedSlideView(session: session)
        .aspectRatio(16.0 / 9.0, contentMode: .fit)
        .frame(
          width: max(0, proxy.size.width - inset * 2),
          height: max(0, proxy.size.height - inset * 2)
        )
        .background(Color.black)
        .clipShape(
          RoundedRectangle(
            cornerRadius: LayoutMetrics.previewCornerRadius,
            style: .continuous
          )
        )
        .overlay {
          RoundedRectangle(
            cornerRadius: LayoutMetrics.previewCornerRadius,
            style: .continuous
          )
          .stroke(.separator.opacity(0.5))
        }
        .shadow(radius: proxy.size.width < 360 ? 3 : 6)
        .frame(
          width: proxy.size.width,
          height: proxy.size.height,
          alignment: .center
        )
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity)
  }
  
  private func inset(for size: CGSize) -> CGFloat {
    size.width < 360 || size.height < 260 ? 12 : 20
  }
}

// MARK: - Notes panel

private struct NotesPanel: View {
  var notes: String
  @Binding var isExpanded: Bool
  
  var body: some View {
    VStack(alignment: .leading, spacing: 0) {
      header
      if isExpanded {
        Divider()
        ScrollView {
          Text(notes.isEmpty ? "No notes for this slide." : notes)
            .font(.system(size: 13))
            .foregroundStyle(notes.isEmpty ? .tertiary : .primary)
            .frame(maxWidth: .infinity, alignment: .topLeading)
            .padding(16)
        }
      }
    }
    .background(.background)
    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
  }
  
  private var header: some View {
    HStack(spacing: 8) {
      Text("Speaker Notes")
        .font(.system(size: 11, weight: .semibold))
        .foregroundStyle(.secondary)
        .textCase(.uppercase)
        .tracking(0.5)
      
      Spacer(minLength: 8)
      
      Button {
        isExpanded.toggle()
      } label: {
        Label(
          isExpanded ? "Collapse Notes" : "Expand Notes",
          systemImage: isExpanded ? "chevron.down" : "chevron.up"
        )
        .labelStyle(.iconOnly)
      }
      .buttonStyle(.borderless)
      .help(isExpanded ? "Collapse speaker notes" : "Expand notes")
    }
    .padding(.horizontal, 16)
    .padding(.vertical, 10)
  }
}

// MARK: - Sidebar

/// Floats over the slide preview instead of squeezing it. `.sidebar`
/// vibrancy comes from the `VisualEffectView` placed behind this in
/// `PresenterWindowView`; `.scrollContentBackground(.hidden)` is required
/// so the List's own opaque background doesn't hide that blur.
private struct SlideListSidebar: View {
  var session: PresentationSession
  @Binding var selection: Int?
  
  var body: some View {
    List(selection: $selection) {
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
}

// MARK: - Toolbar pieces

private struct NavigationControls: View {
  var session: PresentationSession
  
  var body: some View {
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
}

private struct ThemeMenu: View {
  var session: PresentationSession
  
  var body: some View {
    Menu {
      ForEach(0..<session.themeCount, id: \.self) { index in
        Button(session.themeName(at: index)) {
          session.currentTheme = index
        }
      }
    } label: {
      Label("Theme", systemImage: "paintpalette")
    }
  }
}

// MARK: - Layout metrics

private enum LayoutMetrics {
  static let window = WindowSize(
    minimum: CGSize(width: 320, height: 320),
    ideal: CGSize(width: 640, height: 480)
  )
  static let sidebarWidth: CGFloat = 280
  static let previewCornerRadius: CGFloat = 14
  static let notesHeaderHeight: CGFloat = 40
  static let notes = SizeRange(minimum: 128, ideal: 320, maximum: 480)
}

private struct WindowSize {
  let minimum: CGSize
  let ideal: CGSize
}

private struct SizeRange {
  var minimum: CGFloat
  var ideal: CGFloat
  var maximum: CGFloat
  
  init(minimum: CGFloat, ideal: CGFloat, maximum: CGFloat) {
    self.minimum = minimum
    self.ideal = ideal
    self.maximum = maximum
  }
  
  /// A fixed, non-resizable band — used for the collapsed notes header.
  init(repeating value: CGFloat) {
    self.init(minimum: value, ideal: value, maximum: value)
  }
}

// MARK: - Audience window

private struct AudienceWindowView: View {
  var session: PresentationSession
  
  var body: some View {
    RenderedSlideView(session: session)
      .aspectRatio(16.0 / 9.0, contentMode: .fit)
      .frame(maxWidth: .infinity, maxHeight: .infinity)
      .background(Color.black)
  }
}

private final class AudienceWindowController: NSObject, NSWindowDelegate {
  private var window: NSWindow?
  private var onClose: (() -> Void)?
  private var isClosing = false
  
  func show(
    session: PresentationSession,
    title: String,
    onClose: @escaping () -> Void
  ) {
    self.onClose = onClose
    
    if let window {
      window.title = audienceTitle(for: title)
      window.contentView = NSHostingView(
        rootView: AudienceWindowView(session: session)
      )
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
    window.contentView = NSHostingView(
      rootView: AudienceWindowView(session: session)
    )
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
    closeWindow()
  }
  
  private func audienceTitle(for presentationTitle: String) -> String {
    presentationTitle.isEmpty
    ? "Audience Window"
    : "\(presentationTitle) - Audience"
  }
  
  private func closeWindow() {
    guard !isClosing else { return }
    isClosing = true
    
    let windowToClose = window
    let closeCallback = onClose
    onClose = nil
    
    closeCallback?()
    releaseWindowAfterCloseAnimation(windowToClose)
  }
  
  private func releaseWindowAfterCloseAnimation(
    _ closingWindow: NSWindow?
  ) {
    DispatchQueue.main.async { [weak self, weak closingWindow] in
      guard let self else { return }
      if self.window === closingWindow {
        self.window?.delegate = nil
        self.window?.contentView = nil
        self.window = nil
      }
      self.isClosing = false
    }
  }
}

// MARK: - Rendered slide

/// Renders on the presentation's fixed 1280x720 virtual canvas. SwiftUI
/// only scales the finished bitmap to fit the preview.
struct RenderedSlideView: View {
  var session: PresentationSession
  var presenterView: Bool = false
  
  @State private var image: CGImage?
  
  var body: some View {
    Group {
      if let image {
        Image(image, scale: 1, label: Text("Slide"))
          .resizable()
          .interpolation(.medium)
          .aspectRatio(
            CGSize(width: image.width, height: image.height),
            contentMode: .fit
          )
      } else {
        Color.black
      }
    }
    .onChange(of: session.renderToken, initial: true) { _, _ in
      render()
    }
  }
  
  private func render() {
    let width = 1280
    let height = 720
    let stride = width * 4
    var pixels = [UInt8](repeating: 0, count: stride * height)
    
    let didRender = pixels.withUnsafeMutableBytes { buffer in
      let pointer = buffer.bindMemory(to: UInt8.self).baseAddress!
      return presenterView
      ? session.renderPresenterView(
        into: pointer, width: width, height: height
      )
      : session.renderSlide(
        into: pointer, width: width, height: height
      )
    }
    guard didRender else { return }
    
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    guard let provider = CGDataProvider(data: Data(pixels) as CFData)
    else { return }
    
    image = CGImage(
      width: width,
      height: height,
      bitsPerComponent: 8,
      bitsPerPixel: 32,
      bytesPerRow: stride,
      space: colorSpace,
      bitmapInfo: CGBitmapInfo(
        rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
        | CGBitmapInfo.byteOrder32Big.rawValue
      ),
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
      .frame(
        width: LayoutMetrics.window.ideal.width,
        height: LayoutMetrics.window.ideal.height
      )
  } else {
    Text("Could not load preview presentation.")
      .frame(
        width: LayoutMetrics.window.ideal.width,
        height: LayoutMetrics.window.ideal.height
      )
  }
}

import SwiftUI
import UniformTypeIdentifiers

// MARK: - UTType extension

extension UTType {
    static let slidesPresentation = UTType(exportedAs: "com.presenter.slides", conformingTo: .data)
}

// MARK: - Model

struct RecentPresentation: Identifiable {
    let id: UUID
    let title: String
    let location: String
    let lastOpened: Date

    init(id: UUID = UUID(), title: String, location: String, lastOpened: Date) {
        self.id = id
        self.title = title
        self.location = location
        self.lastOpened = lastOpened
    }
}

// MARK: - Sample data

private extension RecentPresentation {
    static let samples: [RecentPresentation] = [
        RecentPresentation(
            title: "Sprint Demo",
            location: "~/Documents/Presentations/Sprint Demo.slides",
            lastOpened: Date().addingTimeInterval(-3600 * 2)
        ),
        RecentPresentation(
            title: "Product Update",
            location: "~/Documents/Presentations/Product Update.slides",
            lastOpened: Date().addingTimeInterval(-3600 * 26)
        ),
        RecentPresentation(
            title: "Architecture Review",
            location: "~/Dropbox/Work/Architecture Review.slides",
            lastOpened: Date().addingTimeInterval(-3600 * 24 * 5)
        ),
    ]
}

// MARK: - Root view

struct ContentView: View {
    @State private var recentPresentations: [RecentPresentation] = RecentPresentation.samples
    @State private var isFileImporterPresented = false
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        HStack(spacing: 0) {
            LeftPanel(
                onCreateNew: createNewPresentation,
                onOpen: openPresentation,
                onOpenExample: openExamplePresentation
            )

            Divider()

            RightPanel(
                recent: recentPresentations,
                onOpenRecent: openRecentPresentation,
                onOpenOther: openPresentation
            )
        }
        .frame(minWidth: 740, minHeight: 440)
        .fileImporter(
            isPresented: $isFileImporterPresented,
            allowedContentTypes: [.slidesPresentation],
            allowsMultipleSelection: false
        ) { result in
            handleFileImportResult(result)
        }
    }

    // MARK: - Actions

    private func createNewPresentation() {
        // TODO: new-file workflow
        print("[Presenter] createNewPresentation()")
    }

    private func openPresentation() {
        isFileImporterPresented = true
    }

    private func openExamplePresentation() {
        // Resolve the demo file bundled next to the executable
        let exampleURL = Bundle.main.url(forResource: "demo", withExtension: "slides")
            ?? URL(fileURLWithPath: "demo/demo.slides")
        launch(url: exampleURL)
    }

    private func openRecentPresentation(_ presentation: RecentPresentation) {
        let url = URL(fileURLWithPath: (presentation.location as NSString).expandingTildeInPath)
        launch(url: url)
    }

    private func handleFileImportResult(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            launch(url: url)
        case .failure(let error):
            print("[Presenter] open failed: \(error.localizedDescription)")
        }
    }

    private func launch(url: URL) {
        // Record in recents (deduplicated by path)
        let path = url.path
        let name = url.deletingPathExtension().lastPathComponent
        if !recentPresentations.contains(where: { $0.location == path }) {
            recentPresentations.insert(
                RecentPresentation(title: name, location: path, lastOpened: Date()),
                at: 0
            )
        }
        openWindow(id: "presenter", value: url)
    }
}

// MARK: - Left panel

private struct LeftPanel: View {
    let onCreateNew: () -> Void
    let onOpen: () -> Void
    let onOpenExample: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Spacer()

            appHeader
                .padding(.bottom, 32)

            VStack(alignment: .leading, spacing: 8) {
                WelcomeActionButton(
                    title: "Create New Presentation",
                    subtitle: "Start with a blank canvas",
                    symbol: "plus.rectangle.on.rectangle",
                    isPrimary: true,
                    action: onCreateNew
                )
                WelcomeActionButton(
                    title: "Open Presentation…",
                    subtitle: "Open a .slides file",
                    symbol: "folder",
                    isPrimary: false,
                    action: onOpen
                )
                WelcomeActionButton(
                    title: "Open Example Presentation",
                    subtitle: "Explore a sample deck",
                    symbol: "play.rectangle",
                    isPrimary: false,
                    action: onOpenExample
                )
            }

            Spacer()
        }
        .padding(.horizontal, 36)
        .frame(width: 300)
        .frame(maxHeight: .infinity)
        .background(.background)
    }

    private var appHeader: some View {
        VStack(alignment: .leading, spacing: 10) {
            Image(systemName: "rectangle.on.rectangle.angled")
                .resizable()
                .scaledToFit()
                .frame(width: 56, height: 56)
                .foregroundStyle(Color.accentColor)
                .symbolRenderingMode(.hierarchical)

            VStack(alignment: .leading, spacing: 2) {
                Text("QuickSlides")
                    .font(.title.bold())
                    .foregroundStyle(.primary)

                Text("Create, review, and present")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

// MARK: - WelcomeActionButton

private struct WelcomeActionButton: View {
    let title: String
    let subtitle: String
    let symbol: String
    let isPrimary: Bool
    let action: () -> Void

    @State private var isHovered = false

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: symbol)
                    .frame(width: 24, height: 24)
                    .font(.system(size: 16, weight: .medium))
                    .foregroundStyle(isPrimary ? Color.white : Color.accentColor)

                VStack(alignment: .leading, spacing: 1) {
                    Text(title)
                        .font(.system(size: 13, weight: isPrimary ? .semibold : .regular))
                        .foregroundStyle(isPrimary ? .white : .primary)

                    Text(subtitle)
                        .font(.system(size: 11))
                        .foregroundStyle(isPrimary ? .white.opacity(0.75) : .secondary)
                }

                Spacer()
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 9)
            .background(buttonBackground)
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .contentShape(RoundedRectangle(cornerRadius: 8))
        }
        .buttonStyle(.plain)
        .onHover { hovering in
            withAnimation(.easeInOut(duration: 0.12)) {
                isHovered = hovering
            }
        }
    }

    @ViewBuilder
    private var buttonBackground: some View {
        if isPrimary {
            RoundedRectangle(cornerRadius: 8)
                .fill(Color.accentColor.opacity(isHovered ? 0.85 : 1.0))
        } else {
            RoundedRectangle(cornerRadius: 8)
                .fill(isHovered ? Color.primary.opacity(0.07) : Color.clear)
        }
    }
}

// MARK: - Right panel

private struct RightPanel: View {
    let recent: [RecentPresentation]
    let onOpenRecent: (RecentPresentation) -> Void
    let onOpenOther: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Recent Presentations")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(.secondary)
                .textCase(.uppercase)
                .tracking(0.5)
                .padding(.horizontal, 20)
                .padding(.top, 20)
                .padding(.bottom, 8)

            Divider()
                .padding(.horizontal, 0)

            if recent.isEmpty {
                EmptyRecentPresentationsView()
            } else {
                recentList
            }

            Divider()

            openOtherFooter
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(.background)
    }

    private var recentList: some View {
        ScrollView {
            LazyVStack(spacing: 0) {
                ForEach(Array(recent.enumerated()), id: \.element.id) { index, presentation in
                    RecentPresentationRow(
                        presentation: presentation,
                        onOpen: { onOpenRecent(presentation) }
                    )

                    if index < recent.count - 1 {
                        Divider()
                            .padding(.leading, 52)
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var openOtherFooter: some View {
        Button(action: onOpenOther) {
            HStack {
                Text("Open Other…")
                    .font(.system(size: 12))
                    .foregroundStyle(Color.accentColor)
                Spacer()
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 12)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

// MARK: - RecentPresentationRow

private struct RecentPresentationRow: View {
    let presentation: RecentPresentation
    let onOpen: () -> Void

    @State private var isHovered = false

    private static let dateFormatter: RelativeDateTimeFormatter = {
        let f = RelativeDateTimeFormatter()
        f.unitsStyle = .abbreviated
        return f
    }()

    var body: some View {
        Button(action: onOpen) {
            HStack(spacing: 12) {
                Image(systemName: "doc.richtext")
                    .frame(width: 28, height: 28)
                    .font(.system(size: 20))
                    .foregroundStyle(.secondary)
                    .symbolRenderingMode(.hierarchical)

                VStack(alignment: .leading, spacing: 2) {
                    Text(presentation.title)
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(.primary)
                        .lineLimit(1)

                    Text(presentation.location)
                        .font(.system(size: 11))
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }

                Spacer()

                Text(Self.dateFormatter.localizedString(for: presentation.lastOpened, relativeTo: Date()))
                    .font(.system(size: 11))
                    .foregroundStyle(.tertiary)
                    .fixedSize()
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
            .background(isHovered ? Color.primary.opacity(0.06) : Color.clear)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hovering in
            withAnimation(.easeInOut(duration: 0.1)) {
                isHovered = hovering
            }
        }
    }
}

// MARK: - EmptyRecentPresentationsView

private struct EmptyRecentPresentationsView: View {
    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: "clock.arrow.circlepath")
                .font(.system(size: 36, weight: .light))
                .foregroundStyle(.tertiary)

            VStack(spacing: 4) {
                Text("No Recent Presentations")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundStyle(.secondary)

                Text("Presentations you open will appear here.")
                    .font(.system(size: 11))
                    .foregroundStyle(.tertiary)
                    .multilineTextAlignment(.center)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}

// MARK: - Preview

#Preview {
    ContentView()
}

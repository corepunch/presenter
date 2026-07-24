import SwiftUI
import UniformTypeIdentifiers

// MARK: - UTType extension

extension UTType {
    static let slidesPresentation = UTType(
        exportedAs: "com.corepunch.slides",
        conformingTo: .package
    )
}

// MARK: - Model

struct PresentationOpenRequest: Codable, Hashable {
    let url: URL
    let bookmarkData: Data?
}

struct SlidesPackageDocument: FileDocument {
    static let readableContentTypes: [UTType] = [.slidesPresentation]

    var presentationXML: Data

    init() {
        let xml = """
        <?xml version="1.0" encoding="UTF-8"?>
        <!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/schemas/presentation.dtd">
        <presentation name="Untitled Presentation">
          <slide layout="title" title="Untitled Presentation">
            <subtitle>Add a subtitle</subtitle>
            <notes>Add presenter notes here.</notes>
          </slide>
        </presentation>

        """
        presentationXML = Data(xml.utf8)
    }

    init(configuration: ReadConfiguration) throws {
        guard configuration.file.isDirectory,
              let manifest = configuration.file.fileWrappers?["presentation.xml"],
              let data = manifest.regularFileContents
        else {
            throw CocoaError(.fileReadCorruptFile)
        }
        presentationXML = data
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        FileWrapper(directoryWithFileWrappers: [
            "presentation.xml": FileWrapper(regularFileWithContents: presentationXML)
        ])
    }
}

struct RecentPresentation: Codable, Identifiable {
    let id: UUID
    let title: String
    let location: String
    let lastOpened: Date
    let bookmarkData: Data?

    init(id: UUID = UUID(),
         title: String,
         location: String,
         lastOpened: Date,
         bookmarkData: Data?) {
        self.id = id
        self.title = title
        self.location = location
        self.lastOpened = lastOpened
        self.bookmarkData = bookmarkData
    }
}

private enum RecentPresentationStore {
    private static let key = "recentPresentations"

    static func load() -> [RecentPresentation] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let presentations = try? JSONDecoder().decode([RecentPresentation].self, from: data)
        else {
            return []
        }
        return presentations
    }

    static func save(_ presentations: [RecentPresentation]) {
        guard let data = try? JSONEncoder().encode(presentations) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }
}

// MARK: - Root view

struct ContentView: View {
    var openPresenterWindow: (PresentationOpenRequest) -> Void = { _ in }
    @State private var recentPresentations: [RecentPresentation] = RecentPresentationStore.load()
    @State private var isFileImporterPresented = false
    @State private var isFileExporterPresented = false
    @State private var newPresentation = SlidesPackageDocument()

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
        .fileExporter(
            isPresented: $isFileExporterPresented,
            document: newPresentation,
            contentType: .slidesPresentation,
            defaultFilename: "Untitled"
        ) { result in
            handleFileExportResult(result)
        }
    }

    // MARK: - Actions

    private func createNewPresentation() {
        newPresentation = SlidesPackageDocument()
        isFileExporterPresented = true
    }

    private func openPresentation() {
        isFileImporterPresented = true
    }

    private func openExamplePresentation() {
        let exampleURL = Bundle.main.url(
            forResource: "Nature Portfolio",
            withExtension: "slides",
            subdirectory: "demo"
        ) ?? URL(fileURLWithPath: "demo/Nature Portfolio.slides")
        launch(url: exampleURL)
    }

    private func openRecentPresentation(_ presentation: RecentPresentation) {
        let url = URL(fileURLWithPath: (presentation.location as NSString).expandingTildeInPath)
        launch(url: url, bookmarkData: presentation.bookmarkData)
    }

    private func handleFileImportResult(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            let bookmarkData = try? url.bookmarkData(
                options: .withSecurityScope,
                includingResourceValuesForKeys: nil,
                relativeTo: nil
            )
            launch(url: url, bookmarkData: bookmarkData)
        case .failure(let error):
            print("[Presenter] open failed: \(error.localizedDescription)")
        }
    }

    private func handleFileExportResult(_ result: Result<URL, Error>) {
        switch result {
        case .success(let url):
            let bookmarkData = try? url.bookmarkData(
                options: .withSecurityScope,
                includingResourceValuesForKeys: nil,
                relativeTo: nil
            )
            launch(url: url, bookmarkData: bookmarkData)
        case .failure(let error):
            print("[Presenter] save failed: \(error.localizedDescription)")
        }
    }

    private func launch(url: URL, bookmarkData: Data? = nil) {
        let path = url.path
        let name = url.deletingPathExtension().lastPathComponent
        recentPresentations.removeAll { $0.location == path }
        recentPresentations.insert(
            RecentPresentation(
                title: name,
                location: path,
                lastOpened: Date(),
                bookmarkData: bookmarkData
            ),
            at: 0
        )
        recentPresentations = Array(recentPresentations.prefix(12))
        RecentPresentationStore.save(recentPresentations)

        openPresenterWindow(PresentationOpenRequest(url: url, bookmarkData: bookmarkData))
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

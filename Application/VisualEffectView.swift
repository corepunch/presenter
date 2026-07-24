import AppKit
import SwiftUI

/// Bridges `NSVisualEffectView` into SwiftUI so content can sit on top of
/// native macOS vibrancy — the same blur Finder uses for its sidebar, Mail
/// uses for its message list, and Xcode uses for its navigator. SwiftUI's
/// `Material` presets (`.regularMaterial` etc.) don't expose the
/// `.sidebar` vibrancy specifically, so this is the correct bridge rather
/// than a workaround.
struct VisualEffectView: NSViewRepresentable {
  var material: NSVisualEffectView.Material = .sidebar
  var blendingMode: NSVisualEffectView.BlendingMode = .withinWindow
  var isEmphasized = false
  
  func makeNSView(context: Context) -> NSVisualEffectView {
    let view = NSVisualEffectView()
    view.state = .active
    return view
  }
  
  func updateNSView(_ view: NSVisualEffectView, context: Context) {
    view.material = material
    view.blendingMode = blendingMode
    view.isEmphasized = isEmphasized
  }
}

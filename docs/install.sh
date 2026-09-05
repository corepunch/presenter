#!/bin/sh
set -eu

REPO="corepunch/presenter"
BINARY="presenter"
INSTALL_DIR="${PRESENTER_INSTALL_DIR:-$HOME/.local/bin}"

# --- detect OS ---
detect_os() {
  os="$(uname -s)"
  case "$os" in
    Linux*)  echo "linux" ;;
    Darwin*) echo "darwin" ;;
    *)       echo "unsupported" ;;
  esac
}

# --- detect arch ---
detect_arch() {
  arch="$(uname -m)"
  case "$arch" in
    x86_64|amd64)   echo "x86_64" ;;
    arm64|aarch64)   echo "arm64" ;;
    *)               echo "unsupported" ;;
  esac
}

# --- download with fallback ---
download() {
  url="$1"
  dest="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$url" -o "$dest"
  elif command -v wget >/dev/null 2>&1; then
    wget -qO "$dest" "$url"
  else
    echo "Error: neither curl nor wget found" >&2
    exit 1
  fi
}

# --- main ---
main() {
  os="$(detect_os)"
  arch="$(detect_arch)"

  if [ "$os" = "unsupported" ]; then
    echo "Error: unsupported OS: $(uname -s)" >&2
    echo "Supported: Linux, macOS" >&2
    exit 1
  fi
  if [ "$arch" = "unsupported" ]; then
    echo "Error: unsupported architecture: $(uname -m)" >&2
    echo "Supported: x86_64, arm64" >&2
    exit 1
  fi

  # determine latest release if no version specified
  version="${1:-}"
  if [ -z "$version" ]; then
    echo "Fetching latest release..."
    redirect_url=""
    if command -v curl >/dev/null 2>&1; then
      redirect_url="$(curl -fsSL -o /dev/null -w '%{url_effective}' "https://github.com/$REPO/releases/latest" 2>/dev/null || true)"
    fi
    if [ -n "$redirect_url" ]; then
      version="$(echo "$redirect_url" | sed 's|.*/tag/||')"
    fi
    if [ -z "$version" ]; then
      echo "Error: could not determine latest version. Specify a version: install.sh v1.0.0" >&2
      exit 1
    fi
  fi

  tag="$version"
  archive="${BINARY}-${os}-${arch}.tar.gz"
  url="https://github.com/${REPO}/releases/download/${tag}/${archive}"

  echo "Installing Presenter ${tag} for ${os}-${arch}..."

  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT

  echo "Downloading $url..."
  download "$url" "$tmpdir/$archive"

  echo "Extracting..."
  tar -xzf "$tmpdir/$archive" -C "$tmpdir"

  # install
  mkdir -p "$INSTALL_DIR"
  cp "$tmpdir/$BINARY" "$INSTALL_DIR/$BINARY"
  chmod +x "$INSTALL_DIR/$BINARY"

  # copy assets and share if present
  if [ -d "$tmpdir/assets" ]; then
    cp -r "$tmpdir/assets" "$INSTALL_DIR/"
  fi
  if [ -d "$tmpdir/share" ]; then
    cp -r "$tmpdir/share" "$INSTALL_DIR/"
  fi

  echo ""
  echo "Installed: $INSTALL_DIR/$BINARY"
  echo ""

  # check PATH
  case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
      echo "Note: $INSTALL_DIR is not in your PATH."
      echo "Add it to your shell profile:"
      echo ""
      echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
      echo ""
      ;;
  esac

  echo "Run: presenter --help"
}

main "$@"

# Third-party components

This project vendors the following third-party code. Each retains its own
license (all MIT-compatible); the original license headers are preserved in the
source files.

| Component | Where | License | Source |
|-----------|-------|---------|--------|
| QR Code generator library (C) | `examples/companion_radio/ui-new/qrcodegen.{h,cpp}` | MIT | Project Nayuki — https://www.nayuki.io/page/qr-code-generator-library |

Used by the on-device chat to render a scanned-message URL as a QR code
(compiled in via `-D HAVE_QRCODEGEN=1`). `qrcodegen.cpp` is the upstream C
source compiled as C++; it is otherwise unmodified.

MeshCore itself and its upstream dependencies are covered by the project's main
license (see `license.txt`).

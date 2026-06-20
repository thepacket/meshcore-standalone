# Roadmap

> ⚠️ **Early development.** This is a direction, not a promise — items and ordering
> will change. Nothing here is committed to a date.

`meshcore-standalone` aims to make a MeshCore companion device fully usable on its
own (no phone app), touch-first, and portable across devices. The closed/paid
**MeshOS** is the reference bar for the *experience* — we take UX/feature ideas as
inspiration only (it is closed source; no code is copied), and differentiate by
being **open, free (MIT), and transparent**.

## Milestones

### M1 — Settings ✅ (done)
Full on-device configuration via a portable, data-driven settings UI (radio,
contacts, message, position, telemetry, experimental, device). Touch + keyboard +
trackball. Exercised via the desktop simulator. *Pending on-hardware validation.*

### M2 — Messaging (next)
On-device reading and sending, **channels-first** (channels are the primary medium),
then direct messages.
- Channels tab (incl. the pre-seeded Public channel) and a DMs tab.
- **Contact-type-aware actions** (`ADV_TYPE_*`): DM a user, log in to a
  repeater/room server, request telemetry from a sensor — not a flat "DM" list.
- Compose via on-screen keyboard / physical keyboard; delivery/ack indicators.
- Baseline history is **volatile, in-RAM** (no storage assumptions) — matches the
  companion firmware, which persists no message history.
- Stretch: store-and-forward replay of missed messages; unread counts; emoji.

### M3 — Persistence (T-Deck SD card)
Optional, device-gated. The T-Deck Plus has an SD slot (MeshOS uses it for saved
messages + maps). Use it to persist recent message history and prefs **as an
enhancement**, while the portable base UI keeps working (volatile) on boards
without storage.

### M4 — Maps
Offline map tiles from SD, node display, share location, route/path tracing,
touch + trackball navigation. Large effort; later.

## Cross-cutting / polish
- Notifications (buzzer/vibration where the board has them), screen-timeout tuning.
- Continued UI refinement (validated in the simulator before hardware).
- Broaden device coverage beyond the T-Deck Plus.

## Gating principle
Nothing is called "stable" until validated on real hardware. The simulator covers
UI logic/layout, not the radio, touch panel, or storage.

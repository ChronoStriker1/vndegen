# VNDegen

SwiftUI app for macOS, iPhone, and iPad that runs PC visual novel and RPG Maker games natively. See `README.md` for features and layout, `docs/roadmap.md` for the agreed plan.

## Building
This repo is edited on Linux; Apple builds run on a Mac over SSH. `scripts/remote.sh scripts/build.sh test|mac|ios-sim|ipa` syncs the checkout to the Mac mini and builds there. `scripts/sim.sh` drives simulators.

## Git and releases
- Work lands on `dev`. Commit and push without asking at sensible points.
- After a new working engine plugin or a major set of updates or fixes, publish a release: fast-forward `main` to `dev`, write `docs/releases/<version>.md`, push, then run `scripts/release.sh <version>` from `main`. It builds the unsigned IPA and the ad-hoc signed macOS app and uploads both to GitHub Releases. Bump the patch version for fixes and the minor version for a new engine.
- the maintainer tests releases on real hardware himself and reports bugs.

## Reviews
Code review is by Codex `gpt-6.1-sol` (xhigh). Claude subagents run Opus 5.5 at medium or high only.

# VNDegen

SwiftUI app for macOS, iPhone, and iPad that runs PC visual novel and RPG Maker games natively. See `README.md` for features and layout, `docs/roadmap.md` for the agreed plan.

## Building
This repo is edited on Linux; Apple builds run on a Mac over SSH. `scripts/remote.sh scripts/build.sh test|mac|ios-sim|ipa` syncs the checkout to the Mac mini and builds there. `scripts/sim.sh` drives simulators.

## Git and releases
- Work lands on `dev`. The coordinator may commit and push without asking at sensible points. Delegates commit only on their assigned branches and never push or merge. Top-level feature threads may push and merge into `dev` when their task instructions authorize it.
- Internal workers read the shared `common.md` in the project's private, gitignored `.audit/briefs/` folder and their task brief. The shared brief replaces the mandatory poteto-agent and poteto-mode reads. Device commands are an optional reference in `device-panel.md` there.
- `scripts/check-public.sh` must pass before anything is published; `release.sh` runs it.
- The release coordinator publishes after a new working engine plugin or a major set of updates or fixes. Feature threads report release readiness and do not run releases. To publish: fast-forward `main` to `dev`, write `docs/releases/<version>.md`, push, then run `scripts/release.sh <version>` from `main`. It sets `MARKETING_VERSION` in `project.yml` to the release version and commits it, builds the unsigned IPA and the ad-hoc signed macOS app, uploads both to GitHub Releases, adds the version to `altstore/source.json` on `main`, and pushes. Then merge `main` back into `dev`. Bump the patch version for fixes and the minor version for a new engine.
- As threads land work, the coordinator updates every project document it affects (`README.md`, `docs/roadmap.md`, engine notes in `docs/`, and the private briefs) and settles each thread once it has met its goal.
- The maintainer tests releases on real hardware and reports bugs.

## Reviews
Code review is by Codex `gpt-6.1-sol` (xhigh). Claude subagents run Opus 5.5 at medium or high only.

## Public repo
The repo is public. Committed files and messages never contain LAN hostnames, IPs, NAS or home paths, or personal names. The build Mac's hostname lives in `~/.config/vndegen/mac-host`.

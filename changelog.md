# v0.3.0-alpha.36

- Add experimental 480 TPS Solve under **Pause → PathWeaver → Settings**. Solve defaults to 240 TPS.
- Play each saved solution at its recorded tick rate and restore the previous Eclipse timing settings afterward.
- Retry an exhausted single-input 480 TPS search with a wider frontier if the first pass discarded states. This can recover tight timings such as buffered cube jumps.
- Use the inputs from the attempt that actually ran when recording corrections after a rewind.

# v0.3.0-alpha.35

- Use the new omega artwork for the Geode logo and pause menu button.

# v0.3.0-alpha.34

- Give native pause and completion menus priority over the debug Replay transport.
- Adopt coin tables on the game thread after the search result is validated; keep worker plan loading separate from session prelude state.
- Snapshot the ladder's HUD threshold before starting its worker and avoid reading its changing anchor for live-reset labels.
- Synchronize checkpoint provenance reads with live probe updates.
- Keep worker attempt labels tied to the job while checkpoint flights advance the live attempt.

# v0.3.0-alpha.33

- Wait for search cancellation before Stop, Quit, either editor exit or a fresh level entry clears shared state.
- Keep one pending transition per search job; repeated requests do not repeat cleanup and explicit exits override queued replay entry.
- Delay budget teardown and core runEnd until worker completion and native unwind. Keep record/save blocking through deferred exits.
- Bind late reaper callbacks to the exact job and retain context if stopping times out.

# v0.3.0-alpha.32

- Allow Export part to use an ordinary from-start flight on a reduced level copy.
- Verify the selected part from a fresh entry on the original level before enabling export.
- Keep copy candidates separate from Save/Continue proof; refuse changed levels, failed verification and mismatched P1/P2 inputs or holds.

# v0.3.0-alpha.31

- Bind the Save and stop acknowledgment to the exact completed search job.
- Keep the shared ladder flag intact until the stop acknowledgment.
- Add a reusable stop barrier and controlled-thread tests. Integration with ordinary Stop, Quit and Edit remains in progress; this is not a complete worker-lifecycle fix.

# v0.3.0-alpha.30

- Fix Save / Continue writes whose temporary filename exceeded Windows' path limit.
- Add a reproducible pinned Windows build with bindings ABI checks and nine headless test suites.
- Add build and reviewer guides; correct the inherited build instructions for the supported game version.

# v0.3.0-alpha.29

- Export the tested start directly from an active Solve with Export part, without finishing the level.
- Stop search through the existing cancellation barrier and verify the selected segment from a fresh entry before export.
- Keep verified partial export available in the current level after closing the format picker.
- Shorten the Geode description and README to focus on use, settings and essential limits.

# v0.3.0-alpha.28

- Split everyday settings from advanced search choices and rewrote help text and keybind descriptions.
- Added Reset search to restore the established search defaults.
- Show the mouse cursor when opening the solved-level prompt and export.
- Renamed Standard / FB to Standard.
- Select several export profiles at once, with separate GDR/GDR2 choices per supported profile. The existing xdBot laboratory adapter remains GDR-only.
- Select and remember an export folder with Path.
- Open the folder picker on first use even when the default exports directory does not exist.
- Display folder names with non-Latin characters and open exported folders on Windows without a false failure alert.
- Keep the export close button inside the window and show N/A for unsupported profile formats.
- Distinguish simultaneous profile exports with filename suffixes while preserving collision numbering.

# v0.3.0-alpha.27

## Interface

- Added the new PathWeaver package logo and pause-menu icon.
- Placed the pause button below the settings gear.
- Updated the GitHub README, Geode description, credits and compatibility notes for this alpha.

## Search and replay

- Enter levels in Normal mode and access PathWeaver from pause.
- Use independent P1/P2 inputs in native section search with shared-world verification.
- Optionally prefer roomier ship/wave corridors and fewer input switches without a hard CPS cap.
- Keep the default HUD compact and enable detailed diagnostics in settings.
- After Solve succeeds, choose Play or Exit instead of starting playback immediately.

## Save and continue

- Save the confirmed search prefix with Save and stop.
- Continue an unchanged level after replaying the saved prefix from a fresh entry.
- Recheck and continue a saved path on an edited level.
- Verify and export the solved part of an unfinished level, including a prefix with no clicks.
- Preserve the saved input boundary and held-button state during partial export.

## Macro export

- Select GDR and GDR2 independently, with Standard / FB, Eclipse 1.9.4 and GDR-only xdBot Lab profiles.
- Verify a saved full solution from the start when needed, then open the export picker.
- Use the level name with underscores and numbered filenames to avoid overwriting existing exports.
- Refuse GDR2 when it cannot preserve the required same-frame P1/P2 event order.
- Mark unfinished segments as Partial and record their exact duration.

## Current scope

- Windows x64, Geometry Dash 2.2081, Geode 5.11.2.
- Solve remains native 240 TPS. Experimental 480 Replay requires a pinned timing setup.
- Save / Continue / Recheck are qualified for classic levels at 240 TPS with Flip Controls off. Resuming levels with moving trigger groups is still experimental.
- Eclipse 1.9.4 continues playing after a partial macro's recorded end. Stop playback manually or use a matching level cut.
- Difficult two-player sections, long traps and route quality remain areas of active development.

This entry summarizes the first PathWeaver alpha prepared for public review, including features developed before alpha.27. The upstream gdsolver release history is available in its own repository.

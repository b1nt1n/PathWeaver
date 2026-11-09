<p align="center">
  <img src="logo.png" width="112" height="112" alt="PathWeaver logo">
</p>
<h1 align="center">PathWeaver</h1>
<p align="center">Automatic path search and macro export for Geometry Dash.</p>
<p align="center">
  <img src="https://img.shields.io/badge/status-alpha-24CFA0?style=flat" alt="Alpha">
  <img src="https://img.shields.io/badge/Windows-x64-647FB6?style=flat" alt="Windows x64">
  <img src="https://img.shields.io/badge/Geometry_Dash-2.2081-424242?style=flat" alt="Geometry Dash 2.2081">
  <img src="https://img.shields.io/badge/Geode-5.11.2-424242?style=flat" alt="Geode 5.11.2">
</p>
<p align="center">
  <a href="https://github.com/b1nt1n/PathWeaver/releases">Download</a> ·
  <a href="#use">Use</a> ·
  <a href="changelog.md">Changelog</a> ·
  <a href="https://github.com/b1nt1n/PathWeaver/issues">Report a bug</a> ·
  <a href="https://discord.gg/VwtQr8ZUqJ">Discord</a>
</p>

PathWeaver finds a path through a classic Geometry Dash level, checks it in the game, and exports the inputs as a macro. This is an alpha.

**Windows x64 · Geometry Dash 2.2081 · Geode 5.11.2 · Solve at 240 TPS**

## Install

1. Download `b1nt1n.pathweaver.geode` from the [release assets](https://github.com/b1nt1n/PathWeaver/releases).
2. Close the game and put it in `<game>/geode/mods/`.
3. Start the game. Disable upstream gdsolver before loading PathWeaver.

## Use

Enter a level and pause. Click the **PathWeaver logo below the settings gear**.

- **Solve** searches from the beginning. At **Level solved!**, choose **Play** to verify the saved solution or **Exit** to leave.
- **Replay** plays a saved solution.
- **Export** saves the selected profiles and formats. Use **Verify** first if requested.
- **Export part**, available during Solve, stops search and verifies the already tested start. The level does not need to reach 100%.
- **Save and stop** keeps a search. Use **Continue** later, or **Recheck and continue** after editing the level.

## Export

Tick GDR/GDR2 separately for **Standard** and **Eclipse 1.9.4**. The **xdBot (lab)** adapter supports GDR only. You can select several profiles at once.

**Path** chooses a folder and remembers it across restarts. Names use the level title with underscores. Multiple profiles add `_Eclipse` and `_xdBot`; repeated exports add a number. Existing files are preserved.

A partial macro contains the solved start and its duration. **Eclipse 1.9.4 keeps the level running after that duration**, so stop playback yourself or use a matching level cut. GDR2 is unavailable when it cannot preserve the required P1/P2 event order.

Use the same level copy and detail mode in the player, at the exported TPS.

## Settings and limits

Keep the search defaults. **Reset search** restores them. **Simpler ship/wave paths** is optional and experimental, with no CPS cap. **Detailed diagnostics** is normally off; it adds the HUD and logs for bug reports.

Classic levels only. Some paths, late traps and difficult two-player sections still fail or take a long time. Save / Continue / Recheck are qualified at 240 TPS with Flip Controls off; moving trigger groups remain experimental. Solve runs at 240 TPS. The 480 TPS Replay profile is experimental.

Automated sessions block normal progress, statistics and achievements. Manual play records normally. To uninstall, close the game and remove the `.geode` file.

## Build

Use a Visual Studio x64 Developer PowerShell with PowerShell 7, Git, CMake 3.29+, Ninja, LLVM 21.1.8 (`clang-cl` and `lld-link`), and Geode CLI 3.9.0. Use Geode SDK 5.11.2 and check out `geode-sdk/bindings` at `1605f500d053f8de122bf5c7dc6d17c24866e02e`.

```powershell
./tools/build-windows.ps1 -SdkPath ../deps/geode-sdk-5.11.2 `
    -BindingsPath ../deps/bindings -BuildDirectory ../build-pathweaver `
    -GeodeCli ../deps/geode.exe
```

The script prepares the GD 2.2081 bindings in the build directory, checks their ABI, and writes `b1nt1n.pathweaver.geode` to the selected output directory.

## Credits

Bug reports should include the level ID, mod version, failing section and other input/timing mods.

Maintained by [b1nt1n](https://github.com/b1nt1n), based on [gdsolver](https://github.com/gdsolver/gdsolver) v0.6.1 at `d09300a6f4c7946f26015fc86a6cb33e8c891a7a`. Original authorship and the MIT notice are preserved in [LICENSE](LICENSE). See [Credits](CREDITS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

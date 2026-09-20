# Envy Client

Envy is a utility mod client for Minecraft: Bedrock Edition (Windows), built on the
open-source [Latite](https://github.com/LatiteClient/Latite) client (GPL-3). It ships
as a single `Envy.dll` you inject into the game and adds a customizable ClickGUI, a
HUD editor, JavaScript plugin support and a large module set (FPS counter, keystrokes,
zoom, freelook, fullbright, potion HUD, totem counter and more).

## Downloads

Grab `Envy.dll` from the [Releases](https://github.com/Envy-Studios/envy-client/releases)
page and load it into Minecraft Bedrock with the DLL injector of your choice. There is
no sign-in or product key - the DLL is the whole client. Early builds are marked as
pre-releases: expect rough edges and report anything odd you run into.

Some antiviruses flag DLL injectors as risky tools. If yours complains, add an exception
for your injector.

## Compatibility

Envy stays compatible with the [Latite scripting API](https://latitescripting.github.io):
existing JavaScript plugins from the community registry keep working.

## Building

- Windows 10/11 with Visual Studio 2022 (MSVC, C++23) or the CMake presets with Ninja
- Open `EnvyRewrite.sln`, or:

```sh
cmake --preset x64-release
cmake --build out/build/x64-release --parallel
```

The build produces `out/build/x64-release/Envy.dll` (the client).

## CI

The `Build DLL` GitHub Actions workflow builds on a Windows runner and publishes
`Envy.dll` as a release on every manual dispatch (Actions > Build DLL > Run workflow).

## License

Envy inherits Latite's GNU GPL-3.0 license - see [LICENSE](LICENSE).

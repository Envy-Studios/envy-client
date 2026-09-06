# Envy Client

Envy is a utility mod client for Minecraft: Bedrock Edition (Windows), forked from
[Latite](https://github.com/Latite-Client/LatiteClient). It injects as a DLL into the
game and adds a fully customizable ClickGUI, a HUD editor, JavaScript plugin support
and 30+ modules (FPS counter, keystrokes, zoom, freelook, waypoints, fullbright and more).

## Downloads

Grab the latest `Envy.dll` from the [Releases](https://github.com/Envy-Studios/envy-client/releases)
page and load it with your preferred DLL loader.

## Building

- Windows 10/11 with Visual Studio 2022 (MSVC, C++23) or the CMake presets with Ninja
- Open `Envy.sln`, or:

```sh
cmake --preset x64-release
cmake --build out/build/x64-release --parallel
```

The built DLL is written to `out/build/x64-release/Envy.dll`.

## CI

The `Build DLL` GitHub Actions workflow builds on a Windows runner and publishes
`Envy.dll` as a release on every manual dispatch (Actions → Build DLL → Run workflow).

## Plugins

Envy supports JavaScript plugins (ChakraCore) and stays compatible with the
[Latite scripting API](https://github.com/LatiteScripting/Scripts).

## License

Envy is licensed under the GNU AGPL-3.0 — see [LICENSE](LICENSE).

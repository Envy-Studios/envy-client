# Envy Client

![Stars](https://img.shields.io/github/stars/Envy-Studios/envy-client?style=flat-square&logo=github&label=Stars&color=8957e5)
![Forks](https://img.shields.io/github/forks/Envy-Studios/envy-client?style=flat-square&logo=github&label=Forks&color=8957e5)
![Watchers](https://img.shields.io/github/watchers/Envy-Studios/envy-client?style=flat-square&logo=github&label=Watchers&color=8957e5)
![Contributors](https://img.shields.io/github/contributors/Envy-Studios/envy-client?style=flat-square&logo=github&label=Contributors&color=8957e5)
![Last Commit](https://img.shields.io/github/last-commit/Envy-Studios/envy-client/main?style=flat-square&logo=github&label=Last%20Commit&color=2da44e)
![Commits/month](https://img.shields.io/github/commit-activity/m/Envy-Studios/envy-client?style=flat-square&logo=github&label=Commits%2Fmonth&color=2da44e)
![Repo Size](https://img.shields.io/github/repo-size/Envy-Studios/envy-client?style=flat-square&logo=github&label=Repo%20Size&color=0969da)
![Top Language](https://img.shields.io/github/languages/top/Envy-Studios/envy-client?style=flat-square&logo=github&label=Top%20Language&color=0969da)
![Release](https://img.shields.io/github/v/release/Envy-Studios/envy-client?style=flat-square&logo=github&label=Release&color=d29922)
![Downloads](https://img.shields.io/github/downloads/Envy-Studios/envy-client/total?style=flat-square&logo=github&label=Downloads&color=d29922)
![Issues](https://img.shields.io/github/issues/Envy-Studios/envy-client?style=flat-square&logo=github&label=Issues&color=d1242f)
![Pull Requests](https://img.shields.io/github/issues-pr/Envy-Studios/envy-client?style=flat-square&logo=github&label=Pull%20Requests&color=d1242f)
![License](https://img.shields.io/github/license/Envy-Studios/envy-client?style=flat-square&logo=github&label=License&color=8250df)

Envy is a utility mod client for Minecraft: Bedrock Edition (Windows), forked from
[Latite](https://github.com/Latite-Client/LatiteClient). It injects as a DLL into the
game and adds a fully customizable ClickGUI, a HUD editor, JavaScript plugin support
and 45+ modules (FPS counter, keystrokes, zoom, freelook, waypoints, fullbright and more).

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

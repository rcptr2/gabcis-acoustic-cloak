# Gabci's Acoustic Cloak

> Tranziensérzékeny fáziselfedés, amely helyet csinál a lábdobnak a basszuson belül.

[![Licenc: AGPL v3](https://img.shields.io/badge/licenc-AGPL--3.0-blue.svg)](LICENSE)
[![Build](https://github.com/rcptr2/gabcis-acoustic-cloak/actions/workflows/build.yml/badge.svg)](https://github.com/rcptr2/gabcis-acoustic-cloak/actions/workflows/build.yml)
![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20macOS%20Intel-lightgrey)
![Formátum](https://img.shields.io/badge/form%C3%A1tum-VST3%20%7C%20Standalone-green)

🇬🇧 *English documentation: [README.md](README.md)*

A sidechain kompresszor úgy oldja meg a lábdob–basszus ütközést, hogy lehalkítja a basszust. Az
Acoustic Cloak szintváltozás nélkül oldja meg: minden lábdob-tranziens idejére elforgatja a basszus
fázisát egy szűk célsávon belül, így a basszus félreáll a lábdob elől, majd visszalép, amikor a
tranziens elmúlt. A basszus érzékelt hangossága ott marad, ahol volt — csak az interferencia tűnik el.

[JUCE](https://juce.com) alapon készült. A `processBlock` az audioszálon allokációmentes.

## Hogyan működik

- **Busz-elrendezés** — sztereó Main be- és kimenet (a basszus), plusz opcionális Sidechain bemeneti
  busz (alapból kikapcsolva, mono vagy sztereó — a lábdob).
- **Sávizolátor** — a kezelést a Target Freq Low és Target Freq High közötti tartományra szűkíti, így
  az ütközési zónán kívül semmi nem sérül.
- **Allpass/Hilbert hálózat** — előállítja azt a kvadratúra-párt, ami a folyamatos,
  amplitúdótartó fázisforgatáshoz kell az izolált sávon belül.
- **Rezonátorbank** — komplex rezonátorok követik a sáv tonális tartalmát, így a forgatás az anyaghoz
  igazodik, nem egy rögzített beállításhoz.
- **Fáziskorrelációs analizátor** — méri a basszus–lábdob viszonyt, ami az elfedést vezérli.
- **Tranziensdetektálás** — az elfedés a lábdob tranziensére kapcsol be, és utána elenged; ezt a
  Transient Sensitivity szabályozza.
- **Fázisradar-kijelző** — poláris megjelenítés az aktuális fázisviszonyról.

A `Lookahead` egy egyszerű késleltetővonal a Main úton, ami időbeli tartalékot tart fenn a
forgatónak. A keletkező latenciát a plugin jelenti a hosztnak, méghozzá az üzenetszálra halasztva —
a jelentett latencia audioszálról való módosítása a legtöbb DAW-ban kimaradásokat okoz.

## Paraméterek

| Paraméter | Tartomány | Alapérték | Leírás |
|---|---|---|---|
| Cloak Depth | 0 – 100 % | 100 % | Mennyire erősen forgatja a fázist a lábdob-tranziens alatt. |
| Lookahead | 0 – 20 ms | 0 ms | Időbeli tartalék a forgatónak; latenciaként jelentve a hosztnak. |
| Target Freq Low | 20 – 200 Hz | 20 Hz | A kezelt sáv alsó határa. |
| Target Freq High | 30 – 300 Hz | 150 Hz | A kezelt sáv felső határa. |
| Transient Sensitivity | 0 – 100 % | 50 % | Az a küszöb, amelynél a lábdob-tranziens bekapcsolja az elfedést. |
| Bypass | be / ki | ki | Teljes kihagyás. |

## Telepítés

A kész binárisok a [Releases](https://github.com/rcptr2/gabcis-acoustic-cloak/releases) oldalon
találhatók.

### Windows x64

1. Töltsd le az `AcousticCloak-vX.Y.Z-Windows-x64-VST3.zip` fájlt.
2. Csomagold ki, és másold az `Acoustic Cloak.vst3` mappát ide: `C:\Program Files\Common Files\VST3\`.
3. Futtass plugin-újrakeresést a DAW-odban.

### macOS (Intel)

A macOS bináris **x86_64 (Intel)**. Intel Maceken natívan fut, Apple Siliconon Rosetta 2-vel, Intel
módban futó hosztban; arm64 változat nincs.

1. Töltsd le az `AcousticCloak-vX.Y.Z-macOS-Intel-VST3.zip` fájlt.
2. Csomagold ki, és másold az `Acoustic Cloak.vst3`-at ide: `/Library/Audio/Plug-Ins/VST3/`
   (vagy `~/Library/Audio/Plug-Ins/VST3/`, ha csak a saját felhasználódnak kell).
3. A build nincs notarizálva, ezért töröld róla a karantén jelzőt:
   ```bash
   xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Acoustic Cloak.vst3"
   ```
4. Futtass plugin-újrakeresést a DAW-odban.

### Routing

Tedd az Acoustic Cloakot a basszus sávra, és a lábdobot vezesd a sidechain bemenetére.

## Fordítás forrásból

### Követelmények

- CMake 3.24 vagy újabb
- C++20-as fordító — Windowson **Visual Studio 2022** („Desktop development with C++"),
  macOS-en **Xcode 15+**
- Git

A JUCE 9.0.0 verziója rögzítve van a `CMakeLists.txt`-ben, és a CMake `FetchContent` konfiguráláskor
automatikusan letölti. MinGW nem támogatott: a JUCE kifejezetten tiltja, és a Windows-backendje
MSVC-intrinsiceket, valamint a Direct2D/DirectWrite fejléceket igényel.

> **A build-mappa útvonalában nem lehet aposztróf.** A JUCE által generált VST3 `POST_BUILD` lépések
> nem escape-elik az aposztrófot az általuk kiadott shell-parancsláncokban. A `CMakeLists.txt` ezt
> ellenőrzi, és érthető hibaüzenettel áll le ahelyett, hogy később hasalna el.

### Windows

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DACOUSTICCLOAK_BUILD_TESTS=OFF
cmake --build build --config Release --target AcousticCloakPlugin_VST3
```

### macOS

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64 -DACOUSTICCLOAK_BUILD_TESTS=OFF
cmake --build build --config Release --target AcousticCloakPlugin_VST3
```

A kész csomag ide kerül:
`build/AcousticCloakPlugin_artefacts/Release/VST3/Acoustic Cloak.vst3`.

### Tesztek

A tesztkészlet [Catch2](https://github.com/catchorg/Catch2)-t használ (automatikusan letöltődik), és
regisztrál a CTest-be:

```bash
cmake -S . -B build -DACOUSTICCLOAK_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Külön konzolalkalmazás, az `AcousticCloakLoadTest`, a blokkonkénti CPU-költséget méri.

## Mappastruktúra

```
Source/          A plugin forrása — processzor, szerkesztő, sávizolátor, Hilbert-hálózat,
                 rezonátorbank, korrelációs analizátor, fázisradar, megjelenés
Tests/           Catch2 egységtesztek és CPU-terhelési konzolalkalmazás
docs/            Tervezési blueprint és PDF-ismertetők (EN/HU)
CMakeLists.txt   Build-definíció; rögzíti a JUCE 9.0.0-t
CHANGELOG.md     Fejlesztési előzmények fázisonként
```

## Tesztelve

- **macOS** (Intel, x86_64) — FL Studio 2026
- **Windows 11 x64** — FL Studio 2026

## Licenc

**GNU Affero General Public License v3.0 vagy újabb** alatt jelenik meg — lásd a [LICENSE](LICENSE)
fájlt.

Ez a választás nem önkényes. Az Acoustic Cloak JUCE 9-cel készült, amely kettős licencű: AGPLv3 vagy
kereskedelmi JUCE-licenc. Az AGPLv3 ág az, ami ingyenesen engedi a forrásból épített bináris
terjesztését — cserébe minden származtatott művet ugyanezen feltételek alatt, elérhető forrással kell
kiadni.

## Attribúció

- [JUCE](https://juce.com) — © Raw Material Software Limited, itt AGPLv3 alatt használva.
- [Catch2](https://github.com/catchorg/Catch2) — Boost Software License 1.0 (csak teszt-buildekhez).
- A VST® a Steinberg Media Technologies GmbH bejegyzett védjegye. A JUCE-szal szállított VST 3 SDK-t
  a Steinberg MIT licenc alatt terjeszti.

## Szerző

Tomori Gábor — *Gabci Audio*

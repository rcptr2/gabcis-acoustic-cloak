# GABCI'S ACOUSTIC CLOAK -- FEJLESZTÉSI TERV ÉS BLUEPRINT (V1.0)

## 1. Projekt Áttekintés és Koncepció (Koncepció C)
Az **Gabci's Acoustic Cloak** (Akusztikus Láthatatlansági Köpeny) egy forradalmi megoldás a mélytartomány keverésére. A hagyományos sidechain kompresszorok lehalkítják a basszust a kick leütésekor. Ez a plugin a fizikai metamateriálok térhajlító logikáját használja: a kick tranziensek beérkezésekor egy valós idejű fázis-rotációt hajt végre a basszus frekvenciáin, így a két jel **fáziskioltás nélkül, 0 dB hangerőveszteséggel (ducking nélkül)** szólalhat meg egyszerre.
Ideális High-Tech Minimal, Techno és DnB producerek számára, ahol a maximális low-end energia a cél.

---

## 2. Tudományos Háttér és Működési Elv
A pluginnek szüksége van egy fő bemenetre (Basszus) és egy Sidechain bemenetre (Kick). 
A DSP analizálja a Kick fázisprofilját a kritikus $20 	ext{ Hz} - 150 	ext{ Hz}$ tartományban, majd egy dinamikusan változó All-Pass Filter (APF) lánccal vagy STFT (Rövid idejű Fourier-transzformáció) fázis-módosítással elforgatja a Basszus fázisát úgy, hogy az korreláljon (konstruktívan összegződjön) a Kick-kel.

**Matematikai alap:**
Kimeneti fázis a $\omega$ frekvencián a $t$ pillanatban:
$$ \phi_{out}(\omega, t) = \phi_{in}(\omega, t) - \Delta \phi_{target}(\omega, t) $$
Ahol $\Delta \phi_{target}$ az a szög, ami maximális pozitív korrelációt biztosít a sidechain jellel.

---

## 3. Főbb Paraméterek (APVTS)
1.  **Cloak Depth (Fázisillesztés Mélysége):** 0% - 100%. Milyen erősen próbálja az algoritmus korrigálni a fázist.
2.  **Lookahead (Előretekintés):** 0 ms - 20 ms. Mivel a fázisanalízis és eltolás időt vesz igénybe, a DAW felé késleltetést (latency) kell jelentenünk.
3.  **Target Frequency Range:** Két slider (pl. 20 Hz - 150 Hz), ami megadja a működési ablakot. Ezen felül az audio érintetlenül megy át.
4.  **Transient Sensitivity:** Mennyire reagáljon élesen a Sidechain jel tranzienseire.

---

## 4. Fejlesztési Fázisok

### Fázis 1: Scaffolding és Sidechain Bus Layout
*   **Feladat:** JUCE projekt felállítása, ahol az `isBusesLayoutSupported()` úgy van megírva, hogy fogadja a 2-csatornás fő bemenetet ÉS egy 2-csatornás Sidechain bemenetet.
*   **Kutatás:** Késleltetés-kompenzáció (Plugin Latency / `setLatencySamples()`) pontos beállítása.

### Fázis 2: FFT / STFT Analízis és Keresztkorreláció
*   **Feladat:** A bejövő hangok valós idejű fázisanalízise a mélytartományban. Kiszámolni a keresztekorrelációt a Sidechain és a Main jel között.
*   **Kockázat (Kritikus!):** A CPU terhelés. STFT (Overlap-Add módszer) használata esetén az ablakméret (Window Size) és a Hop Size optimalizálása létfontosságú. Ha túl nagy az ablak, az effekt lassú; ha túl kicsi, nincs elég felbontás a sub-basszusnál ($<50	ext{ Hz}$).

### Fázis 3: A Phase Rotator Motor (A Láthatatlansági Köpeny)
*   **Feladat:** A kiszámolt $\Delta \phi$ alkalmazása a fő jelen. Ezt vagy STFT bin-fázis módosítással, vagy egy láncolt, dinamikusan hangolt első/másodrendű All-Pass szűrő hálózattal oldjuk meg.
*   **Kockázat:** Pre-ringing (elő-csengés) jelenség az FFT/Linear Phase beavatkozásoknál. Megfelelő ablakfüggvénnyel (Hann/Blackman) tompítani kell.

### Fázis 4: UI / UX Tervezés (Holografikus Radar)
*   **Design:** Futurisztikus, orvosi/tudományos műszer megjelenés (sötétzöld/neon monitor).
*   **Vizualizáció:** Egy valós idejű fázis-korrelációs mérő, amely a Kick és a Sub metszetét mutatja (Lissajous-görbe vagy hőtérkép). Ahogy a *Cloak* bekapcsol, a káoszból egy gyönyörű, konstruktív (összeadódó) geometriai forma rajzolódik ki a kijelzőn.

### Fázis 5: Tesztelés és Elvárások
*   **Teszt:** Egy masszív 50 Hz-es szinusz basszus és egy hosszú lecsengésű 808 Kick egyidejű lejátszása (szándékos fázis-inverzióval).
*   **Elvárt eredmény:** A Master kimeneten a hangerő (Peak dB) magasabb lesz bekapcsolt effektnél, miközben hallható ducking (lehalkulás) nem történik.

---

## 5. Indító Prompt az AI Számára
*Kérlek, másold ki a lenti részt és add át az AI-nak a munka megkezdéséhez:*

> Kedves AI fejlesztő! Egy forradalmi, metamateriál-logikán alapuló VST3 plugint építünk "Acoustic Cloak" néven. A cél egy sidechain fázis-illesztő rendszer, amely hangerő-ducking helyett dinamikus fázis-rotációval (FFT/STFT overlap-add vagy APF lánc alapon) biztosít 100%-os konstruktív korrelációt a Kick és a Sub-basszus között.
> Kérlek, olvasd át a teljes tervet! Kezdjük a **Fázis 1 és Fázis 2** megvalósításával C++20 / JUCE 9 keretrendszerben. A prioritás: helyes Main + Sidechain busz konfiguráció beállítása, és egy extrém hatékony valós idejű fázis-analizáló/keresztkorrelációs mag megírása a Sidechain és a Main I/O között. A latency menedzsmentre (`setLatencySamples`) és a pre-ringing megelőzésére (ablakfüggvények) szigorúan figyelj! Kérjük az alap kódot.

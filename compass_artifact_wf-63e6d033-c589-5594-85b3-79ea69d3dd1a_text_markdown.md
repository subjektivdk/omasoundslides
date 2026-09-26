# Soundslides-klon til Omarchy: gennemførlighed, tech stack og en realistisk plan for en nybegynder

Ja, det kan lade sig gøre, og der findes en genvej: Omarchy har siden version 4 ("Quattro") selv en lille videoapp, Omacut, bygget efter præcis det mønster du skal bruge (Qt Quick/QML-brugerflade plus ffmpeg som kaldt proces).\[1\] Dit projekt er i bund og grund "Omacut med et billedspor i stedet for et videoklip". Den svære del er ikke eksporten. Med FFmpegs `xfade`-filter kan et script på få hundrede linjer lave en færdig MP4 med overgange og lyd. Det svære er den interaktive tidslinje med preview. Start derfor med et rent FFmpeg-/Python-script, og byg først GUI'en ovenpå, når scriptet virker.

## TL;DR

- **Ja, det kan lade sig gøre, men det er ikke et weekendprojekt, hvis du vil have en rigtig app.** Et kommandolinjescript, der laver billeder + varighed + overgange + lyd om til MP4, er realistisk på 1–3 weekender med en kodeagent (fx Pi). En pæn GUI med en to-spors tidslinje, preview og eksport tager for en helt ny begynder snarere 2–4 måneders deltidsarbejde.
- **Den mest Omarchy-agtige stack er Qt Quick (QML) + FFmpeg**, altså samme opskrift som Omarchys egne apps Omacut, Omawrite og Omacalc (C++/Qt, "TINY (~1mb combined!)" ifølge DHH). For dig anbefales PySide6 (Python) + QML: brugerfladen bliver den samme som Omacuts, men koden er lettere at læse. GTK4/libadwaita er et fint alternativ, men ikke det Omarchy selv bruger til sine nye apps. Electron/Tauri er overkill.
- **Første milepæl skal være et rent FFmpeg-script** med en projektfil (JSON) som input. Det kan det hele: per-billede-varighed, per-billede-overgang via `xfade` og flere lydfiler via `concat`. Scriptet bliver senere "motoren" bag GUI'en, så intet arbejde går tabt.

## 1. Hvad Soundslides gør: målspecifikationen

Soundslides blev lanceret af fotojournalisten Joe Weiss den 21. august 2005 som en OS X-app. Den gamle desktopversion eksporterede til Flash. Den nuværende Soundslides 3 er en ren browserapp fra et andet firma, og den kræver en lydfil for at kunne redigere og eksportere til video.\[2\]\[3\] Kernen er uændret: færdige billeder + færdig lyd + præcis timing.

**Kernefunktioner (fra Soundslides' egen hjælp):**

| Område | Soundslides-adfærd | Relevans for din app |
|---|---|---|
| Import | Billeder som JPEG/PNG (gerne >1600 px på den længste led). Lyd som .mp3, .m4a, .wav, .aiff, .ogg, .aac | Kopiér listen 1:1. FFmpeg læser det hele |
| Mængde | Op til 360 billeder pr. show, under 120 anbefales | En god designgrænse, og vigtig for FFmpeg-hukommelsen (se afsnit 5) |\[4\]
| Timing, tilstand 1 | **Auto-spaced**: én skyder, "secs per image", og appen viser hvor mange billeder der passer til lyden | Let at bygge: varighed = lydlængde / antal billeder |
| Timing, tilstand 2 | **Precise timing** i Timeline Editor: træk billedernes "in points" hen til det rigtige sted i lydsporet | Det er den egentlige to-spors tidslinje |
| Overgange | Presets: Crossfade (Fast/Medium/Slow), Straight-cut, Fade out/Fade in (Fast/Medium/Slow) | Passer direkte til FFmpeg: `fade`/`dissolve`, ingen overgang, og `fadeblack` |
| Overgangsniveauer | **Project Inspector** (globalt for hele showet) og **Item Inspector** (per billede, med mulighed for at "bruge projektets standard") | Datamodel: global standard + valgfri override per billede |
| Organisering | Grid-rude til at bytte billeders plads og Library-rude til importerede billeder, der ikke er på tidslinjen | Fase 2-funktion |\[5\]
| Overlays | "Lower third"-tekst med in point og varighed | Senere, via FFmpegs `drawtext` |\[6\]
| Eksport | Video op til 1080p (1920×1080). HD kun på betalte konti | Din app: 1080p H.264/AAC MP4 som standard |\[4\]\[7\]

Soundslides 3 bruger en LGPL-3.0-fork af BBC's **Peaks.js** til bølgeformsvisningen af lyden.\[2\] Det viser, at bølgeform-tidslinjen er et kendt, løst problem, som du ikke selv skal opfinde.

**Din målspecifikation (MVP):** importér N billeder og 1+ lydfiler → to spor (billeder øverst, lyd nederst) → per billede: varighed + overgangstype + overgangsvarighed (med en global standard) → eksportér en MP4. "Auto-spaced"-tilstanden er en gratis bonus, fordi det bare er en division.

## 2. Anbefalet tech stack til Omarchy, og hvorfor

### Hvad Omarchy selv gør (vigtigste fund)

- Omarchy er flyttet fra `basecamp/omarchy` til **`github.com/omacom/omarchy`**. GitHubs releases-side viste i september 2026 "Fork 5k · Star 43.1k", og den seneste udgivelse er v4.0.4 (15. september). Et ældre øjebliksbillede fra star-history.com viser 37.1k stjerner, så tallet vokser hurtigt. Version 4.0.0 "Quattro" blev tagget den 14. august 2026. I Quattro er hele skrivebordsskallen genopbygget i **Quickshell**, som er QML-baseret: bar, launcher, menuer, notifikationer og låseskærm. Waybar, Walker, Mako, SwayOSD, hyprlock m.fl. er fjernet.\[8\]\[9\]
- Quattro leverer tre nye førstepartsapps: **Omacut, Omawrite og Omacalc**. DHH skrev: *"They're all built with C++ and Qt, so they're TINY (~1mb combined!). But they still manage to sync with themes."*\[10\]
- DHH er åben om, at han ikke selv har skrevet C++-koden bag Omawrite.\[9\] Den er agent-skrevet. Omarchy 4 lader dig desuden vælge standard-kodeagent (Claude Code, Codex, OpenCode, **Pi**, Oh My Pi, Gemini m.fl.), som startes med `Super + Shift + Ctrl + A`.\[8\]\[11\] Hele distroens filosofi ("When you can vibe code whatever app comes to your mind…") passer altså til din arbejdsform.\[12\]

Konklusion: i Omarchy-økosystemet anno 2026 er **Qt Quick/QML** det idiomatiske valg til små native apps, ikke GTK4 og ikke web. Det er en ændring i forhold til tidligere Omarchy-versioner, hvor GTK-apps (Nautilus, Evince m.fl.) og Chromium-webapps dominerede.

### Sammenligning af mulighederne

| Mulighed | Passer til Omarchy? | Begyndervenlighed | Video-integration | Vurdering |
|---|---|---|---|---|
| **Qt Quick/QML + C++** (som Omacut) | ★★★ Præcis det DHH bruger | ★ C++-fejl er svære at forstå som ny | QProcess → ffmpeg, Qt Multimedia til preview | Mest "rigtig". Brug den, hvis agenten skriver det meste |
| **Qt Quick/QML + Python (PySide6)** | ★★★ Samme QML-lag og samme look | ★★★ Python er læsbart. PySide6 er Qt's officielle LGPL-binding | `subprocess` → ffmpeg, QtMultimedia til preview | **Anbefalet til dig** |\[13\]
| GTK4/libadwaita + Python (PyGObject) | ★★ Fint på Wayland, temaer via GTK | ★★ God, men færre moderne eksempler på tidslinjer | Naturlig adgang til GStreamer/GES | Godt alternativ, især hvis du vælger GES som motor |\[14\]
| Electron/Tauri/lokal webapp | ★ Omarchys "webapps" er PWA'er af online-tjenester, ikke lokale medieværktøjer | ★★ Hvis du kender web | Du skal stadig kalde ffmpeg | Frarådes: tungt og ikke Omarchy-stil |\[15\]
| Rails + browser | ★ DHH's hjemmebane, men ikke til desktop-medie | ★★ | Samme ffmpeg-kald | Kun hvis du hellere vil have en web-app end en desktop-app |

**Hvorfor PySide6 + QML frem for C++:** QML-filerne (brugerfladen) kan næsten kopieres direkte fra Omacuts stil. Du får derfor Omarchy-looket og samme mønstre (temafarver, portal-filvælger, tastaturgenveje), mens logikken er Python, som du og agenten kan fejlsøge sammen. Hvis appen senere skal pakkes "officielt" i Omarchy-stil som én lille binær fil, kan en agent portere Python-backend'en til C++, fordi QML-laget forbliver det samme.

### Medie-motoren: FFmpeg, MLT eller GStreamer?

| Motor | Hvem bruger den | Styrker | Svagheder for dig |
|---|---|---|---|
| **FFmpeg (CLI som subprocess)** | Omacut, Imagination, de fleste slideshow-scripts | `xfade` har over 40 overgange og `acrossfade` til lyd, alt fra kommandolinjen. Enormt mange eksempler, og agenter kender den rigtig godt | Ingen indbygget "tidslinje-model". Du beregner selv offsets |\[16\]\[17\]
| MLT (`melt` + XML) | Kdenlive, Shotcut | Ægte multitrack-model (tractor/playlist/transition), kan rendere en XML-projektfil direkte | Stejl læringskurve, få begyndereksempler |\[18\]\[19\]
| GStreamer Editing Services (GES) | Pitivi | Rigtig NLE-API med lag, klip og auto-transitions. Python via GObject Introspection | Mindre dokumentation, sværere fejlsøgning |\[20\]\[21\]

**Anbefaling: FFmpeg.** Det er det samme valg som Omacut, dit brugstilfælde (et billedspor, et lydspor, ingen overlappende spor) kræver ikke en fuld NLE-motor, og en kodeagent laver færre fejl med FFmpeg end med MLT eller GES.

## 3. Open source-referencer, og hvad du skal lære af hver

### Omacut (github.com/omacom/omacut): din vigtigste skabelon
"Omacut" er ikke en tastefejl. Det er Omarchys egen *"dead-simple video length trimmer"*, lavet af DHH og installeret som standard fra Quattro.\[8\]\[22\] Du åbner en video, trækker to håndtag for start og slut, ser preview og eksporterer.\[1\] Det er ca. 46 stjerner og 10 forks, MIT-licens.\[23\]
- **Arkitektur:** Qt Quick (QML) med Material-stil, *"the same Qt stack Quickshell builds on"*, plus **ffmpeg og ffprobe fra PATH ved runtime**. C++-delen kompileres til én binær fil, og QML er indlejret via Qt resources.\[1\] Bygges med `qmake6` (ingen CMake).\[24\] Afhænger af `qt6-base`, `qt6-declarative`, `qt6-multimedia`.\[23\]
- **Omarchy-konventioner du bør kopiere:** brugerfladen følger temaets accentfarve. Filvælgeren går gennem `xdg-desktop-portal`. Alt kan styres fra tastaturet (Space = afspil, piletaster = flyt playhead ±1 s, Shift = ±5 s, Alt = ±0,2 s, `Z` = zoom, `Ctrl+O`, `Ctrl+S` = eksportér, `Q` = afslut med advarsel om ikke-eksporteret arbejde, `?` = vis genveje). Eksporten har Original/1080p/720p, skalerer aldrig op og bevarer altid billedformatet.\[1\]\[23\] Mappestrukturen `bin/build`, `bin/test`, `bin/install` bruger `makepkg` og en `pkgbuild/`-mappe til Arch-pakken.\[23\]
- **Hvad du skal lære:** filmstrip-tidslinjen med træk-håndtag, playhead-logikken og hvordan en QML-app starter ffmpeg og viser fremdrift. Community-forken `otomist/omacut-plus` har tilføjet et "caption lane" (et ekstra spor med tekst).\[1\] Det er et direkte eksempel på, hvordan man tilføjer et spor til Omacut-modellen.

### Omawrite (github.com/omacom/omawrite)
Skabelon for Omarchy-"app-hygiejne": autosave, genskabelse af kladder efter nedbrud, advarsel ved eksterne filændringer, tekststørrelse der følger `omarchy display text size`, og portal-baseret åbn/gem.\[25\]\[26\] En fork (JustNak/Oma-Own-Note) beskriver, at appen læser `~/.local/state/omarchy/current/theme/colors.toml` og følger temaskift.\[27\] Det er sandsynligvis sådan tema-synkroniseringen virker, men verificér det i den officielle kildekode.

### Omarchy selv (github.com/omacom/omarchy)
Lær konventionerne: små scripts i `bin/`, ét pr. opgave, med kommentar-metadata som `# omarchy:summary=…`, plain config-filer, Hyprland-konfiguration i Lua (fra 4.0) og pakker via Omarchy Package Repository/pacman.\[15\]\[28\] Din app bør have en `.desktop`-fil, et ikon, et PKGBUILD og gerne en Hyprland-keybinding, præcis som Omacut.

### Kdenlive: modellen for tidslinjen (men ikke koden)
- **Arkitektur:** Kdenlive er en ren GUI oven på MLT. Hver tidslinje er en `Mlt::Tractor`, hvert spor er en `Mlt::Playlist`, og klip er `Mlt::Producer`. I C++ pakkes de ind i `TimelineItemModel`, `TrackModel` og `ClipController`. Alle ændringer, der kan fortrydes, går gennem `QUndoCommand`.\[29\]\[30\] Det er et godt mønster at kopiere i lille skala.
- **Overgange:** MLT-transitions kombinerer et A- og et B-frame fra to spor. Siden Kdenlive 21.08 findes **"Mixes"** (overgange på samme spor), som standard 1 sekund baseret på Luma-kompositionen. Internt bruger hvert spor to playlists for at kunne overlappe. Overgangens standardvarighed kan konfigureres (siden 16.08.1; før var den fast 65 frames).\[18\]\[31\]\[32\]\[33\]\[34\]
- **Rendering:** sker i en separat proces (`kdenlive_render`/`melt`), så et nedbrud under eksporten ikke vælter editoren.\[19\]\[35\] **Den lektion skal du tage med:** kør altid ffmpeg som en separat proces, og læs fremdriften asynkront.
- **Til dig:** brug Kdenlive som *begrebsmodel* (spor → klip → overgang mellem nabo-klip), ikke som kode. Den er alt for stor til en begynder.

### Mindre og mere tilgængelige referencer

| Projekt | Stack | Hvorfor relevant |
|---|---|---|
| **Imagination** (colossus73/imagination) | C, GTK+3, Cairo, FFmpeg | Den klassiske Linux-slideshow-maker: 69 overgange, Ken Burns, tekst. Udviklingen genstartede i 2024 med en multitrack-tidslinje og direkte libav-encoding. Dit tætteste funktionelle forbillede |\[36\]\[37\]
| **PhotoFilmStrip** | Python, wxPython (GTK-port på Linux) | Python-slideshow med per-billede-varighed, fade/roll-overgange, Ken Burns, baggrundslyd og **kommandolinje til batch**. Viser "motor + GUI"-opdelingen i Python |\[38\]\[39\]\[40\]\[41\]
| **Shotcut** | Qt/QML + MLT | Shotcuts officielle blogindlæg om version 20.06.28 (juni 2020), "Slideshow Maker, Proxy Editing, and 360° Video Filters", skriver: "Added Playlist > menu > Add Selected to Slideshow slideshow generator!" Den indbyggede **Slideshow Generator** har billedvarighed, zoom-effekt, overgangstype og overgangsvarighed, og senere versioner tilføjede en separat Audio/Video-varighed. Et godt UX-forbillede for "auto-spaced"-dialogen |
| **tanersener/ffmpeg-video-slideshow-scripts** | Bash + FFmpeg, MIT | Dusinvis af færdige overgangs-scripts. README'en advarer selv: "Unfortunately some scripts consume too much memory. If you experience memory issues, you may try to split your images/videos into two or more smaller sets, create partial videos for each set and concatenate them" |
| **0x464e/slideshow-video** | Node.js + FFmpeg | Har præcis din datamodel: per billede `duration`, `transition`, `transitionDuration` + global standard + lydfil |\[42\]
| Pitivi / GES | Python + GTK + GStreamer | Hvis du vælger GTK-sporet: GES har lag, klip og auto-transitions med Python-bindings |\[21\]\[43\]
| OpenShot / libopenshot | C++ med Python-bindings, PyQt-GUI | Stor NLE. Nyttig til at se en Timeline/Clip/Keyframe-model, men ikke som begynderreference |\[44\]\[45\]

## 4. Konkret foreslået arkitektur

```
┌─────────────────────────────────────────────┐
│  GUI (QML): filmstrip-spor + lydspor +      │
│  inspector-panel + preview + eksport-knap   │
└──────────────┬──────────────────────────────┘
               │ læser/skriver
        ┌──────▼──────┐
        │ project.json │  ← den eneste "sandhed"
        └──────┬──────┘
               │
┌──────────────▼──────────────────────────────┐
│ Motor (Python): validér → normalisér        │
│ billeder → beregn offsets → byg ffmpeg-     │
│ kommando → kør som subprocess → rapportér   │
│ fremdrift (-progress pipe:1)                │
└─────────────────────────────────────────────┘
```

**Projektfil (forslag):**
```json
{
  "output": {"width": 1920, "height": 1080, "fps": 30},
  "defaults": {"duration": 5.0, "transition": "fade", "transition_duration": 1.0},
  "images": [
    {"path": "01.jpg", "duration": 6.0},
    {"path": "02.jpg", "transition": "fadeblack", "transition_duration": 0.5},
    {"path": "03.jpg", "transition": "none"}
  ],
  "audio": ["intro.wav", "interview.mp3"]
}
```

**Nøglebeslutninger:**
1. **Motoren er et selvstændigt CLI-værktøj** (`slideshow render project.json out.mp4`). GUI'en kalder bare det. Det er Omacut-mønstret (ffmpeg som proces) og Kdenlive-mønstret (render i en separat proces) på én gang.
2. **Timing-model:** hvert billede har en `duration` (tiden det er på skærmen, inklusive udgående overgang) og en overgang *til næste billede*. Offset for overgang k er summen af varighederne af billede 1..k minus summen af overgangsvarighederne 1..k.\[16\] Det er standardformlen *"length of current input + previous offset − length of transition"*.\[46\] Bemærk, at hver overgang "æder" tid, så den samlede video er Σvarigheder − Σovergange. Vis det tal tydeligt i GUI'en, og sammenlign det med lydens længde.
3. **Oversættelse af Soundslides-presets:** Crossfade → `xfade=transition=fade` (Fast/Medium/Slow = fx 0,5/1,0/2,0 s). Straight-cut → ingen `xfade`, bare `concat`, eller et meget kort xfade. Fade out/Fade in → `fadeblack`. Senere kan du give adgang til resten af `xfade`s liste (wipeleft, slideleft, circleopen, dissolve, pixelize m.fl.).
4. **Normalisering først:** FFmpegs filterdokumentation for `xfade` (som findes siden FFmpeg 4.3) siger: "Both inputs must be constant frame-rate and have the same resolution, pixel format, frame rate and timebase." Overgangens varighed kan ifølge samme dokumentation være 0–60 sekunder, med 1 sekund og `fade` som standard. Kør derfor hvert billede gennem `scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,format=yuv420p`. Det håndterer også blandede stående og liggende billeder med sorte kanter.
5. **Lyd:** flere lydfiler sættes sammen med `concat`-filteret (`a=1:v=0`) eller krydsfades med `acrossfade`.\[47\]\[48\] Brug `-shortest` eller en eksplicit `-t` for at undgå, at videoen fryser på sidste billede, mens lyden kører videre.\[16\]\[17\]\[49\]
6. **Preview i GUI'en:** vis ikke en live-render. Vis det aktuelle billede efter playhead-position og afspil lyden med Qt Multimedia. Tilbyd "Quick preview", der renderer en 480p-version i baggrunden. Det er langt enklere end ægte realtids-komposition.

**Eksempel på det motoren skal generere (3 billeder, 5 s hver, 1 s crossfade, lyd):**
```bash
ffmpeg \
  -loop 1 -t 5 -framerate 30 -i 01.jpg \
  -loop 1 -t 5 -framerate 30 -i 02.jpg \
  -loop 1 -t 5 -framerate 30 -i 03.jpg \
  -i lyd.mp3 \
  -filter_complex "\
   [0]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,format=yuv420p[v0];\
   [1]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,format=yuv420p[v1];\
   [2]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,format=yuv420p[v2];\
   [v0][v1]xfade=transition=fade:duration=1:offset=4[x1];\
   [x1][v2]xfade=transition=fadeblack:duration=1:offset=8[vout]" \
  -map "[vout]" -map 3:a -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest -movflags +faststart ud.mp4
```
Offset 4 = 5 − 1. Offset 8 = 4 + 5 − 1. Denne kommando er i praksis hele "eksport-motoren". Resten er at generere den ud fra JSON.

## 5. Ærlig sværhedsgradsvurdering og trin-for-trin-plan

### Samlet vurdering

| Del | Sværhedsgrad for en helt ny begynder (med agent) | Kommentar |
|---|---|---|
| FFmpeg-script (billeder + varighed + overgange + lyd → MP4) | **Let til middel.** 1–3 weekender | Agenter er rigtig gode til FFmpeg. Det svære er at forstå offset-matematikken og fejlmeddelelserne |
| Simpel GUI: liste af billeder + felter for varighed/overgang + eksport-knap | **Middel.** 2–4 uger | Ingen tidslinje endnu, bare en "inspector" og en filmstrip-liste |
| Rigtig to-spors tidslinje: trækbare in points, zoom, bølgeform, playhead synket med lyd | **Svær.** 1–3 måneder | **Det er den sværeste del.** Mus- og tastatur-interaktion, koordinat↔tid-konvertering, undo/redo |
| Pakning til Omarchy (PKGBUILD, .desktop, tema-sync) | Let til middel. Få dage | Kopiér Omacuts `pkgbuild/` og `bin/install` |

**Samlet: et seriøst hobbyprojekt over flere måneder, ikke et weekendprojekt**, hvis målet er Soundslides-niveau. Et **brugbart værktøj** (script + enkel GUI uden trækbar tidslinje) er realistisk på 4–6 uger ved siden af et job med en kodeagent.

### De sværeste dele, i rækkefølge
1. **Tidslinje-UI'en** (træk, snap, zoom, bølgeform og playhead der følger lyden). Det er her de fleste hobbyprojekter går i stå. Både Imagination og Kdenlive har brugt år på det.
2. **Preview synkroniseret med lyd.** Løs det billigt: vis det statiske billede for playhead-tiden og lad Qt Multimedia afspille lyden. Byg ikke en real-time-kompositor.
3. **Hukommelse og skalering i FFmpeg.** En kæde af over 100 `-loop 1`-input i ét `filter_complex` kan æde RAM. README'en i tanersener/ffmpeg-video-slideshow-scripts siger ordret: "Unfortunately some scripts consume too much memory." Den advarer også om, at et script fejler, "If the sum of transition durations for a video is equal or bigger than the video duration". Løsning: render i bidder af fx 20 billeder og sæt dem sammen til sidst, eller forbehandl billederne til korte klip. Soundslides' egen anbefaling om under 120 billeder er et fornuftigt loft.\[4\]
4. **Timing-semantik.** Beslut tidligt, om et billedes "in point" betyder starten på overgangen ind eller tidspunktet, hvor billedet er fuldt synligt. Beskriv det i README'en, og hold dig til det.
5. **Blandede billedformater og -størrelser**, EXIF-rotation og meget store billeder. Normalisér altid som beskrevet ovenfor.

### Er et rent FFmpeg-CLI-script en god første milepæl? **Ja, klart.**
Det er den rigtige første milepæl af tre grunde. (1) Det opfylder dit hårde krav (eksport af en færdig videofil) fra dag ét. (2) Det isolerer den del, der kan fejle på mærkelige måder (codecs, formater, offsets), fra GUI-problemerne. (3) Scriptet *er* motoren, som GUI'en senere kalder, så intet bliver smidt væk. Omacut er bygget på præcis den måde: en tynd QML-GUI oven på ffmpeg.

### Anbefalet trin-for-trin-plan

**Trin 0 – Opsætning (1 aften):** `sudo pacman -S ffmpeg python pyside6` (tjek pakkenavnet `pyside6`/`python-pyside6` i Arch). Opret et git-repo i `~/Work/slideshow`, og start din agent dér med `Super+Shift+Ctrl+A`.

**Trin 1 – Håndlavet FFmpeg-kommando (1 weekend):** få eksemplet i afsnit 4 til at virke med 3 af dine egne billeder og en lydfil. Leg med `transition=`, `duration=` og `offset=`, så du forstår matematikken. Det er den vigtigste læring i hele projektet.

**Trin 2 – Python-motor med JSON (1–2 weekender):** bed agenten om et script `render.py project.json out.mp4`, der (a) validerer filer med `ffprobe`, (b) udfylder standardværdier, (c) beregner offsets, (d) genererer og kører ffmpeg-kommandoen og (e) udskriver fremdrift. Skriv 3–4 tests (fx: "3 billeder à 5 s med 1 s overgange giver 13 s video"). **Nu har du en fungerende MVP.**

**Trin 3 – "Auto-spaced"-tilstand (1 aften):** `--auto` fordeler lydens længde ligeligt på billederne. Det er Soundslides' simpleste tilstand, og den er gratis at lave.

**Trin 4 – Minimal QML-GUI (2–4 uger):** klon Omacut og læs `src/`. Lav et vindue med (a) "Importér billeder/lyd" via portal-dialogen, (b) en vandret filmstrip med thumbnails i rækkefølge, (c) et inspector-panel med varighed, overgangstype og overgangsvarighed for det valgte billede plus en "brug projektstandard"-afkrydsning (som Soundslides' Item Inspector), (d) en eksport-knap der kalder motoren med en fremdriftsbjælke, og (e) gem/åbn projekt-JSON.

**Trin 5 – Rigtig tidslinje (1–3 måneder):** thumbnails med bredde proportional med varigheden, trækbare kanter/in points, et lydspor med bølgeform (generér det med ffmpegs `showwavespic`-filter som et PNG, hvilket er meget lettere end live-bølgeform), en playhead med Omacut-genvejene (Space, pile, Shift/Alt) og undo/redo.

**Trin 6 – Omarchy-polering (få dage):** tema-accentfarve, `?`-genvejsoversigt, `Q` med advarsel om ikke-eksporteret arbejde, Original/1080p/720p-eksport, PKGBUILD, `.desktop`-fil og en Hyprland-keybinding.

**Tips til arbejdet med agenten:**
- Giv agenten Omacuts README og kildekode som kontekst ("byg i samme stil som omacom/omacut"), og giv den denne rapports JSON-format og offset-formel som specifikation.
- Lad agenten skrive tests for offset-beregningen, før den skriver GUI-kode.
- Hold motoren og GUI'en i adskilte filer eller moduler, så du altid kan køre motoren alene, når noget går galt.

## Forbehold

- Oplysningerne om Omarchy 4/Quattro, Omacut og Omawrite er fra august–september 2026 og ændrer sig hurtigt. Repoet er flyttet fra `basecamp` til `omacom`, og ældre guides linker stadig til det gamle navn.
- At Omarchy-apps læser `colors.toml` for temaet, stammer fra en forks README og ikke fra den officielle dokumentation. Verificér det i Omacuts kildekode.
- Tidsestimaterne er vurderinger for en helt ny begynder med kodeagent, ikke målte tal. Agenter gør scriptdelen markant hurtigere, men den interaktive tidslinje kræver stadig, at du selv forstår og tester UI-logikken.
- PySide6-anbefalingen er en afvejning mellem læsbarhed og idiomatik. Omarchy selv bruger C++.\[10\] Vil du have maksimal "Omarchy-ægthed" og lader agenten skrive næsten alt, så vælg C++/QML direkte fra Omacut-skabelonen.
- Soundslides' timing-model ("in points" på en lydtidslinje) og din model (varighed per billede) er to sider af samme sag, men konverteringen mellem dem skal defineres præcist, ellers opstår der drift mellem billede og lyd.

## Sources

1. [GitHub - otomist/omacut-plus: Cut a video to the right trim · GitHub](https://github.com/otomist/omacut-plus)
2. [About | Soundslides Help](https://help.soundslides.com/about/)
3. [Migrating | Soundslides Help](https://help.soundslides.com/guide/migrating)
4. [Shows | Soundslides Help](https://help.soundslides.com/guide/shows)
5. [Editing | Soundslides Help](https://help.soundslides.com/guide/editing)
6. [Overlays | Soundslides Help](https://help.soundslides.com/guide/overlays)
7. [Sharing | Soundslides Help](https://help.soundslides.com/guide/sharing)
8. [Release v4.0.0 · omacom/omarchy](https://github.com/omacom/omarchy/releases/tag/v4.0.0)
9. [Omarchy Quattro gives Linux a desktop you can rearrange - Botmonster Tech](https://botmonster.com/self-hosting/omarchy-quattro-release/)
10. [DHH on X: "Omarchy Quattro ships with three new bespoke apps: Omacut, Omawrite, Omacalc. They're all built with C++ and Qt, so they're TINY (\~1mb combined!). But they still manage to sync with themes, and look awesome." / X](https://x.com/dhh/status/2084701529957621890)
11. [Omarchy Quattro by dhh · Pull Request #6231 · omacom/omarchy](https://github.com/omacom/omarchy/pull/6231)
12. [Omarchy - Beautiful, fun & agentic Linux by DHH](https://omarchy.org/)
13. [Creating Python GUI Applications in Linux: Qt vs GTK+, Desktop Environment Compatibility, and Best IDEs Compared — pythontutorials.net](https://www.pythontutorials.net/blog/creating-gui-with-python-in-linux/)
14. [10 Best Python GUI Library Options for Modern Apps (2026)](https://www.wondermentapps.com/blog/best-python-gui-library/)
15. [Web Apps - Omarchy Manual](https://omarchy.org/manual/web-apps/)
16. [FFmpeg xfade Transitions: The Complete Guide (30+ Examples) — FFmpegLab](https://www.ffmpeglab.com/articles/ffmpeg-xfade-transitions-guide.html)
17. [How to Create a Video Slideshow from Images with FFmpeg - FFmpeg Micro Blog](https://www.ffmpeg-micro.com/blog/ffmpeg-create-slideshow-from-images)
18. [TheDiveO/Int'l: Inside Kdenlive Projects: MLT Concepts](https://thediveo-e.blogspot.com/2016/07/inside-kdenlive-projects-mlt-concepts.html)
19. [How Kdenlive Uses the MLT Multimedia Framework - kdenlive](https://salivity.github.io/kdenlive/article/how-kdenlive-uses-the-mlt-multimedia-framework)
20. [Building APIs on top of GStreamer \[LWN.net\]](https://lwn.net/Articles/571266/)
21. [Editing Video with Code Using GStreamer Editing Services](https://www.jamesh.id.au/talks/plug-2021-05/ges-slides.pdf)
22. [GUIs - Omarchy Manual](https://omarchy.org/manual/guis/)
23. [GitHub - omacom/omacut: Cut a video to the right trim](https://github.com/omacom/omacut)
24. [GitHub - joelgaff/omacut: Cut a video to the right trim · GitHub](https://github.com/joelgaff/omacut)
25. [GitHub - omacom/omawrite: The essence of writing · GitHub](https://github.com/omacom/omawrite)
26. [GitHub - shakes76/omalorem: The essence of writing (Omawrite) with a placeholder math enabled preview · GitHub](https://github.com/shakes76/omalorem)
27. [GitHub - JustNak/Oma-Own-Note: The essence of writing · GitHub](https://github.com/JustNak/Oma-Own-Note)
28. [omarchy/bin/omarchy-launch-webapp at dev · basecamp/omarchy](https://github.com/basecamp/omarchy/blob/dev/bin/omarchy-launch-webapp)
29. [Developer Guide | KDE/kdenlive | DeepWiki](https://deepwiki.com/KDE/kdenlive/8-developer-guide)
30. [KDE/kdenlive | DeepWiki](https://deepwiki.com/KDE/kdenlive)
31. [Timeline System | KDE/kdenlive | DeepWiki](https://deepwiki.com/KDE/kdenlive/3-timeline-system)
32. [Mixes / Same-track Transitions — Kdenlive 26.08 Manual 26.08 documentation](https://docs.kdenlive.org/en/compositing/transitions/mixes.html)
33. [Kdenlive Transitions — Kdenlive 26.08 Manual 26.08 documentation](https://docs.kdenlive.org/en/tips_and_tricks/useful_info/kdenlive_transitions.html)
34. [Configuring the Default Transition Duration - Kdenlive](https://kdenlive.org/en/project/configuring-the-default-transition-duration/)
35. [Rendering System | KDE/kdenlive | DeepWiki](https://deepwiki.com/KDE/kdenlive/6-rendering-system)
36. [Imagination, a lightweight and simple slide show maker](https://imagination.sourceforge.net/)
37. [GitHub - colossus73/imagination: A GTK+3 slide showmaker in development since 2009 featuring 69 transitions effects aiming to be user friendly and intuitive using Cairo to achieve the transition effects, FFmpeg to encode the video and ALSA to play the audio during the preview. · GitHub](https://github.com/colossus73/imagination)
38. [Create Slideshow Video Easily on Ubuntu using PhotoFilmStrip](https://www.ubuntubuzz.com/2012/04/create-slideshow-video-easily-on-ubuntu.html)
39. [Linux Mint - Community](https://community.linuxmint.com/software/view/photofilmstrip)
40. [PhotoFilmStrip](https://www.photofilmstrip.org/en/)
41. [Make A Movie Out Of Photos In Ubuntu Using PhotoFilmStrip \~ Web Upd8: Ubuntu / Linux blog](http://www.webupd8.org/2010/08/make-movie-out-of-photos-in-ubuntu.html)
42. [GitHub - 0x464e/slideshow-video: NodeJS and FFmpeg powered automated creation of video slideshows from input images. Strives for "one config fits all"](https://github.com/0x464e/slideshow-video)
43. [gst-editing-services/ChangeLog at master · GStreamer/gst-editing-services](https://github.com/GStreamer/gst-editing-services/blob/master/ChangeLog)
44. [OpenShot - Gentoo wiki](https://wiki.gentoo.org/wiki/OpenShot)
45. [Developers — OpenShot Video Editor 4.0.0 documentation](https://www.openshot.org/static/files/user-guide/developers.html)
46. [How to Create a Slideshow from Images with FFmpeg - Bannerbear](https://www.bannerbear.com/blog/how-to-create-a-slideshow-from-images-with-ffmpeg/)
47. [\[FFmpeg-user\] Use concat filter with a fade](https://ffmpeg.org/pipermail/ffmpeg-user/2022-June/055010.html)
48. [FFmpeg Filters Documentation](https://ffmpeg.org/ffmpeg-filters.html)
49. [Merge Videos with FFmpeg: Concat Demuxer, Filter & Protocol - FFmpeg Micro Blog](https://www.ffmpeg-micro.com/blog/ffmpeg-concat-merge-videos)

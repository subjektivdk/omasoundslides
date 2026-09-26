# omasoundslides

Billeder + lyd → video, i stil med Soundslides. Bygget med C++/Qt 6 og ffmpeg, efter samme mønster som Omarchys Omacut.

Status: **trin 3, tidslinje og afspilning** oven på motoren fra trin 1. Brugerfladen og kommandolinjen er på engelsk.

## Byg og test

Kræver `qt6-base`, `qt6-declarative`, `qt6-multimedia`, `ffmpeg`, `xdg-desktop-portal` og `qmake6`.

```bash
bin/build   # → build/omasoundslides
bin/test    # motor-tests (med rigtige renderinger) + vinduet testet uden skærm
```

## Vinduet

```bash
omasoundslides                      # tomt projekt
omasoundslides projekt.json         # åbn et projekt
```

- Tilføj billeder og lyd med knapperne, med Ctrl+I og Ctrl+L, eller træk filerne ind i vinduet. Billeder sættes ind efter det valgte billede og sorteres efter filnavn (IMG_9 før IMG_10).
- **Tidslinjen** har et billedspor, hvor hvert billede er lige så bredt, som det varer (overgangen ses som overlap), og et lydspor med waveform. Klik eller træk på linealen eller lydsporet for at flytte afspilningen. Scroll over et billede eller et talfelt for at ændre varigheden (±0,1 s, Shift ±0,5 s), eller træk i dets højre kant. Ctrl + scroll zoomer.
- **Afspilning** (Space) spiller lyden og viser billederne med overgangene, som de bliver i videoen.
- **Image-fanen:** varighed, overgang ind og overgangens længde for det valgte billede. Farvede værdier er sat på billedet, grå kommer fra projektet, og ↺ nulstiller til projektets standard.
- **Project-fanen:** projektets navn (bruges som filnavn), standardværdier, "Fit images to the audio" (Soundslides' auto-spaced), fade ind/ud på lyden, opløsning og billeder pr. sekund.
- Statuslinjen viser videoens og lydens længde og siger til, når de ikke passer.
- Eksport (Ctrl+E) renderer til en midlertidig fil, som først får det rigtige navn, når den er færdig.
- Space afspiller/pauser altid, undtagen mens du skriver projektets navn. Et klik uden for et felt giver tastaturet tilbage til genvejene.
- Tryk `?` for alle tastaturgenveje. `Q` afslutter og advarer om ikke-gemte ændringer.

### Tastaturgenveje

Genvejene ligger i `~/.config/omasoundslides/keybindings.conf`, som oprettes med standardværdierne første gang. Filen genindlæses, så snart du gemmer den (Ctrl+, åbner den).

```
play_pause = Space
seek_back  = Left H        # flere taster: adskil med mellemrum
seek_forward = Right L
remove_image =             # tom værdi slår handlingen fra
duration_scroll_step = 0.1 # sekunder pr. hak med musehjulet
```

Lyd-preview og waveform caches i `~/.cache/omasoundslides/`.

## Kommandolinje

```bash
omasoundslides info   projekt.json [--auto]          # vis tidslinjen, tjek filer
omasoundslides render projekt.json ud.mp4 [--auto]   # lav videoen
omasoundslides render projekt.json ud.mp4 --dry-run  # vis ffmpeg-kommandoen
omasoundslides transitions                           # liste over overgange
```

- `--auto`: Soundslides' "auto-spaced". Alle billeder får samme varighed, så videoen varer præcis lige så længe som lyden.
- `--overwrite`: erstat udfilen, hvis den findes.

## Projektfil

```json
{
  "output":   {"width": 1920, "height": 1080, "fps": 30},
  "defaults": {"duration": 5.0, "transition": "crossfade", "transition_duration": 1.0},
  "images": [
    "01.jpg",
    {"path": "02.jpg", "duration": 6.0},
    {"path": "03.jpg", "transition": "fadeblack", "transition_duration": 0.5},
    {"path": "04.jpg", "transition": "cut"}
  ],
  "audio": ["intro.wav", "interview.mp3"],
  "audio_fade": {"in": 2.0, "out": 4.0}
}
```

- Stier er relative til projektfilens mappe.
- Felter, der udelades på et billede, tager værdien fra `defaults`.
- Lydfiler afspilles efter hinanden. `audio_fade` er valgfri; fade ud slutter, hvor lyden slutter i videoen (lydens egen slutning eller videoens, hvis den kommer først).
- `name` er valgfri og bruges som filnavn i vinduet.
- Se `examples/testmateriale.json`.

## Timing-model

Hvert billede har en **varighed** og en **overgang ind** (fra det forrige billede). Overgangen på det første billede ignoreres.

En overgang lægger slutningen af det forrige billede oven i starten af det næste, så den "æder" sin egen længde:

```
start[0] = 0
start[i] = start[i-1] + varighed[i-1] − overgang[i]
videolængde = Σ varighed − Σ overgang
```

Et billedes overgang ind og overgang ud må tilsammen ikke være længere end billedets varighed.

Hvis lyden er længere end billederne, bliver den klippet af. Er den kortere, fyldes der op med stilhed. `info` og `render` advarer om begge dele.

## Overgange

Kun den originale Soundslides' overgange. I vinduet vælges de som ét valg, der sætter både type og længde:

| Valg                        | `transition`          | `transition_duration` |
|-----------------------------|-----------------------|-----------------------|
| Straight cut                | `none` (eller `cut`)  | –                     |
| Crossfade – Fast/Medium/Slow| `fade` (eller `crossfade`) | 0,5 / 1 / 2 s    |
| Fade out/in – Fast/Medium/Slow | `fadeblack` (eller `fadeout`) | 0,5 / 1 / 2 s |

Fade out/in går via sort. Andre ffmpeg-overgange (wipes, slides …) afvises. En anden længde kan skrives direkte i projektfilen og vises så som fx "Crossfade – 1.50 s". `omasoundslides transitions` viser listen.

## Opbygning

```
src/core/project.*        projektfil: indlæs, gem, udfyld standardværdier
src/core/timeline.*       timing-matematik og validering (ingen ffmpeg)
src/core/ffmpegcommand.*  bygger filtergrafen og ffmpeg-argumenterne
src/core/probe.*          ffprobe: findes filen, hvor lang er lyden
src/core/prepare.*        projekt → færdigt, valideret renderjob
src/core/renderer.*       kører ffmpeg som proces og melder fremdrift
src/core/projectmodel.*   projektet som Qt-model, som GUI'en redigerer
src/core/audiopreview.*   lydfilerne samlet til én preview-fil + waveform
src/core/keybindings.*    keybindings.conf: indlæs, standardværdier, genindlæs
src/app/controller.*      åbn/gem, tilføj filer, eksport i baggrunden
src/app/portalfilepicker.* filvælger via xdg-desktop-portal (fra Omacut)
src/app/theme.*           følger Omarchy-temaets accentfarve (fra Omacut)
src/app/waveformitem.*    tegner den synlige del af waveformen
src/qml/                  brugerfladen (Qt Quick, Material)
src/cli.cpp               kommandolinjen
src/main.cpp              vælger mellem vindue og kommandolinje
tests/tst_core.cpp        Qt Test
```

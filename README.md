# omasoundslides

Billeder + lyd → video, i stil med Soundslides. Bygget med C++/Qt 6 og ffmpeg, efter samme mønster som Omarchys Omacut.

Status: **trin 1, motoren (kommandolinje)**. GUI'en kommer ovenpå senere og kalder den samme motor.

## Byg og test

Kræver `qt6-base`, `ffmpeg` og `qmake6`.

```bash
bin/build   # → build/omasoundslides
bin/test    # unit-tests + én rigtig rendering
```

## Brug

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
  "audio": ["intro.wav", "interview.mp3"]
}
```

- Stier er relative til projektfilens mappe.
- Felter, der udelades på et billede, tager værdien fra `defaults`.
- Lydfiler afspilles efter hinanden.
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

Alle ffmpeg-`xfade`-navne (`fade`, `fadeblack`, `wipeleft`, `slideleft`, `dissolve` …) plus `none` for et hårdt klip. Soundslides-navne virker også:

| Soundslides   | her                   |
|---------------|-----------------------|
| Crossfade     | `crossfade` = `fade`  |
| Straight-cut  | `cut` = `none`        |
| Fade out/in   | `fadeout` = `fadeblack` |

## Opbygning

```
src/core/project.*        projektfil: indlæs, gem, udfyld standardværdier
src/core/timeline.*       timing-matematik og validering (ingen ffmpeg)
src/core/ffmpegcommand.*  bygger filtergrafen og ffmpeg-argumenterne
src/core/probe.*          ffprobe: findes filen, hvor lang er lyden
src/core/prepare.*        projekt → færdigt, valideret renderjob
src/core/renderer.*       kører ffmpeg som proces og melder fremdrift
src/main.cpp              kommandolinjen
tests/tst_core.cpp        Qt Test
```

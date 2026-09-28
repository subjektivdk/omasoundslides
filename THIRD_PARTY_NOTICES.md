# Third-party notices

omasoundslides is © 2026 Martin Jensen and released under the MIT License (see `LICENSE`). Every source file states its copyright and license in an SPDX header, following the [REUSE](https://reuse.software/) specification.

## Code included from other projects

### Omacut

- Source: <https://github.com/omacom-io/omacut> (commit `0948c46`)
- License: MIT
- Used in:
  - `src/app/portalfilepicker.cpp` / `.h`: the xdg-desktop-portal file chooser (D-Bus filter and choice types, request and response handling)
  - `src/app/theme.cpp`: reading the Omarchy theme's `colors.toml` and watching for theme changes
  - `src/qml/Main.qml`: the dialog button and the help and confirm overlays

The code has been changed and extended. Omacut's license, as required by its terms:

```
MIT License

Copyright (c) 2026 David Heinemeier Hansson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Used at run time, not included

These are installed separately as system packages. omasoundslides links to them dynamically or starts them as separate programs, and ships none of their code.

| Component | License | How it's used |
|---|---|---|
| [Qt 6](https://www.qt.io/) (Core, Gui, Qml, Quick, Quick Controls, Multimedia, D-Bus, Concurrent) | LGPL-3.0 | Dynamically linked system libraries |
| [FFmpeg](https://ffmpeg.org/) (`ffmpeg`, `ffprobe`) | LGPL-2.1+ / GPL-2.0+ depending on the build | Started as separate programs |
| [xdg-desktop-portal](https://flatpak.github.io/xdg-desktop-portal/) | LGPL-2.1+ | Called over D-Bus |

## Ideas and formats, no code

- **Kdenlive** (GPL-3.0): timeline ideas such as snapping, the overlap shape of transitions, fade handles and undo. Everything was written from scratch; no Kdenlive code is included.
- **Audacity** (GPL-2.0+/GPL-3.0): the label-track text file format, which omasoundslides reads and writes for interoperability. The parser and writer are original.
- **Soundslides**: the transition choices (straight cut, crossfade and fade out/in at three speeds) and the idea of timing images to audio. No code.

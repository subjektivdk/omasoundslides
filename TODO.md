# TODO

## Start som flydende vindue

Appen skal som standard åbne som et "popped out" vindue, dvs. flydende og fastgjort (float + pin), det samme som SUPER+O gør i Omarchy.

## Kun Soundslides' egne overgange

Overgangslisten skal kun indeholde det, den originale Soundslides har (se `help.soundslides.com/guide/editing`):

- **Crossfade**: Fast / Medium / Slow
- **Straight-cut** (hårdt klip)
- **Fade out/Fade in**: Fast / Medium / Slow (via sort, svarer til `fadeblack`)

Punkter at afklare:
- **Længder:** Hvilke længder skal Fast/Medium/Slow have? Rapporten foreslår 0,5 / 1,0 / 2,0 s.
- **Fri længde:** Skal man stadig kunne skrive en længde selv, eller skal det kun være de tre hastigheder?
- **Ældre projekter:** Projekter med andre ffmpeg-overgange (wipeleft osv.) skal stadig kunne åbnes. Enten falder de tilbage til crossfade, eller de fjernes kun fra listen.

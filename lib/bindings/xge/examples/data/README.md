# Font fixture

`font_fixture.ttf` is the small, generated XGE OpenType fixture. It contains a
limited set of test glyphs rather than a general UI font. No system font or
externally licensed font bytes were copied. It is freely redistributable with
this example under the XGE/xlang project notices.

Authoritative generator: `D:/GIT/xge/tests/xge/generate_opentype_fixture.py`.
Authoritative native asset: `tests/xge/data/xge_opentype_fixture.ttf`.
The acceptance report records its exact SHA-256. FontTools is needed only to
regenerate the fixture, not to run this example.

## Audio tone

`tone.wav` is an original, generated 400 Hz / 250 ms, mono 8 kHz PCM16 tone.
It contains no third-party recording. Regenerate it deterministically with
`python -B tools/gen_xge_audio_example.py` from the repository root. The generator
uses fixed integer samples; Python is not needed for example playback.

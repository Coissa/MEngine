# MEngine Third-Party Notices

## stduuid

- Version: v1.2.3, commit `3afe719`
- Source: https://github.com/mariusbancila/stduuid
- License: MIT
- Local changes: none; `include/uuid.h` is vendored as `stduuid/uuid.h` and wrapped behind MEngine asset types.

## Dear ImGui

- Version: 1.87 WIP (`IMGUI_VERSION_NUM` 18608)
- Source: https://github.com/ocornut/imgui
- License: MIT
- Local changes: MEngine CMake/build integration and backend selection.

## stb_image

- Version: 2.27
- Source: https://github.com/nothings/stb
- License: MIT or public domain
- Local changes: none to the vendored `stb_image.h`; MEngine controls where the implementation is compiled.

## Piccolo reference assets

- Reference revision: `f505370`
- Source: https://github.com/BoomingTech/Piccolo
- License: MIT; the retained asset directory also carries its local license copy.
- Local changes: Piccolo files under `Assets/Textures/Sky/Piccolo` are retained only as historical reference and are excluded from runtime packages.

`GeneratedDayBlack_v1` is generated specifically for MEngine and is not derived from Piccolo. Its provenance remains documented in its adjacent `SOURCE.md`.

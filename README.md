<<<<<<< HEAD
# `it_from_bit` -- standalone build of the paper **and** the simulator

Created 20 Sep 2026 as a **basic version of the project to re-evaluate it**: the construction and the
measurements of the rule set, the simulator that produces them (CPU build, with the GUI), and nothing
else. No experiment tree, no harnesses, no attic, no retired material.

## The paper

| file | why |
|---|---|
| `it_from_bit.tex` | the document: Sections 1-8 (the construction), the Conclusion, the Nomenclature, Appendices A-D |
| `ijuc.cls` | document class (loads only the standard `article` class) |
| `manuscript.bib` | bibliography (34 keys; read by `biber`) |
| `fig1.png` | the **only** figure the retained text includes (`\includegraphics{fig1}`, line 389) |
| `build.bat` | `pdflatex -> biber -> pdflatex x2`, then opens the PDF |

Build with `build.bat`: expect **39 pages, 0 errors, 0 undefined references**.

The document is a reduced form of `doc/manuscript.tex` in the repository `E:\alpha`, generated there by
`attic/make_it_from_bit.ps1`: Section 9 (*Results*), Section 10 (*Conjectures and prospects*) and the
*Reproducibility* subsection are removed, and the 32 cross-references into that material are repaired.
What is missing is the numerical results, the particle taxonomy, the quantum-formalism bridge and the
prospects -- and every pointer to them. Section grading is kept: (P) postulate, (M) measured, (C)
candidate or conjecture. Read the grade markers literally.

## The simulator (model + GUI)

| path | what |
|---|---|
| `src/*.cpp` | the GUI and framework: `main`, `GUI*`, `scene`, `input`, `hud`, `text_renderer`, `recorder`, `replay`, `stats`, `tomography`, `tinyfiledialogs`, ... (41 files) |
| `src/model/*.cpp` | **the rule set**: `initSim`, `interaction`, `simulation`, `utils`, `config`, `geometry`, `polarization`, `charges`, `attractor`, `bridge` |
| `src/include/**` | all headers, including `src/include/zlib` (the CPU reference) and `src/include/model/*.h` (the transition rules) |
| `glad/glad.c`, `lib/*.lib` | OpenGL loader and the import libraries (`glfw3dll`, `zlib`) |
| `fonts/arial.ttf` | the HUD font (runtime) |
| `logo.png`, `logo_bar.png` | the GUI logos (runtime) |
| `automaton.cfg` | reference configuration, copied into `build\` by the build |
| `Makefile` | byte-identical to the repository's: `nmake` target list, object rules, `check-odr` gate, `dlls`/`assets`/`copy_config` |
| `experiments/odr_gate_tu{1,2}.cpp` | the two translation units of the ODR gate only (1.6 kB total) -- kept so `nmake check-odr` works and the `Makefile` needs no edit |
| `build_gui.bat` | the build: ODR gate, then `nmake`, then the font copy |

Build with:

```
build_gui.bat
```

Then run `build\automaton.exe` (the working directory must be `build\`, where the assets and DLLs are).

**Setup screen (splash):** the lattice side `L` and the winding layers `W` are picked in the splash
window before any mode starts.  The three buttons (`Simulation`, `Statistics`, `Replay`) and the
`Enter` shortcut read the *same* selection; `Start Paused` is honoured by `Simulation` and `Replay`
(`Statistics` has its own pause flag, which its `start()` resets).

The values actually used are written back into `automaton.cfg` (`simulation.scenario`,
`simulation.lattice`, `simulation.layers`) and are what the splash opens with on the next run; the
other keys and the comments of the file are left untouched, and a value the UI cannot represent
(`lattice = 88`, `layers = 5000`) snaps to the closest offered one (`89`, `4096`).

A combination that cannot fit in memory is refused before the allocator is touched: the lattice
needs `3 x L^3 x W x 160` bytes, so the default 21/10 takes ~42 MB while the largest pair the
dropdowns offer (89/364) would need ~117 GB.  The screen shows that estimate live (cells, RAM,
`RMAX`, island topology, light-frame length) next to the presets and the labels of the three
controls, and the layout is recomputed from the window size, so resizing the window does not
scatter the widgets.

Keyboard: `Enter` starts the mode the focus ring is on (Simulation by default), `Tab` / `Up` /
`Down` move the ring, `Left` / `Right` change the focused value (or toggle `Start Paused`), and
`Esc` quits from the setup screen.  Clicking a control moves the ring to it as well.

**External dependency, not vendored:** the Visual Studio toolchain (`cl`, `nmake`) and vcpkg at
`E:\vcpkg\installed\x64-windows` for `freetype`, `brotli`, `bz2`, `zlib`, `glfw3` headers and libraries.
`build_gui.bat` sets `VCPKG_ROOT` to that path explicitly, because the environment may define it as the
Visual Studio bundled vcpkg, which carries no libraries -- that mistake stops the build at `ft2build.h`.
The runtime DLLs (`glfw3`, `freetype`, `zlib1`, `bz2`, `brotli*`, `libpng16`) are copied into `build\` by
the `dlls` target.

**Verified on creation:** ODR gate OK; `nmake` -> `build\automaton.exe` (465 kB) with 0 errors; the GUI
launched from `build\` and ran (CPU active, `[Config] Loading: automaton.cfg`, no stderr output).

## Editing here

This directory is a working copy, not the source of truth: edits here are **not** reflected in the
repository, and `it_from_bit.tex` (and `src/`) will be overwritten if the repository copies are copied
back over them. Two workable habits:

1. edit here freely while re-evaluating the project, then port the decided changes back into `E:\alpha`
   (`doc/manuscript.tex` for the paper, `src/` for the code); or
2. treat the repository as the source of truth and copy the files out again whenever they change.

`build_odr.log` and `build_gui.log` are the logs of the last build (kept as evidence, regenerated on
every run); `obj\` and `build\` are build outputs and can be deleted at any time.

=======
# it_from_bit
Physics emerging from a cellular automaton
>>>>>>> cc534895581c4806553829f43773a1643f288480

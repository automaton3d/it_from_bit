# `it_from_bit` -- standalone build of the paper **and** the simulator

*Physics emerging from a cellular automaton.*

Created 20 Sep 2026 as a **basic version of the project to re-evaluate it**: the construction and the
measurements of the rule set, the simulator that produces them (CPU build, with the GUI), and nothing
else. No experiment tree, no harnesses, no attic, no retired material.

## The paper

| file | why |
|---|---|
| `it_from_bit.tex` | the document: Sections 1-8 (the construction), the Conclusion, the Nomenclature, Appendices A-D |
| `ijuc.cls` | document class (loads only the standard `article` class) |
| `manuscript.bib` | bibliography (39 entries; read by `biber`) |
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
| `src/*.cpp` | the GUI and framework: `main`, `GUI*`, `scene`, `input`, `hud`, `text_renderer`, `recorder`, `replay`, `stats`, `tomography`, `splash`, `cortina`, ..., plus `config.cpp` and `tinyfiledialogs.c` (37 .cpp files + that one .c) |
| `src/model/*.cpp` | **the rule set** that the build links: `initSim`, `interaction`, `simulation`, `utils`, `geometry`, `polarization`, `charges`, `bridge` (8 files, all in `OBJ_COMMON`) |
| `src/model/attractor.cpp`, `src/model/wavefront.cpp` | read-only diagnostics/instrumentation: they compile, their headers are part of the ODR gate, but nothing in the simulation calls them, so they are deliberately **not** linked into `automaton.exe`. `nmake check-extras` compiles them so they cannot rot unnoticed |
| `src/include/**` | all headers, including `src/include/zlib` (the CPU reference) and `src/include/model/*.h` (the transition rules) |
| `src/subregion_box.*`, `src/subregion_modal.*` | the region of the lattice: the model (bounds, counts, the printed report; no GL) and the 3D overlay that edits it (see *Region of the lattice* below) |
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
`nmake check-odr` (run first by `build_gui.bat`) links the two gate TUs, and `nmake check-extras`
compiles the two unlinked diagnostics of `src/model\` (`attractor.cpp`, `wavefront.cpp`).

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

The 3-D view's data tickboxes (left edge of the scene) are named after what they draw.  The one
that draws the bubble's spherical cavity -- the Fibonacci sphere inscribed in the unit cube
(`renderCavity()`, 850 points) -- is `Cavity`; it used to be labelled `Lattice`, which described
neither the drawing nor the switch.  Its configuration key follows (`data3D.cavity`), and the old
`data3D.lattice` is still read, so a file written before the rename keeps its setting.  The
function was renamed with it (`renderCube` -> `renderCavity`: the wireframe cube in its body has
been commented out for a long time, only the sphere is drawn).

The setup window is created 900x660: a run calls `glfwMaximizeWindow` anyway, so this size only
affects the screen where the parameters and the region are chosen, which now has to fit three cards.

**Region of the lattice (3D overlay).**  The region a run should use -- a box inside the `L x L x L`
lattice -- is chosen in an overlay opened by the `Select region...` button (or by `Enter` with the
ring on it).  The overlay is a full-window view with only what the choice needs:

* the lattice drawn in 3D: its twelve edges plus the grid of the three faces turned towards the
  camera (the grid follows the camera, so orbiting never leaves the lattice looking solid or empty);
* the gizmo: the box being selected, with its three camera-facing faces translucent and its twelve
  edges bright, and one square handle in the middle of each of its six faces (the active one orange,
  the one under the cursor light blue);
* the readouts: `L`, the active face and its keys, the bounds `x a..b  y c..d  z e..f`, the cells per
  layer and the percentage of `L^3`, the same with `W`, and the estimated memory for three lattices;
* the camera is the program's own `OrbitCamera`, so it orbits exactly like the main scene: left-drag
  on a face moves that face, left-drag elsewhere (or middle-drag) orbits, `Ctrl`+middle-drag pans and
  the wheel zooms.  `Up`/`Down` move the active face (`Shift` for five cells at a time), `Left`/`Right`
  pick which face that is, `Home` goes back to the whole lattice, and `Enter` or `Esc` returns to the
  setup screen.  Nothing has to be confirmed: the region is edited live, and the splash shows the new
  numbers and the button's summary as soon as the overlay closes.

Every change is **printed** on stdout as one line, so the widget can be checked before anything in
the model reads it:

```
[Subregion] x 0..10   y 3..20   z 0..20 | per layer 11 x 18 x 21 = 4,158 of 9,261 cells (44.9%) |
W = 10 -> 41,580 of 92,610 cells | est. RAM 19.0 MB of 42.4 MB (3 lattices x 160 bytes/cell)
```

**The region is what runs (20 Sep 2026):** the model now uses it.
`automaton::configureLatticeFromRegion()` (src/model/initSim.cpp) is the single
entry point the setup screen calls instead of `calculateParameters` +
`tryAllocate`: it takes the region's bounds, turns its **extents** into the lattice
edges and allocates only that volume.  So a region of `11 x 11 x 11` with `W = 10`
allocates 13,310 cells (~6 MB) where the full `21^3 x 10` allocates 92,610
(~42 MB), and the schedule follows: `RMAX = 5`, `CENTER = 5`, `FRAME = 216`,
`ISLAND_COUNT = 99`.

The box's **position is deliberately not used**: the lattice is periodic and the
initial condition is translation-invariant (every bubble is born at the lattice
centre -- `initCenters`), so a translated region is the same experiment on the
same torus.  Only the size matters, and the size is what the memory bill depends
on.

Two consequences the widget honours, because the model requires them:

* **odd edges** -- the seed sits on the centre cell and the periodic wrap has to
  be symmetric (the same assumption the sieve algebra has always made), so a face
  never produces an even edge: it keeps the parity of the opposite face, which
  means it moves two cells at a time (`Up`/`Down`, `Shift` for ten).  Even or
  under-5 edges are refused at start-up with a reason instead of being adjusted
  silently;
* **cubic or not** -- a cubic region goes through the legacy path
  (`calculateParameters` + `tryAllocate`), so selecting the whole lattice is
  **bit-identical** to the way runs were configured before (checked with a
  deterministic digest of the lattice every 50 ticks, 15 samples, all equal);
  an anisotropic region uses `tryAllocateTube`, which the model already had
  (per-axis edges, `RMAX` = short side, schedule scale = long edge).  A
  `11 x 21 x 21` region was run to 2,600 ticks with no error.

The setup panel shows the lattice that will actually run (`lattice = 11 x 11 x 11
x W 10 (region)`, its cells and RAM) and the comparison with the full lattice, and
the pre-flight memory guard measures the region's volume too -- so a small region
can make an `L x W` that would not fit on its own fit.

**Two ways to select it.**  Free editing moves one face at a time (above); `C` in the
overlay switches to **centred cube**, where the whole selection is one number -- the
side `S` -- and the box stays centred on the lattice (`c-k .. c+k`, `c = (L-1)/2`).
`Up`/`Down` change `S` by two cells (`Shift` ten), `Left`/`Right` have no face to pick,
`Home` returns to the whole lattice (which is itself a centred cube) and `C` goes back to
free faces; dragging any face in cube mode scales the cube symmetrically.  Entering cube
mode collapses the box to the centred cube of its *smallest* extent, so switching never
grows the region.  The splash keeps a row of centred-cube presets (`cube 11`, `cube 15`,
`cube 21`) under the L/W ones: one click sets the side, no overlay needed.

Because the model re-centres the seed on the lattice it is given, the cube is a
**convenience**, not a different run: a centred `11^3` and the `11 x 21 x 21`-style box
with the same extents produce the same `calculateParameters` line, the same schedule and
the same allocation (checked in a run: `region 11 x 11 x 11 (from x 5..15 ...)` gave
`EL=11, RMAX=5, FRAME=216` and 13,310 cells, exactly as the non-centred `x 0..10` case
did).  What the mode buys is gesture count: the `11^3` selection is `C` + `Down x5`
instead of `Down x5, Right x2` three times.

The overlay also states the model's own validity rule before start-up: a region whose
edges are not odd and at least 5 cells shows `cannot run: every edge must be odd and at
least 5 cells`, instead of the user only discovering it in the start-up refusal.

The region is **persisted**: six keys (`simulation.subX0` ... `simulation.subZ1`)
go back into `automaton.cfg` with the same in-place rewrite the other managed keys
use, so the setup screen reopens on the region the last run used and the log says so
(`[Subregion] opened with the region saved in automaton.cfg`).  A file without the
keys means the whole lattice -- the default is resolved against `simulation.lattice`
once the whole file has been read, because the full-lattice bounds depend on the side.
A region that cannot be a lattice (outside the side, inverted, or with an even extent)
is clamped on load and reported:

```
[Config] region x 0..9 y 0..400 z 18..2 is not a lattice region (inside 0..20, odd
extents); using x 1..9 y 0..20 z 2..18
```

**Modes:** `Replay` renders through the same HUD as `Simulation` (it used to start without `initHUD`,
so its first frame read an empty `data3D` vector and a null `layerList`; the process died with
`0xC0000005` before drawing anything).  `render3DObjects()` now also returns early when the HUD is
missing, so a mode without widgets draws an empty scene instead of crashing.  Replay draws its own
progress bar (`Frame: n / N` plus a yellow pointer), a quarter of the window wide, at
`ReplayProgressBar::kDefaultProgressY` = 100 px below the top edge -- the same band as the simulation
bar -- and it is redrawn both on start-up and on window resize through that one constant.

`Help` (the splash link and the HUD hyperlink) opens the repository's `README.md` through
`ShellExecuteA`, from a single place (`framework::openHelpPage()` in `help.cpp`).  It used to call
`system("start https://github.com/automaton3d/automaton/blob/master/help.md")` from two call sites: a
different repository, a file that does not exist there, one shell process per click, and the URL
duplicated.

**External dependency, not vendored:** the Visual Studio toolchain (`cl`, `nmake`) and vcpkg at
`E:\vcpkg\installed\x64-windows` for `freetype`, `brotli`, `bz2`, `zlib`, `glfw3` headers and libraries.
`build_gui.bat` sets `VCPKG_ROOT` to that path explicitly, because the environment may define it as the
Visual Studio bundled vcpkg, which carries no libraries -- that mistake stops the build at `ft2build.h`.
The runtime DLLs (`glfw3`, `freetype`, `zlib1`, `bz2`, `brotli*`, `libpng16`) are copied into `build\` by
the `dlls` target.

**Verified on creation:** ODR gate OK; `nmake` -> `build\automaton.exe` (465 kB) with 0 errors; the GUI
launched from `build\` and ran (CPU active, `[Config] Loading: automaton.cfg`, no stderr output).

**Verified after the last pass (20 Sep 2026):** `check-odr` and `check-extras` OK; `build_gui.bat` ->
`build\automaton.exe` with 0 errors; splash -> `Enter` runs the simulation; splash -> `Tab` x6 ->
`Enter` starts the replay and the process stays alive (it used to die with `0xC0000005` in its first
frame); and the HUD is now clean of GL errors -- `glGetError` probes (temporary, removed afterwards)
showed `GL_INVALID_OPERATION` from `Button::drawAsHyperlink` on every frame, which is fixed in
`Button::cleanup()`.

**GL objects per frame: audit (20 Sep 2026).**  The class of bug behind item 11 (a VAO+VBO
pair created and deleted inside a per-frame draw) was swept again, site by site: all 20
`glGenVertexArrays`/`glGenBuffers` call sites in `src/` are now one-time or guarded, so a
frame creates no GL objects at all.

| Site | Why it is safe |
|---|---|
| `logo.cpp:117` | one pair per `Logo` instance, built on the first `draw()` and refilled with `glBufferSubData` (in the tree since `6c41be7`; it was reverted once during a bisect and re-applied) |
| `draw_utils.cpp:25` (`init`) | `if (vao) return;` -- the shared 2D quads/lines/outlines/fans |
| `draw_utils.cpp:154` (`drawLine2D_new`) | one static pair for the 3D lines of several callers |
| `GUI_3D.cpp:73` (`ensurePrimitiveBuffers`) | `if (vao) return;` -- points/lines/quads share it |
| `GUI_3D.cpp:516` (`renderAxes`) | its own static pair, `if (!axisVao)`, because the layout is pos+colour |
| `button.cpp:55/85/108` | `setupGeometry()`, i.e. the constructor and a real `setPosition`/`setSize` change |
| `button.cpp:324` | the hyperlink underline, `if (!underlineVAO \|\| !underlineVBO)` -- `cleanup()` zeroes the handles |
| `menubar.cpp:95` | the `MenuBar` constructor |
| `progress.cpp:79..86/119` | `initBuffers()`, called once from the constructor |
| `stats.cpp:306` | `ensureVAO()`, `if (sVao) return;` |
| `subregion_modal.cpp:60` | `ensureBuffers()`, `if (vao) return;` |
| `text_renderer.cpp:107` | `init()`, once per renderer |

The two cases where the *caller* could still ask for a rebuild every frame are handled at
the source: `Button::setPosition`/`setSize` return early when the value did not change (the
HUD hyperlink used to call them each frame), and the per-frame widgets are positioned by
the layout, not by a new value every tick.

**Setup-screen clicking (20 Sep 2026):** the widget part of `mouseButtonCallback` never looked at
`action`, so every click acted twice -- on the press and again on the release, which arrives at the
same point.  The visible symptom was the `Start Paused` tickbox, whose state flipped on the press and
straight back on the release (a `[dbg]` trace with a real click showed `press ... state=on` immediately
followed by `release ... state=off`).  The same double action made a dropdown open and close in one
click, launched a mode twice and opened the help page twice.  The handler now returns unless the action
is `GLFW_PRESS`.

`Tickbox::hitTest()` is new: a tickbox answers to a click anywhere on the control, its label included.
Only the 18x18 box used to react, so aiming at "Start Paused" did nothing; the splash uses the new test
and prints the new state (`[Splash] Start Paused = yes`) so the result is visible in the log too.
`Tickbox::contains()` and the HUD call sites are unchanged.

Verified with real clicks (the cursor moved with `SetCursorPos` and the button posted as a window
message; `glfwGetCursorPos` then reports the real position): clicking the label, the label again and the
box printed `yes`, `no`, `yes`; clicking `Simulation` started the run with `startPaused = yes`; and a
frame dump taken after a click on the `L` dropdown shows the list still open.

**Replay: the loop is wired (20 Sep 2026).**  The progress bar was already drawn
(hud.cpp, in REPLAY mode, quarter of the window wide, `Frame: n / N` plus a yellow
pointer); what was missing were the three switches, none of which anything in this
tree ever flipped:

* `recordFrames` was never set to true, so `recorder.recordFrame` (core.cpp) never
  ran and a run recorded nothing.  There is a **`Record` tickbox** in the simulation
  HUD now (above `Visited`, drawn in SIMULATION only, since that is the mode that
  records).  It follows the atomic rather than the other way round, so when the
  recorder hits its memory cap and clears the flag itself the box shows that.
* The File menu items were stubs (`New` printed, `Open replay` was an empty lambda,
  `Save replay` printed).  They now call `newReplay()`, `loadReplay()` and
  `saveReplay()`; `newReplay()` is new and empties the recorder, which until now
  could only grow to its cap.
* `replayFrames` was never set either, so even a loaded replay never played.
  `loadReplay()` rewinds the play head and starts it, and `updateReplay()` returns
  false past the last frame -- core.cpp clears `replayFrames` on that, which used to
  never happen: a finished replay kept the loop spinning four times a second.

The loading itself moved to `loadReplayFrom(path)`, so a path can be loaded without
going through the modal dialog.  The guards (a snapshot needs a settled simulation)
now print **why** they refuse instead of doing nothing:

```
[Replay] save needs a paused-free SIMULATION with recording and playback off
(mode=1, recording=1, replaying=0, paused=0)
```

Verified with the app: `Record` on/off from the HUD printed `recording started` and
`recording stopped (2 frames, 0 KB)`; `File > Save replay` opened the OS dialog and
wrote a real 69-byte `replay.dat` (1 frame); loading it printed `[Replay] playing 1
frames from replay.dat` and fed the bar exactly once (`update(1, 1)`), after which the
play loop stopped -- and the guard above is that refusal path, seen in the final build.
The save dialog was driven by script; the load path was verified through
`loadReplayFrom` (the same code the dialog path calls), not by typing into the dialog.

**Text the renderer could not draw (20 Sep 2026):** `TextRenderer` loads glyphs for bytes 0..127 only,
so the non-ASCII characters in three setup-screen labels (`L - lattice side (odd)` and the two others
used an em dash, and the presets caption a middle dot) were silently dropped -- the labels read with a
gap where the dash was.  They are ASCII now.  Worth remembering when labelling a widget: anything above
0x7F disappears without a word.

**Region-overlay pass (20 Sep 2026):** the setup screen was regrouped into three cards (parameters,
summary, and one card that keeps `Start Paused` together with the three mode buttons, so nothing is
left outside a box and the tickbox is no longer inside the summary card).  Two latent projection bugs
turned up and were fixed: `TextRenderer::RenderText`'s 5-argument overload projected onto stale
800x600 members instead of the live viewport (the dropdown values landed ~760 px above their boxes
once the window grew), and `drawTriangleFan2D` ignored its colour argument because `uColorLoc` was
never assigned.  The overlay itself was verified with temporary `glReadPixels` dumps of the app's own
frames (the screen captures of this environment come back blank or stale) plus a closed-loop
self-test that grabbed a face handle at its projected position and dragged it to where it should be
for `x1 = 10`.  That test first failed and exposed a real bug: the drag used ray-versus-face-plane,
and every ray hits that plane at the plane's own coordinate, so the value could never change; it now
takes the closest point between the ray and the face's axis, and the same test lands exactly on 10.
`Tab` x3 then `Enter` opens the overlay, `Right`/`Up` move faces with every change printed, `Esc`
returns to the splash, and the next `Enter` starts the run.

## Editing here

This directory is a working copy, not the source of truth: edits here are **not** reflected in the
repository, and `it_from_bit.tex` (and `src/`) will be overwritten if the repository copies are copied
back over them. Two workable habits:

1. edit here freely while re-evaluating the project, then port the decided changes back into `E:\alpha`
   (`doc/manuscript.tex` for the paper, `src/` for the code); or
2. treat the repository as the source of truth and copy the files out again whenever they change.

`build_odr.log` and `build_gui.log` are the logs of the last build (kept as evidence, regenerated on
every run); `obj\` and `build\` are build outputs and can be deleted at any time.
`build_odr.log` and `build_gui.log` are the logs of the last build (kept as evidence, regenerated on
every run); `obj\` and `build\` are build outputs and can be deleted at any time. Since commit
`77e9a0e`, `.gitignore` keeps all of them out of git (`obj/`, `build/`, `*.log`), so a rebuild no
longer shows up as a dirty tree; the files themselves stay on disk.

Note on the merge: this file was left with unresolved conflict markers
(`<<<<<<< HEAD`, `=======`, `>>>>>>> cc53489`) by the merge that created the tree; the two sides are
now merged into the single text above (the title from `HEAD`, the one-line description from the
other side).

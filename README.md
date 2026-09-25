# It from Bit — simulator and paper

Hi all,

I'm developing a toy model of the universe based on the cellular automaton paradigm.  The underlying
research ideas are described in the paper this repository builds:

**https://doi.org/10.5281/zenodo.3818302**

This repository contains the standalone effort to implement a computationally small version of the
proposed universal automaton **and** the paper that describes it, so that every number the text quotes
can be recomputed from the same tree.  The implementation is intentionally compact and the underlying
rules are independent of the model's memory size; the project is written in C++ and the core automaton
lives in `src/model/`.

It was created on 20 Sep 2026 as a **basic version of the project to re-evaluate it**: the construction
and the measurements of the rule set, the simulator that produces them (CPU build, with the GUI), the
paper, and nothing else -- no experiment tree, no attic, no retired material.  The working log -- every
measurement, and the number that says which reading of it survives -- is the long form below.

> **Language directive: American English.**  Text written in this repository -- the paper, `FSM.txt`,
> this file, the research log, the code and its comments -- uses American English spelling (`color`,
> `behavior`, `center`, `normalize`, `-ize`/`-yze`).  Prose that still says "centre" or "behaviour" is
> corrected as it is touched; quotations, section titles and the harness's printed field names
> (`reloc-at-centre`, `CoM-(centre)`) keep their spelling, because changing those would change what a log
> says.

---

## 🚀 Build and Run

### Requirements

* Windows
* MSVC (Visual Studio or Build Tools) with `nmake`
* a **vcpkg** tree that carries `freetype`, with `VCPKG_ROOT` pointing at it.  The copy bundled with
  Visual Studio has none, so if the link stops at `LNK1181: cannot open input file 'freetype.lib'`,
  build with `nmake VCPKG_ROOT=E:/vcpkg/installed/x64-windows`
* an OpenGL-capable GPU (for the GUI)
* (optional) CUDA Toolkit
* (for the paper) MiKTeX or TeX Live, with `pdflatex` and `biber`

### Build

Open a **Developer Command Prompt for Visual Studio** and run:

```text
nmake
```

That is the **promoted configuration** -- the four macros `CHARGE_DISPERSION_FSM`,
`POLAR_SEED_FROM_PLACEMENT`, `PAIR_SAME_OCTANT`, `PAIR_OWN_AXIS_EXCHANGE` -- which is what the paper's
Sect. 8.4 candidate bullet describes.  The paper's *reference* configuration (no macro at all) is one
switch away, and gets its own object tree:

```text
nmake REFERENCE=1        # the reference build the paper quotes (objects in obj_reference\)
nmake CANDIDATES=1       # alias of the default, kept so older notes keep working
nmake trace-first-era    # the headless trace harness -> build\first_era_trace.exe
build_gui.bat            # the GUI alone
```

### Run

```text
nmake run
build\automaton.exe
```

The harness takes its scenario on the command line -- `build\first_era_trace.exe <L> <s2b_target>
<frames>` -- and prints one annotated block per light frame; `FSM.txt` lists every field of those blocks.

### Verify

```text
nmake check-docs     # the lint: the documents against the sources and the trace logs (18 rules)
nmake check-trace    # the regression: four configurations against experiments\golden\
nmake check-odr      # two translation units, so a header cannot define with external linkage
nmake check-extras   # the auxiliary gates
```

`check-docs` checks the cited paths, the macro registry against the `#ifdef`s, and the machine-checkable
claims against the logs in `build\` -- a log that is not built is a note, never a failure.  `check-trace`
compares whole logs line by line; the model has no RNG, no address dependence and no scan-order
dependence, so a difference is a change in the dynamics and never noise.

---

## 📁 Project structure

```text
it_from_bit.tex        # the paper: Sections 1-8, the Conclusion, the Nomenclature, Appendix A
manuscript.bib         # its bibliography (39 entries, read by biber)
it_from_bit.pdf        # what build.bat produces: 39 pages, 0 errors, 0 undefined references
automaton.cfg          # the runtime configuration (scenario, lattice, sieve)

src/
  model/               # the automaton: simulation, interaction, charges, polarization, geometry,
                       # the attractor census, the wavefront
  include/model/       # its headers
  include/             # GUI and render headers (glm, glad, GLFW, zlib and stb are vendored there)
  *.cpp                # rendering, GUI, input, recorder and replay, statistics

experiments/           # the trace harness, the variant builds, the lint, the golden expectations
FSM.txt                # the finite-state-machine map: every rule, macro, mask bit and traced counter
Makefile               # nmake: build, run, check-docs, check-trace, check-odr, trace-first-era
build.bat, build_gui.bat

build/                 # the executable and its assets (generated, ignored)
obj/, obj_reference/   # one object tree per configuration (generated, ignored)
```

---

## 🧠 Features

- 3D cellular automaton with a charge word per cell and per layer
- OpenGL visualisation and a custom GUI (panels, subregion boxes, HUD, replay recorder)
- tomographic slicing (XY, YZ, ZX) and a cell inspector
- deterministic transport: no RNG, no address dependence, no scan-order dependence
- optional CUDA backend; the CPU builds do not need it
- one build switch between the promoted configuration and the paper's reference
- a headless harness that prints, per light frame, the census, the clock, the charge-word roster and
  every displacement channel, annotated by writer
- two gates that keep the documents honest: the 18-rule lint and the golden-log regression

---

## 🔬 Research goals

This tree is the standalone build of the paper **and** of the simulator that produces its numbers, so
the program's goals are made checkable here:

- **reproduce the construction and every number the text quotes** from one tree: the rule set, the
  encounter's transport, and the census of the frozen plateau.  The reference configuration and the
  promoted one are one build switch apart -- not a fork -- and both are built from the same sources;
- **settle the mechanism of each displacement channel**, not its statistics: the dispersal and the era-1
  cascade are measured to the sign, and for the later eras the log reduces the residual to a single
  question -- why a layer's center is re-derived with no impulse behind it -- and names the probe that
  would answer it;
- **measure the finite-size behavior** of the dimensionless observables: the plateau relation
  `K = W - 8` and the eight-word roster hold at four sizes while the transport's alignment does not, and
  the two measurements that would sharpen it -- the `L = 11` era-1 dynamics and the `L = 13` static row
  -- are priced in the log, not run;
- **keep every claim machine-checked**: an 18-rule lint ties the documents to the sources and to the
  trace logs, and a golden-log regression pins the era-1 dynamics of four configurations, so a change in
  the dynamics or a re-worded sentence fails the build.

The wider program -- the Poincaré cycle at `L = 8, 16, 32`, charge quantization at `L = 32`, the
entropy cycle -- lives in the companion repository (`automaton3d/automaton`); this CPU build measures up
to `L = 11`, and the paper here is the reduced variant that carries the construction rather than the
results.  Every measurement is in the research log.

## 📊 What is measured so far

Every line below is a measurement made in this tree, and every one of them is derived in the log.

- **the reference configuration moves nothing**: `m = 0` on every layer, 0 displacements, one occupied
  center, in every frame read;
- **the dispersal is exact**: frame 2 moves all 147 layers, every sign on its own octant, and leaves the
  ledger at `K = W - 8`, `D = 8`; the era-1 cascade is 32 flight steps, all aligned (46 steps with 39
  aligned once the pair rules are on);
- **the ordering survives the pair rules for twelve eras**: the Orbis--Umbra gap floors at a few tenths of
  a cell instead of vanishing, and the saturation `K + D = W = 3L^2` holds in every frame;
- **the transport is charge-aligned, and the residual is not its direction**: of 2129 thrust calls over
  twelve eras, 0 aim against the layer's own octant; the 336 off-octant *measured* steps are 278
  relocations that no impulse explains, plus 58 along a partial-axis impulse;
- **the plateau relation holds where the transport's alignment does not**: `K = W - 8` and a pairable
  fraction near 38% at four sizes, while the era-1 aligned share falls with `L`;
- **each of those was checked against its own failure mode**, including the readings that had to be
  withdrawn.

## 📚 The long form

Every measurement behind the lines above -- the rule set measured, the displacement channel one probe at
a time, and the gates that keep both honest -- is the research log:

**➡️ [RESEARCH_LOG.md](RESEARCH_LOG.md)**

---

## 👤 About

I'm an independent researcher interested in fundamental physics, cellular automata, and emergent
computation.

ResearchGate:

https://www.researchgate.net/

---

# ❤️ Support This Project

If you find this research interesting and would like to help its development, you can support it in one
of the following ways.

## ☕ Buy Me a Coffee

You can make a secure international donation through Buy Me a Coffee:

**https://buymeacoffee.com/afurtado?new=1**

Every contribution helps fund computing resources, software, and the time required to continue this
research.

## 🇧🇷 PIX (Brazil)

<a href="pix.jpg">
  <img src="pix.jpg" alt="PIX QR Code" width="120">
</a>

**PIX key:** *(scan the QR code above)*

Any contribution, no matter how small, is greatly appreciated and directly supports the continued
development of this project.

---

*Last update: September 24, 2026.*

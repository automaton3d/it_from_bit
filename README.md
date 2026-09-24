# `it_from_bit` -- standalone build of the paper **and** the simulator

*Physics emerging from a cellular automaton.*

Created 20 Sep 2026 as a **basic version of the project to re-evaluate it**: the construction and the
measurements of the rule set, the simulator that produces them (CPU build, with the GUI), and nothing
else. No experiment tree, no harnesses, no attic, no retired material.

## The paper

| file | why |
|---|---|
| `it_from_bit.tex` | the document: Sections 1-8 (the construction), the Conclusion, the Nomenclature, Appendix A |
| `manuscript.bib` | bibliography (39 entries; read by `biber`) |
| `fig1.png` | the only included **image file** (`\includegraphics{fig1}`, one occurrence); the other figure, Fig. 1 (the charge word), is drawn in TikZ and needs no file |
| `build.bat` | `pdflatex -> biber -> pdflatex x2`, then opens the PDF |

Build with `build.bat`: expect **39 pages, 0 errors, 0 undefined references**.

The document is a reduced form of `doc/manuscript.tex` in the repository `E:\alpha`, generated there by
`attic/make_it_from_bit.ps1`: Section 9 (*Results*), Section 10 (*Conjectures and prospects*) and the
*Reproducibility* subsection are removed, and the 32 cross-references into that material are repaired.
What is missing is the numerical results, the particle taxonomy, the quantum-formalism bridge and the
prospects. Section grading is kept: (P) postulate, (M) measured, (C) candidate or conjecture. Read the
grade markers literally.

The **file paths quoted in the prose survive that reduction**, because they are evidence pointers
rather than LaTeX references: they locate material in the study archive of the program, which lives
in the companion repositories -- `E:\alpha` for the experiments and the flux harness, `E:\spiral` for
the walker visualization, `E:\automaton\tests` for the charge-algebra studies -- and is deliberately
not part of this repository.  Section 1 now says so in its "Source and supporting material" paragraph,
and puts them on record:

| quoted in the text | lives in |
|---|---|
| `experiments/DYNAMIC_QUANTIZATION_DERIVATION.md`, `experiments/J1_SPIN.md`, `experiments/census_ref_el9_s64/census.csv` | `E:\alpha\experiments\` |
| `experiments/GEOMETRIC_QUANTUM.md` | `E:\alpha\attic\closed_programmes\` |
| `quantization/`, `quantize_stdlib.py` | `E:\alpha\quantization\` |
| `spiral/spiral.c` | `E:\spiral\` (standalone SDL project) |
| `combine.c`, `virada.c` | `E:\automaton\tests\studies\` |
| `polar_fidelity.cpp` | `E:\automaton\tests\` |

The reference `\cite{af_neto}` used to point to the older repository
(`github.com/automaton3d/automaton`); it now points to this one
(`github.com/automaton3d/it_from_bit`), which is where `simulation.cpp` and `interaction.cpp` -- the
two files the text names as "the repository" -- actually live, and which is also the home of the
implementation the cell-structure table cites.

Appendix D was removed.  It preserved the historical two-bubble campaign, whose runner program and
design log are retired material kept with the study archive -- neither is distributed here.  Nothing
in the document pointed at the removed appendix (no LaTeX reference and no textual pointer).  One
mention of that experiment survives in the limitations list
-- "the controlled two-bubble experiment yields a quantitative null result there: of 6,996 overlapping
cell-frame events only 18 (0.26%) pass the s2B sieve" -- which states its own numbers and needs no
appendix.

Appendix C (*The charge-conjugation census: definitions, tables, and caveats*) was removed as well.
It documented a hand-scripted random recombination of the six-bit charge words -- the charge-algebra
studies `combine.c` / `virada.c` of the archive, not a run of the automaton -- and it was the only
place in the document that reported the charge-combination table and the proton--electron ratio.  Its
label was referenced three times (the roadmap of Sect. 1, the cross-section limitation of Sect. 8, and
the sensitivity paragraph of Appendix B) and each pointer was dropped, with no claim added or changed.
A sentence-level item in the open-problems list -- "(iii) turn the charge-combination census into
genuine cross-section predictions with a unit mapping" -- is kept: the charge-algebra studies it names
are still pointed at, as external material, in the "Source and supporting material" paragraph of
Sect. 1.  That left the variant ending at Appendix B.

The Nomenclature row that listed the charge-census totals (`$D_{tot}$, $d_{pair}$, $d_{free}$`) was
dropped too: those symbols were defined nowhere else in the variant, so the entry pointed at material
this document no longer contains.

Appendix A (*Pair-rule charge examples*) was removed as well.  It was the only place in the document
that tabulated the six pair rules R1-R6 as concrete bit-strings, and its table was the last table of
the document (Table 4).  The body text pointed at it twice, in one sentence of Sect. 5 (*Encounter*)
-- "To make the six rules concrete, Table~\ref{tab:charge-examples} in
Appendix~\ref{app:charge-examples} lists representative bit-strings and the pairs they form (...)" --
and that pointer was repaired by dropping it while keeping the notation note the sentence carried, so
it now reads "The bit order ... and the color code follow Table~\ref{tab:Cell-structure}."  Nothing
else named the appendix, no citation lived in it (the `.bbl` is unchanged by the removal), and, being
the last table, it renumbered nothing: Tables 1-3 remain.  LaTeX re-lettered the former Appendix B
(*Calculation of X and L*) to Appendix A on its own, and no literal "Appendix B" occurs in the text,
so the surviving appendix references resolved without edits.  The document was still 36 pages at that
point; the only thing lost is the worked example table itself.

Appendix A was then rewritten so that the **Planck length appears nowhere** in the document (this
variant, unlike the `E:\alpha` copy, carries no such reference).  Gone are the route that fixed
`X = l_P` and gave `L = 3.1e61`, the content route framed as a *failed consistency check*
(`L = 3.1e30`), the whole *discrepancy* subsection (the `1.0e31` ratio and the "matter density of
order the Planck density" remark), the Nomenclature row for `l_P` and the `H1 = l_P` hypothesis.  The
only mention of the term left in the body -- the related-work sentence on 't Hooft, which described
his information loss as occurring "at the Planck scale" -- now reads "at the smallest scale": that
wording is the one place where this change touched a cited author's description, and it can be
reverted on its own.

Appendix A was then rewritten again, with the inputs the model itself declares, and renamed *The scale of
the lattice* (the label `sec:Calculation-of-X` is kept, so every cross-reference still resolves).  The
geometric relation now uses the **cavity radius** -- `O/2 = (L/2) X`, i.e. `O = L X`, since `RMAX = L/2`
cells is the radius of the spherical cavity the rule evolves -- instead of the diagonal of the enclosing
cube.  The material relation now uses the **internal address ledger** `W = N_I l = 3L^2`, read as
`N_I = 192L` charge fragments of `l = L/64` addresses each, and one bubble per internal address:
`n_bT = N_I l = 3L^2 = 2[f] n_gT O/c`.  The appendix then solves the two relations under the two identifications
of what a bubble occupies.  Counting internal addresses gives `L = (2[f] n_gT O / 3c)^(1/2) = 1.3e61` and
`X = O/L = 6.6e-35 m`, with `W = 5.3e122` addresses, `N_I = 2.6e63` fragments of `2.1e59` addresses each,
and `L^3 = 2.4e183` spatial cells.  Counting spatial cells (`n_bT = L^3`) gives `L = 8.1e40` and
`X = 1.1e-14 m = 11 fm`.  Observation separates them: a lattice carries only wavelengths above `2X`, so
`X <= hbar c / E_max = 2.0e-27 m` (the LHC alone gives `3.0e-20 m`).  The cell-based reading exceeds that
by `5.5e12` -- twelve orders in the cell size, with `(X/lambda)^2 = 3.0e25` where the observed absence of
energy-dependent arrival times in cosmic rays constrains it far below unity -- so it, and the
identification that produces it, are refuted; the address-based solution passes with
`(X/lambda)^2 = 1.1e-15`.  The `7.5 fm` of the previous revision was the cell-based reading under the
older inputs, and is refuted for the same reason.  That reading is **not** presented as a co-equal
solution: Appendix A now has one solution (A.3, "The lattice side") and carries the cell-based count only
as the excluded alternative inside the refutation (A.4), so no number there is labeled a result of the
model.  Nothing was lost in the demotion: every figure of the deleted display (L = 8.1e40,
X = 1.1e-14 m = 11 fm, the 22 fm zone edge, 9 MeV, the 1.46 fm pion, the 3.0e-5 fm LHC proton,
X/lambda = 5.5e12, (X/lambda)^2 = 3.0e25) survives in the refutation paragraph, together with the
statements that the two readings share the content and the radius relation, that they differ by a factor
`L` in the number of places, and that nothing in the relations singles out the internal addresses.

The **9L x L/3 grouping** that the simulator's seed uses (`ISLAND_COUNT = 9*EL`,
`ISLAND_SIZE = W/(9*EL) = L/3`) is, as the author states, a residue of an abandoned line of research.  It
is no longer presented as the model's declared partition anywhere: the appendix uses only the ledger
product `W = N_I l = 3L^2`, Sect. 8's chief-ratio sentence (which compared the measured chiefs with the
abandoned `N_I = 9L` through the factors 2.9x at `L = 9` and 4.9x at `L = 15`) was rewritten to drop
those factors -- the measured chief counts themselves are untouched -- and a provenance note now stands
at the two simulator sites (`initSim.cpp:511`, `utils.cpp:58`).  The parity of `L` is the remaining
question: the runtime demands odd edges for a unique center (`CENTER = (L-1)/2`, "every edge must be
odd"), which makes `l = L/64` an exact ratio rather than an integer count of addresses; moving to `L` a
multiple of 64 would make `RMAX = L/2` and the era period exactly `L`, but it would need revised center
rules, a re-run, and it would retire the published odd-`L` runs (`L = 7, 9, 15, 31`).  The paper depends
only on the product `W = 3L^2 = N_I l`, so the calibration is unaffected either way.

The lattice-side constraint was cleaned to match: `L` is now simply **a large integer**.  The odd-edge
restriction is named as a convention of the reference seed (a single central cell, `CENTER = (L-1)/2`),
and the **physical ledger takes `L` a multiple of 64**, which makes the ledger's copies `l = L/64`
integers, with `RMAX = L/2`, the era period and the antipode then exact.  The `divisible by 3` was dropped
because it served only the abandoned grouping (`ISLAND_SIZE = W/(9*EL) = L/3` plus the `EL%3` census
diagnostic); the `w%3` / `w/3` addressing in the interaction code needs `W` divisible by 3, which
`W = 3L^2` gives for every `L`, and at `L = 7` the seed's family size was already truncated
(`147/63 = 2`).  A measurement settled the parity on its merits: the shells around the central cell are
*identical* for odd and even `L` at small radius (`1, 26, 98, 218` cells), so the odd rule protects
nothing about small-radius isotropy; the parity shows up only at the end of the expansion, where even `L`
closes on a **single** antipodal cell (with `6` and `12` cells just below, the cube's face and edge
orbits) and odd `L` closes on a `2x2x2` block of **eight** cells at equal distance.  The reference runs
at odd `L` (`7, 9, 15`) are untouched, since the rule is general in parity.  The era/period sentence of
Sect. 4 ("each branch lasts `L/2` frames, so one era has period `L = 2 RMAX`") was deliberately left as
written: it is exact for the physical ledger (a multiple of 64) and reads `L-1` ticks for the simulated
odd lattices, a consequence of the seed convention rather than a statement about the rule.

The factor **4** of the content relation (`n_pT = 4 Ed n_p`) no longer appears unexplained: it counts
matter and antimatter -- the charge word and its complement, produced in pairs by construction -- in the
two internal sectors Orbis (`w1 = 0`) and Umbra (`w1 = 1`), which coexist spatially, the two up-quark pair
rules R5 and R6 differing only in `w1`.  It is flagged as a modeling assumption and listed as `A1`; the
other assumptions are `A2` (the partition `N_I = 192L`, `l = L/64`) and `A3` (one bubble per internal
address -- the reading the refuted alternative replaces), while the measured inputs are `H1` = `O`,
`H2` = `c`, `H3` = `m_p`, `H4` = `Ed` and `H5` = `[f]`, which cancels.  Sensitivities are given for both
readings: in the surviving one, a factor 2 in any single input moves both `L` and `X` by `2^(1/2) = 1.4`.
The document is 39 pages after the charge-word figure recorded below.

Section 3.4 (*Charges*) then gained **Figure 1, "The charge word"** -- a TikZ field diagram spanning half
the text width (8.2 cm of 16.5 cm).  It shows the six bits most significant first (`w1 w0 q c2 c1 c0`,
with the bit indices above the boxes), braces the three sectors underneath (weak/chirality, electric,
color), and lists the eight color codes with their signature `sig = c2 + c1 + c0` and the class it
fixes (`M` for `sig < 2`, `Mbar` for `sig >= 2`, with `N` at `sig = 0` and `Nbar` at `sig = 3`).  The
figure draws what the surrounding text already states; the only text added is the pointer sentence
"Figure 1 draws the field layout and the full code table."  No claim and no number changed.  Two
consequences: `fig:charge-word` is the first figure in the source, so the other figures shift by one
(all references go through `\ref`; no hard-coded figure number exists anywhere in the text), and the
preamble gained `decorations.pathreplacing` for the sector braces.  Figure and caption occupy about a
third of a page -- the 38 -> 39 page change.

Appendix A then had an **annotation pass** (no number or claim changed; ~250 words added).  Each relation is
now labeled for what it is: Eq. (14) of the appendix is "an identification of scale" and Eq. (15) "an
identification of capacity", with a roadmap sentence keeping identifications, consequences, test and
inputs apart (A.1-A.2 / A.3 / A.4 / A.5); A.4 was retitled *What the observations exclude* and opens by
saying that it is the only step in which observation does work.  A paragraph *What a bubble is, in this
counting* states the three premises the content count had left implicit: the count is an **instantaneous
inventory** (no lifetimes, overlap, reuse or internal state -- a rate would be a different quantity); the
bubble's time scale is the **era**, not the tick, so its fundamental frequency is `f0 = c/(L X)` (the same
`f0` the chain uses, derived from the shell advancing one cell per light frame, not a new assumption); and
the counting fixes an **energy per bubble**, `h f0 = hc/(L X)`, one quantum of the cavity's fundamental
mode, which is the bridge from the rule's integer photon frequency to hertz.  A closing paragraph *Scale*
records what is and is not known about the step from `L <= 31` to the calibrated value: the only measured
scaling relation is the ledger saturation `K + D = W = 3L^2` of the frozen plateau (exact at `L = 9` and
`L = 15`), which is made of the seed's singletons and is not evidence of aggregation; extrapolated it would
put ~`1e42` bubbles per baryon family and, if it held, as many islands as addresses, which cannot be
reconciled with a particle being one island.  No asymptotic analysis is offered, and the appendix says so.
A.1 also now records that the anchor choice (side, circumference or diagonal of the enclosing cube) moves
`X` by the square root of the anchor, i.e. by less than a factor two.

The audit's duplication item was closed as well.  Sect. 8.3 (*Origin of W and FSM addressing*) had restated,
as two display equations, the size `W = 3L^2` and the partition `W = N_I l` that Sect. 3 and Sect. 4.1
already give.  It is now a single cross-referenced sentence -- the quadratic scaling is pointed at Sect. 3,
the partition at Sect. 4.1 -- with the whole FSM-register content of the subsection kept (the `W`
coordinate as a pair of registers, the locality argument, the one-time initialization and the fact that no
aggregation result depends on the particular values).  Two numbered equations disappeared, which shifted
the appendix's numbers: `eq:obs` is now Eq. (14), `eq:content` Eq. (15) and `eq:liv-bound` Eq. (16); all
references are label-based, so nothing else changed and the build reports no undefined reference.

Three items from a simulated peer review (*Foundations of Physics*, Major Revision) were then applied --
**framing only, no claim and no number changed**: (1) the **abstract** now states that the lattice scale is
not derived and that the one place where it meets observation is an order-of-magnitude calibration, and
**Sect. 1** (*Claims and status*) adds that the only place where the dimensional parameters are matched to
observation is Appendix A, a calibration whose single observational test rejects one reading rather than
validating the other; (2) **Appendix A (A.3)** now labels `L` and `X` explicitly as order-of-magnitude
illustrations of Eqs. (14) and (15), with the sensitivities moving them only by factors of order unity;
(3) **Sect. 9.4** gained a *No finite-size scaling study* item -- every measurement comes from `L <= 31`,
the rule carries no length scale to renormalize, so what is missing is a finite-size scaling study of the
dimensionless observables (membership, population quantum, capture/escape rates, `K/W`), and the only
measured scaling relation is `K + D = W = 3L^2` at two sizes.  Two review items are left out **by decision**
for this stage, and the reasoning is recorded so the omission reads as deliberate: a **Bell /
hidden-variable** section is not added, because Sect. 1 already states that the model is "not offered as an
interpretation of quantum mechanics, but as a primitive descriptive layer" -- the theorem constrains models
that reproduce quantum correlations, which this text does not claim to do, and opening the topic would
invite a critique of an argument the paper never makes; and **no unitarity discussion** is added, for the
same reason (the text links its irreversibility to measurement as a novelty and asserts no unitary
microscopic limit).  Those two remain the points a *Foundations of Physics* referee is most likely to
raise, and they are the first things to add if the paper is ever repositioned as a quantum-foundations
proposal rather than a descriptive toy.

A note in the header comment of `it_from_bit.tex` records this and the other edits made
here, so the file's provenance against `E:\alpha` stays readable.

The journal class `ijuc.cls` is no longer used: the document is typeset by the standard `article`
class (`\documentclass[a4paper]{article}`) and `ijuc.cls` was removed from the repository along with
it.  The IJUC layout went with the class -- the 108mm text block, the Times fonts, the symbol
footnotes, the upper-case section headings, the `FIGURE`/`TABLE` captions, the
`\institute`/`\email`/`\inst` title macros and the `\vfill` + `\newpage` that gave the paper a
separate title page.  The title block is now standard LaTeX, with the e-mail as a `\thanks` footnote
on the author line; the PACS numbers and the keywords lines are content rather than layout and were
kept as plain centered blocks.  `geometry` already set the page size, so the class switch changed no
page geometry (the document was still 36 pages at that point): what changed is the type face (Computer
Modern), the caption style, the footnote marks (numbers instead of symbols) and the heading style.

## Two text-to-code corrections, and the charge-word letters

Both discrepancies the first-era trace uncovered are now fixed in the text (no number changed):

* the **sieve update** is no longer described as propagated between frames.  The text said
  `s2B' = s2B and active`, while `simulation.cpp` assigns `d.s2B = active && trigger` inside
  `phase_step`: the bit is recomputed from the local `u` and `t` at every phase step, so what the
  cell stores is the test of the current step.  The sentence now says that;
* the **dispersion** of Sect. 5.4 is marked for what it is: a candidate mechanism, compiled off in
  the reference build (candidate macro `HOMB_PRODUCER_FSM`, whose counter `homb_events` stays 0),
  with the reference run separating the sectors through its ordinary encounter channels instead.
  The timed condition is now written `floor(RMAX/2)` -- at least one cell for every admissible `L`
  (one at `L = 5, 7`; two at `L = 9`) and never the center; the floor used to be implicit.

In Figure 1 the six charge bits are set in bold dark navy (`\definecolor{chargeblue}`) one size up
(`\Large` instead of `\large`), so the word the whole charge sector rests on is where the eye
lands.  The box is untouched, and that is measured rather than asserted: an A/B render of page 8 at
150 dpi gives the same outline rows to the pixel (top and bottom edges both span x = 416..872, the
color strip below at x = 416..887), while the letters went from 14 px to 18 px of ink height and from
0 to 73-116 blue pixels per cell.
## The impulse channel, the abandoned /3, and charge-seeded dispersion

Three changes in the model, all measured with the first-era harness.

**1. The impulse hand-off (fix, reference path).** The Encounter booked its displacement in
`sourceAfter[w].reloc` -- a per-tick working array that is re-seeded from the lattice at every
`beginSourceTick()` and discarded at the end of the tick -- while only `commitSourceTick()` hands
the value to the draft that `applyMomentum()` consumes, so a booking had nowhere durable to live.
A model-level, linker-visible queue now carries it: `reemitSourceAt()`/`bookImpulse()` append,
`commitSourceTick()` drains into the draft's source-centre cell **after** its field-copy loop (that
loop *assigns* `dst.reloc`, so an earlier drain would be overwritten), and `resetSourceTransactions()`
clears it on a new run. Measured with the sieve open (L=7): 1456 non-zero bookings, 1456 drained,
`|reloc|` sum 12376 reaching the draft. The regression control -- the reference configuration
(sieve closed), where no booking exists at all -- is unchanged: all counters zero,
`occupiedCenters = 1`.

**2. The abandoned `/3` grouping.** The candidate homing block grouped by `x[3]/3` and tested
`x[3] % 3 == 0`, i.e. it hardcoded `ISLAND_SIZE == 3` (true only at `L = 9`, where `L/3 = 3`); at
every other size it grouped by triples of addresses. It now uses the model's own runtime helpers
(`islandOf()`, `isIslandChief()`, `ISLAND_COUNT`) and the manuscript's dispersion condition, the
sector `w1` (the middle colour bit). Its address tie-break -- which contradicted the stance
`elect()` states ("no address, hash, global dial, or scan order may decide a tie") -- is replaced by
the charge-derived direction below. The block is compiled off, so this is a correctness change for
whoever enables it.

**3. Charge-seeded dispersion (candidate, `/D CHARGE_DISPERSION_FSM`).** While a layer has no
elected direction (`m == 0`, the phase delimiter), and once the shell has left the centre
(`effective_t(t) == 1`), each layer takes one unit step along the octant of its own colour triplet:
`step = (±c2, ±c1, ±c0)`. Because the seed's word is a function of `island mod 8`, the three colour
bits *are* the three coordinate signs, so the sector bit `w1 = c1` is the y-sign: Orbis steps `-y`,
Umbra `+y`, and the four `k <-> 7-k` colour-complement classes get exactly antipodal steps (matter
and antimatter separate, centre of mass preserved). Diversity comes from the charge word alone: no
W address, no hash, no scan order, no RNG. The latch re-arms at the era edge and the rule stops for
a layer as soon as a direction is elected for it.

Acceptance (first era, sieve closed, charge rule ON):

| observable | L=7 (W=147) | L=9 (W=243) |
|---|---|---|
| steps booked / drained / applied | 147 / 147 / 147 | 243 / 243 / 243 |
| `net-layers` / `net-max` (signed net per layer) | 147 / 3 | 243 / 3 |
| `occupiedCenters` (was 1) | **8** | **8** |
| Orbis mean y / Umbra mean y | -1.000 / +1.000 | -1.000 / +1.000 |
| centre of mass minus lattice centre | (-0.020, -0.020, -0.007) | (-0.012, -0.012, -0.012) |

The eight positions are the eight colour classes; the sectors separate by 2 cells (one per era);
the residual centre of mass is exactly the population imbalance (75 vs 72 addresses at L=7,
123 vs 120 at L=9). The same run before the change gave `net-layers = 0` with every layer's net
cancelling -- the encounter's steps are antisymmetric within a layer, which is why the reference
fabric carries no net displacement however the plumbing is arranged.

## Closing the phase: the election seeded by the placement

The dispersion above only runs while a layer has no momentum (`m == 0`), so the phase has to be able
to *end* -- and it could not: `elect()` ranks existing `(pol_u,pol_v)` only, `pol` is identically
zero in the reference build, and the loop "election -> broadcast -> reconstruction" is therefore at
a fixed point. A second candidate macro (`/D POLAR_SEED_FROM_PLACEMENT`) closes it:

* in `elect()`, when the classic candidate finds **nothing at all** (`!candidate.present`) and the
  layer's centre has left the lattice centre, the **source centre itself** is elected -- the classic
  path already derives the axis from a cell position (`installAxis(w, cell - CENTER)`), so the axis
  becomes the octant the layer has already stepped along. No address, no hash, no scan order, no
  RNG: the stance `elect()` states for the tie-break is preserved;
* the rest of the pipeline is untouched: `bstamp` reset, helical walker, broadcast, reconstruction
  in `phase_step()`, and the *next* turnaround elects `m` from the live polarisation.

Measured (L=7, sieve closed, `CHARGE_DISPERSION_FSM` + `POLAR_SEED_FROM_PLACEMENT`):

| frame | what happens | numbers |
|---|---|---|
| 2 | charge dispersion fires (shell at radius 1) | 147 steps; `occupiedCenters` 1 -> 8; Orbis y -1.000, Umbra +1.000 |
| 4 | turnaround: placement-seeded election | `polar-seed = 147`; `m != 0` **147/147 sources**; `pol != 0` **13671 cells** (both were 0 forever) |
| 6-7 | the encounter's own transport takes over (m is live) | `reemit = 139`, `cB = 463`, 94 layers moved, 7093 clock resets |
| 8-10 | era 2: **the charge rule does not fire again** | `charge-dispersion = 0` -- the phase closed |
| 9-10 | re-election from the new geometry | `polar-seed = 94`, 14 layers still moved by the encounter |

The separation freezes at Orbis y = -1.853 / Umbra y = +1.931, and the y-component of the centre of
mass stays **exactly 0.000** while x and z drift (-0.088, -0.156): once a direction exists the fabric
drifts, which is the expected consequence of momentum being real rather than a defect of the
antisymmetry. The reference build is unaffected (both macros off: all counters zero,
`occupiedCenters = 1`).

## Multi-era trace: what the movement does

Item 2 of the follow-up list -- trace several eras with the movement on. Runs (same harness, sieve
closed): `first_era_trace.exe 5 16384 20` (5 eras, era = 4 frames) and
`first_era_trace.exe 7 16384 36` (6 eras, era = 6 frames); logs `build\multi_L5.log`,
`build\multi_L7.log`.  **Variant note**: those two runs are the *transport-enabled baseline*
(dispersal + polar seed, pair rules untouched), not the pair-rule builds -- `build\base_L7.out`,
rebuilt here from the flags alone, reproduces `multi_L7.log` frame for frame (30 identical
`positions` lines, the length of the new run) which is what settles it.  (Those logs also predate the
rename of the harness binary to `build\first_era_trace.exe` and the split/align counters.)
An `L = 9` run was started and abandoned: once the
state fills with delegates
a frame costs minutes, and 3 frames were not worth the wait (they do confirm the era-1 part:
dispersal at frame 2, `occupiedCenters` 1 -> 8, 243 layers moved, Orbis y -1.000 / Umbra +1.000,
`K = 235`, `D = 8`).

**`L = 5`: a rigid limit cycle.** Era 1 does the dispersal (occ 1 -> 8, `charge-dispersion` 75) and
the placement election, and then *nothing moves again*: eras 2-5 have `occ = 8`, `K = 67`, `D = 8`
(`K + D = 75 = W`), `resets = 0`, `reemit = 0`, `moved = 0`, `charge-dispersion = 0`, positions
frozen at Orbis y -1.000 / Umbra +1.000, and the shell repeating its exact period-4 pattern
(75, 1950, 4950, 1950). The cavity is too small (`RMAX = 2`) for the encounter transport to have
anywhere to go. Note `pol` stays 0 here while `m` is 75/75: at this size the placement election
alone keeps the momentum alive, with no reconstruction step.

**`L = 7`: the state mixes instead of freezing.** Per-era means (era = 6 frames; `occ` = distinct
cells hosting a source; `pol`/`m` in cells/sources):

| era | frames | occ | K | D | pol | m | resets | reemit | charge-disp | net-layers | moved | Orbis y / Umbra y | max &#124;CoM&#124; |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1-6 | 1..29 | 109.3 | 13.2 | 6836 | 74 | 0 | 23.2 | 147 | 40.2 | 40.2 | -0.975 / +0.989 | 0.156 |
| 2 | 7-12 | 29..69 | 119.5 | 27.5 | 13671 | 147 | 7167 | 56.7 | 0 | 28.3 | 28.3 | -1.340 / +1.452 | 0.163 |
| 3 | 13-18 | 94..118 | 100.3 | 46.7 | 13671 | 147 | 18550 | 145.3 | 0 | 70.8 | 62.8 | -0.222 / +0.188 | 0.265 |
| 4 | 19-24 | 111..122 | 92.3 | 54.7 | 13671 | 147 | 29302 | 215.2 | 0 | 90.5 | 84.7 | +0.027 / +0.058 | 0.231 |
| 5 | 25-30 | 109..124 | 87.2 | 59.8 | 13671 | 147 | 28863 | 271.0 | 0 | 87.5 | 83.7 | +0.073 / +0.037 | 0.177 |
| 6 | 31-36 | 114..120 | 90.8 | 56.2 | 13671 | 147 | 31868 | 273.3 | 0 | 93.8 | 86.5 | +0.213 / +0.132 | 0.415 |

What this establishes:

* the charge rule does close -- `charge-dispersion` is 147 in era 1 and **0 in every later era**, at
  both sizes (the `m == 0` delimiter does its job);
* the momentum channel is **self-sustaining**: once elected, `pol` stays at 13671 cells and `m` at
  147/147 in every frame, so the encounter transport keeps running (it is what the resets, `reemit`
  and `moved` columns are);
* **the census is now generated, not inherited.** In every single frame `K + D = W` exactly, but `D`
  is no longer the seed's 8: it climbs to ~50-60, i.e. a third to a half of the layers become
  delegates of a chief, and `occupiedCenters` climbs from 8 to ~120 of the 343 cells. The frozen
  plateau of the reference build (single centre, `D = 8` forever) is gone;
* **the charge ordering does not survive the mixing.** The clean sector split of era 1 (Orbis -y,
  Umbra +y, 2 cells) is washed out by era 3 (-0.222 / +0.188), inverted by era 4 (+0.027 / +0.058)
  and no longer tracks the charge at all by era 6; the centre of mass, exactly 0 in `y` while the
  dispersal is the only mover, drifts to |CoM| = 0.42 cells. Size matters: at `L = 5` nothing mixes,
  at `L = 7` everything does.

**Diagnosis, and the next step it implies.** The engine of eras 2+ is the encounter transport, and
`m` is elected from the polarisation field, which is anchored to the *geometry* of the cavity, not to
the charge word: it is charge-blind. So the dispersal supplies the initial asymmetry and the
transport then stirs it away. If the intent is structure (islands that keep their sector) rather than
mixing, either the elected axis has to be correlated with the charge (the same octant map, used as
the tie-break `elect()` currently takes from the *first* candidate), or the charge step has to stay
active instead of being gated on `m == 0` -- it would then act as a restoring term rather than a
one-shot kick. Both are one candidate macro each, and the acceptance test is already instrumented:
`occupiedCenters` should stay near 8, the sector means should hold their sign and grow, and |CoM|
should stay at the population-imbalance level (0.02 at `L = 7`) rather than drifting to 0.4.

## Included in the paper: subsection 8.4, "Candidate mechanisms for the encounter's transport"

The candidate work of this session is now in `it_from_bit.tex`, as the last subsection of Sect. 8
(*Emergent island formation and the scaling of $W$*), immediately before the Conclusion.  It is the
first time any of it appears in the paper -- until now it lived only in this file.

What the subsection states, in the paper's own voice (measured versus candidate, macro names in
`\texttt`, American English):

* the measured state of the **reference build**: at frame 1 the $W$ sources are one singleton each at
  the lattice center sharing one clock value, so the ledger is settled by the first transition (by
  frame 2 it is at its plateau, $K=W-8$, $D=8$ at $L=7$, and does not change again within the era);
  the polarization pair and the momentum are identically zero ($(pol_u,pol_v)=\mathbf 0$,
  $\boldsymbol m=\mathbf 0$) because the zero-polarization seed is the fixed point of the election /
  broadcast / reconstruction loop; and the encounter's pair walk displaces $94$ layers in the era-1
  cascade with **none** of them following the layer's own color octant (79 differ in exactly one sign);
* four **candidate mechanisms**, each behind its own macro, all compiled off in the reference build
  and measured on $L\le 9$: `CHARGE_DISPERSION_FSM` (the $W$ layers resolve into eight blocks, Orbis
  at negative $y$, Umbra at positive $y$, center of mass at the population imbalance, 147 of 147
  displacement signs matching the octant), `POLAR_SEED_FROM_PLACEMENT` (the fixed point closes:
  $\boldsymbol m\neq\mathbf 0$ on every layer, 13 671 cells with a reconstructed pair),
  `PAIR_SAME_OCTANT` with `PAIR_OWN_AXIS_EXCHANGE` (the split holds its sign for four eras and beyond,
  never inverting where the reference inverts by era 4; the ledger stays at the plateau; every flight
  displacement is octant-aligned, and the single-axis cohesion steps are the only axial motion);
* an explicit closing caveat: these are candidates, not results of the reference build, and nothing is
  claimed about the transport at the lattice size the appendix takes for the fabric.

Verification: 39 pages (unchanged), 0 errors, 0 undefined references, 4 overfull boxes (the same
pre-existing set), and the new text passes the manuscript's American-English policy -- my own first
draft relapsed into "colour"/"centre"/"polarisation" and was corrected before the build.

Page-count note for the twelve-era text that follows: the three algorithm floats were declared
`[ht]`, which forbids float pages, so they had been pushed to the tail of the document (the appendix
ended and three float-only pages followed).  With the new paragraph added, Algorithm 3 no longer fit
and the document grew to 40 pages with a lone float page at the end.  Declaring them `[htp]` lets
them pack onto float pages next to the subsection that cites them (Algorithms 1-3 now on pages 23-26)
and the document is back to **39 pages**, with Appendix C's table as the last page.  Same content,
same 4 overfull boxes.

## The mixtures, named: thrust + cohesion, and every one of them aligned

The last open thread was the handful of moved layers per cascade whose displacement was not
octant-aligned.  The writer mask (`s2bTraceWriterMask`, one bit per site that can displace a layer,
tagged at `reemitSourceAt`, `reseatStepToward`, `reseatAtContact`, the cohesion table, the charge
dispersion and the own-axis thrust; count reported by the harness line `move-mask`) settles it.  Run
`build\mask3_L7.log`, L = 7, frames 6-9, reading `& 0x3F` so that the queue-drain bit is not counted
as a mover of its own:

| frame | one distinct mover (aligned) | two or more (aligned) | flight (split line) | cohesion |
|---|---|---|---|---|
| 6 | 39 (32) | **7 (7)** | 32 (32) | 14 |
| 8-9 | 14 (0) | 0 | 0 | 14 |

* **genuine mixtures do exist, and they are small: 7 of 46 at the cascade.**  Those 7 layers received
  **both** a cohesion face step and an own-axis thrust in the same frame, and **all 7 came out
  octant-aligned** -- the thrust dominates the axial cohesion step;
* the remaining 7 unaligned moves are **cohesion-only** layers (one mover, axial by design);
* so the account at the cascade frame closes exactly: 32 thrusts (all aligned) + 7 thrust+cohesion
  mixtures (all aligned) + 7 cohesion-only (axial) = 46 moved, 39 aligned, and **nothing
  unexplained**;
* the pending-impulse dump confirms the same thing from the other side: every impulse pending at the
  commit carries **mask 0x20** (the own-axis thrust) and is an integer multiple of the layer's own
  octant (1x, 2x, 8x, 16x -- the accumulated ticks of one frame).

Two instrumentation traps were hit and fixed on the way, both worth remembering: the mask vector was
never sized, so `tagWriter` was a no-op and the first masked run reported "no mixtures" over a column
of zeros; and the drain bit made the cohesion path look like two movers.

## The probe: the pending impulses are aligned, and half the residual was the harness

`S2B_DUMP_PENDING` prints every impulse found pending at the commit, with the layer and its charge
word.  Run `build\dump_L7.log` (the probe needs `-D S2B_DUMP_PENDING`).  What it printed:

```
[pending] w=80 reloc=(-2,-2,-2) ch=000 kind=2      (class 0, octant -,-,-)
[pending] w=82 reloc=(-2,-2, 2) ch=019 kind=2      (class 1, -,-,+)
[pending] w=84 reloc=(-2, 2,-2) ch=02a kind=2      (class 2, -,+,+)
[pending] w=88 reloc=( 2,-2,-2) ch=004 kind=2      (class 4, +,-,-)
[pending] w=94 reloc=( 2, 2, 2) ch=037 kind=2      (class 7, +,+,+)
```

Every booked impulse matches its own octant, on all eight classes, with magnitude 2 (two ticks of the
own-axis thrust accumulated before the frame's `applyMomentum`).  So the impulses were never off-axis
-- and that sent the search back to the measurement, where the bug was:

**the harness computed displacements on unwrapped coordinates.**  A centre that crosses the lattice
boundary moves by one cell, but `x_now - x_prev` reads `+-(L-1)`, which looks exactly like an
off-octant jump -- and only appears once the centres have drifted, i.e. in era 3 onward, which is when
the "off side" flights first showed up.  Fixed with torus normalisation in the harness
(`wdelta`), the frame-12 row changes from

| frame 12 | on side (aligned) | off side (aligned) |
|---|---|---|
| unwrapped (before) | 2 (1) | 4 (**0**) |
| wrapped (after) | 2 (1) | 4 (**2**) |

so **half of the apparent residual was the metric**, not the model.  The two remaining cases are a
drifted layer whose step genuinely ran against its octant; the re-anchor macro does not touch them
because the only in-frame booker is the own-axis thrust, which is aligned by construction -- so those
two are either a mixture of movers in one frame or a mover still unidentified.

**The same bug contaminated the earlier tables -- re-measured, the picture is different.**  With the
displacements *and* the sector means normalised (`torusDelta` in the harness; run
`build\remeasure_L7.log`, 40 frames, same macros):

| frame | era | Orbis y / Umbra y (normalised) | = raw? | flight on side (aligned) | flight off side (aligned) | raw off side |
|---|---|---|---|---|---|---|
| 6 | 1 | -1.480 / +1.583 | yes | 32 (32) | 0 | 0 |
| 12 | 2 | -1.480 / +1.403 | yes | 2 (1) | 4 (**2**) | 4 (0) |
| 14 | 3 | -0.813 / +1.208 | yes | 25 (22) | 23 (**19**) | 23 (1) |
| 18 | 3 | -0.867 / +1.014 | yes | 9 (8) | 21 (**11**) | 21 (8) |
| 21 | 4 | -1.080 / +0.611 | yes | 5 (2) | 17 (**10**) | - |
| 24 | 4 | -1.080 / +0.847 | yes | 5 (3) | 19 (6) | - |

Two corrections to what was written above:

* **the split's decay is real.**  Every `y` value is identical with and without normalisation, so the
  sector means never wrap in `y`: the magnitude genuinely falls from about +-1.5 to +-0.9 over four
  eras while the sign holds.  The fabric drifts and slowly mixes within the sectors;
* **the off-side layers are mostly aligned after all.**  Normalising turned the off-side column from
  1 of 23 and 8 of 21 into **19 of 23 (83 %) and 11 of 21 (52 %)**.  The claim that drifted layers
  step against their octant was, for the most part, the unwrapped metric.  What remains is a handful
  of genuine cases per cascade (4 and 10 out of 23 and 21) -- consistent with a layer receiving two
  movers in one frame, not with a systematic inversion.

## (6) Re-anchoring the step: implemented, and it changes nothing -- the residual is not a booked step

Candidate `/D PAIR_OCTANT_REANCHOR`: when the geometric step (the sign of `shortestDelta`) disagrees
with the layer's own charge octant, take the octant instead.  Applied at every funnel that books a
walk impulse -- `moveOneStep`, `moveOneStepAway` and the contact reemission `reemitAtContact` (which
writes `reloc` itself and was missed by the first attempt).  A zero component is not a disagreement,
so a no-op stays a no-op.

**Measured: no effect.**  With the macro on the frames are identical to the run without it -- frame 12
`on-side 2 (1 aligned) / off-side 4 (0 aligned)`, frame 14 `on-side 25 (23) / off-side 23 (1)`
(`build\reanc_L7.log`, `build\reanc2_L7.log`).  So the off-octant flight steps are **not** produced by
those three paths.

**What the counters then say** (frame 14 of the same run): `reemit = 0` (the reset counter of
`reemitSourceAt`, which is incremented unconditionally, so that function was never called),
`booked-on-lattice = 8` — exactly the cohesion table's 8 — `reseat-step = 0`,
`reseat-at-contact = 0`; and yet **64 layers carry `reloc != 0` at the commit** and **72 impulses are
applied** (64 + the 8 drained from the queue).  Every other writer of `reloc` in the model is inside a
macro no build defines: the three sites inside `applyMomentum` itself
(`old.reloc[bestAx] += bestStep` at two places, and the pair step `old.reloc[axis] += sign` /
`partner.reloc[axis] -= sign`) sit inside `HOMB_CONSUMER_TRANSPORT`, `ADDRESS_TARGET_FSM` and
`ORPHAN_MEDIATOR_SUSTAIN` respectively.

So the residual is **not a step booked this frame**: the impulses are *inherited* -- the commit's
`dst.reloc = s.reloc` copies whatever `sourceAfter` holds, `sourceAfter` is re-seeded from
`lattice_curr` each tick, and `applyMomentum` zeroes `reloc` only on the **centre** cell of a layer it
moves.  That makes the off-octant residual a **second application of stale impulses**, i.e. a defect
rather than a design choice.

**Next step (one probe)**: count, at the commit, how many of the pending impulses were already present
at the end of the previous frame -- or simply zero the draft's `reloc` for the layers the commit
writes.  Either confirms the inheritance and removes the residual.

## Multi-era run with (a)+(b): the split holds its sign, not its size

`build\long_L7.log`, `L = 7`, 40 frames (6.6 eras) launched, read to frame 27 (4.5 eras; the run was
stopped there once the trend was clear).  Per-era values of the sector means and the flight channel:

| era | end-of-era Orbis y / Umbra y | max &#124;CoM&#124; | flight moved / octant-aligned | cohesion |
|---|---|---|---|---|
| 1 | -1.480 / +1.583 | 0.061 | 32 / 32 | 14 |
| 2 | -1.480 / +1.403 | 0.109 | 7 / 12 | 18 |
| 3 | -0.867 / +1.014 | 0.422 | 95 / 41 | 28 |
| 4 | -0.920 / +0.500 | 0.224 | 43 / 20 | 30 |

Two readings:

* **the sign of the split never inverts**: Orbis stays negative and Umbra positive through four eras,
  where the reference run inverted it by era 4 (Orbis +0.027 / Umbra +0.058).  The ordering the
  dispersal established is *not* destroyed;
* **but the magnitude decays** (from about +-1.5 to +-0.5-0.9) and the centre of mass wanders up to
  0.44 cells, so the state is not stationary: the fabric drifts and slowly mixes within the sectors.

**A new fact appears in era 3**: the flight channel itself stops being fully octant-aligned (era 1-2:
32/32 and 7 of 12; era 3: 41 of 95; era 4: 20 of 43).  `m` does not change -- the axis-align line
still reports all 147 layers parallel to their charge octant -- and the cohesion steps are counted
separately, so this is a *flight* displacement that is not along the layer's axis.

**Measured, and the explanation holds** (harness line `move-drift`, run `build\drift_L7.log`, `L = 7`,
30 frames launched and read to frame 18).  For every flight step the harness now reports whether the
layer's centre is still **on its octant's side of the lattice centre in all three axes** (bucket
"on side") or has crossed it in at least one ("off side"):

| frame | era | flight on side (aligned) | flight off side (aligned) |
|---|---|---|---|
| 6 | 1 | 32 (**32**) | 0 (-) |
| 11 | 2 | 6 (**6**) | 0 (-) |
| 12 | 2 | 2 (1) | 4 (**0**) |
| 14 | 3 | 25 (**23**) | 23 (**1**) |
| 18 | 3 | 9 (**9**) | 21 (**8**) |

The off-octant flight steps are overwhelmingly the drifted layers: off-side they are aligned 1 of 23
and 8 of 21, on-side 23 of 25 and 9 of 9.  The mechanism is the one predicted: the walk books
`sign(shortestDelta(receiver, partner))`, and `shortestDelta` takes the short way round the torus, so
once a centre has crossed the lattice centre in an axis the shortest offset to a same-octant partner
points the *other* way and the step runs against the octant.  (Two residual on-side misalignments at
frame 14 show the bucket is the receiver's side, not the pair's: a pair can straddle the midpoint.)

**The fix the measurement implies**: re-anchor the step to the layer's own octant when the shortest
offset to the partner disagrees with it -- i.e. step along the direction the layer owns instead of
the geometric offset.  Acceptance test: both buckets, on side and off side, should read 100 %
aligned at every era.

## (b) Own-axis exchange: the transport comes back aligned, and the sector split survives

Design (b), implemented as `/D PAIR_OWN_AXIS_EXCHANGE`: in the D x D cross-tribe branch the reaction
keeps the **per-axis magnitude** the exchange would have delivered but takes its **direction from the
receiver's own charge octant** (`c2,c1,c0 -> x,y,z` signs, the map the dispersal uses).  The
geometric repulsion is kept only for same-octant partners, where "away from the partner" already lies
on the shared axis.  No address, hash, scan order or RNG enters.

Measured at `L = 7`, era 1 (`build\own_L7.log` = (b) alone, `build\both_L7.log` = (b) + (a)):

| variant | frame 6 moved | `move-align` 0/1/2/3 |
|---|---|---|
| transport-enabled baseline (dispersal + polar seed) | 94 | 0/15/79/0 |
| (a) alone (contact handler only, without the exchange guard -- see the note below the twelve-era run) | 46 | 0/7/39/0 |
| (b) alone | 101 | 0/22/40/39 |
| **(a) + (b)** | 46 | **0/7/0/39** |

* (b) alone already puts **39 displacements on the layers' own axes** (the baseline had none), but
  the unguarded pair-walk steps still contribute 40 two-sign moves;
* with (a) and (b) together the cross-octant signature is **gone** -- 0 at two signs, 39 fully
  aligned -- and the 7 at one sign are a small channel not yet identified (see below).

**And the ordering survives.** Comparison at the era-2 turnaround (frame 11-12), where the baseline
(transport enabled, interaction unmodified) loses it:

| | (a)+(b) | baseline |
|---|---|---|
| Orbis y / Umbra y at f11 | **-1.533 / +1.583** | -0.333 / +0.347 |
| at f12 | **-1.480 / +1.403** | -0.293 / +0.639 |
| `moved` at f11 | **8** | 96 |
| census | `K = 139`, `D = 8` (the reference plateau) | `K = 76`, `D = 71` (churn) |
| `&#124;CoM&#124;` max over frames 6-13 | **0.109** | 0.265 by era 3 |

So with the interaction redirected onto each layer's own axis the fabric keeps the sector split the
dispersal established -- signs and magnitude held across two eras -- stays at the reference plateau
instead of churning, and the centre of mass stays near the population-imbalance level.  The transport
is alive but *quiet*: only the era-boundary cascades move anything (46 at frame 6, 8 at frame 11).
(For what the same variant does over twelve eras, see the run tabulated below: the ordering survives,
the magnitude floors at a few tenths of a cell, and the Umbra mean crosses the lattice centre.)

**The residual channel, identified** (instrumentation pass: counters for the two `reloc` writers that
had none, plus the applied vectors; `build\relay_L7.log`):

* the relay shuttle is **not** it -- `reseatStepToward` and `reseatAtContact` are **0 in every frame**
  of the run, including the cascade frames (hypothesis falsified by measurement);
* it is the **cohesion transport**: the `moves[]` table of `resolveInternalContacts`, booked through
  `bookImpulse` (line 806), which steps K-D pairs that share a chief.  Its step is `faceStep(...)` --
  deliberately a **single-axis face step** (`interaction.cpp:701`, "Select a face step, not a
  displacement of |m| cells"), never a displacement along the diagonal;
* the numbers close exactly: at frames 8-9 the table books **14** impulses and the harness measures
  **14** moved layers with alignment `7/7/0/0` -- 7 axial steps along their own axis sign and 7
  against, which is precisely what an axial step gives when compared with a diagonal octant.  At
  frame 6 the same split appears inside the 46: 32 exchange thrusts (all three signs, aligned) + 14
  face steps (`7/7`) = 46, matching the measured `0/7/0/39`.

So with (a)+(b) the **charge-flight displacements are 100 % octant-aligned at the cascades** (147/147
at the dispersal, 32/32 at the era-1 cascade, 6/6 at frame 11; the twelve-era run tabulated below
shows the share falling in the later eras) and what remains is
*cohesion* motion, axial **by design** and not expected to be charge-aligned: it exists to hold an
island's members together, not to move a species.

**The two channels are now reported separately** (harness line `move-split`: the model flags, per
layer and per light frame, the impulses booked by the cohesion table, `s2bTraceCohesionFlag`).  Run
`build\split_L7.log`, `L = 7`, 13 frames:

| frame | flight | flight 0/1/2/3 | cohesion | cohesion 0/1/2/3 |
|---|---|---|---|---|
| 2 (charge dispersal) | 147 | 0/0/0/**147** | 0 | - |
| 6 (era-1 cascade) | **32** | 0/0/0/**32** | **14** | 0/7/0/7 |
| 8-9 (era 2) | 0 | - | 14 | 7/7/0/0 |

Every flight displacement matches all three signs; no cohesion displacement ever does (a single-axis
face step can match at most one, and at frame 6 seven of the fourteen carry two because a layer was
stepped along *two* different axes by the K-D and propeller passes).  The acceptance criterion can
therefore be read on the flight column alone, which is what it was meant to measure.

## Decision pack: promoting a candidate configuration to the default build

"Promoting" means making the four candidate macros (`CHARGE_DISPERSION_FSM`,
`POLAR_SEED_FROM_PLACEMENT`, `PAIR_SAME_OCTANT`, `PAIR_OWN_AXIS_EXCHANGE`) part of the default build
instead of `#ifdef`-guarded options.  This section is the evidence for that decision: which canonical
numbers survive, which do not, what the build mechanics are, and what would have to be re-measured.
All numbers below come from the trace harness (census columns of its per-frame table) in three
configurations at `L = 7` and `L = 9`, all with the sieve closed (`S = 16384`):

* **reference** -- no macro (`build\long2ref_L7.out`, 42 frames; `build\long9ref_L9.out`, 4 frames);
* **baseline** -- dispersal + polar seed only (`build\base_L7.out`, 30 frames;
  `build\long9base_L9.out`, 8 frames);
* **promoted** -- all four macros, i.e. the twelve-era run (`build\long2_L7.out`, 72 frames;
  `build\long9_L9.out`, 16 frames).

| configuration | seed (frame 1) | plateau (frame 2) | frozen? | first frame off the plateau | first frame with `dt != 1` | max `dt` | last frame read |
|---|---|---|---|---|---|---|---|
| L=7 reference | S=147 | K=139, D=8, dt=1 | **yes** | never (f<=42) | never | 1 | K=139, D=8 (f42) |
| L=7 baseline | S=147 | K=139, D=8, dt=1 | no | f6 (K=100, D=47) | f6 | 6 | K=65, D=82 (f30) |
| L=7 promoted | S=147 | K=139, D=8, dt=1 | no | f14 (K=136, D=11) | f6 | 5 | K=124, D=23 (f72) |
| L=9 reference | S=243 | K=235, D=8, dt=1 | **yes** | never (f<=4) | never | 1 | K=235, D=8 (f4) |
| L=9 baseline | S=243 | K=235, D=8, dt=1 | no | f7 (K=158, D=85) | f7 | 2 | K=86, D=157 (f8) |
| L=9 promoted | S=243 | K=235, D=8, dt=1 | no | f8 (K=234, D=9) | f7 | 5 | K=231, D=12 (f16) |

**What survives promotion, exactly as the paper states it.**  The seed frame is identical in all
three configurations (`K = D = P = 0`, `S = W`, one clock value), the frame-2 census is the same
`K = W - 8`, `D = 8`, `S = 0`, `P = 0`, `dt = 1` at both sizes, and the outbound shell follows the
exact torus counts identically (`147, 3822, 9702, 23226` at `L = 7`; `243, 6318, 16038, 38394` at
`L = 9`, i.e. the per-layer `1, 26, 66, 158` cells).  So every *frame-2* number the paper quotes --
the plateau, the shell geometry, the settled ledger -- is invariant under promotion, at both sizes
measured.

**What promotion destroys: the freeze.**  The reference configuration does not move at all
(`K`, `D`, `dt = 1` constant for all 42 frames read at `L = 7`), which is what the paper's
reference-build paragraph and the frozen-state appendix item describe.  Both other configurations
leave the plateau: the baseline immediately (f6 at `L = 7`, i.e. inside era 1) and the promoted build
later but certainly (f14 at `L = 7`, era 3; f8 at `L = 9`, the end of era 1), with a clock spread that
grows to `dt = 5`.  Note the ordering of the two: the **pair rules slow the churn down** (the
promoted build keeps `K = W - 8`, `D = 8` for about two eras where the baseline loses it in the first
cascade) without restoring the freeze.

**Claim-by-claim impact list** (what has to be re-scoped, reworded or re-measured if the four macros
become the default):

1. Sect. 8.4, reference-build paragraph: "the polarization pair is identically zero and no axis is
   ever elected", "not a single layer is displaced in the seven eras of a control run" -- both become
   false by construction in a promoted build (they are reference-only statements and would need that
   qualifier);
2. the same paragraph's "does not change again within the era": true in the promoted build at
   `L = 7` for era 1 (the plateau holds f2-f13) but **false at `L = 9`** (off the plateau at f8, the
   last frame of era 1);
3. the frozen-state appendix item (`K = W - 8`, `D = 8` ... "freezes at `K = 235`, `D = 8`, `S = 0`
   ... maximum group population 2"): the `K`, `D`, `S` values are promotion-invariant, the word
   *freezes* and the population quantum are not -- and the quantum is not printed by this harness (it
   comes from the archived census tool cited there, which is not in this repository);
4. the `L = 15` item (`K = 667`, `D = 8`, "no candidate macro involved"): stays true as stated, but
   it would remain a statement about the *reference* configuration; re-measuring it with the macros
   on is out of reach here (`L = 15` is 675 layers of 3375 cells, about 110x the `L = 7` cell count,
   and the `L = 9` control already costs roughly a minute per frame);
5. the balance/partition argument ("the partition enters only through the seed address map ... and
   through the two candidate macros, which is why their outcome is a declared multiplicity rather
   than a derived quantum"): promotion moves the partition into the default rule, so the sentence's
   framing -- declared option versus rule -- changes even though its content does not;
6. this README's own "reference plateau" numbers (first-era section) are invariant, as shown above.

**Mechanics, implemented and verified.**  The Makefile carries the promotion as a switch:

```
nmake                  # reference build: no macro, the configuration the paper quotes
nmake CANDIDATES=1     # promoted build: the four macros on CFLAGS
nmake REFERENCE=1      # the same reference build, named explicitly (obj_reference\)
```

All three write the same output names (`build\automaton.exe`, `build\first_era_trace.exe`), because
promoting *is* replacing the default; object files go to separate trees (`obj\`, `obj_candidates\`,
`obj_reference\`) so a configuration can never silently reuse the other's objects (nmake does not
track flag changes).  The explicit `REFERENCE=1` is the escape hatch for the day the default is
flipped: the reference stays buildable *by name* instead of by the absence of a flag, and the two
switches are mutually exclusive (`nmake CANDIDATES=1 REFERENCE=1` stops at `U1050` instead of
silently choosing one).  Verified end to end: `nmake CANDIDATES=1 trace-first-era` builds, and its
frame-2 trace reads `flight=147`, `0/0/0/147` (the candidate signature) with the census still at
`K = 139`, `D = 8`; `nmake trace-first-era` restores the reference binary (`occupiedCenters=1`, no
displacement at frame 2), and so does `nmake REFERENCE=1 trace-first-era`.

Not implemented, deliberately: inverting the guards in the sources so that the candidate behaviour
is the default and the reference numbers need a flag.  That has a much larger blast radius (every log
label, the paper's reference-build statements, and -- importantly -- the behaviour of the simulator
the paper points readers to at its published address).

**One operational fact for the decision.**  These mechanisms are **compile-time only**: their names
appear nowhere outside the `#ifdef`s of the model sources -- not in `automaton.cfg`, not in the GUI
code.  Promoting them is therefore a build decision; if the intent is instead "let a user switch the
transport on", that is a separate and larger change (a configuration key feeding the guards, plus the
paper's reproducibility statement about the reference build).

### The dispersion alone, measured (the `disp` variant)

The measurement above is of the four macros together, and of the pair (a)+(b).  The decision "promote
`CHARGE_DISPERSION_FSM` on its own" needs one more configuration, because the rule's gate is `m == 0`
and `m == 0` is exactly the fixed point that `POLAR_SEED_FROM_PLACEMENT` closes: alone, the macro is
never switched off by anything.  `experiments\trace_build_variants.bat` now carries a seventh variant,
`disp` (`/D S2B_TRACE /D CHARGE_DISPERSION_FSM`, no polar seed), and `build\disp_L7.out` is
`L = 7`, `S = 16384`, 24 frames = 4 eras (about four and a half minutes; `build\frozen_b1_L7.out` is an
earlier probe of the same configuration, 9 frames, and agrees frame for frame).

| frame | steps | move-align / move-mask | class offsets | ledger (K, D, S, P, dt) | m != 0 · pol != 0 |
|---|---|---|---|---|---|
| 2 | 147 | 147/147, one writer (dispersion) | ±1 | 139, 8, 0, 0, 1 | 0/147 · 0 |
| 8 | 147 | 147/147, one writer | ±2 | 139, 8, 0, 0, 1 | 0/147 · 0 |
| 14 | 147 | 147/147, one writer | ±3 | 139, 8, 0, 0, 1 | 0/147 · 0 |
| 20 | 147 | 147/147, one writer, but all 147 read **off side** | **∓3 (the centres wrapped)** | 139, 8, 0, 0, 1 | 0/147 · 0 |
| the other 20 frames | 0 | -- | unchanged | 139, 8, 0, 0, 1 | 0/147 · 0 |

Folded by the repository's own tool (`experiments\era_summary.ps1 -Log build\disp_L7.out -Era 6`),
which is the form the other tables in this file use:

| era | end frame | Orbis y | Umbra y | max &#124;CoM&#124; | cascade frame: flight moved / aligned | era sums: flight moved/aligned, cohesion moved/aligned |
|---|---|---|---|---|---|---|
| 1 | 6 | -1.000 | +1.000 | 0.029 | f2: 147 / 147 | 147/147, 0/0 |
| 2 | 12 | -2.000 | +2.000 | 0.060 | f8: 147 / 147 | 147/147, 0/0 |
| 3 | 18 | -3.000 | +3.000 | 0.089 | f14: 147 / 147 | 147/147, 0/0 |
| 4 | 24 | **+3.000** | **-3.000** | 0.089 | f20: 147 / 147 | 147/147, 0/0 |

The `cohesion 0/0` column is the second face of the same fact: with `m == 0` the pair rules never
book a step, so the dispersion is not merely the largest mover, it is the **only** one -- and the
separation is the exact arithmetic series 1, 2, 3, 4 ... rather than a settling distance.

**It fires once per era, for ever.**  `m != 0` is 0/147 and `pol != 0` is 0 cells in all 24 frames: no
axis is ever elected, nothing ever stops the rule, the latch re-arms at every era edge (`r == 0` then
`r == 1`) and the dispersion fires again at f8, f14 and f20.  It is the only mover (one writer,
writer-mask 16) and the encounter stays completely inert -- `cB = kB = homB = reemit = 0` in every
frame, as in the reference.

**Everything else the paper quotes survives, exactly.**  From frame 2 on (frame 1 is the seed,
`K = D = P = 0` with `S = W`) the ledger holds at `K = 139`, `D = 8`, `S = 0`, `P = 0`, `dt = 1`, and
`dev` and the shell counts (147, 3822, 9702, 23226, ...) are frame-for-frame the reference values;
the harness still finds the same 6-frame period.
So this configuration is "the reference, plus eight separated blocks" -- which is precisely why it is
the cheapest possible way to have movement, and precisely why it cannot stand as the default.

**What does not survive is any bound on the geometry.**  The offsets grow by exactly one cell per era
(1, 2, 3), so what the rule produces is a *rotation* of the eight blocks around the torus, not a
dispersion that settles; and at f20 (era 4) the layer centres cross the far face of the region and
wrap.  Two readings of that frame: the class offsets invert to ∓3 (Orbis reads +3.000, Umbra -3.000 --
the sector ordering the paper quotes is reversed), and the harness's own side test flips from 147 "on
its octant side" to 147 "off side" although every step is still 147/147 along the layer's own octant.
The wrap is a boundary artefact, not dynamics: at era 3 the offsets are already ±3 on a 7-cell torus,
i.e. the two extreme classes sit one cell apart *across* the boundary, so the separation has closed on
itself by then.  The useful window of the mechanism at `L = 7` is therefore about `RMAX - 1` eras, and
it has no stopping condition of its own.

**The same rule at `L = 9`** (`build\disp_L9.out`, `L = 9`, `S = 16384`, 12 frames; here `RMAX = 4`, so
an era is 8 frames and the run covers era 1 plus the era-2 firing): f2 books 243 steps, 243/243
aligned, ledger `K = 235`, `D = 8`; f10 books 243 again, 243/243 aligned, the class offsets at ±2, the
ledger still `235, 8, 0, 0, 1`, `m != 0` 0/243 and `pol != 0` 0 cells.  So the cadence is the era
(`2 RMAX`), not a frame number particular to `L = 7`, and it is a control at a second size rather than a
second window: the wrap is an `L/2` effect, so at `L = 9` the offsets must reach 5 (`L/2 = 4.5`) before
the centres cross the face -- era 5, around frame 34, beyond the frames read here.

**So the two halves are not separable at the level of a decision.**  With the polar seed (`base`) the
same rule fires once, at frame 2, and the phase closes at frame 4; that is the only configuration in
which the dispersion is a bounded, self-terminating step, and it is the one the acceptance table at
the top of this file measures.  Adopting the macro alone buys movement with no dynamics -- no election,
no cascade, no transport -- and with a geometry that leaves the seed-centred cavity within three eras.
The invariant list in `FSM.txt` (section 10) now carries both readings, since "the dispersion fires
once, at frame 2" is a statement about the DISP + POLAR_SEED build.

**My read, for what it is worth.**  The promotion costs six re-scoped statements and one
re-measurement that this repository cannot perform (the population quantum); it buys a default
simulator that is not frozen -- i.e. the mechanism the paper spends Sect. 8.4 on would be visible
without rebuilding.  The honest middle path, if the freeze is wanted for reference purposes, is the
switch above: `nmake` for the text's numbers, `nmake CANDIDATES=1` for the dynamics.

## Item 3 of the follow-up list: twelve eras of the candidate build

`build\long2_L7.out` -- the `(a)+(b)` build (`/D CHARGE_DISPERSION_FSM /D PAIR_SAME_OCTANT
/D PAIR_OWN_AXIS_EXCHANGE /D POLAR_SEED_FROM_PLACEMENT /D S2B_TRACE`, flags only, no source edit),
`L = 7`, `S = 16384`, 72 frames = 12 eras, ten to twelve minutes (the run shared the CPU with the
control for part of it).  The control `build\long2ref_L7.out`
is the same harness with **no** macro defined.  Both binaries come from `experiments\trace_build_variants.bat`
(one `cl` line per variant, seven variants: `ref`, `disp` -- the dispersion with **no** polar
seed, see "The dispersion alone, measured" below -- `polar`, `base`, `aonly`, `own`, `bothab`), and the
table below is folded out of the log by `experiments\era_summary.ps1 -Log build\long2_L7.out -Era 6`
(one era = `2 RMAX` = 6 light frames at `L = 7`; the split is read at the end of each era, the counters
are summed over the era).

**The series is the paper's series, continued.**  All 27 frames the earlier session read
(`build\long_L7.log`, the same variant) are reproduced **value for value** -- every `positions` line
identical -- so eras 1-4 are exactly the numbers Sect. 8.4 quotes ($-1.480$/$+1.583$,
$-1.480$/$+1.403$, $-0.867$/$+1.014$, $-0.920$/$+0.500$) and the extension begins where those stop.

| era | end frame | Orbis y | Umbra y | gap (Umbra - Orbis) | max &#124;CoM&#124; | cascade frame: flight moved / aligned | era sums: flight moved/aligned, cohesion moved/aligned |
|---|---|---|---|---|---|---|---|
| 1 | 6 | -1.480 | +1.583 | **+3.063** | 0.067 | f2: 147 / 147 | 179/179, 14/7 |
| 2 | 12 | -1.480 | +1.403 | **+2.883** | 0.135 | f11: 6 / 6 | 12/9, 32/0 |
| 3 | 18 | -0.867 | +1.014 | **+1.881** | 0.441 | f14: 48 / 41 | 78/60, 36/6 |
| 4 | 24 | -0.920 | +0.500 | **+1.420** | 0.441 | f24: 24 / 9 | 59/24, 38/5 |
| 5 | 30 | -0.400 | -0.028 | +0.372 | 0.367 | f30: 64 / 39 | 90/48, 30/3 |
| 6 | 36 | -0.173 | +0.236 | +0.409 | 0.403 | f35: 45 / 18 | 79/35, 44/3 |
| 7 | 42 | -0.467 | +0.139 | +0.606 | 0.268 | f38: 33 / 15 | 43/19, 34/1 |
| 8 | 48 | -0.333 | +0.069 | +0.402 | 0.265 | f44: 23 / 10 | 40/21, 18/0 |
| 9 | 54 | -0.347 | +0.056 | +0.403 | 0.365 | f54: 32 / 18 | 44/18, 32/1 |
| 10 | 60 | -0.427 | +0.319 | +0.746 | 0.519 | f59: 16 / 3 | 24/7, 52/1 |
| 11 | 66 | +0.000 | -0.194 | **-0.194** | 0.519 | f65: 63 / 23 | 109/38, 58/2 |
| 12 | 72 | -0.053 | +0.167 | +0.220 | 0.198 | f72: 25 / 8 | 54/17, 46/3 |

What the twelve eras say:

* **the magnitude floors, it does not decay to zero.**  The gap falls from 3.06 cells to 1.42 by
  era 4, and then stops falling: eras 5-12 stay between 0.2 and 0.75 (and briefly negative), i.e. the
  fabric settles into a residual ordering of a few tenths of a cell rather than mixing away;
* **the ordering inverts once, transiently.**  `gap <= 0` occurs in exactly four frames of the 72:
  frame 1 (the bare seed, before any dispersal) and frames 65-67 (era 11), where the Orbis `y` mean
  sits at exactly 0.000 and the Umbra mean at -0.194.  Era 12 has it back (-0.053 / +0.167).  So the
  sign of the split is not a permanent invariant -- it is a *long-lived* one, repaired by the next
  era's cascade, and by era 11 the two means are within a fifth of a cell of each other anyway;
* **the Umbra mean crosses the lattice centre and returns** (era 5: -0.400 / -0.028).  "The split
  holds" therefore means *Orbis stays below Umbra*, not *each sector stays on its original side*:
  after era 5 both sector means are negative and the ordering is what survives;
* **the flight channel is aligned at the cascades, not everywhere.**  The 100 % alignment is
  reproduced exactly where it was claimed (147/147 at the dispersal, frame 2; 32/32 at the era-1
  cascade, frame 6; 6/6 at frame 11), but from era 3 on the aligned share of the flight steps drops
  (41 of 48 at frame 14, 39 of 64 at frame 30, 23 of 63 at frame 65).  Those later eras are the
  **inherited-impulse** regime documented above (`S2B_DUMP_PENDING`), not a second transport
  mechanism, and the claim "every flight displacement is octant-aligned" is a statement about the
  cascade;
* **the centre of mass is not stationary**: its offset (norm) wanders up to 0.52 cells (era 10) while
  the census stays at the reference plateau in every frame;
* **the reference build has no split at all.**  The control run reports `m != 0 = 0/147`,
  `pol != 0 = 0`, `moved-this-frame = 0` and Orbis/Umbra means of exactly 0.000 in each of the seven
  eras it was read for.  Every number in this table is a candidate-build number.

**Attribution correction, found while building the control.**  The "reference transport" row of the
(a)/(b) tables (94 displacements, `0/15/79/0`) is **not** the reference build: with no macro defined
the cascade displaces *nothing*.  That row is the **transport-enabled baseline** (dispersal + polar
seed), and the `base` variant reproduces it exactly -- `occupiedCenters = 29`, `moved = 94`,
`0/15/79/0`, `m != 0 = 147/147`, `pol != 0 = 13 671` (`build\probe_b4.out`).  The same build also
reproduces the older 30-frame multi-era log **frame for frame** (`build\base_L7.out` vs
`build\multi_L7.log`: identical `positions` lines for all 30 frames the rebuild ran, era 1
-1.853/+1.931 through era 5 +0.067/+0.194; the old log is 36 frames long), which is what identified
`multi_L7.log` as a baseline run rather than a pair-rule run.  Two further rows are reproduced by flag-only builds: `own` (101 moved,
`0/22/40/39`, identical to `own_L7.log`; the split line of the current source adds flight 87 +
cohesion 14) and `bothab` (46 moved, `0/7/0/39`, the run tabulated above).

**One row is *not* reproducible from the current source, and that is itself a result.**  The
"(a) alone | 46 | 0/7/39/0" row of the (a)/(b) table is the intermediate variant *same-octant contact
handler without the exchange guard*, which is what `oct2`/`oct4` measured.  In the current source
`PAIR_SAME_OCTANT` also guards the momentum exchange, so the `aonly` variant freezes the transport
instead: `build\probe_aonly.out` (dispersal + polar seed + `PAIR_SAME_OCTANT`), frame 6, measures
`moved = 0`, `flight = 0`, `cohesion = 0` -- i.e. exactly the `oct5` row of the (a) table ("+ the
momentum-exchange guard" gives `moved = 0`).  The macro as it stands therefore does not isolate the
46-step behaviour, and the only way to re-obtain that row is to revert the exchange guard inside the
macro.  The labels in the (a)/(b) tables have been corrected to say what they are, and the variant
names above are the ones `experiments\trace_build_variants.bat` builds.

The baseline read over five eras for comparison (`build\base_L7.out`, 30 frames, same harness):

| era | end frame | Orbis y | Umbra y | gap | flight moved / aligned (era sums) |
|---|---|---|---|---|---|
| 1 | 6 | -1.853 | +1.931 | +3.784 | 241 / 147 |
| 2 | 12 | -0.293 | +0.639 | +0.932 | 142 / 0 |
| 3 | 18 | -0.213 | -0.208 | +0.005 | 273 / 20 |
| 4 | 24 | +0.187 | +0.097 | **-0.090** | 442 / 26 |
| 5 | 30 | +0.067 | +0.194 | +0.127 | 462 / 35 |

So the two variants differ in kind, not only in size: without (a)+(b) the gap walks to zero by era 3
and then oscillates around zero (inverted at era 4, back at era 5) with almost none of the flight
steps aligned (26 of 442 at era 4), while with (a)+(b) it floors at 0.2-0.75 of a cell and every
cascade frame is fully aligned.  The baseline also runs hotter: 442 and 462 flight steps per era
against 59 and 90.

**The same comparison at `L = 9`** (era = `2 RMAX` = 8 frames; `build\long9_L9.out` = candidate, 16
frames, `build\long9base_L9.out` = baseline, 8 frames -- a frame costs a minute once the delegates
fill, so the control was read for one era only):

| build | frame 2 (dispersal) | end of era 1 (frame 8) | gap | end of era 2 (frame 16) | gap | era-1 flight moved / aligned |
|---|---|---|---|---|---|---|
| candidate (a)+(b) | 243 / **243** aligned | -0.553 / +0.825 | **+1.378** | -0.374 / +0.608 | +0.982 | 285 / 251 |
| baseline | 243 / **243** aligned | **+0.122 / -0.417** | **-0.539** | not read | - | 528 / 261 |

Two things follow.  The dispersal is identical in both builds and scales with the lattice (243
layers, all of them on their own octant, Orbis at -1.000 and Umbra at +1.000).  But by the end of the
first era the baseline has **already inverted the ordering** at this size (Orbis above Umbra), where
at `L = 7` it needed until era 4 to do so, and most of its era-1 displacements are off-octant (`528`
moved, `261` aligned; `185` of them at the frame-8 cascade, `101` matching no sign at all).  The
candidate keeps Orbis below Umbra through both eras read (gap 2.00 -> 1.38 -> 0.98), so the decay is
faster at the larger lattice as well, but the ordering is still there when the baseline has lost it.
The failure mode is therefore not a small-lattice artefact: it sets in *earlier* as the lattice
grows, and the pair rules are what hold the ordering in both sizes.

Metric note: the `max |CoM|` column here is the **norm** of the centre-of-mass offset; the older
tables' column held the largest single *component* (frame 6: 0.067 as a norm against 0.061 as a
component; frame 12: 0.135 against 0.109).  The two conventions differ; the values above are the
norm.

## (a) Same-octant pairing: halves the cascade, does not yet align it

The first design of the corrected step, implemented as `/D PAIR_SAME_OCTANT`: every mover of the
encounter's contact handler -- the adiabatic drift, the K x K repulsion, the S x K absorption step,
the S x S repulsion and the S x D pair step (`interaction.cpp`, the handler around lines
2317-2419) -- plus the per-tick mediated kick (`applyRecruitKick`) is applied **only when the two
partners share the colour triplet** (`(chA ^ chB) & 7 == 0`), i.e. only when both layers sit in the
same charge octant.  The charge word is a property of the layer, so no address, hash, scan order or
RNG enters.

Measured at `L = 7`, era 1 (`build\oct2_L7.log` handler-only, `build\oct4_L7.log` with the kick
guard added as well, `build\oct5_L7.log` with the momentum exchange guarded too; baseline
`build\mv2_plc_L7.log`):

| variant | frame 6 `occupiedCenters` | moved | `move-align` 0/1/2/3 |
|---|---|---|---|
| transport-enabled baseline (dispersal + polar seed) | 29 | 94 | 0/15/79/0 |
| same-octant, contact handler | 32 | **46** | 0/7/39/0 |
| + the kick guard | 32 | **46** | 0/7/39/0 (identical: the kick is **not called** in this run) |
| + the momentum-exchange guard | **8** | **0** | - (no displacement at all, frames 5-9) |

**The real mover was the momentum exchange, and it is the whole transport.** Guarding the pair-walk
steps alone left 46 displacements, still none of them octant-aligned; the moment the exchange
`currDraft.reloc += partnerSrc.m` (the D x D cross-tribe branch) is restricted to same-octant
partners, **the transport stops**: `moved = 0` in every frame from 5 on.  So the exchange is where
the reference displacement comes from, and it transfers the *partner's* `m` -- parallel to the
partner's octant, not to the layer's own, which is exactly the measured signature (two of three signs
matching, since a ladder pair's two classes usually differ in one colour bit).

What the frozen run looks like (`oct5`, frames 2-9, all identical): the dispersal's geometry is
**preserved exactly** -- `occupiedCenters = 8`, Orbis y = -1.000, Umbra y = +1.000, CoM at the
population imbalance (-0.020,-0.020,-0.007) -- the census stays at the reference plateau (`K = 139`,
`D = 8`) and the clock never splits (`dt = 1` over all 50421 cells).  In other words: **forbidding the
cross-octant exchange conserves the charge order perfectly and costs the entire transport** (no
same-octant cross-tribe D x D contacts occur in this configuration, which is consistent with the
ladder arithmetic: a same-octant partner needs an offset that is a multiple of `8 * ISLAND_SIZE`).

An earlier note in this section blamed a second handler at `interaction.cpp:1960-2022`; that was
wrong -- those sites sit inside `#ifdef EM_FIRST_FSM`, a candidate macro no build defines, so they are
not compiled.  The site list of the `reloc` writers (481/541/565, 1361, 1998, 2422) is what locates a
mover, not the `moveOneStep` call list.

**The design the data now points to.** With (a) the fabric is either mixing (reference) or frozen;
what is missing is a displacement that is *both* charge-aligned and non-zero.  The natural candidate
is to make the exchange transfer the layer's **own** charge axis -- add its own `m` (or the shared
octant when the octants agree) instead of the partner's -- i.e. a common-mode drift along the charge,
which keeps the transport alive and keeps every step on the layer's axis.  The acceptance test is
unchanged: the cascades' `move-align` should show the dispersal's signature (all three signs) with
`moved > 0`.

## The axis was already charge-correlated: where the mixing really comes from

Correlating the elected axis with the charge was the step the item-2 diagnosis asked for, so it was
implemented as a candidate macro: `/D POLAR_AXIS_FROM_CHARGE` restricts the `elect()` tournament to
cells whose offset from the lattice centre lies in the layer's own octant (`c2 -> x`, `c1 -> y`,
`c0 -> z`), and installs the charge octant itself when no candidate exists (the tournament, the
broadcast and the reconstruction are untouched; no address, hash, scan order or RNG enters).  The
macro changes **nothing**: with it on, the L=7 trajectory is byte-identical to the
`POLAR_SEED_FROM_PLACEMENT` run (same `occ`, `moved`, positions and CoM at frames 10, 11, 12).

The reason is measured, and it falsifies the diagnosis that motivated the macro:

* **`axis-align`** (new harness line: how many of the three signs of a layer's `m` agree with the
  octant of its own charge word): from the first election onwards, **147/147 layers match all three
  signs** -- `m` is exactly parallel to the charge octant of every layer, in every frame, with the
  axis macro off.  (`m == 0` for all 147 before the first election, frames 1-3.)  The placement rule
  already gets this right: the dispersal puts each layer's centre on its octant ray and
  `installAxis(w, centre - CENTER)` normalises that ray, so the axis *is* the octant;
* **`move-align`** (how many of the three signs of a layer's actual displacement agree with its
  octant): the dispersal is perfectly aligned -- frame 2, `moved = 147`, **147/147 with all three
  signs** -- and **not one** of the transport's displacements is:

  | frame | what | moved | sign-match 0/1/2/3 |
  |---|---|---|---|
  | 2 | charge dispersal | 147 | 0/0/0/**147** |
  | 6 | era-1 encounter cascade | 94 | 0/15/**79**/0 |
  | 9-10 | era 2 | 14 | 7/7/0/0 |
  | 11 | era-2 cascade | 96 | **80**/7/9/0 |
  | 12 | era 2 | 46 | 13/9/**24**/0 |

So the ordering is not lost through the momentum -- `m` is charge-aligned throughout -- but through
the **displacement channel**: the encounter's pair walk, whose partner is found on the `W`-address
ladder (`w + k` rotation over the Encounter window) and whose two bubbles walk toward each other
(which is why the net impulse per layer measured zero before the queue was introduced).  By era 2 the
displacements are *anti*-aligned as often as aligned (frame 11: 80 of 96 with no matching sign).

**The corrected next step**: charge-correlate the *pair selection or the walk direction* of the
encounter, not the elected axis.  The acceptance test is the `move-align` line: the transport's
cascades should show the dispersal's signature (all three signs) instead of 0/1/2, and only then
should the sector split survive era 2.  Two candidate designs suggest themselves: let the ladder pair
partners of the same charge octant (so pairs walk toward each other along the charge), or bias the
walk step by the charge octant the way the dispersal does.

## Step 3: the magnitude of the charge step

The octant fixes the *direction* of a layer's dispersal but not how far it steps, so every copy of a
class moved as one block and all eight classes moved one cell. Two candidate magnitudes, both read
from the seed's own structure and neither from the `W` address (`interaction.cpp`,
`chargeStepMagnitude()`; `/D CHARGE_STEP_MAGNITUDE` and `/D CHARGE_STEP_SUBBLOCK`, both OFF by
default):

* **class magnitude** -- `mag = 2` for `sig` in `{0,3}`, `1` for `sig` in `{1,2}`, where
  `sig = c2+c1+c0`. The colour complement `k <-> 7-k` sends `sig -> 3-sig`, so the antipodal
  partners keep equal magnitude and their steps stay exactly opposite: the rule does not move the
  centre of mass. The eight classes then separate at two speeds;
* **sub-block** -- `mag +=` bit 3 of the family index (`island`). This is the *only* non-address way
  to split a class, and the bit is invariant under `island <-> (island ^ 7)` -- the pairing that
  flips the low three bits, i.e. turns a word into its colour complement -- so the antipodal partners
  still get equal magnitude. (The pairing is exact only while `island ^ 7` is inside the family
  count; beyond it one family of a pair is absent and the imbalance shows up in the centre of mass.)

**The impossibility result that motivated the split.** The seed's family map makes the six-bit word a
function of `island mod 8` (`w1` = bit 1, `w0` = bit 0, `q` their xor, `c2c1c0` the low three bits),
so **the charge word carries no information that tells the copies of one class apart**. Any
charge-derived step is therefore identical for all of them, and intra-class diversity cannot come
from the charge at all: it has to come from the family index (what the sub-block uses) or from the
internal state (`pol_u,pol_v`/`pB`/`sB`, which is what the momentum election is for). That is a
design fact about the map in Sect. 4, worth stating wherever the charge word is presented as the
source of diversity.

Measured (`L = 7`, sieve closed, `CHARGE_DISPERSION_FSM` + `POLAR_SEED_FROM_PLACEMENT`; the
per-class line of the harness gives the displacement spectrum; logs `build\mag_L7.log`,
`build\sub_L7.log`):

| variant | frame-2 per-class displacement | `occupiedCenters` f2 | f6 | moved f6 | reemit f6 | CoM at f2 |
|---|---|---|---|---|---|---|
| unit step (baseline) | all eight at distance 1 | 8 | 29 (-> 69 by era 2, ~120 by era 6) | 94 | 139 | (-0.020,-0.020,-0.007) |
| class magnitude | `k0,k7` at 2; `k1..k6` at 1 | 8 | 19 | 51 | 84 | (-0.034,-0.034,-0.020) |
| + sub-block | `k0` 2.50, `k7` 2.44, `k1..k6` 1.44-1.47 | **16** | 23 | 23 | 23 | (-0.054,-0.054,-0.027) |

* the **spectrum is exactly as designed**: magnitude 2 for the two `sig in {0,3}` classes and 1 for
  the other six, at both lattice sizes (at `L = 5` the same numbers appear, wrapped by the tiny
  torus: `RMAX = 2`, so a 3-cell step crosses the cavity);
* the **sub-block splits each class into two shells** -- 16 cells instead of 8, i.e. exactly two per
  class -- confirming the `island <-> island ^ 7` invariance;
* the **centre of mass is unmoved by the rule**: at frame 2 it is at the population-imbalance level
  (5/147 for the class magnitude, 8/147 with the sub-block, against 3/147 for the unit step). The
  growth is the imbalance of the paired classes being weighted by the magnitude, not a broken
  antisymmetry. From frame 6 on, the values also contain the transport's own effect;
* **the downstream cascade is strongly reduced**: the era-1 encounter cascade drops from 139
  re-emissions (94 layers moved) to 84 (51) and then 23 (23), and both variants have gone quiet by
  frame 8 (`resets = 0`, `reemit = 0`, `moved = 0`), where the unit-step run is still churning
  (about 32,000 clock resets per frame by era 6). Whether "quiet" means *structure preserved* or
  merely *fewer contacts* is not settled by 8 frames.

The reference build is unchanged (both magnitude macros off: `occupiedCenters = 1`, `moved = 0`,
`K = 139`, `D = 8`, shell 3822/9702 exact -- re-verified after this change).

## First-era measurements (headless trace)

`experiments/first_era_trace.cpp`, built by `nmake trace-first-era` and run as
`build\first_era_trace.exe <L> <s2b_target> <frames>` from the repository root, prints one line
per light frame of the first era: shell radius and cell count, the census (`K`, `D`, `S`, `P`
halves), the number of distinct charge words, the structurally pairable addresses, the clock spread
and the in-loop `s2B` counters (logs in `build\*.log`).  What it measured:

**Multi-era and candidate runs.**  The harness is the same source in every candidate measurement on
this page; only the macros change, and no source is edited.  Four scripts make that reproducible:

* `experiments\trace_build_variants.bat` builds, from a developer prompt, the seven variants used here
  -- `ref` (counters only, the reference transport channel), `disp` (the dispersion with **no** polar
  seed, the "fires once per era" build of the decision pack), `polar` (`POLAR_SEED_FROM_PLACEMENT`),
  `base` (dispersal + polar seed, the 94-displacement baseline), `aonly`, `own`, `bothab` -- as
  `build\trace_<name>.exe`, one `cl` line each (the flags travel in a variable, never through `call`
  arguments, which would strip the quotes of `/D "..."`);
* `experiments\era_summary.ps1 -Log <log> [-Era 6]` folds any trace log into the per-era table used
  below (era = `2 RMAX` light frames; sector means read at the end of the era, flight/cohesion
  counters summed over it, with the octant-aligned share).  Run it with
  `powershell -NoProfile -ExecutionPolicy Bypass -File experiments\era_summary.ps1 ...` -- the
  default execution policy refuses script files, and the cultures differ (`-` vs `,` as decimal
  separator), so the script pins the invariant culture itself.

* `experiments\check_docs.ps1` (`nmake check-docs`) is the lint over the claims themselves: the cited
  paths, checked against this tree, the README's "lives in" table and `experiments\check_docs.allow`
  (a cited path that is neither present nor declared fails); the FSM.txt registry against the
  `#ifdef`/`defined()` of the sources, both directions (a registry macro with no `#ifdef` and a
  Makefile `/D` the sources never test both fail); and the machine-checkable rows of FSM.txt section 10
  recomputed from the trace logs in `build\`, where a log that is not built is a note and never a
  failure.  Each rule quotes the sentence it verifies, so re-wording a claim in the documents fails the
  lint instead of passing silently.  It is the in-tree successor of the retired claims lint
  (`nmake check-claims` still answers, as an alias).  Its first run is what tightened the frame-1
  wording of "The dispersion alone, measured";

* `experiments\check_trace.bat` (`nmake check-trace`) is the regression over the dynamics: it builds
  four configurations -- the reference and the three of the promotion decision -- runs each at `L = 7`
  with the sieve closed for one era (`6` frames, ending on the era-1 cascade frame) and compares the
  log, line by line, with the expectation committed in `experiments\golden\`.  That scenario is what
  separates the four (`ref` moves nothing, `disp` the 147-step dispersal at f2, `base` the 94-step
  cascade at f6, `bothab` 46), and the comparison can be exact because the model is deterministic.
  An intended difference is accepted with `nmake refresh-trace-golden`, and the report quotes the light
  frame of the first difference, so a failure says where the dynamics moved.
* the shell advances **one cell per light frame** from radius 0 to RMAX and back, and the
  per-layer shell counts are the exact lattice counts 1, 26, 66, 158, 234 for radii 0..4, so one
  era is `2 RMAX` frames (6 at `L = 7`, 8 at `L = 9`, i.e. `L - 1` on the odd lattices the seed
  admits);
* **the whole census is settled in the frame 1 -> 2 transition** and is constant afterwards: 235 `K`
  + 8 `D` with the sieve at its reference value, 145 `K` + 5 `D` + 93 pair halves with the gate
  open, at `L = 9` (both reproduce the numbers the manuscript quotes).  Frame 1 is the bare seed:
  `K = D = P = 0`, `W` singletons, one clock value for the whole lattice;
* the seed realizes **8 of the 64 charge words** (`000000, 011001, 101010, 110011, 000100, 011101,
  101110, 110111`), and only three of them admit a pair rule inside that roster (`R3` on `000000`,
  `R6` on `101010` and `101110`): the pairable address count is 56 at `L = 7` and 93 at `L = 9`;
* with the gate open the encounter reissues the seed's bubbles **in that one frame** (1,456
  re-emissions at `L = 7`), each resetting a clock to 0, and the `flood` operator then pulls 36,288
  cells to the minimum of their six neighbors -- 27,720 of them to 0.  The phase field splits into
  exactly two values and the shell is eroded (2,422 cells at radius 1 instead of 3,822).  With the
  gate closed no re-emission and no pull occurs (`dt = 1` throughout) and the era is an exact
  Poincare cycle;
* the open-gate shell reaches a **limit cycle** of period `2 RMAX` that never revisits the seed
  (2,422 / 7,462 / 18,074 / 14,854 / 6,062 / 1,547 at `L = 7`); at `L = 5` the whole recorded state
  closes with period 4, while at `L = 7` the `s2B` bookkeeping does not close with period 6, nor
  with any period up to 15 over frames 31-60 of a 60-frame run.

Two text-to-code discrepancies found while tracing this, both open:

* the **dispersion** of Sect. 5.4 ("at `t = RMAX/2`") is implemented only inside
  `#ifdef HOMB_PRODUCER_FSM`, a candidate macro that no build defines, so the model's own counter
  `homb_events` stays 0 and the timed dispersion does not fire in the reference build.  Where the
  condition does appear it pins the shell to radius `floor(RMAX/2)`, at least 1 for every
  admissible `L` (1 at `L = 5, 7`; 2 at `L = 9`);
* the text gives the sieve update as `s2B' = s2B and active`, while `simulation.cpp` assigns
  `d.s2B = active and trigger`: the bit is recomputed each frame from `u` and `t`, not carried
  forward.

The in-loop counters sit behind `#ifdef S2B_TRACE` (`simulation.cpp`, `interaction.cpp`,
`simulation.h`); the default build is unaffected (verified: every model translation unit compiles
without the macro).

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
(`renderCavity()`, 850 points) -- is `Cavity`; it used to be labeled `Lattice`, which described
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
center -- `initCenters`), so a translated region is the same experiment on the
same torus.  Only the size matters, and the size is what the memory bill depends
on.

Two consequences the widget honours, because the model requires them:

* **odd edges** -- the seed sits on the center cell and the periodic wrap has to
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
overlay switches to **centered cube**, where the whole selection is one number -- the
side `S` -- and the box stays centered on the lattice (`c-k .. c+k`, `c = (L-1)/2`).
`Up`/`Down` change `S` by two cells (`Shift` ten), `Left`/`Right` have no face to pick,
`Home` returns to the whole lattice (which is itself a centered cube) and `C` goes back to
free faces; dragging any face in cube mode scales the cube symmetrically.  Entering cube
mode collapses the box to the centered cube of its *smallest* extent, so switching never
grows the region.  The splash keeps a row of centered-cube presets (`cube 11`, `cube 15`,
`cube 21`) under the L/W ones: one click sets the side, no overlay needed.

Because the model re-centers the seed on the lattice it is given, the cube is a
**convenience**, not a different run: a centered `11^3` and the `11 x 21 x 21`-style box
with the same extents produce the same `calculateParameters` line, the same schedule and
the same allocation (checked in a run: `region 11 x 11 x 11 (from x 5..15 ...)` gave
`EL=11, RMAX=5, FRAME=216` and 13,310 cells, exactly as the non-centered `x 0..10` case
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
| `GUI_3D.cpp:516` (`renderAxes`) | its own static pair, `if (!axisVao)`, because the layout is pos+color |
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
gap where the dash was.  They are ASCII now.  Worth remembering when labeling a widget: anything above
0x7F disappears without a word.

**Lattice tickbox (20 Sep 2026):** the 3-D view's tickbox list grew a tenth entry, `Lattice`, which
draws the box the simulation lives in -- the same way the region overlay does it: the twelve edges
plus the grid of the only three faces turned towards the camera, so the box reads as a lattice
instead of its interior becoming a thicket of lines (`renderLattice()`, GUI_3D.cpp; cell size
`0.5 / EL` and the lattice centered on the origin, exactly like `renderGrid()` and `enhanceVoxel()`;
the per-axis edges `ELX/ELY/ELZ` are used, so a run on an anisotropic region shows the box it has).

Its state is `Config::data3DLattice`, **not** `data3D[9]`: `Config::data3D` is a fixed nine-entry
array mirrored positionally by the tickboxes, and widening it would move every member declared after
it in a header that every translation unit includes.  So the new flag was appended at the end of the
struct, and the two tickboxes above it (`Cavity`, the Fibonacci sphere, and `Lattice`, the box) are
distinct switches with distinct keys -- before this pass `data3D.lattice` meant the cavity;
`data3D.cavity` is the cavity's key now, and `data3D.lattice` is the box's.

Verified by dumping the app's own frame: labeled `Lattice` in the list, `1,888` edge-coloured and
`4,727` grid-coloured pixels inside the 3-D area when on, `0` and `44` when off, and the ten tickboxes
fit the column (`110 + 9 x 25 = 335`, above the "Delays" section at 390).

**Region-overlay pass (20 Sep 2026):** the setup screen was regrouped into three cards (parameters,
summary, and one card that keeps `Start Paused` together with the three mode buttons, so nothing is
left outside a box and the tickbox is no longer inside the summary card).  Two latent projection bugs
turned up and were fixed: `TextRenderer::RenderText`'s 5-argument overload projected onto stale
800x600 members instead of the live viewport (the dropdown values landed ~760 px above their boxes
once the window grew), and `drawTriangleFan2D` ignored its color argument because `uColorLoc` was
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
every run); `obj\` and `build\` are build outputs and can be deleted at any time.  Since commit
`161fe54`, `.gitignore` keeps all of them out of git (`obj/`, `build/`, `*.log`), so a rebuild no
longer shows up as a dirty tree; the files themselves stay on disk.  The history was then rewritten to
drop the build outputs it had recorded before that commit, which is why hashes quoted in older notes
-- this line's former `77e9a0e` among them -- no longer exist.  `E:\alpha` keeps the unreduced copy of
the paper and is unaffected either way.

Note on the merge: the merge that created this tree left conflict markers in this file
(`<<<<<<< HEAD`, `=======`, `>>>>>>> cc53489`); the two sides are merged into the single text above
(the title from `HEAD`, the one-line description from the other side), and the duplication that merge
left in the build-outputs paragraph was cleaned when the history was rewritten to drop the binaries.

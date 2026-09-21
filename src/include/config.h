#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config
{
    // =========================
    // data3D
    // =========================
    bool data3D[9] = {0};

    // =========================
    // delays
    // =========================
    struct {
        bool convol  = false;
        bool diffuse = false;
        bool reloc   = false;
    } delays;

    // =========================
    // VIEW (actual camera state)
    // =========================
struct {
    float rot_x = 0.0f;
    float rot_y = 0.0f;
    float cam_dist = 3.0f;
    float zoom = 45.0f;
    float ortho_scale = 0.55f;   // orthographic projection scale
    int vis_dx = 0;
    int vis_dy = 0;
    int vis_dz = 0;
} view;

    // =========================
    // INPUT (separado da view)
    // =========================
    struct {
        float camera_speed      = 1.0f;
        float mouse_sensitivity = 0.1f;
    } input;

    // =========================
    // PROJECTION
    // =========================
    struct {
        float fov         = 45.0f;
        float near_plane  = 0.1f;
        float far_plane   = 1000.0f;
        bool  perspective = true;
    } projection;

    // =========================
    // SIMULATION
    // =========================
    struct {
        int scenario = -1;  // -1 = use splash selection

        // Fatia 2 — M/Mbar turnaround hook (fsm.md §10), default OFF.
        double   mm_eps   = 0.0;  // C-violating bias; 0 disables the hook
        double   mm_pbase = 1.0;  // base conjugation probability per turnaround
        unsigned mm_seed  = 1;    // xorshift32 seed (deterministic runs)

        // Lattice selection shown by the splash screen.  Persisted so the setup
        // screen opens on the run chosen last time (keys simulation.lattice /
        // simulation.layers).  Appended AFTER the existing members on purpose:
        // the note at the end of this struct explains why appending cannot
        // shift the offsets that already-compiled objects read.
        int lattice = 21;   // L, lattice side: odd, 5..89
        int layers  = 10;   // W, winding layers: >= 2

        // Region of the lattice the last run used (the setup screen's 3-D
        // overlay).  Inclusive cell bounds in the L x L x L grading; the model
        // runs the region's extents, so these bounds are what the next start
        // reopens with, and what the memory bill depends on.
        //
        // A file without these keys means "the whole lattice": loadConfig()
        // resolves them against simulation.lattice once the whole file has been
        // read, because the full-lattice bounds depend on the side.
        int subX0 = 0, subX1 = 20;
        int subY0 = 0, subY1 = 20;
        int subZ0 = 0, subZ1 = 20;
    } simulation;

    // =========================
    // TOMOGRAPHY
    // =========================
    struct {
        bool enabled = false;

        int axis = 2;           // 0=X,1=Y,2=Z
        float slice = 0.5f;     // normalizado

        bool invert = false;
        bool animate = false;

        float thickness = 0.01f;
    } tomography;

    // =========================
    // GUI-only display flags
    //
    // Kept at the END of the struct on purpose: this header is included by
    // nearly every translation unit, and the Makefile objects can lag behind a
    // header-only change.  Appending here cannot shift the offsets of the
    // members above, so a stale object still reads the same layout for all of
    // them (inserting in the middle silently corrupts them).
    // =========================
    bool data3DVisited = false;   // "Visited" toggle of the 3-D view

    // "Lattice" toggle of the 3-D view: draws the lattice box (the twelve edges
    // plus the grid of the faces turned towards the camera, see renderLattice()).
    //
    // It is its own flag rather than a tenth slot of data3D[9] on purpose: this
    // struct is mirrored by the tickboxes positionally, and widening the array
    // would move every member after it -- a stale object would then read the wrong
    // offsets.  Appended here, at the end, nothing moves (see the note above).
    bool data3DLattice = false;
};

// global
extern Config gConfig;

// Path that the last successful loadConfig() actually read ("" if none was
// read).  saveConfig() writes to this path, so the values chosen in the splash
// screen go back into the file the program is really using (running from
// build\ updates build\automaton.cfg, not a new file next to the exe).
extern std::string gConfigPath;

// loader
bool loadConfig(const std::string& path);

// writer: updates the managed keys (simulation.scenario / simulation.lattice /
// simulation.layers / simulation.subX0..subZ1) in place, preserving the comments
// and the layout of the rest of the file; keys the file does not have are
// appended at the end.  Returns false when the path cannot be read or written.
bool saveConfig(const std::string& path);

#endif
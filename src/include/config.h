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
};

// global
extern Config gConfig;

// loader
bool loadConfig(const std::string& path);

#endif
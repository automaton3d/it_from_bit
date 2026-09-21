// config.cpp

#include "config.h"
#include "model/simulation.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <vector>

Config gConfig;

// Set by loadConfig(); used by the splash screen as the write target of
// saveConfig() so the persisted selection lands in the file that was read.
std::string gConfigPath;

using namespace automaton;

static bool parseBool(const std::string& v)
{
    return (v == "1" || v == "true" || v == "TRUE");
}

static void trim(std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");

    if (start == std::string::npos)
    {
        s.clear();
        return;
    }

    s = s.substr(start, end - start + 1);
}

static void stripComment(std::string& s)
{
    size_t pos = s.find('#');
    if (pos != std::string::npos)
    {
        s = s.substr(0, pos);
        trim(s);
    }
}

// The splash screen offers odd sides only (5, 7, ..., 89).  Snap to that grid
// instead of silently running with a value the setup screen cannot display.
static int clampLattice(int L)
{
    if (L < 5)  L = 5;
    if (L > 89) L = 89;
    if ((L & 1) == 0) ++L;   // nearest odd above; 89 is already odd
    return L;
}

static int clampLayers(int W)
{
    if (W < 2)    W = 2;
    if (W > 4096) W = 4096;
    return W;
}

// Region bounds: inside the lattice, at least one cell per axis, and every extent
// odd (the model needs odd edges -- see automaton::configureLatticeFromRegion).
// Values that only stick out are clamped; an even extent snaps to the closest odd
// one, the same way an unrepresentable L snaps to the offered grid.
struct Region
{
    int x0, x1, y0, y1, z0, z1;
};

static Region clampRegion(int x0, int x1, int y0, int y1, int z0, int z1, int L)
{
    const int last = (L > 0) ? L - 1 : 0;
    Region r{ x0, x1, y0, y1, z0, z1 };

    int* lo[3] = { &r.x0, &r.y0, &r.z0 };
    int* hi[3] = { &r.x1, &r.y1, &r.z1 };

    for (int a = 0; a < 3; ++a)
    {
        int a0 = *lo[a];
        int a1 = *hi[a];

        if (a0 > a1) std::swap(a0, a1);      // inverted: read it the right way round
        if (a1 < 0 || a0 > last) { a0 = 0; a1 = last; }   // entirely outside

        if (a0 < 0)    a0 = 0;
        if (a1 > last) a1 = last;

        // extent = a1 - a0 + 1, so an EVEN difference means an odd extent, which
        // is what the model wants; an odd difference shrinks by one, inward.
        if (((a1 - a0) & 1) != 0 && a1 > a0) ++a0;
        if (a0 > a1) a0 = a1;                             // one-cell region

        *lo[a] = a0;
        *hi[a] = a1;
    }

    return r;
}

bool loadConfig(const std::string& path)
{
    std::ifstream file(path);

    // Fallback: try parent directory (useful when running from build/)
    std::string actualPath = path;
    if (!file)
    {
        std::string fallback = "../" + path;
        file.open(fallback);
        if (file)
            actualPath = fallback;
    }

    if (!file)
    {
        std::cout << "[Config] File not found: " << path
                  << " (also tried ../" << path << ")" << std::endl;
        return false;
    }

    std::cout << "[Config] Loading: " << actualPath << std::endl;
    gConfigPath = actualPath;

    // Reset to defaults before loading
    gConfig = Config{};

    std::string line;

    // Bit per axis: the whole-lattice default can only be resolved once the file
    // has been read, because it depends on simulation.lattice.
    unsigned regionKeys = 0u;

    while (std::getline(file, line))
    {
        trim(line);

        if (line.empty())
            continue;

        if (line[0] == '#' || line[0] == '/')
            continue;

        // Parse key=value
        size_t pos = line.find('=');

        if (pos == std::string::npos)
            continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        trim(key);
        stripComment(value);
        trim(value);

        if (key.empty() || value.empty())
            continue;

        // =========================
        // data3D
        // =========================
        if (key == "data3D.wavefront")
            gConfig.data3D[0] = parseBool(value);

        else if (key == "data3D.momentum")
            gConfig.data3D[1] = parseBool(value);

        else if (key == "data3D.spin")
            gConfig.data3D[2] = parseBool(value);

        else if (key == "data3D.sine_mask")
            gConfig.data3D[3] = parseBool(value);

        else if (key == "data3D.polarization" || key == "data3D.hunting")
            gConfig.data3D[4] = parseBool(value);

        else if (key == "data3D.centers")
            gConfig.data3D[5] = parseBool(value);

        // The tickbox is called "Cavity" (it draws the bubble's spherical cavity:
        // the Fibonacci sphere inscribed in the unit cube).  `data3D.lattice` is
        // the old name of the same switch and is still read, so a configuration
        // written before the rename keeps working.
        else if (key == "data3D.cavity" || key == "data3D.lattice")
            gConfig.data3D[6] = parseBool(value);

        else if (key == "data3D.axes")
            gConfig.data3D[7] = parseBool(value);

        else if (key == "data3D.visited")
            gConfig.data3DVisited = parseBool(value);

        else if (key == "data3D.plane")
            gConfig.data3D[8] = parseBool(value);

        // =========================
        // delays
        // =========================
        else if (key == "delay.convol")
        {
            gConfig.delays.convol = parseBool(value);
            convol_delay = gConfig.delays.convol;
        }

        else if (key == "delay.diffuse")
        {
            gConfig.delays.diffuse = parseBool(value);
            diffuse_delay = gConfig.delays.diffuse;
        }

        else if (key == "delay.reloc")
        {
            gConfig.delays.reloc = parseBool(value);
            reloc_delay = gConfig.delays.reloc;
        }

        // =========================
        // view
        // =========================
        else if (key == "view.zoom")
        {
            gConfig.view.zoom = std::stof(value);
        }

        else if (key == "view.vis_dx")
        {
            gConfig.view.vis_dx = std::stoi(value);
        }

        else if (key == "view.vis_dy")
        {
            gConfig.view.vis_dy = std::stoi(value);
        }

        else if (key == "view.vis_dz")
        {
            gConfig.view.vis_dz = std::stoi(value);
        }

        // =========================
        // projection
        // =========================
        else if (key == "projection.fov")
        {
            gConfig.projection.fov = std::stof(value);
        }

        else if (key == "projection.near")
        {
            gConfig.projection.near_plane = std::stof(value);
        }

        else if (key == "projection.far")
        {
            gConfig.projection.far_plane = std::stof(value);
        }

        else if (key == "projection.perspective")
        {
            gConfig.projection.perspective = parseBool(value);
        }

        // =========================
        // simulation
        // =========================
        else if (key == "simulation.scenario" || key == "scenario")
        {
            gConfig.simulation.scenario = std::stoi(value);
        }

        else if (key == "simulation.mm_eps")
        {
            gConfig.simulation.mm_eps = std::stod(value);
        }

        else if (key == "simulation.mm_pbase")
        {
            gConfig.simulation.mm_pbase = std::stod(value);
        }

        else if (key == "simulation.mm_seed")
        {
            gConfig.simulation.mm_seed = (unsigned)std::stoul(value);
        }

        // Lattice of the last run started from the splash screen.
        else if (key == "simulation.lattice" || key == "lattice")
        {
            gConfig.simulation.lattice = clampLattice(std::stoi(value));
        }

        else if (key == "simulation.layers" || key == "layers")
        {
            gConfig.simulation.layers = clampLayers(std::stoi(value));
        }

        // Region of the lattice edited in the setup screen's 3-D overlay.  The
        // six keys are read one by one; the whole-lattice default (which depends
        // on the side) is resolved after the whole file has been read.
        else if (key == "simulation.subX0") { gConfig.simulation.subX0 = std::stoi(value); regionKeys |= 1u; }
        else if (key == "simulation.subX1") { gConfig.simulation.subX1 = std::stoi(value); regionKeys |= 1u; }
        else if (key == "simulation.subY0") { gConfig.simulation.subY0 = std::stoi(value); regionKeys |= 2u; }
        else if (key == "simulation.subY1") { gConfig.simulation.subY1 = std::stoi(value); regionKeys |= 2u; }
        else if (key == "simulation.subZ0") { gConfig.simulation.subZ0 = std::stoi(value); regionKeys |= 4u; }
        else if (key == "simulation.subZ1") { gConfig.simulation.subZ1 = std::stoi(value); regionKeys |= 4u; }

        // =========================
        // tomography
        // =========================
        else if (key == "tomography.enabled")
        {
            gConfig.tomography.enabled = parseBool(value);
        }

        else if (key == "tomography.axis")
        {
            gConfig.tomography.axis = std::stoi(value);
        }

        else if (key == "tomography.slice")
        {
            gConfig.tomography.slice = std::stof(value);
        }

        else if (key == "tomography.invert")
        {
            gConfig.tomography.invert = parseBool(value);
        }

        else if (key == "tomography.animate")
        {
            gConfig.tomography.animate = parseBool(value);
        }

        else if (key == "tomography.thickness")
        {
            gConfig.tomography.thickness = std::stof(value);
        }
        else if (key == "view.ortho_scale")
        {
            gConfig.view.ortho_scale = std::stof(value);
        }
    }

    // Region bounds.  A file that does not carry (all six of) the region keys
    // means the whole lattice, which is the side that was just read.
    if (regionKeys != 7u)
    {
        gConfig.simulation.subX0 = gConfig.simulation.subY0 = gConfig.simulation.subZ0 = 0;
        gConfig.simulation.subX1 = gConfig.simulation.subY1 = gConfig.simulation.subZ1 =
            gConfig.simulation.lattice - 1;
    }

    const Region raw{
        gConfig.simulation.subX0, gConfig.simulation.subX1,
        gConfig.simulation.subY0, gConfig.simulation.subY1,
        gConfig.simulation.subZ0, gConfig.simulation.subZ1 };

    const Region reg = clampRegion(raw.x0, raw.x1, raw.y0, raw.y1, raw.z0, raw.z1,
                                   gConfig.simulation.lattice);

    gConfig.simulation.subX0 = reg.x0; gConfig.simulation.subX1 = reg.x1;
    gConfig.simulation.subY0 = reg.y0; gConfig.simulation.subY1 = reg.y1;
    gConfig.simulation.subZ0 = reg.z0; gConfig.simulation.subZ1 = reg.z1;

    if (reg.x0 != raw.x0 || reg.x1 != raw.x1 ||
        reg.y0 != raw.y0 || reg.y1 != raw.y1 ||
        reg.z0 != raw.z0 || reg.z1 != raw.z1)
    {
        std::cout << "[Config] region x " << raw.x0 << ".." << raw.x1
                  << " y " << raw.y0 << ".." << raw.y1
                  << " z " << raw.z0 << ".." << raw.z1
                  << " is not a lattice region (inside 0.." << (gConfig.simulation.lattice - 1)
                  << ", odd extents); using x " << reg.x0 << ".." << reg.x1
                  << " y " << reg.y0 << ".." << reg.y1
                  << " z " << reg.z0 << ".." << reg.z1 << std::endl;
    }

    std::cout << "[Config] scenario = "
              << gConfig.simulation.scenario
              << ", lattice L = " << gConfig.simulation.lattice
              << ", layers W = " << gConfig.simulation.layers
              << ", region x " << reg.x0 << ".." << reg.x1
              << " y " << reg.y0 << ".." << reg.y1
              << " z " << reg.z0 << ".." << reg.z1
              << std::endl;

    return true;
}

bool saveConfig(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "[Config] Cannot write (unreadable): " << path << std::endl;
        return false;
    }

    // The keys the setup screen owns.  Everything else in the file is left
    // byte-for-byte as it was, comments included.
    struct ManagedKey
    {
        const char* key;
        std::string value;
    };

    const int subX0 = gConfig.simulation.subX0, subX1 = gConfig.simulation.subX1;
    const int subY0 = gConfig.simulation.subY0, subY1 = gConfig.simulation.subY1;
    const int subZ0 = gConfig.simulation.subZ0, subZ1 = gConfig.simulation.subZ1;

    std::vector<ManagedKey> managed = {
        { "simulation.scenario", std::to_string(gConfig.simulation.scenario) },
        { "simulation.lattice",  std::to_string(gConfig.simulation.lattice)  },
        { "simulation.layers",   std::to_string(gConfig.simulation.layers)   },

        // Region of the lattice (the setup screen's 3-D overlay), one key per
        // face.  Keeping them one per line makes the file readable and lets the
        // in-place rewrite above work unchanged.
        { "simulation.subX0",    std::to_string(subX0) },
        { "simulation.subX1",    std::to_string(subX1) },
        { "simulation.subY0",    std::to_string(subY0) },
        { "simulation.subY1",    std::to_string(subY1) },
        { "simulation.subZ0",    std::to_string(subZ0) },
        { "simulation.subZ1",    std::to_string(subZ1) }
    };

    std::vector<bool>        replaced(managed.size(), false);
    std::vector<std::string> lines;

    std::string line;
    while (std::getline(in, line))
    {
        std::string probe = line;
        trim(probe);

        if (!probe.empty() && probe[0] != '#' && probe[0] != '/')
        {
            size_t pos = probe.find('=');
            if (pos != std::string::npos)
            {
                std::string key = probe.substr(0, pos);
                trim(key);

                for (size_t i = 0; i < managed.size(); ++i)
                {
                    if (!replaced[i] && key == managed[i].key)
                    {
                        line = std::string(managed[i].key) + " = " + managed[i].value;
                        replaced[i] = true;
                        break;
                    }
                }
            }
        }

        lines.push_back(line);
    }
    in.close();

    // Keys the file did not carry yet are appended at the end.
    for (size_t i = 0; i < managed.size(); ++i)
    {
        if (!replaced[i])
            lines.push_back(std::string(managed[i].key) + " = " + managed[i].value);
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        std::cerr << "[Config] Cannot write: " << path << std::endl;
        return false;
    }

    for (const std::string& l : lines)
        out << l << "\n";

    std::cout << "[Config] Saved: " << path
              << " (scenario=" << gConfig.simulation.scenario
              << ", lattice=" << gConfig.simulation.lattice
              << ", layers="  << gConfig.simulation.layers
              << ", region x " << subX0 << ".." << subX1
              << " y " << subY0 << ".." << subY1
              << " z " << subZ0 << ".." << subZ1
              << ")" << std::endl;

    return true;
}
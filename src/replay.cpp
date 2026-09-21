/*
 * replay.cpp
 */

#include "replay.h"
#include "recorder.h"
#include "globals.h"
#include "replay_progress.h"
#include "GUI.h"
#include "tinyfiledialogs.h"

namespace framework
{
  extern FrameRecorder recorder;
  extern bool savePopup;
  extern bool loadPopup;
  extern Tickbox *scenarioHelpToggle;
  extern size_t replayIndex;
  extern ReplayProgressBar *replayProgress;


  /**
   * Opens a Save File dialog and returns the chosen filename
   * Returns empty string if user cancels
   */
  std::string getSaveFileName()
  {
	    const char* filters[] = { "*.dat" };

	    const char* filename = tinyfd_saveFileDialog(
	        "Save Replay As",                // Title
	        "replay.dat",                    // Default filename
	        1,                               // Number of filters
	        filters,                         // Filters
	        "Replay Files (*.dat)"           // Description
	    );

	    if (filename) {
	        return std::string(filename);
	    }
	    return "";
	}

	/**
	 * Opens an Open File dialog and returns the chosen filename
	 * Returns empty string if user cancels
	 */
	std::string getOpenFileName()
	{
	    const char* filters[] = { "*.dat" };

	    const char* filename = tinyfd_openFileDialog(
	        "Open Replay",                   // Title
	        "",                              // Default path/filename
	        1,                               // Number of filters
	        filters,                         // Filters
	        "Replay Files (*.dat)",          // Description
	        0                                // Allow multiple selection? (0 = no)
	    );

	    if (filename) {
	        return std::string(filename);
	    }
	    return "";
	}

	/**
	 * Saves the current replay to file
	 */
	void saveReplay()
	{
	    if (currentMode == SIMULATION && !recordFrames && !replayFrames && !paused)
	    {
	        std::string filename = getSaveFileName();
	        if (!filename.empty())
	        {
	            try {
	                recorder.saveToFile(filename);
	                savePopup = true;

	                std::cout << "[Replay] saved " << recorder.getFrameCount()
	                          << " frames (" << recorder.getMemoryUsageKB()
	                          << " KB) to " << filename << std::endl;
	            }
	            catch (const std::exception& e) {
	                toastMessage = "Failed to save replay: " + std::string(e.what());
	                toastStartTime = glfwGetTime();
	                toastActive = true;
	            }
	        }
	    }
	    else
	    {
	        // Say why nothing happened instead of ignoring the click: the guard is
	        // real (a snapshot has to be taken from a settled simulation) but silent.
	        std::cerr << "[Replay] save needs a paused-free SIMULATION with recording and "
	                     "playback off (mode=" << (int)currentMode
	                  << ", recording=" << (recordFrames ? 1 : 0)
	                  << ", replaying=" << (replayFrames ? 1 : 0)
	                  << ", paused=" << (paused ? 1 : 0) << ")" << std::endl;
	    }
	}

	/**
	 * Loads a replay from file
	 */
	void loadReplay()
	{
	    if (currentMode == REPLAY)
	    {
	        if (!recordFrames && !replayFrames && !paused)
	        {
	            std::string filename = getOpenFileName();
	            if (!filename.empty())
	                loadReplayFrom(filename);
	        }
	        else
	        {
	            std::cerr << "[Replay] load needs a paused-free REPLAY with the recorder idle "
	                         "(recording=" << (recordFrames ? 1 : 0)
	                      << ", replaying=" << (replayFrames ? 1 : 0)
	                      << ", paused=" << (paused ? 1 : 0) << ")" << std::endl;
	        }

	        // Ensure scenario help is disabled when loading replay
	        scenarioHelpToggle->setState(false);
	    }
	    else
	    {
	        std::cerr << "[Replay] load is only available in replay mode" << std::endl;
	    }
	}

	// The loading itself, without the file dialog.  Kept apart from loadReplay()
	// so a path can be loaded by anything (a test, a future command line) without
	// going through a modal dialog.
	void loadReplayFrom(const std::string& filename)
	{
	    if (filename.empty())
	    {
	        std::cerr << "[Replay] load: no file given" << std::endl;
	        return;
	    }

	    try {
	        recorder.loadFromFile(filename);
	        timer = recorder.savedTimer;
	        gConfig.simulation.scenario = recorder.savedScenario;
	        loadPopup = true;

	        // Start playing what was loaded.  The head has to be rewound (loading
	        // replaces the frames, and a previous playthrough left the index at the
	        // end), and replayFrames is the switch the simulation thread reads
	        // before calling updateReplay().
	        replayIndex = 0;
	        replayFrames = true;

	        if (replayProgress)
	            replayProgress->update(0, recorder.frames.size());

	        std::cout << "[Replay] playing " << recorder.frames.size()
	                  << " frames from " << filename << std::endl;
	    }
	    catch (const std::exception& e) {
	        toastMessage = "Failed to load replay: " + std::string(e.what());
	        toastStartTime = glfwGetTime();
	        toastActive = true;

	        std::cerr << "[Replay] load failed: " << e.what() << std::endl;
	    }
	}

	// File > New: drop the recorded frames and rewind the play head.  Nothing else
	// owned the recorder, so it grew until the memory cap and could never be
	// emptied on purpose.
	void newReplay()
	{
	    recorder.clearFrames();
	    replayIndex = 0;
	    replayFrames = false;

	    if (replayProgress)
	        replayProgress->update(0, 0);

	    std::cout << "[Replay] recorder cleared" << std::endl;
	}

	  bool updateReplay()
	  {
	      if (recorder.frames.empty())
	          return false;

	      if (replayIndex < recorder.frames.size())
	      {
	          const framework::Frame& currentFrame = recorder.frames[replayIndex];

	          // Reconstruct voxels and lattice
	          recorder.reconstructVoxels(currentFrame, voxels, framework::layerList->getSelected());
	          recorder.applyFrame(currentFrame, automaton::lattice_curr);

	          // Update lcenters
	          for (const auto& layer : currentFrame.layers)
	          {
	              if (layer.w < automaton::W_USED)
	              {
	                  automaton::lcenters[layer.w][0] = layer.center_x;
	                  automaton::lcenters[layer.w][1] = layer.center_y;
	                  automaton::lcenters[layer.w][2] = layer.center_z;
	              }
	          }

	          // Update replay progress bar
	          if (framework::replayProgress)
	          	framework::replayProgress->update(replayIndex + 1, recorder.frames.size());


	          ++replayIndex;
	          timer += automaton::FRAME;
	      }

	      // Past the last frame the play loop stops: core.cpp clears replayFrames
	      // when this returns false.  It used to return true unconditionally, so a
	      // finished replay kept the loop spinning (a timer tick and a sleep, four
	      // times a second) instead of ending.
	      return replayIndex < recorder.frames.size();
	  }


}

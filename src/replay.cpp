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
	            }
	            catch (const std::exception& e) {
	                toastMessage = "Failed to save replay: " + std::string(e.what());
	                toastStartTime = glfwGetTime();
	                toastActive = true;
	            }
	        }
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
	            {
	                try {
	                    recorder.loadFromFile(filename);
	                    timer = recorder.savedTimer;
	                    gConfig.simulation.scenario = recorder.savedScenario;
	                    loadPopup = true;
	                }
	                catch (const std::exception& e) {
	                    toastMessage = "Failed to load replay: " + std::string(e.what());
	                    toastStartTime = glfwGetTime();
	                    toastActive = true;
	                }
	            }
	        }
	        // Ensure scenario help is disabled when loading replay
	        scenarioHelpToggle->setState(false);
	    }
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

	      return true;
	  }


}

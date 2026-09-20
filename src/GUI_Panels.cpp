/*
 * GUI_Panels.cpp
 */

#include "GUI.h"
#include "globals.h"
#include "tickbox.h"
#include "radio.h"
#include <vector>


namespace framework
{
	extern TextRenderer hudText;   // defined in hud.cpp
	extern Tickbox* scenarioHelpToggle;

	void renderScenarioHelpToggle()
	{
	    if (scenarioHelpToggle) {
	        scenarioHelpToggle->setFontScale(0.6f);
	        scenarioHelpToggle->draw(hudText);
	    }
	}

    // -----------------------------------------------------------------
    // 3D data tickboxes
    // -----------------------------------------------------------------
    void render3Dboxes()
    {
        for (Tickbox& checkbox : data3D)
        {
            checkbox.setFontScale(0.6f);
            checkbox.draw(hudText);
        }
    }

    // -----------------------------------------------------------------
    // Delay tickboxes
    // -----------------------------------------------------------------
    void renderDelays()
    {
        for (Tickbox& checkbox : delays)
        {
            checkbox.setFontScale(0.6f);
            checkbox.draw(hudText);
        }
    }

    // -----------------------------------------------------------------
    // Viewpoint radios
    // -----------------------------------------------------------------
    void renderViewpointRadios()
    {
        for (Radio& radio : views)
        {
            radio.setFontScale(0.6f);
            radio.draw(hudText);                  // modern Radio::draw()
        }
    }

    // -----------------------------------------------------------------
    // Projection radios (Ortho / Perspective)
    // -----------------------------------------------------------------
    void renderProjectionRadios()
    {
        for (Radio& radio : projectRads)
        {
            radio.setFontScale(0.6f);
            radio.draw(hudText);
        }
    }

    // -----------------------------------------------------------------
    // Tomography enable tickbox
    // -----------------------------------------------------------------
    void renderTomoControls()
    {
        if (tomoEnable) {
            tomoEnable->setFontScale(0.6f);
            tomoEnable->draw(hudText);
        }
    }

    // -----------------------------------------------------------------
    // "Visited" toggle: bottom-right corner of the 3-D scene, just below
    // the right-hand panel (which ends at screenH - 110).
    // Coordinates are top-left origin, like every other tickbox.
    // -----------------------------------------------------------------
    void renderSineVisitedToggle(int screenW, int screenH)
    {
        if (!sineVisitedToggle)
            return;

        sineVisitedToggle->setPosition(screenW - 250, screenH - 95);
        sineVisitedToggle->setFontScale(0.6f);
        sineVisitedToggle->draw(hudText);
    }

    // -----------------------------------------------------------------
    // Tomography direction radios (only when tomography is enabled)
    // -----------------------------------------------------------------
    void renderTomoRadios()
    {
        if (tomoEnable && tomoEnable->getState())
        {
            for (Radio& radio : tomoDirs)
            {
                radio.setFontScale(0.6f);
                radio.draw(hudText);
            }
        }
    }

} // namespace framework

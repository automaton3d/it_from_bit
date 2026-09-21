/*
 * help.h
 *
 *  Created on: 29 de nov. de 2025
 *      Author: Alexandre
 */

#ifndef INCLUDE_HELP_H_
#define INCLUDE_HELP_H_


#pragma once
#include <vector>
#include <string>

namespace framework
{
    // Declarations for global variables
    extern const std::vector<std::string> ui_help;
    extern const std::vector<std::string> record_help; // <-- The key declaration
    extern std::vector<std::string> scenarioHelpTexts;

    // Opens the project's help page (the README of the repository) in the
    // default browser.  Single home of the URL: the splash `Help` link and the
    // HUD hyperlink both go through here.
    void openHelpPage();
}


#endif /* INCLUDE_HELP_H_ */

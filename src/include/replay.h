/*
 * replay.h
 *
 *  Created on: 29 de nov. de 2025
 *      Author: Alexandre
 */

#ifndef INCLUDE_REPLAY_H_
#define INCLUDE_REPLAY_H_

#include <string>

namespace framework
{

	std::string getSaveFileName();
	std::string getOpenFileName();
	void saveReplay();
	void loadReplay();
	// The loading itself, without the modal file dialog (see replay.cpp).
	void loadReplayFrom(const std::string& filename);
	bool updateReplay();

	// Discards the recorded frames and rewinds the play head (File > New).  The
	// recorder had no way to be emptied, and the play head was never rewound.
	void newReplay();
}

#endif /* INCLUDE_REPLAY_H_ */

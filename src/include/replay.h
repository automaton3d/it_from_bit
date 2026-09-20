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
	bool updateReplay();

}

#endif /* INCLUDE_REPLAY_H_ */

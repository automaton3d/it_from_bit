/*
 * sound.cpp (stub implementation)
 *
 * Provides stub implementations for sound functions.
 * To enable sound, install OpenAL and libsndfile, then define HAS_OPENAL.
 */

#include <iostream>
#include <string>

namespace framework {

  // Stub implementation - no sound
  void playSound(const std::string& filename, bool loop) {
    // Do nothing - sound disabled
    // To enable sound support:
    // 1. Install OpenAL: https://www.openal.org/
    // 2. Install libsndfile: http://www.mega-nerd.com/libsndfile/
    // 3. Add HAS_OPENAL define to your build system
    // 4. Replace this file with the full OpenAL implementation
    
    (void)filename;  // Suppress unused parameter warning
    (void)loop;      // Suppress unused parameter warning
  }

  void sound(bool loop) {
    // Do nothing - sound disabled
    (void)loop;
  }

}
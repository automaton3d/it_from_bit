/*
 * animator.h (adapted)
 *
 * Defines the states of the animation (roll, zoom etc.).
 */

#ifndef ANIMATOR_H
#define ANIMATOR_H

#include <chrono>

namespace framework
{

    enum AnimationType
    {
      //NONE,
      FIRST_PERSON,
      ORBIT,
      //PAN,
      //ROLL,
      //ZOOM
    };

    inline void animate() {}
    inline double elapsedSeconds() { return 0.0; }
    inline void firstperson() {}
    inline void orbit() {}
    inline void pan() {}
    inline void reset() {}
    inline void roll() {}
    inline void setAnimation(AnimationType type) {}
    inline void setScreenSize(int w, int h) {}
//    void setInteractor(TrackBallInteractor *i);
    inline void stopwatch() {}
    inline void zoom() {}

    inline AnimationType mAnimation_;
  //  TrackBallInteractor *mInteractor_;
    inline int mFrame_;
    inline int mFrames_;
    inline float mFramesPerSecond_;
 //   int mHeight_;
    inline std::chrono::time_point<std::chrono::system_clock> mTic_;
   // int mWidth_;

} // namespace framework

#endif // ANIMATOR_H

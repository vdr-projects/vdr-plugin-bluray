/*
 * bdplayer.h: A player for BluRay discs
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _BDPLAYER_H
#define _BDPLAYER_H

#include <vdr/player.h>
#include <vdr/tools.h>

class cBDPlayer;

class cBDControl : public cControl {
private:
  static int active;
  cBDPlayer *player;
  cString disc_name;

  cBDControl();
  cBDControl(cBDPlayer *Player);

  void Play();
  void Pause();
  void SkipSeconds(int seconds);
  void Goto(int seconds);

  cSkinDisplayReplay *displayReplay;
  bool visible, modeOnly, shown;
  int lastCurrent, lastTotal;
  bool lastPlay, lastForward;
  int lastSpeed;
  time_t timeoutShow;
  bool timeSearchActive, timeSearchHide;
  int timeSearchTime, timeSearchPos;

  void TimeSearchDisplay(void);
  void TimeSearchProcess(eKeys Key);
  void TimeSearch(void);
  void ShowTimed(int Seconds = 0);
  void ShowMode(void);
  bool ShowProgress(bool Initial);

public:
  static cControl *Create(const char *Path);
  static bool Active(void) { return active > 0; }

  virtual ~cBDControl();

  bool Visible(void) { return visible; }

  virtual void Show(void);
  virtual void Hide(void);

  virtual cString GetHeader(void);
  virtual eOSState ProcessKey(eKeys Key);
};

#endif //_BDPLAYER_H

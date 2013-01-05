/*
 * titlemenu.h: BluRay title menu
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _TITLEMENU_H
#define _TITLEMENU_H

#include <vdr/menuitems.h>

class cBDControl;

class cTitleMenu : public cOsdMenu {
 private:
  cBDControl *ctrl;

 public:
  cTitleMenu(cBDControl *Ctrl);

  virtual eOSState ProcessKey(eKeys Key);
};

#endif //_TITLEMENU_H

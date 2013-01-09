/*
 * titlemenu.h: BluRay disc library menu
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _DISCMENU_H
#define _DISCMENU_H

#include <vdr/menuitems.h>

#include "discmgr.h"

class cDiscMenu : public cOsdMenu {
 private:
  void Scan(cString& Root);

  cDiscMgr& mgr;

 public:
  cDiscMenu(cDiscMgr& Mgr, cString& Root);

  virtual eOSState ProcessKey(eKeys Key);
};

#endif //_DISCMENU_H

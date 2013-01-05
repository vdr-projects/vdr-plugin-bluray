/*
 * titlemenu.c:
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <libbluray/bluray.h>

#include <vdr/tools.h>
#include <vdr/osdbase.h>

#include "bdplayer.h"

#include "titlemenu.h"

/*
 * cTitleItem
 */

class cTitleItem : public cOsdItem
{
 private:
  int playlist;
  int length;

 public:
  cTitleItem(unsigned Index, unsigned Playlist, unsigned Seconds);

  int GetPlaylist() { return playlist; }

  virtual int Compare(const cListObject &ListObject) const {
    const cTitleItem *o = (const cTitleItem *)&ListObject;
    return o->length - length;
  }
};

cTitleItem::cTitleItem(unsigned Index, unsigned Playlist, unsigned Seconds) :
    cOsdItem(cString::sprintf("Title %d (%02d:%02d:%02d)",
                              Index,
                              Seconds / 3600, (Seconds / 60) % 60, Seconds % 60),
             osUser1)
{
  playlist = Playlist;
  length = Seconds;
}

/*
 * cTitleMenu
 */

cTitleMenu::cTitleMenu(cBDControl *Ctrl) :
    cOsdMenu("BluRay Titles")
{
  ctrl = Ctrl;

  /* load title list */
  BLURAY *bd = ctrl->BDHandle();
  unsigned num_title_idx = bd_get_titles(bd, TITLES_RELEVANT, 0);
  isyslog("%d titles", num_title_idx);

  for (unsigned i = 0; i < num_title_idx; i++) {
    BLURAY_TITLE_INFO *info = bd_get_title_info(bd, i, 0);
    if (info) {

      Add(new cTitleItem(i+1, info->playlist, info->duration / 90000));

      bd_free_title_info(info);
    }
  }

  Sort();
  Display();
}

eOSState cTitleMenu::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osUser1: {
      cTitleItem *ti = (cTitleItem*)Get(Current());
      if (ti) {
        ctrl->SelectPlaylist(ti->GetPlaylist());
        return osEnd;
      }
      break;
    }
    default:      break;
  }

  return state;
}

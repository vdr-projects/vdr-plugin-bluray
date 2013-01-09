/*
 * discmenu.c: BluRay disc library menu
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/tools.h>
#include <vdr/osdbase.h>

#include "bdplayer.h"

#include "discmenu.h"

/*
 *
 */

static bool IsBluRayFolder(const char *fname)
{
  struct stat st;

  if (stat(cString::sprintf("%s/BDMV/index.bdmv", fname), &st) == 0)
    return true;

  return false;
}

/*
 * cDiscItem
 */

class cDiscItem : public cOsdItem
{
 private:
  cString Root;
 public:
  cDiscItem(const char *title, const char *root);
  cDiscItem(const char *title);

  const char *GetRoot() { return Root; }

  virtual int Compare(const cListObject &ListObject) const {
    const cDiscItem *o = (const cDiscItem *)&ListObject;
    return strcmp(o->Root, Root);
  }
};

cDiscItem::cDiscItem(const char *title, const char *root) :
cOsdItem(title, osUser1)
{
  Root = root;
}

cDiscItem::cDiscItem(const char *title) :
cOsdItem(title, osUser2)
{
}

/*
 * cDiscMenu
 */

cDiscMenu::cDiscMenu(cDiscMgr& Mgr, cString& Root) :
    cOsdMenu("BluRay Discs"),
    mgr(Mgr)
{
  Scan(Root);

  Sort();

  if (mgr.IsMounted()) {
    Ins(new cDiscItem(cString::sprintf("BluRay disc (%s)", mgr.GetDev())));
    //SetHelp("Eject");
  } else {
    Ins(new cDiscItem("(Disc not mounted)"));
    //SetHelp("Mount");
  }

  Display();
}

void cDiscMenu::Scan(cString& Root)
{
  DIR *d = opendir(Root);
  if (d) {
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") &&
          e->d_name[0] != '.') {
        cString buffer = cString::sprintf("%s/%s", *Root, e->d_name);
        struct stat st;
        if (stat(buffer, &st) == 0) {

          // check symlink destination
          if (S_ISLNK(st.st_mode)) {
            buffer = ReadLink(buffer);
            if (!*buffer || stat(buffer, &st))
              continue;
          }

          // folders
          if (S_ISDIR(st.st_mode)) {

            if (IsBluRayFolder(buffer)) {
              Add(new cDiscItem(e->d_name, buffer));

            } else {
              Scan(buffer);
            }
          }
        }
      }
    }
    closedir(d);
  }
}

eOSState cDiscMenu::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);
  switch (state) {
    case osUser1: {
      isyslog("disc select");
      cDiscItem *di = (cDiscItem*)Get(Current());
      if (di) {
        isyslog("- root %s", di->GetRoot());

        cControl::Shutdown();

        cControl *control = cBDControl::Create(di->GetRoot());
        if (control) {
          cControl::Launch(control);
        }

        return osEnd;
      }
      break;
    }

    case osUser2: {
      isyslog("device select");

      if (!mgr.CheckDisc()) {
        return osContinue;
      }

      cControl::Shutdown();

      cControl *control = cBDControl::Create(mgr.GetPath());
      if (control) {
        cControl::Launch(control);
        return osEnd;
      }
      break;
    }

    default:      break;
  }
  return state;
}

/*
 * discmgr.c: BluRay disc / drive manager
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <unistd.h>

#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/skins.h>

#include "discmgr.h"


static bool DeviceOk(const char *DevName)
{
  struct stat ds;

  if (stat(DevName, &ds) == 0) {
    if (S_ISBLK(ds.st_mode)) {
      if (access(DevName, R_OK) == 0)
        return true;
      else
        esyslog("ERROR: can't access device %s", DevName);
    }
    else
      esyslog("ERROR: %s is not a device", DevName);
  }
  else
    LOG_ERROR_STR(DevName);

  return false;
}

static bool PathOk(const char *DirName)
{
  struct stat ds;

  if (stat(DirName, &ds) == 0) {
    if (S_ISDIR(ds.st_mode)) {
      if (access(DirName, R_OK | X_OK) == 0)
        return true;
      else
        esyslog("ERROR: can't access mount point %s", DirName);
    }
    else
      esyslog("ERROR: mount point %s is not a directory", DirName);
  }
  else
    LOG_ERROR_STR(DirName);

  return false;
}

cDiscMgr::cDiscMgr()
{
  Device     = DEFAULT_DEVICE;
  Path       = DEFAULT_PATH;
  MountCmd   = DEFAULT_MOUNTER;
  UnMountCmd = DEFAULT_UNMOUNTER;
  EjectCmd   = DEFAULT_EJECT;
}

bool cDiscMgr::IsMounted()
{
  return PathOk(cString::sprintf("%s/BDMV", *Path));
}

void cDiscMgr::Mount(bool Retry)
{
  cString cmd = cString::sprintf("%s \"%s\" \"%s\"", *MountCmd, *Device, *Path);
  isyslog("executing '%s'", *cmd);
  SystemExec(cmd);

  if (Retry && !IsMounted()) {
    /* retry */

    cCondWait::SleepMs(3000);

    isyslog("executing '%s'", *cmd);
    SystemExec(cmd);
  }
}

void cDiscMgr::UnMount()
{
  cString cmd = cString::sprintf("%s \"%s\"", *UnMountCmd, *Device);
  isyslog("executing '%s'", *cmd);
  SystemExec(cmd);
}

void cDiscMgr::CloseTray()
{
  cString cmd = cString::sprintf("%s -t \"%s\"", *EjectCmd, *Device);
  isyslog("executing '%s'", *cmd);
  SystemExec(cmd);

  cCondWait::SleepMs(3000);
}

void cDiscMgr::Eject()
{
  cString cmd = cString::sprintf("%s \"%s\"", *EjectCmd, *Device);
  isyslog("executing '%s'", *cmd);
  SystemExec(cmd);
}

bool cDiscMgr::CheckDisc()
{
  if (!PathOk(Path)) {
    Skins.Message(mtError, tr("Mount point does not exist!"));
    return false;
  }

  if (!IsMounted()) {

    if (!DeviceOk(Device)) {
      Skins.Message(mtError, tr("Can't access device!"));
      return false;
    }

    Mount(false);

    if (!IsMounted()) {
      Skins.Message(mtWarning, tr("Failed to mount BluRay disc, retry..."));

      CloseTray();
      Mount();

      if (!PathOk(cString::sprintf("%s/BDMV/", *Path))) {
        Skins.Message(mtError, tr("Failed to mount BluRay disc!"));

        return false;
      }
    }
  }

  return true;
}

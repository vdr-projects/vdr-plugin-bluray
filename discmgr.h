/*
 * discmgr.h: BluRay disc / drive manager
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _DISCMGR_H
#define _DISCMGR_H

#include <vdr/tools.h>

#define DEFAULT_DEVICE    "/dev/sr0"
#define DEFAULT_PATH      "/media/cdrom"
#define DEFAULT_MOUNTER   "/bin/mount"
#define DEFAULT_UNMOUNTER "/bin/umount"
#define DEFAULT_EJECT     "/usr/bin/eject"

class cDiscMgr {

private:

  cString Device, Path, MountCmd, UnMountCmd, EjectCmd;

  bool IsMounted(void);
  void Mount(bool Retry = true);
  void UnMount(void);
  void CloseTray(void);

 public:
  cDiscMgr();

  const char *GetPath(void)         { return Path; }

  void SetDevice(const char *D)     { Device = D; }
  void SetPath(const char *P)       { Path = P; }
  void SetMountCmd(const char *M)   { MountCmd = M; }
  void SetUnMountCmd(const char *U) { UnMountCmd = U; }
  void SetEjectCmd(const char *E)   { EjectCmd = E; }

  bool CheckDisc(void);
  void Eject(void);
};

#endif //_DISCMGR_H

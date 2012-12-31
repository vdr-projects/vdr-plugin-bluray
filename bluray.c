/*
 * bluray.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <getopt.h>
#include <vdr/plugin.h>

#include "discmgr.h"
#include "bdplayer.h"

static const char *VERSION        = "0.0.1";
static const char *DESCRIPTION    = "BluRay Player";
static const char *MAINMENUENTRY  = "Play BluRay Disc";


class cPluginBluray : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cDiscMgr mgr;

public:
  cPluginBluray(void);
  virtual ~cPluginBluray();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void);
  };

cPluginBluray::cPluginBluray(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginBluray::~cPluginBluray()
{
  // Clean up after yourself!
}

const char *cPluginBluray::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return
    "  -D DEV,    --device=DEV   device used for BluRay playback (default "DEFAULT_DEVICE")\n"
    "  -p DIR,    --path=DIR     mount point for BluRay discs (default "DEFAULT_PATH")\n"
    "  -m CMD,    --mount=CMD    program used to mount BluRay disc (default "DEFAULT_MOUNTER")\n"
    "  -u CMD,    --umount=CMD   program used to unmount BluRay disc (default "DEFAULT_UNMOUNTER")\n"
    "  -e CMD,    --eject=CMD    program used to eject BluRay disc (default "DEFAULT_EJECT")\n";
}

bool cPluginBluray::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  // Implement command line argument processing here if applicable.
  static const struct option long_options[] = {
    { "device",   optional_argument, NULL, 'D' },
    { "path",     optional_argument, NULL, 'p' },
    { "mount",    optional_argument, NULL, 'm' },
    { "umount",   optional_argument, NULL, 'u' },
    { "eject",    optional_argument, NULL, 'e' },
    { NULL,       no_argument,       NULL,  0  }
  };

  int c;
  while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
    switch (c) {
      case 'D':
        mgr.SetDevice(optarg);
        break;
      case 'p':
        mgr.SetPath(optarg);
        break;
      case 'm':
        mgr.SetMountCmd(optarg);
        break;
      case 'u':
        mgr.SetUnMountCmd(optarg);
        break;
      case 'e':
        mgr.SetEjectCmd(optarg);
        break;
      default:
        return false;
    }
  }

  return true;
}

cOsdObject *cPluginBluray::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  if (cBDControl::Active()) {
    return NULL;
  }

  if (!mgr.CheckDisc()) {
    return NULL;
  }

  cControl::Shutdown();

  cControl *control = cBDControl::Create(mgr.GetPath());
  if (control) {
    cControl::Launch(control);
  }

  return NULL;
}

VDRPLUGINCREATOR(cPluginBluray); // Don't touch this!

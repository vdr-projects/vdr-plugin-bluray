/*
 * bdplayer.c: A player for BluRay discs
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "bdplayer.h"

#include <vdr/remote.h>
#include <vdr/tools.h>
#include <vdr/status.h>
#include <vdr/recording.h>  // cMarks

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>

#define MIN_TITLE_LENGTH   (180)               // seconds
#define M2TS_SIZE          (188 + 4)           // size of m2ts packet
#define ALIGNED_UNIT_SIZE  (32 * M2TS_SIZE)    // size of aligned unit (32 packets)

// --- cBDPlayer --------------------------------------------------------

class cBDPlayer : public cPlayer, cThread {
private:
  BLURAY *bd;
  BLURAY_TITLE_INFO *title_info;

  cMarks marks;
  int current_chapter;

  enum ePlayModes { pmPlay, pmPause };
  ePlayModes playMode;

  uchar buffer[ALIGNED_UNIT_SIZE];
  int   pos, packs;

  int   current_playlist;
  int   current_clip;

  bool DoRead(void);
  bool DoPlay(void);

  virtual void Activate(bool On);

  void UpdateTracks(unsigned int current_clip);
  void UpdateMarks();
  void HandleEvents(BD_EVENT *ev);
  void Empty();

protected:
  void Action(void);

public:
  cBDPlayer(BLURAY *bd);
  ~cBDPlayer();

  void Goto(int Seconds);
  void SkipChapters(int Chapters);
  void SkipSeconds(int seconds);
  void Play();
  void Pause();
  bool SelectPlaylist(int pl);
  BLURAY *BDHandle() { return bd; }
  cMarks *Marks() { return &marks; }
  cString PosStr();

  virtual bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
  virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
};

cBDPlayer::cBDPlayer(BLURAY *Bd)
{
  bd = Bd;
  title_info = NULL;
  playMode = pmPlay;
  pos = packs = 0;
  current_clip = 0;
  current_playlist = -1;
  current_chapter = -1;
}

cBDPlayer::~cBDPlayer()
{
  if (title_info) {
    bd_free_title_info(title_info);
    title_info = NULL;
  }

  if (bd) {
    bd_close(bd);
    bd = NULL;
  }
}

void cBDPlayer::UpdateTracks(unsigned int current_clip)
{
  if (title_info && current_clip < title_info->clip_count) {
    BLURAY_CLIP_INFO *clip = &title_info->clips[current_clip];
    int i;

    DeviceClrAvailableTracks();

    for (i = 0; i < clip->audio_stream_count; i++) {
      DeviceSetAvailableTrack(ttDolby, i,
                              clip->audio_streams[i].pid,
                              (const char *)clip->audio_streams[i].lang);
    }
    for (i = 0; i < clip->pg_stream_count; i++) {
      DeviceSetAvailableTrack(ttSubtitle, i,
                              clip->pg_streams[i].pid,
                              (const char *)clip->pg_streams[i].lang);
    }
  }
}

void cBDPlayer::UpdateMarks()
{
  ((cList<cMark> *)&marks)->Clear();

  if (title_info && title_info->chapter_count > 1) {
    marks.Add(0);
    for (unsigned i = 1; i < title_info->chapter_count; i++) {
      marks.Add(title_info->chapters[i].start / 90000 * 25 - 1);/// 90000 * 25;//DEFAULTFRAMESPERSECOND; // assume 25fps ...
      marks.Add(title_info->chapters[i].start / 90000 * 25);/// 90000 * 25;//DEFAULTFRAMESPERSECOND; // assume 25fps ...
    }
  }
}

void cBDPlayer::HandleEvents(BD_EVENT *ev)
{
  while (ev->event != BD_EVENT_NONE) {

    switch (ev->event) {

    //case BD_EVENT_ANGLE:
    //case BD_EVENT_TITLE:

    case BD_EVENT_PLAYLIST:
      if (title_info) {
        bd_free_title_info(title_info);
        title_info = NULL;
      }
      title_info = bd_get_playlist_info(bd, ev->param, 0);
      current_playlist = ev->param;
      current_chapter = -1;
      current_clip = -1;
      UpdateMarks();
      break;

    case BD_EVENT_PLAYITEM:
      current_clip = ev->param;
      UpdateTracks(ev->param);
      break;

    case BD_EVENT_CHAPTER:
      current_chapter = ev->param;
      break;

    case BD_EVENT_END_OF_TITLE:
      isyslog("END_OF_TITLE");
      Cancel(-1);
      break;

    default:
      break;
    }

    /* get next event */
    if (!bd_get_event(bd, ev))
      break;
  }
}

bool cBDPlayer::DoRead()
{
  BD_EVENT ev = {0, 0};

  LOCK_THREAD;

  pos = 0;
  packs = bd_read_ext(bd, buffer, ALIGNED_UNIT_SIZE, &ev);

  if (packs == 0) {
    if (ev.event == BD_EVENT_NONE) {
      // title without video
      cCondWait::SleepMs(3);
    }
  } else if (packs < 0) {
    // ERROR
    esyslog("bd_read() error");
    return false;
  }
  packs /= 192;

  HandleEvents(&ev);
  return true;
}

bool cBDPlayer::DoPlay()
{
  cPoller Poller;

  if (DevicePoll(Poller, 10)) {

    LOCK_THREAD;

    for (;pos < packs; pos++) {

      uint16_t pid = (((buffer)[1+4+pos*192] << 8) | (buffer)[2+4+pos*192]) & 0x1fff;
      if (pid >= 1200 && pid < 1300) {
	// skip PG streams
	continue;
      }
      if (pid >= 1400 && pid < 1500) {
	// skip IG streams
	continue;
      }

      int w = PlayTs(buffer + pos*192 + 4, 188, false);

      if (w == 188) {
	continue;
      } else if (w > 0) {
	esyslog("PlayTs() error: partial ts packet accepted");
	continue;
      } else if (w == 0) {
	//esyslog("PlayTs() error: data not accepted");
	break;
      } else {
	esyslog("PlayTs() error");
	return false;
      }
    }
  }

  return true;
}

void cBDPlayer::Activate(bool On)
{
  if (On && bd) {
    Start();
  } else {
    Cancel(6);
  }
}

void cBDPlayer::Action()
{
  DoRead();

  while (Running()) {

    if (pos >= packs) {
      if (!DoRead()) {
	break;
      }
    }

    if (!DoPlay()) {
      break;
    }
  }

  isyslog("End BluRay playback");
}

void cBDPlayer::SkipSeconds(int seconds)
{
  uint64_t tick = bd_tell_time(bd);
  if (tick < 0) {
    isyslog("bd_tell_time() failed");
    return;
  }

  seconds += tick / 90000;
  if (seconds < 0) {
    seconds = 0;
  }

  Goto(seconds);
}

void cBDPlayer::Goto(int seconds)
{
  LOCK_THREAD;

  Empty();
  uint64_t tick = seconds;
  tick *= 90000;

  isyslog("Seek to %d", seconds);
  bd_seek_time(bd, tick);
}

void cBDPlayer::SkipChapters(int Chapters)
{
  LOCK_THREAD;

  if (title_info && current_chapter > 0) {
    int chapter = current_chapter + Chapters;
    if (chapter < 1) chapter = 1;
    if (chapter > (int)title_info->chapter_count) chapter = title_info->chapter_count;

    Empty();

    isyslog("Seek to chapter %d", chapter);
    bd_seek_chapter(bd, chapter - 1);
  }
}

void cBDPlayer::Empty(void)
{
  LOCK_THREAD;

  pos = packs = 0;

  DeviceClear();
}

bool cBDPlayer::SelectPlaylist(int pl)
{
  bool end_of_title;

  LOCK_THREAD;

  Empty();

  end_of_title = !bd_select_playlist(bd, pl);
  isyslog("bd_select_playlist -> %s", end_of_title ? "FAIL" : "OK");
  return !end_of_title;
}

void cBDPlayer::Pause(void)
{
  // from vdr-1.7.34
  if (playMode == pmPause) {
    Play();
  } else {
    LOCK_THREAD;

    DeviceFreeze();
    playMode = pmPause;
  }
}

void cBDPlayer::Play(void)
{
  // from vdr-1.7.34
  if (playMode != pmPlay) {
    LOCK_THREAD;

    DevicePlay();
    playMode = pmPlay;
  }
}

cString cBDPlayer::PosStr()
{
  cString pl = current_playlist >= 0 ? cString::sprintf("PL %d",  current_playlist) : cString("");
  cString cl = current_clip     >= 0 ? cString::sprintf(" CL %d", current_clip)     : cString("");
  cString ch = current_chapter  >= 1 ? cString::sprintf(" C %d",  current_chapter)  : cString("");
  return cString::sprintf("%s%s%s", *pl, *cl, *ch);
}

bool cBDPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  LOCK_THREAD;

  if (title_info) {
    Total = title_info->duration / 90000 * 25;
    Current = bd_tell_time(bd) / 90000 * 25;
    return true;
  }

  Current = Total = -1;
  return false;
}

bool cBDPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  Play = (playMode == pmPlay);
  Forward = true;
  Speed = -1;
  return true;
}

// --- cBDControl -------------------------------------------------------

#include "titlemenu.h"

#define MODETIMEOUT       3 // seconds

int cBDControl::active = 0;

cBDControl::cBDControl(cBDPlayer *Player)
:cControl(Player)
{
  player = Player;
  active++;

  displayReplay = NULL;
  visible = modeOnly = shown = false;
  lastCurrent = lastTotal = -1;
  lastPlay = lastForward = false;
  lastSpeed = -2; // an invalid value
  timeoutShow = 0;
  timeSearchActive = false;
  chapterSeekTime = 0;

  disc_name = tr("BluRay");
  menu = NULL;

  cStatus::MsgReplaying(this, "BluRay", NULL, true);
}

cBDControl::~cBDControl()
{
  active--;

  delete player;

  cStatus::MsgReplaying(this, NULL, NULL, false);
}

cControl *cBDControl::Create(const char *Path)
{
  const struct meta_dl *meta_data = NULL;
  BLURAY *bd;

  /* open disc */
  bd = bd_open(Path, NULL);
  if (!bd) {
    isyslog("opening BluRay disc %s failed", Path);
    return NULL;
  }

  /* load title list */
  unsigned num_title_idx = bd_get_titles(bd, TITLES_RELEVANT, MIN_TITLE_LENGTH);
  if (num_title_idx < 1) {
    esyslog("BluRay: no titles found");
    return NULL;
  }
  isyslog("BluRay: %d titles", num_title_idx);

  /* guess the main title */

  unsigned title_idx = 0;
  uint64_t duration = 0;
  int playlist = 99999;

  for (unsigned i = 0; i < num_title_idx; i++) {
    BLURAY_TITLE_INFO *info = bd_get_title_info(bd, i, 0);
    if (info) {
      if (info->duration > duration) {
        title_idx = i;
        duration  = info->duration;
        playlist  = info->playlist;
      }
      bd_free_title_info(info);
    }
  }
  isyslog("BluRay main title: #%d (%05d.mpls)\n", title_idx, playlist);

  /* init event queue */
  bd_get_event(bd, NULL);

  /* select playlist */
  if (bd_select_title(bd, title_idx) <= 0) {
    esyslog("bd_select_title(%d) failed", title_idx);
    return NULL;
  }

  cBDControl *control = new cBDControl(new cBDPlayer(bd));

  /* get disc name */
  meta_data = bd_get_meta(bd);
  if (meta_data && meta_data->di_name && strlen(meta_data->di_name) > 1) {
    control->disc_name = meta_data->di_name;
  }

  return control;
}

void cBDControl::Pause(void)
{
  if (player)
    player->Pause();
}

void cBDControl::Play(void)
{
  if (player)
    player->Play();
}

BLURAY *cBDControl::BDHandle()
{
  if (player)
    return player->BDHandle();
  return NULL;
}

bool cBDControl::SelectPlaylist(int pl)
{
  if (player)
    return player->SelectPlaylist(pl);
  return false;
}

void cBDControl::SkipSeconds(int seconds)
{
  if (player)
    player->SkipSeconds(seconds);
}

void cBDControl::SkipChapters(int chapters)
{
  if (player)
    player->SkipChapters(chapters);
}

void cBDControl::Goto(int seconds)
{
  if (player)
     player->Goto(seconds);
}

cString cBDControl::GetHeader(void)
{
  return disc_name;
}

void cBDControl::ShowTimed(int Seconds)
{
  // from vdr-1.7.34
  if (modeOnly)
     Hide();
  if (!visible) {
     shown = ShowProgress(true);
     timeoutShow = (shown && Seconds > 0) ? time(NULL) + Seconds : 0;
     }
  else if (timeoutShow && Seconds > 0)
     timeoutShow = time(NULL) + Seconds;
}

void cBDControl::Show(void)
{
  // from vdr-1.7.34
  ShowTimed();
}

void cBDControl::Hide(void)
{
  // from vdr-1.7.34
  if (visible) {
     delete displayReplay;
     displayReplay = NULL;
     SetNeedsFastResponse(false);
     visible = false;
     modeOnly = false;
     lastPlay = lastForward = false;
     lastSpeed = -2; // an invalid value
     timeSearchActive = false;
     timeoutShow = 0;
     }
}

void cBDControl::ShowMode(void)
{
  // from vdr-1.7.34
  if (visible || Setup.ShowReplayMode && !cOsd::IsOpen()) {
     bool Play, Forward;
     int Speed;
     if (GetReplayMode(Play, Forward, Speed) && (!visible || Play != lastPlay || Forward != lastForward || Speed != lastSpeed)) {
        bool NormalPlay = (Play && Speed == -1);

        if (!visible) {
           if (NormalPlay)
              return; // no need to do indicate ">" unless there was a different mode displayed before
           visible = modeOnly = true;
           displayReplay = Skins.Current()->DisplayReplay(modeOnly);
           }

        if (modeOnly && !timeoutShow && NormalPlay)
           timeoutShow = time(NULL) + MODETIMEOUT;
        displayReplay->SetMode(Play, Forward, Speed);
        lastPlay = Play;
        lastForward = Forward;
        lastSpeed = Speed;
        }
     }
}

bool cBDControl::ShowProgress(bool Initial)
{
  // from vdr-1.7.34
  int Current, Total;

  if (GetIndex(Current, Total) && Total > 0) {
     if (!visible) {
        displayReplay = Skins.Current()->DisplayReplay(modeOnly);
        displayReplay->SetMarks(player->Marks());
        SetNeedsFastResponse(true);
        visible = true;
        }
     if (Initial) {
        lastCurrent = lastTotal = -1;
        }
     if (Current != lastCurrent || Total != lastTotal) {
        if (Total != lastTotal) {
           int Index = Total;
           displayReplay->SetTotal(IndexToHMSF(Index, false, FramesPerSecond()));
           if (!Initial)
              displayReplay->Flush();
           }
        displayReplay->SetProgress(Current, Total);
        if (!Initial)
           displayReplay->Flush();
        displayReplay->SetCurrent(IndexToHMSF(Current, false, FramesPerSecond()));

        cString Title;
        cString Pos = player ? player->PosStr() : cString(NULL);
        if (*Pos && strlen(Pos) > 1) {
          Title = cString::sprintf("%s (%s)", *disc_name, *Pos);
        } else {
          Title = disc_name;
        }
        displayReplay->SetTitle(Title);

        displayReplay->Flush();
        lastCurrent = Current;
        }
     lastTotal = Total;
     ShowMode();
     return true;
     }
  return false;
}

void cBDControl::TimeSearchDisplay(void)
{
  // from vdr-1.7.34
  char buf[64];
  // TRANSLATORS: note the trailing blank!
  strcpy(buf, tr("Jump: "));
  int len = strlen(buf);
  char h10 = '0' + (timeSearchTime >> 24);
  char h1  = '0' + ((timeSearchTime & 0x00FF0000) >> 16);
  char m10 = '0' + ((timeSearchTime & 0x0000FF00) >> 8);
  char m1  = '0' + (timeSearchTime & 0x000000FF);
  char ch10 = timeSearchPos > 3 ? h10 : '-';
  char ch1  = timeSearchPos > 2 ? h1  : '-';
  char cm10 = timeSearchPos > 1 ? m10 : '-';
  char cm1  = timeSearchPos > 0 ? m1  : '-';
  sprintf(buf + len, "%c%c:%c%c", ch10, ch1, cm10, cm1);
  displayReplay->SetJump(buf);
}

void cBDControl::TimeSearchProcess(eKeys Key)
{
  // from vdr-1.7.34
#define STAY_SECONDS_OFF_END 10
  int Seconds = (timeSearchTime >> 24) * 36000 + ((timeSearchTime & 0x00FF0000) >> 16) * 3600 + ((timeSearchTime & 0x0000FF00) >> 8) * 600 + (timeSearchTime & 0x000000FF) * 60;
  int Current = int(round(lastCurrent / FramesPerSecond()));
  int Total = int(round(lastTotal / FramesPerSecond()));
  switch (Key) {
    case k0 ... k9:
         if (timeSearchPos < 4) {
            timeSearchTime <<= 8;
            timeSearchTime |= Key - k0;
            timeSearchPos++;
            TimeSearchDisplay();
            }
         break;
    case kFastRew:
    case kLeft:
    case kFastFwd:
    case kRight: {
         int dir = ((Key == kRight || Key == kFastFwd) ? 1 : -1);
         if (dir > 0)
            Seconds = min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
         SkipSeconds(Seconds * dir);
         timeSearchActive = false;
         }
         break;
#if 0
    case kPlayPause:
#endif
    case kPlay:
    case kUp:
    case kPause:
    case kDown:
    case kOk:
         Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
         Goto(Seconds);
         timeSearchActive = false;
         break;
    default:
         if (!(Key & k_Flags)) // ignore repeat/release keys
            timeSearchActive = false;
         break;
    }

  if (!timeSearchActive) {
     if (timeSearchHide)
        Hide();
     else
        displayReplay->SetJump(NULL);
     ShowMode();
     }
}

void cBDControl::TimeSearch(void)
{
  // from vdr-1.7.34
  timeSearchTime = timeSearchPos = 0;
  timeSearchHide = false;
  if (modeOnly)
     Hide();
  if (!visible) {
     Show();
     if (visible)
        timeSearchHide = true;
     else
        return;
     }
  timeoutShow = 0;
  TimeSearchDisplay();
  timeSearchActive = true;
}


eOSState cBDControl::ProcessKey(eKeys Key)
{
  // from vdr-1.7.34
  if (!Active())
     return osEnd;

  // Handle menus
  if (menu) {
    eOSState state = menu->ProcessKey(Key);
    if (state == osBack) {
      delete menu;
      menu = NULL;
      return osEnd;
    }
    if (state == osEnd) {
      Hide();
      delete menu;
      menu = NULL;
    }
    return osContinue;
  }

  if (visible) {
     if (timeoutShow && time(NULL) > timeoutShow) {
        Hide();
        ShowMode();
        timeoutShow = 0;
        }
     else if (modeOnly)
        ShowMode();
     else
        shown = ShowProgress(!shown) || shown;
     }
  if (timeSearchActive && Key != kNone) {
     TimeSearchProcess(Key);
     return osContinue;
     }
  bool DoShowMode = true;

  switch ((int)Key) {
    case kUp:
    case kPlay:   Play();
                  break;
    case kDown:
    case kPause:  Pause();
                  break;
    case kRed:    TimeSearch(); break;
    case kGreen:
    case kPrev:   SkipSeconds(-60);
                  break;
    case kYellow:
    case kNext:   SkipSeconds(60);
                  break;
    case kBlue:
    case kStop:   Hide();
                  return osEnd;
    case k4:      if (chapterSeekTime < time(NULL) - 2)
                    SkipChapters(0);
                  else
                    SkipChapters(-1);
                  chapterSeekTime = time(NULL);
                  break;
    case k6:      SkipChapters(1);
                  break;
    default: {
      DoShowMode = false;
      switch (int(Key)) {
        default: {
          switch (Key) {
            case kOk:      if (visible && !modeOnly) {
                              Hide();
                              DoShowMode = true;
                              }
                           else
                              Show();
                           break;
            case kBack:    Hide();
                           menu = new cTitleMenu(this);
                           break;
            default:       return osUnknown;
          }
        }
      }
    }
  }
  if (DoShowMode)
    ShowMode();

  return osContinue;
}

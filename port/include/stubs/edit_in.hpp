#pragma once

// editloop.cpp's globals that this unit redeclares static after a header declares them extern:
// MWCC makes this unit's copies file-local, clang's -fms-extensions keeps them global and the
// port's link would have two of each. Renamed apart here.
#define Chara EditIn_Chara
#define MainCamera EditIn_MainCamera
#define NowCamera EditIn_NowCamera
#define TalkCamera EditIn_TalkCamera
#define NowTime EditIn_NowTime
#define TexAnimeData EditIn_TexAnimeData
#define camera_dist_mode EditIn_camera_dist_mode
#define fix_chara_pos EditIn_fix_chara_pos
#define fix_chara_rot EditIn_fix_chara_rot

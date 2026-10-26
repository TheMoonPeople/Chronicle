#include "common.h"
#include "nowload.hpp"

#include "texture.hpp"

int end_flag = 1;

CTexture nl_tex;
CTexture nl_tex2;

int   now_loding_flag;
int   nl_start_cnt;
float col_cnt;
float col_add;
int   logo_count;
int   map_title_no;
int   now_loding_off;
int   now_loading_vsync_end;

void clear_now_loading_vsync_end() {
    now_loading_vsync_end = 0;
}

int check_now_loading_vsync_end() {
    return now_loading_vsync_end;
}

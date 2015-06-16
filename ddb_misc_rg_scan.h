/*
 * ddb_misc_rg_scan.h - libEBUR128-based Replay Gain scanner plugin
 *                      for the DeaDBeeF audio player
 *
 * Copyright (c) 2015 Ivan Pilipenko
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef __DDB_RG
#define __DDB_RG

#include <deadbeef/deadbeef.h>

typedef struct{
    DB_misc_t misc;

    int (*rg_scan) (DB_playItem_t **scan_items,     // tracks to scan
                    const int *num_tracks,          // how many tracks
                    float *out_track_rg,            // individual track replay gain
                    float *out_track_pk,            // indivirual track peak
                    float *out_album_rg,            // album track replay gain
                    float *out_album_pk,            // album peak
                    float *targetdb,                // our target loudness
                    int *abort);                    // will be set to 1 if scanning was aborted

    int (*rg_apply) (DB_playItem_t *track,
                     float *out_track_rg,
                     float *out_track_pk,
                     float *out_album_rg,
                     float *out_album_pk);

    void (*rg_remove) (DB_playItem_t **work_items,
                       const int *num_tracks);
} rg_scan_t;

#endif //__DDB_RG
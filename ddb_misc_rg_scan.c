/*
 * ddb_misc_rg_scan.c - libEBUR128-based Replay Gain scanner plugin
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

#include "ddb_misc_rg_scan.h"

#include <string.h>
#include <stdlib.h>

#include <deadbeef/deadbeef.h>              // deadbeef SDK
#include <ebur128.h>                        // libEBUR128

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt,...)

static rg_scan_t plugin;                    // our plugin structure
static DB_functions_t *deadbeef;            // the deadbeef functions api

// definition of the plugin
DB_plugin_t* ddb_misc_replaygain_scan_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}

int rg_scan (DB_playItem_t **scan_items,     // tracks to scan
             const int *num_tracks,          // how many tracks
             float *out_track_rg,            // individual track replay gain
             float *out_track_pk,            // indivirual track peak
             float *out_album_rg,            // album track replay gain
             float *out_album_pk,            // album peak
             float *targetdb,                // our target loudness
             int *abort)                     // will be set to 1 if scanning was aborted
{
    ebur128_state **status_gain = NULL;
    ebur128_state **status_peak = NULL;
    double loudness;
    char *buffer;
    char *bufferf;
    ddb_waveformat_t fmt;

    *out_album_pk = 0;
    *out_album_rg = 0;

    // allocate status array
    status_gain = malloc((size_t) *num_tracks * sizeof(ebur128_state*));
    status_peak = malloc((size_t) *num_tracks * sizeof(ebur128_state*));

    // calculate gain for each track
    for(int i = 0; i < *num_tracks; ++i){
        if (abort && *abort) {
            fprintf (stdout, "rg scan: user asked to abort, main loop aborted.\n");
            return -2;
        }
        if (deadbeef->pl_get_item_duration (scan_items[i]) <= 0) {
            deadbeef->pl_lock ();
            fprintf (stderr, "rg scan: stream %s doesn't have finite length, skipped\n", deadbeef->pl_find_meta (scan_items[i], ":URI"));
            deadbeef->pl_unlock ();
            return -1;
        }

        DB_decoder_t *dec = NULL;
        DB_fileinfo_t *fileinfo = NULL;

        deadbeef->pl_lock ();
        dec = (DB_decoder_t *)deadbeef->plug_get_for_id (deadbeef->pl_find_meta (scan_items[i], ":DECODER"));
        deadbeef->pl_unlock ();


        if (dec) { // we have our decoder
            fileinfo = dec->open (0);
            if (fileinfo && dec->init (fileinfo, DB_PLAYITEM (scan_items[i])) != 0) {
                deadbeef->pl_lock ();
                fprintf (stderr, "rg scan: failed to decode file %s\n", deadbeef->pl_find_meta (scan_items[i], ":URI"));
                deadbeef->pl_unlock ();
                return -1;
            }

            if (fileinfo) { // we have all info needed to scan
                // this is a status object for ebur128 gain scanning
                status_gain[i] = ebur128_init(fileinfo->fmt.channels,   // channels
                                              fileinfo->fmt.samplerate, // samplerate
                                              EBUR128_MODE_I);          // mode: Integrated (over the length of the track)

                // this is a status object for ebur128 peak scanning - needs a different mode, so separate
                status_peak[i] = ebur128_init(fileinfo->fmt.channels,   // channels
                                              fileinfo->fmt.samplerate, // samplerate
                                              EBUR128_MODE_SAMPLE_PEAK);// mode: find sample peak
                if(status_gain[i] == NULL || status_peak[i] == NULL)
                { 
                    deadbeef->pl_lock ();
                    fprintf (stderr, "rg scan: failed to init libebur128 object for file %s, aborting\n", deadbeef->pl_find_meta (scan_items[i], ":URI"));
                    deadbeef->pl_unlock ();
                    return -1;
                }

                // setting channel map
                switch(fileinfo->fmt.channels)
                {
                    case 1: // mono
                        ebur128_set_channel (status_gain[i], 0, EBUR128_CENTER);

                        ebur128_set_channel (status_peak[i], 0, EBUR128_CENTER);
                        break;
                    case 2: // stereo
                        ebur128_set_channel (status_gain[i], 0, EBUR128_LEFT);
                        ebur128_set_channel (status_gain[i], 1, EBUR128_RIGHT);

                        ebur128_set_channel (status_peak[i], 0, EBUR128_LEFT);
                        ebur128_set_channel (status_peak[i], 1, EBUR128_RIGHT);
                        break;
                    case 3: // 3.1
                        ebur128_set_channel(status_gain[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_gain[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_gain[i], 2, EBUR128_CENTER);

                        ebur128_set_channel(status_peak[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_peak[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_peak[i], 2, EBUR128_CENTER);
                        break;
                    case 4:
                        ebur128_set_channel(status_gain[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_gain[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_gain[i], 2, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_gain[i], 3, EBUR128_RIGHT_SURROUND);

                        ebur128_set_channel(status_peak[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_peak[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_peak[i], 2, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_peak[i], 3, EBUR128_RIGHT_SURROUND);
                        break;
                    case 5:
                        ebur128_set_channel(status_gain[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_gain[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_gain[i], 2, EBUR128_CENTER);
                        ebur128_set_channel(status_gain[i], 3, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_gain[i], 4, EBUR128_RIGHT_SURROUND);

                        ebur128_set_channel(status_peak[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_peak[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_peak[i], 2, EBUR128_CENTER);
                        ebur128_set_channel(status_peak[i], 3, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_peak[i], 4, EBUR128_RIGHT_SURROUND);
                        break;
                    case 6:
                        ebur128_set_channel(status_gain[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_gain[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_gain[i], 2, EBUR128_CENTER);
                        // LFE is not being taken into account when scanning
                        // see R128 spec at https://tech.ebu.ch/docs/tech/tech3341.pdf
                        ebur128_set_channel(status_gain[i], 3, EBUR128_UNUSED);
                        ebur128_set_channel(status_gain[i], 4, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_gain[i], 5, EBUR128_RIGHT_SURROUND);

                        ebur128_set_channel(status_peak[i], 0, EBUR128_LEFT);
                        ebur128_set_channel(status_peak[i], 1, EBUR128_RIGHT);
                        ebur128_set_channel(status_peak[i], 2, EBUR128_CENTER);
                        ebur128_set_channel(status_peak[i], 3, EBUR128_UNUSED);
                        ebur128_set_channel(status_peak[i], 4, EBUR128_LEFT_SURROUND);
                        ebur128_set_channel(status_peak[i], 5, EBUR128_RIGHT_SURROUND);
                        break;
                    default:
                        deadbeef->pl_lock ();
                        fprintf (stderr, "rg scan: file %s has %d channels - libebur128 only supports up to 6. Aborting.\n", 
                                         deadbeef->pl_find_meta (scan_items[i], ":URI"), 
                                         fileinfo->fmt.channels);
                        deadbeef->pl_unlock ();
                        return -1;
                }

                int samplesize = fileinfo->fmt.channels * fileinfo->fmt.bps / 8;

                int bs = 2000 * samplesize;

                buffer = malloc (bs/samplesize*sizeof(float)*8*48);
                // FIXME: don't they have to be the same size?
                bufferf = (char*) malloc(status_gain[i]->samplerate * status_gain[i]->channels * sizeof(float));
                memcpy (&fmt, &fileinfo->fmt, sizeof (fmt));
                fmt.bps = 32;
                fmt.is_float = 1;

                int eof = 0;
                for (;;) {
                    if (eof) {
                        break;
                    }
                    if (abort && *abort) {
                        fprintf (stdout, "rg scan: user asked to abort, scanning aborted.\n");
                        break;
                    }

                    int sz = dec->read (fileinfo, buffer, bs); // read one sample

                    if (sz != bs) {
                        eof = 1;
                    }

                    // convert from native output to float
                    deadbeef->pcm_convert (&fileinfo->fmt, buffer, &fmt, bufferf, sz);
                    int frames = sz / samplesize;

                    ebur128_add_frames_float(status_gain[i], (float*) bufferf, frames); // collect data
                    ebur128_add_frames_float(status_peak[i], (float*) bufferf, frames); // collect data 
                }
            }
        }

        // calculating track peak
        // libEBUR128 calculates peak per channel, so we have to pick the highest value
        double tr_peak = 0;
        double ch_peak = 0;
        int res;
        for (int ch = 0; ch < fmt.channels; ++ch)
        {
            res = ebur128_sample_peak(status_peak[i], ch, &ch_peak);
            if (res == EBUR128_ERROR_INVALID_MODE){
                fprintf (stderr, "rg scan: internal error: invalid mode set\n");
                *abort = 1;
                return -1;
            }
            trace ("rg scan: peak for ch %d: %f\n", ch, ch_peak);
            if (ch_peak > tr_peak){
                trace ("rg scan: %f > %f\n", ch_peak, tr_peak);
                tr_peak = ch_peak;
            }
        }
        out_track_pk[i] = (float) tr_peak;

        // update album peak if necessary
        if (*out_album_pk < tr_peak){
            *out_album_pk = tr_peak;
        }

        // calculate track loudness
        ebur128_loudness_global(status_gain[i], &loudness);
        /*
         * EBUR128 sets the target level to -23 LUFS = 84dB
         * -> -23 - loudness = track gain to get to 84dB
         * 
         * The old implementation of RG used 89dB, most people still use that
         * -> the above + (targetdb - 84) = track gain to get to 89dB (or user specified)
         */
        out_track_rg[i] = (float) (-23 - loudness + *targetdb - 84);

        // clean up
        if (buffer) {
            free (buffer);
            buffer = NULL;
        }
        if (bufferf) {
            free (bufferf);
            bufferf = NULL;
        }
    }
    // calculate album loudness
    ebur128_loudness_global_multiple(status_gain, (size_t) *num_tracks, &loudness);
    *out_album_rg = -23 - (float) loudness + *targetdb - 84; // see above

    // clean up
    if (status_gain){
        for (int i = 0; i < *num_tracks; ++i) {
            ebur128_destroy(&status_gain[i]);
        }
        free(status_gain);
    }
    if (status_peak){
        for (int i = 0; i < *num_tracks; ++i) {
            ebur128_destroy(&status_peak[i]);
        }
        free(status_peak);
    }
    return 0;
}

int rg_write_meta (DB_playItem_t *track){
    deadbeef->pl_lock ();
    const char *dec = deadbeef->pl_find_meta_raw (track, ":DECODER");
    char decoder_id[100];
    if (dec) {
        strncpy (decoder_id, dec, sizeof (decoder_id));
    }
    int match = track && dec;
    deadbeef->pl_unlock ();
    if (match) {
        int is_subtrack = deadbeef->pl_get_item_flags (track) & DDB_IS_SUBTRACK;
        if (is_subtrack) {
            return 0; // only write tags for actual tracks
        }
        // find decoder
        DB_decoder_t *dec = NULL;
        DB_decoder_t **decoders = deadbeef->plug_get_decoder_list ();
        for (int i = 0; decoders[i]; i++) {
            if (!strcmp (decoders[i]->plugin.id, decoder_id)) {
                dec = decoders[i];
                if (dec->write_metadata) {
                    dec->write_metadata (track);
                }
                break;
            }
        }
    }
    else {
        deadbeef->pl_lock ();
        fprintf (stderr, "rg scan: could not find matching decoder for %s\n", deadbeef->pl_find_meta (track, ":URI"));
        deadbeef->pl_unlock ();
        return -1;
    }
    return 0;
}

int rg_apply (DB_playItem_t *track,
              float *out_track_rg,
              float *out_track_pk,
              float *out_album_rg,
              float *out_album_pk){

    // set RG tags 
    deadbeef->pl_set_item_replaygain (track, DDB_REPLAYGAIN_ALBUMGAIN, *out_album_rg);
    deadbeef->pl_set_item_replaygain (track, DDB_REPLAYGAIN_ALBUMPEAK, *out_album_pk);
    deadbeef->pl_set_item_replaygain (track, DDB_REPLAYGAIN_TRACKGAIN, *out_track_rg);
    deadbeef->pl_set_item_replaygain (track, DDB_REPLAYGAIN_TRACKPEAK, *out_track_pk);

    // tags are NOT written yet - they are merely data in the playlist item, so "flush" them to file
    return rg_write_meta (track);
}

void rg_remove (DB_playItem_t **work_items, const int *num_tracks){
    for (int it = 0; it < *num_tracks; ++it){
        deadbeef->pl_delete_meta (work_items[it], ":REPLAYGAIN_ALBUMGAIN");
        deadbeef->pl_delete_meta (work_items[it], ":REPLAYGAIN_ALBUMPEAK");
        deadbeef->pl_delete_meta (work_items[it], ":REPLAYGAIN_TRACKGAIN");
        deadbeef->pl_delete_meta (work_items[it], ":REPLAYGAIN_TRACKPEAK");

        DB_playItem_t *track = work_items[it];
        deadbeef->pl_lock ();
        const char *dec = deadbeef->pl_find_meta_raw (track, ":DECODER");
        char decoder_id[100];
        if (dec) {
            strncpy (decoder_id, dec, sizeof (decoder_id));
        }
        int match = track && dec;
        deadbeef->pl_unlock ();
        if (match) {
            int is_subtrack = deadbeef->pl_get_item_flags (track) & DDB_IS_SUBTRACK;
            if (is_subtrack) {
                continue;
            }
            // find decoder
            DB_decoder_t *dec = NULL;
            DB_decoder_t **decoders = deadbeef->plug_get_decoder_list ();
            for (int i = 0; decoders[i]; i++) {
                if (!strcmp (decoders[i]->plugin.id, decoder_id)) {
                    dec = decoders[i];
                    if (dec->write_metadata) {
                        dec->write_metadata (track);
                    }
                    break;
                }
            }
        }
        deadbeef->pl_item_unref (work_items[it]);
    }
}
// plugin structure and info
static rg_scan_t plugin = {
    .misc.plugin.api_vmajor = 1,
    .misc.plugin.api_vminor = 8,
    .misc.plugin.version_major = 1,
    .misc.plugin.version_minor = 0,
    .misc.plugin.type = DB_PLUGIN_MISC,
    .misc.plugin.name = "Replay Gain Scanner",
    .misc.plugin.id = "rgscanner",
    .misc.plugin.descr = "Calculates and writes Replay Gain tags, based on the EBUR128 spec.\n"
                         "Requires a GUI plugin, e.g. the GTK2 RG GUI plugin, to work.\n",
    .misc.plugin.copyright = "libEBUR128-based Replay Gain scanner plugin for the DeaDBeeF audio player\n"
                             "\n"
                             "Copyright (c) 2015 Ivan Pilipenko\n"
                             "\n"
                             "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
                             "of this software and associated documentation files (the \"Software\"), to deal\n"
                             "in the Software without restriction, including without limitation the rights\n"
                             "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
                             "copies of the Software, and to permit persons to whom the Software is\n"
                             "furnished to do so, subject to the following conditions:\n"
                             "\n"
                             "The above copyright notice and this permission notice shall be included in\n"
                             "all copies or substantial portions of the Software.\n"
                             "\n"
                             "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
                             "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
                             "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
                             "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
                             "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
                             "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n"
                             "THE SOFTWARE.",
    //.misc.plugin.website = "http://none.net",
    .rg_scan = rg_scan,
    .rg_apply = rg_apply,
    .rg_remove = rg_remove
};
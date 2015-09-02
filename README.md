# ddb_misc_replaygain_scan
libEBUR128-based ReplayGain scanner plugin for the DeaDBeeF audio player

This plugin allows calculating and writing ReplayGain tags for music files supported by DeaDBeeF. 
It uses libEBUR128 as a backend, and is included for easier compilation.

Currently, only two actions are available:

- scan as single album: treats all selected items as one album
- remove replaygain info: self-explanatory

In the future, I'm planning to add:

- scan as single track(s): scans all selected tracks but doesn't calculate nor write album tags
- edit replaygain info: editing of the tags for a single track

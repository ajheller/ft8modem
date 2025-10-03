#! /bin/bash

# you may need to adjust this depending on where ft8code, 
#  ft8encode, and jt9 are installed on your system.

# On Linux, ft8code and jt9 are in /usr/bin
# On MacOS, they're in /Applications/wsjtx.app/Contents/MacOS/
# On Windows, (I need to check)

export PATH=/usr/local/bin:$PATH

# echo $PATH

which ft8code
which ft8encode
which jt9

ft8code -t

ft8code "CQ AK6IM CM87"
# ft8code "AK6IM HK2N -20"
# ft8code "HK2N AK6IM R-15"
# ft8code "AK6IM HK2N RR73"
# ft8code "HK2N AK6IM 73"


FT8WAV=CQ00000000.WAV
ft8encode FT8 12000 1500 $FT8WAV 'CQ AK6IM CM87'
jt9 -8 -d 3 $FT8WAV

sox $FT8WAV -n spectrogram


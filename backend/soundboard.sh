#!/bin/bash

# Konfiguration
REAL_VOL=$(pactl get-source-volume @DEFAULT_SOURCE@ | grep -oP '\d+%' | head -n1)
REAL_VOL_NUM=${REAL_VOL%\%}
echo "Sound wird gespielt"
REAL_MICROPHONE=$(pactl info | grep "Default Source" | awk '{print $3}')
AUDIO_FILE="sounds/$1"

VIRT_SINK=$(pactl load-module module-null-sink sink_name=virtmic_sink sink_properties=device.description=Virtual_Mic_Sink)

#LOOPBACK_MIC=$(pactl load-module module-loopback source=$REAL_MICROPHONE sink=virtmic_sink)

LOOPBACK_MIC=$(pactl load-module module-loopback source=$REAL_MICROPHONE sink=virtmic_sink)

VIRT_MIC_SOURCE="virtmic_sink.monitor"
pactl set-default-source $VIRT_MIC_SOURCE


#paplay -d virtmic_sink "$AUDIO_FILE"
(ffmpeg -re -i "$AUDIO_FILE" -filter:a "volume=0.2" -f s16le -ar 48000 -ac 2 pipe:1 | pacat --raw -d virtmic_sink)  > /dev/null 2>&1

sleep 0.1

pactl set-default-source "$REAL_MICROPHONE"
pactl set-source-volume "$REAL_MICROPHONE" "$REAL_VOL"

pactl unload-module $LOOPBACK_MIC
pactl unload-module $VIRT_SINK

echo "Sound fertig"
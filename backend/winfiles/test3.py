import subprocess
import ffmpeg
import sounddevice as sd
import numpy as np
import threading
import os
import sys

def get_device_id(device_name):
    # Aufruf des C++-Programms mit dem Gerätenamen
    result = subprocess.run(['getid.exe', device_name], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Fehler beim Abrufen der Device-ID: {result.stderr.strip()}")
    print (result.stdout.strip())
    return result.stdout.strip()


# Windows-spezifisch: Standard-Audiogerät setzen (nur Ausgabe)
def set_default_audio_device(device_id):
    cmd = f'powershell -Command "Set-AudioDevice -Id \'{device_id}\'"'
    subprocess.run(cmd, shell=True)

# Windows: aktuelles Standard-Ausgabegerät auslesen (ID)
def get_default_device_id():
    result = subprocess.run(
        'powershell -Command "(Get-AudioDevice -Recording).Id"',
        capture_output=True, text=True, shell=True)
    return result.stdout.strip()

# Alle verfügbaren Audio-Geräte auflisten (Index + Name + Kanäle)

# MP3 abspielen auf gewähltem Ausgabegerät (z.B. VB Cable Input)
def play_with_volume_clipping_safe(file_path, index, volume=1.0):
    file_path = os.path.abspath(file_path)
    
    try:
        # ffmpeg dekodiert die Audiodatei zu 32-bit float PCM mit 2 Kanälen und 48 kHz
        out, _ = (
            ffmpeg
            .input(file_path)
            .output('pipe:', format='f32le', acodec='pcm_f32le', ac=2, ar=48000)
            .run(capture_stdout=True, capture_stderr=True)
        )
    except ffmpeg.Error as e:
        print("[Fehler] ffmpeg konnte die Datei nicht dekodieren:")
        print(e.stderr.decode())
        raise

    # Rohdaten zu float32 Array konvertieren und in Stereo umwandeln
    samples = np.frombuffer(out, dtype=np.float32).copy().reshape(-1, 2)
    samples *= float(volume)  # <- hier sicher konvertieren
    samples = np.clip(samples, -1.0, 1.0)
    try:
        with sd.OutputStream(
            samplerate=48000,
            device=index,
            channels=2,
            dtype='float32',
            latency='low',
            extra_settings=sd.WasapiSettings(auto_convert=True)
        ) as stream:
            blocksize = 1024
            for start in range(0, len(samples), blocksize):
                chunk = samples[start:start + blocksize]
                stream.write(chunk)
    except Exception as e:
        print(f"[Fehler] beim Abspielen: {e}")
        raise
    
    # Mikrofon aufnehmen und auf VB-Cable Output (Input-Gerät) ausgeben (Mischen kannst du noch erweitern)
def mix_mic_and_playback(mic_device, vb_cable_input_device, stop_event):
    samplerate = int(sd.query_devices(mic_device)['default_samplerate'])
    blocksize = 1024

    mic_channels = sd.query_devices(mic_device)['max_input_channels']
    out_channels = sd.query_devices(vb_cable_input_device)['max_output_channels']

    channels = min(mic_channels, out_channels, 2)
    if channels < 1:
        raise RuntimeError(f"Zielgerät unterstützt keine {channels} Kanäle.")

    with sd.InputStream(device=mic_device, channels=channels, samplerate=samplerate, blocksize=blocksize) as mic_stream, \
         sd.OutputStream(device=vb_cable_input_device, channels=channels, samplerate=samplerate, blocksize=blocksize) as out_stream:

        print("Mix läuft... wird gestoppt, wenn der Sound fertig ist.")
        while not stop_event.is_set():
            mic_data, _ = mic_stream.read(blocksize)
            out_stream.write(mic_data)
        print("Mix beendet.")


def main(path,volume,id):
    devices=""
    for i, api in enumerate(sd.query_hostapis()):
        if(api['name']=='Windows WASAPI'):
            devices =sd.query_hostapis(i)
            break

    devicesin=devices['devices']

  
    vb_cable_input = None    # virtuelles Ausgabegerät (Output) - max_output_channels > 0
    vb_cable_output = None   # virtuelles Eingabegerät (Input) - max_input_channels > 0
    mic_device = None
    all_devices = sd.query_devices()  # Alle Geräte mit Details
    #print(all_devices)
    for i in devicesin:
        if 'CABLE Input' in all_devices[i]['name']:
            vb_cable_input = i
        if 'CABLE Output' in all_devices[i]['name']:
            vb_cable_output = i
        if 'Mikrofon' in all_devices[i]['name']:
            mic_device = i

    #vb_cable_input=17
    #vb_cable_output=19
    #mic_device=20
    if None in (vb_cable_input, vb_cable_output, mic_device):
        print("Nicht alle benötigten Geräte gefunden!")
        return
    
    print(f"VB Cable Input (Output-Gerät): {vb_cable_input}")
    print(f"VB Cable Output (Input-Gerät): {vb_cable_output}")
    print(f"Mikrofon Device: {mic_device}")

    old_default = get_default_device_id()
    print(f"Aktuelles Standardgerät: {old_default}")

    # Setze Standardausgabegerät auf VB Cable Input (damit Sound vom System dahin geht)
    set_default_audio_device(id)

    # Starte MP3-Playback im Thread (ausgegeben auf VB Cable Input)
    sound_file = path
    stop_event = threading.Event()

    def play_and_signal_done():
        play_with_volume_clipping_safe(sound_file, vb_cable_input,float(volume)/100)
        stop_event.set()  # Signal zum Beenden der Mikrofonaufnahme

    t = threading.Thread(target=play_and_signal_done)
    t.start()
    

    mix_mic_and_playback(mic_device, vb_cable_input, stop_event)

    t.join()
    # Standardgerät wieder zurücksetzen
    set_default_audio_device(old_default)
    print("Standardgerät zurückgesetzt.")

if __name__ == "__main__":
    if len(sys.argv)>1 and len(sys.argv)<=4:
        main(sys.argv[1], sys.argv[2],sys.argv[3])

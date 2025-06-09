param(
    [Parameter(Mandatory=$true)]
    [string]$AudioFile,
    [string]$vol,
    [string]$deviceId
)

Import-Module AudioDeviceCmdlets

$virtualMicName = "CABLE Output (VB-Audio Virtual Cable)"  # ggf. anpassen
$defaultRecording = Get-AudioDevice -Recording
Write-Host "Device ID: $deviceId" 
Write-Host "Device ID: $vol" 
Write-Host "Setze Standardaufnahmegerät auf virtuelles Mikrofon: $virtualMicName"
Set-AudioDevice -Id $deviceId | Out-Null
Write-Host "Virtuelles Mikrofon ist bereits Standardaufnahmegerät"


$convertedFile = Join-Path (Split-Path $AudioFile) "output.wav"

if (-not (Test-Path $convertedFile) -or ((Get-Item $AudioFile).LastWriteTime -gt (Get-Item $convertedFile).LastWriteTime)) {
    Write-Host "Konvertiere $AudioFile nach $convertedFile ..."
    $ffmpegArgs = @(
        '-y',
        '-i', $AudioFile,
        '-acodec', 'pcm_f32le',
        '-ac', '2',
        '-ar', '48000',
        $convertedFile
    )
    Start-Process -FilePath 'ffmpeg.exe' -ArgumentList $ffmpegArgs -NoNewWindow -Wait
} else {
    Write-Host "Verwende vorhandene konvertierte Datei: $convertedFile"
}
$convertedFile = Join-Path $PSScriptRoot "..\sounds\output.wav"
$convertedFile = (Resolve-Path $convertedFile).Path
Write-Host "Spiele Sounddatei: $convertedFile"
$wavplayPath = Join-Path $PSScriptRoot "wavplay.exe"
Start-Process -NoNewWindow -Wait -FilePath $wavplayPath -ArgumentList $convertedFile, '"CABLE Input (VB-Audio Virtual Cable)"'

# Original Geräte zurücksetzen
Set-AudioDevice -Id $defaultRecording.Id | Out-Null

Write-Host "Sound fertig."

param(
    [Parameter(Mandatory=$true)]
    [string]$AudioFile,
    [string]$vol
)

Import-Module AudioDeviceCmdlets

echo "HHHH"

# Alle Audio-Geräte einmal holen
$allDevices = Get-AudioDevice -List

# Geräte filtern
$virtualMicName = "CABLE Output (VB-Audio Virtual Cable)"  # Bitte ggf. anpassen

$virtualOut = $allDevices | Where-Object { $_.Type -eq 'Playback' -and $_.Name -like "*CABLE Input*VB-Audio*" }
#$outstd = $allDevices | Where-Object { $_.Type -eq 'Playback' -and $_.Default }
$originalDevice = $allDevices | Where-Object { $_.Type -eq 'Recording' -and $_.Default }

Write-Host "Original Standardaufnahmegerät: $($originalDevice.Name)"

# Virtuelles Mikrofon holen
$virtualMicId = ($allDevices | Where-Object { $_.Name -eq $virtualMicName }).ID

Write-Host "Standardaufnahmegerät gesetzt auf virtuelles Mikrofon: $virtualMicName"
Set-AudioDevice -Id $virtualMicId

# ffplay prüfen
$ffplay = "ffplay.exe"
if (-not (Get-Command $ffplay -ErrorAction SilentlyContinue)) {
    Write-Error "ffplay ist nicht im PATH oder nicht installiert."
    exit 1
}

#Set-AudioDevice -Id $virtualOut.Id

Write-Host "Spiele Sounddatei: $AudioFile"
#Start-Process -NoNewWindow -Wait -FilePath $ffplay -ArgumentList "-nodisp", "-autoexit", "-volume", $vol, "`"$AudioFile`""
Start-Process -NoNewWindow -Wait -FilePath "wavplay.exe" -ArgumentList '.\sounds\output.wav', '"CABLE Input (VB-Audio Virtual Cable)"'

# Original Geräte zurücksetzen
#Set-AudioDevice -Id $outstd.Id
Set-AudioDevice -Id $originalDevice.Id

Write-Host "Standardaufnahmegerät zurückgesetzt auf: $($originalDevice.Name)"
Write-Host "Sound fertig."
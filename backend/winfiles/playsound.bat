@echo off
:: Name der Audiodatei abfrage
:: Skriptverzeichnis setzen (Pfad anpassen, falls nötig)
set scriptpath=%~dp0PlaySoundVirtualMic.ps1

:: PowerShell-Skript ausführen
powershell -ExecutionPolicy Bypass -File "%scriptpath%" -AudioFile "%~1" -vol "%~2" -deviceId "%~3"

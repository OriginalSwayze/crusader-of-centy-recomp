@echo off
title Crusader of Centy Recomp builder
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\builder.ps1" %*

@echo off
rem Python for tools (PyPy: a plain executable; the Store python alias hangs inside Codex sandboxes)
if defined KEGG_PY ("%KEGG_PY%" %*) else ("C:\Users\jiriv\AppData\Local\Microsoft\WinGet\Packages\PyPy.PyPy.3.11_Microsoft.Winget.Source_8wekyb3d8bbwe\pypy3.11-v7.3.20-win64\pypy.exe" %*)

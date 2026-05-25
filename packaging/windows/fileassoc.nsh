; File association registration for .pfkr files.
; Included by the NSIS installer at install time.
; Plain NSIS syntax — no CMake escaping required.
WriteRegStr HKCR ".pfkr" "" "PartialFKR.Project"
WriteRegStr HKCR "PartialFKR.Project" "" "PartialFKR Project"
WriteRegStr HKCR "PartialFKR.Project\DefaultIcon" "" "$INSTDIR\bin\PartialFKR.exe,0"
WriteRegStr HKCR "PartialFKR.Project\shell" "" ""
WriteRegStr HKCR "PartialFKR.Project\shell\open" "" ""
WriteRegStr HKCR "PartialFKR.Project\shell\open\command" "" '"$INSTDIR\bin\PartialFKR.exe" "%1"'
System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
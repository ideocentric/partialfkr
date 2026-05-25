; File association cleanup for .pfkr files.
; Included by the NSIS uninstaller at uninstall time.
DeleteRegKey HKCR ".pfkr"
DeleteRegKey HKCR "PartialFKR.Project"
"{Windows7SDKToolsPath}UICC.exe" "{XmlFilename}" "{BmlFilename}" /res:"{RcFilename}"
"{Windows7SDKToolsPath}rc.exe" /v "{RcFilename}"
cmd /c "("%VS100COMNTOOLS%..\..\VC\bin\vcvars32.bat") && ("%VS100COMNTOOLS%..\..\VC\bin\link.exe" /VERBOSE /NOENTRY /DLL /OUT:"{DllFilename}" "{ResFilename}")"
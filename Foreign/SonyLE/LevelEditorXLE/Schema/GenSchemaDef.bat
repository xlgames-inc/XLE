@set domgen=..\..\..\wws_atf\DevTools\DomGen\bin\DomGen.exe
@IF NOT EXIST %domgen% ( set domgen=..\..\ATF\DevTools\DomGen\DomGen.exe)

%domgen% xleroot.xsd Schema.cs "gap" LevelEditorXLE

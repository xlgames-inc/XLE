//****************************************************************************
//
//  File:       RibbonCOMGuids.cs
//
//  Contents:   Guids of the interfaces and classes related to Windows Ribbon 
//              Framework, based on UIRibbon.idl from windows 7 SDK
//
//****************************************************************************

namespace RibbonLib.Interop
{
    public static class RibbonIIDGuid
    {
        // IID GUID strings for relevant Ribbon COM interfaces.
        public const string IUISimplePropertySet = "c205bb48-5b1c-4219-a106-15bd0a5f24e2";
        public const string IUIRibbon = "803982ab-370a-4f7e-a9e7-8784036a6e26";
        public const string IUIFramework = "F4F0385D-6872-43a8-AD09-4C339CB3F5C5";
        public const string IUIContextualUI = "EEA11F37-7C46-437c-8E55-B52122B29293";
        public const string IUICollection = "DF4F45BF-6F9D-4dd7-9D68-D8F9CD18C4DB";
        public const string IUICollectionChangedEvent = "6502AE91-A14D-44b5-BBD0-62AACC581D52";
        public const string IUICommandHandler = "75ae0a2d-dc03-4c9f-8883-069660d0beb6";
        public const string IUIApplication = "D428903C-729A-491d-910D-682A08FF2522";
        public const string IUIImage = "23c8c838-4de6-436b-ab01-5554bb7c30dd";
        public const string IUIImageFromBitmap = "18aba7f3-4c1c-4ba2-bf6c-f5c3326fa816";
    }

    public static class RibbonCLSIDGuid
    {
        // CLSID GUID strings for relevant coclasses.
        public const string UIRibbonFramework = "926749fa-2615-4987-8845-c33e65f2b957";
        public const string UIRibbonImageFromBitmapFactory = "0F7434B6-59B6-4250-999E-D168D6AE4293";
    }

}

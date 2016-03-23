// -------------------------------------------------------------------------------------------------------------------
// Generated code, do not edit
// Command Line:  DomGen "level_editor.xsd" "Schema.cs" "gap" "LevelEditor"
// -------------------------------------------------------------------------------------------------------------------

using System;
using System.Collections.Generic;

using Sce.Atf.Dom;

namespace LevelEditor
{
    public static class Schema
    {
        public const string NS = "gap";

        public static void Initialize(XmlSchemaTypeCollection typeCollection)
        {
            Initialize((ns,name)=>typeCollection.GetNodeType(ns,name),
                (ns,name)=>typeCollection.GetRootElement(ns,name));
        }

        public static void Initialize(IDictionary<string, XmlSchemaTypeCollection> typeCollections)
        {
            Initialize((ns,name)=>typeCollections[ns].GetNodeType(name),
                (ns,name)=>typeCollections[ns].GetRootElement(name));
        }

        private static void Initialize(Func<string, string, DomNodeType> getNodeType, Func<string, string, ChildInfo> getRootElement)
        {
            gameType.Type = getNodeType("gap", "gameType");
            gameType.nameAttribute = gameType.Type.GetAttributeInfo("name");
            gameType.gameObjectFolderChild = gameType.Type.GetChildInfo("gameObjectFolder");
            gameType.layersChild = gameType.Type.GetChildInfo("layers");
            gameType.bookmarksChild = gameType.Type.GetChildInfo("bookmarks");
            gameType.gameReferenceChild = gameType.Type.GetChildInfo("gameReference");
            gameType.gridChild = gameType.Type.GetChildInfo("grid");

            gameObjectFolderType.Type = getNodeType("gap", "gameObjectFolderType");
            gameObjectFolderType.nameAttribute = gameObjectFolderType.Type.GetAttributeInfo("name");
            gameObjectFolderType.visibleAttribute = gameObjectFolderType.Type.GetAttributeInfo("visible");
            gameObjectFolderType.lockedAttribute = gameObjectFolderType.Type.GetAttributeInfo("locked");
            gameObjectFolderType.gameObjectChild = gameObjectFolderType.Type.GetChildInfo("gameObject");
            gameObjectFolderType.folderChild = gameObjectFolderType.Type.GetChildInfo("folder");

            gameObjectType.Type = getNodeType("gap", "gameObjectType");
            gameObjectType.transformAttribute = gameObjectType.Type.GetAttributeInfo("transform");
            gameObjectType.translateAttribute = gameObjectType.Type.GetAttributeInfo("translate");
            gameObjectType.rotateAttribute = gameObjectType.Type.GetAttributeInfo("rotate");
            gameObjectType.scaleAttribute = gameObjectType.Type.GetAttributeInfo("scale");
            gameObjectType.pivotAttribute = gameObjectType.Type.GetAttributeInfo("pivot");
            gameObjectType.transformationTypeAttribute = gameObjectType.Type.GetAttributeInfo("transformationType");
            gameObjectType.nameAttribute = gameObjectType.Type.GetAttributeInfo("name");
            gameObjectType.visibleAttribute = gameObjectType.Type.GetAttributeInfo("visible");
            gameObjectType.lockedAttribute = gameObjectType.Type.GetAttributeInfo("locked");

            transformObjectType.Type = getNodeType("gap", "transformObjectType");
            transformObjectType.transformAttribute = transformObjectType.Type.GetAttributeInfo("transform");
            transformObjectType.translateAttribute = transformObjectType.Type.GetAttributeInfo("translate");
            transformObjectType.rotateAttribute = transformObjectType.Type.GetAttributeInfo("rotate");
            transformObjectType.scaleAttribute = transformObjectType.Type.GetAttributeInfo("scale");
            transformObjectType.pivotAttribute = transformObjectType.Type.GetAttributeInfo("pivot");
            transformObjectType.transformationTypeAttribute = transformObjectType.Type.GetAttributeInfo("transformationType");

            layersType.Type = getNodeType("gap", "layersType");
            layersType.layerChild = layersType.Type.GetChildInfo("layer");

            layerType.Type = getNodeType("gap", "layerType");
            layerType.nameAttribute = layerType.Type.GetAttributeInfo("name");
            layerType.gameObjectReferenceChild = layerType.Type.GetChildInfo("gameObjectReference");
            layerType.layerChild = layerType.Type.GetChildInfo("layer");

            gameObjectReferenceType.Type = getNodeType("gap", "gameObjectReferenceType");
            gameObjectReferenceType.refAttribute = gameObjectReferenceType.Type.GetAttributeInfo("ref");

            bookmarksType.Type = getNodeType("gap", "bookmarksType");
            bookmarksType.bookmarkChild = bookmarksType.Type.GetChildInfo("bookmark");

            bookmarkType.Type = getNodeType("gap", "bookmarkType");
            bookmarkType.nameAttribute = bookmarkType.Type.GetAttributeInfo("name");
            bookmarkType.cameraChild = bookmarkType.Type.GetChildInfo("camera");
            bookmarkType.bookmarkChild = bookmarkType.Type.GetChildInfo("bookmark");

            cameraType.Type = getNodeType("gap", "cameraType");
            cameraType.eyeAttribute = cameraType.Type.GetAttributeInfo("eye");
            cameraType.lookAtPointAttribute = cameraType.Type.GetAttributeInfo("lookAtPoint");
            cameraType.upVectorAttribute = cameraType.Type.GetAttributeInfo("upVector");
            cameraType.viewTypeAttribute = cameraType.Type.GetAttributeInfo("viewType");
            cameraType.yFovAttribute = cameraType.Type.GetAttributeInfo("yFov");
            cameraType.nearZAttribute = cameraType.Type.GetAttributeInfo("nearZ");
            cameraType.farZAttribute = cameraType.Type.GetAttributeInfo("farZ");
            cameraType.focusRadiusAttribute = cameraType.Type.GetAttributeInfo("focusRadius");

            gameReferenceType.Type = getNodeType("gap", "gameReferenceType");
            gameReferenceType.nameAttribute = gameReferenceType.Type.GetAttributeInfo("name");
            gameReferenceType.refAttribute = gameReferenceType.Type.GetAttributeInfo("ref");
            gameReferenceType.tagsAttribute = gameReferenceType.Type.GetAttributeInfo("tags");

            gridType.Type = getNodeType("gap", "gridType");
            gridType.sizeAttribute = gridType.Type.GetAttributeInfo("size");
            gridType.subdivisionsAttribute = gridType.Type.GetAttributeInfo("subdivisions");
            gridType.heightAttribute = gridType.Type.GetAttributeInfo("height");
            gridType.snapAttribute = gridType.Type.GetAttributeInfo("snap");
            gridType.visibleAttribute = gridType.Type.GetAttributeInfo("visible");

            prototypeType.Type = getNodeType("gap", "prototypeType");
            prototypeType.gameObjectChild = prototypeType.Type.GetChildInfo("gameObject");

            prefabType.Type = getNodeType("gap", "prefabType");
            prefabType.gameObjectChild = prefabType.Type.GetChildInfo("gameObject");

            textureMetadataType.Type = getNodeType("gap", "textureMetadataType");
            textureMetadataType.uriAttribute = textureMetadataType.Type.GetAttributeInfo("uri");
            textureMetadataType.keywordsAttribute = textureMetadataType.Type.GetAttributeInfo("keywords");
            textureMetadataType.compressionSettingAttribute = textureMetadataType.Type.GetAttributeInfo("compressionSetting");
            textureMetadataType.memoryLayoutAttribute = textureMetadataType.Type.GetAttributeInfo("memoryLayout");
            textureMetadataType.mipMapAttribute = textureMetadataType.Type.GetAttributeInfo("mipMap");
            textureMetadataType.colorSpaceAttribute = textureMetadataType.Type.GetAttributeInfo("colorSpace");

            resourceMetadataType.Type = getNodeType("gap", "resourceMetadataType");
            resourceMetadataType.uriAttribute = resourceMetadataType.Type.GetAttributeInfo("uri");
            resourceMetadataType.keywordsAttribute = resourceMetadataType.Type.GetAttributeInfo("keywords");

            resourceReferenceType.Type = getNodeType("gap", "resourceReferenceType");
            resourceReferenceType.uriAttribute = resourceReferenceType.Type.GetAttributeInfo("uri");

            visibleTransformObjectType.Type = getNodeType("gap", "visibleTransformObjectType");
            visibleTransformObjectType.transformAttribute = visibleTransformObjectType.Type.GetAttributeInfo("transform");
            visibleTransformObjectType.translateAttribute = visibleTransformObjectType.Type.GetAttributeInfo("translate");
            visibleTransformObjectType.rotateAttribute = visibleTransformObjectType.Type.GetAttributeInfo("rotate");
            visibleTransformObjectType.scaleAttribute = visibleTransformObjectType.Type.GetAttributeInfo("scale");
            visibleTransformObjectType.pivotAttribute = visibleTransformObjectType.Type.GetAttributeInfo("pivot");
            visibleTransformObjectType.transformationTypeAttribute = visibleTransformObjectType.Type.GetAttributeInfo("transformationType");
            visibleTransformObjectType.visibleAttribute = visibleTransformObjectType.Type.GetAttributeInfo("visible");
            visibleTransformObjectType.lockedAttribute = visibleTransformObjectType.Type.GetAttributeInfo("locked");

            transformObjectGroupType.Type = getNodeType("gap", "transformObjectGroupType");
            transformObjectGroupType.transformAttribute = transformObjectGroupType.Type.GetAttributeInfo("transform");
            transformObjectGroupType.translateAttribute = transformObjectGroupType.Type.GetAttributeInfo("translate");
            transformObjectGroupType.rotateAttribute = transformObjectGroupType.Type.GetAttributeInfo("rotate");
            transformObjectGroupType.scaleAttribute = transformObjectGroupType.Type.GetAttributeInfo("scale");
            transformObjectGroupType.pivotAttribute = transformObjectGroupType.Type.GetAttributeInfo("pivot");
            transformObjectGroupType.transformationTypeAttribute = transformObjectGroupType.Type.GetAttributeInfo("transformationType");
            transformObjectGroupType.visibleAttribute = transformObjectGroupType.Type.GetAttributeInfo("visible");
            transformObjectGroupType.lockedAttribute = transformObjectGroupType.Type.GetAttributeInfo("locked");
            transformObjectGroupType.objectChild = transformObjectGroupType.Type.GetChildInfo("object");

            gameObjectComponentType.Type = getNodeType("gap", "gameObjectComponentType");
            gameObjectComponentType.nameAttribute = gameObjectComponentType.Type.GetAttributeInfo("name");
            gameObjectComponentType.activeAttribute = gameObjectComponentType.Type.GetAttributeInfo("active");

            transformComponentType.Type = getNodeType("gap", "transformComponentType");
            transformComponentType.nameAttribute = transformComponentType.Type.GetAttributeInfo("name");
            transformComponentType.activeAttribute = transformComponentType.Type.GetAttributeInfo("active");
            transformComponentType.translationAttribute = transformComponentType.Type.GetAttributeInfo("translation");
            transformComponentType.rotationAttribute = transformComponentType.Type.GetAttributeInfo("rotation");
            transformComponentType.scaleAttribute = transformComponentType.Type.GetAttributeInfo("scale");

            gameObjectWithComponentType.Type = getNodeType("gap", "gameObjectWithComponentType");
            gameObjectWithComponentType.transformAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("transform");
            gameObjectWithComponentType.translateAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("translate");
            gameObjectWithComponentType.rotateAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("rotate");
            gameObjectWithComponentType.scaleAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("scale");
            gameObjectWithComponentType.pivotAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("pivot");
            gameObjectWithComponentType.transformationTypeAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("transformationType");
            gameObjectWithComponentType.nameAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("name");
            gameObjectWithComponentType.visibleAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("visible");
            gameObjectWithComponentType.lockedAttribute = gameObjectWithComponentType.Type.GetAttributeInfo("locked");
            gameObjectWithComponentType.componentChild = gameObjectWithComponentType.Type.GetChildInfo("component");

            objectOverrideType.Type = getNodeType("gap", "objectOverrideType");
            objectOverrideType.objectNameAttribute = objectOverrideType.Type.GetAttributeInfo("objectName");
            objectOverrideType.attributeOverrideChild = objectOverrideType.Type.GetChildInfo("attributeOverride");

            attributeOverrideType.Type = getNodeType("gap", "attributeOverrideType");
            attributeOverrideType.nameAttribute = attributeOverrideType.Type.GetAttributeInfo("name");
            attributeOverrideType.valueAttribute = attributeOverrideType.Type.GetAttributeInfo("value");

            prefabInstanceType.Type = getNodeType("gap", "prefabInstanceType");
            prefabInstanceType.transformAttribute = prefabInstanceType.Type.GetAttributeInfo("transform");
            prefabInstanceType.translateAttribute = prefabInstanceType.Type.GetAttributeInfo("translate");
            prefabInstanceType.rotateAttribute = prefabInstanceType.Type.GetAttributeInfo("rotate");
            prefabInstanceType.scaleAttribute = prefabInstanceType.Type.GetAttributeInfo("scale");
            prefabInstanceType.pivotAttribute = prefabInstanceType.Type.GetAttributeInfo("pivot");
            prefabInstanceType.transformationTypeAttribute = prefabInstanceType.Type.GetAttributeInfo("transformationType");
            prefabInstanceType.visibleAttribute = prefabInstanceType.Type.GetAttributeInfo("visible");
            prefabInstanceType.lockedAttribute = prefabInstanceType.Type.GetAttributeInfo("locked");
            prefabInstanceType.prefabRefAttribute = prefabInstanceType.Type.GetAttributeInfo("prefabRef");
            prefabInstanceType.objectChild = prefabInstanceType.Type.GetChildInfo("object");
            prefabInstanceType.objectOverrideChild = prefabInstanceType.Type.GetChildInfo("objectOverride");

            renderComponentType.Type = getNodeType("gap", "renderComponentType");
            renderComponentType.nameAttribute = renderComponentType.Type.GetAttributeInfo("name");
            renderComponentType.activeAttribute = renderComponentType.Type.GetAttributeInfo("active");
            renderComponentType.translationAttribute = renderComponentType.Type.GetAttributeInfo("translation");
            renderComponentType.rotationAttribute = renderComponentType.Type.GetAttributeInfo("rotation");
            renderComponentType.scaleAttribute = renderComponentType.Type.GetAttributeInfo("scale");
            renderComponentType.visibleAttribute = renderComponentType.Type.GetAttributeInfo("visible");
            renderComponentType.castShadowAttribute = renderComponentType.Type.GetAttributeInfo("castShadow");
            renderComponentType.receiveShadowAttribute = renderComponentType.Type.GetAttributeInfo("receiveShadow");
            renderComponentType.drawDistanceAttribute = renderComponentType.Type.GetAttributeInfo("drawDistance");

            meshComponentType.Type = getNodeType("gap", "meshComponentType");
            meshComponentType.nameAttribute = meshComponentType.Type.GetAttributeInfo("name");
            meshComponentType.activeAttribute = meshComponentType.Type.GetAttributeInfo("active");
            meshComponentType.translationAttribute = meshComponentType.Type.GetAttributeInfo("translation");
            meshComponentType.rotationAttribute = meshComponentType.Type.GetAttributeInfo("rotation");
            meshComponentType.scaleAttribute = meshComponentType.Type.GetAttributeInfo("scale");
            meshComponentType.visibleAttribute = meshComponentType.Type.GetAttributeInfo("visible");
            meshComponentType.castShadowAttribute = meshComponentType.Type.GetAttributeInfo("castShadow");
            meshComponentType.receiveShadowAttribute = meshComponentType.Type.GetAttributeInfo("receiveShadow");
            meshComponentType.drawDistanceAttribute = meshComponentType.Type.GetAttributeInfo("drawDistance");
            meshComponentType.refAttribute = meshComponentType.Type.GetAttributeInfo("ref");

            spinnerComponentType.Type = getNodeType("gap", "spinnerComponentType");
            spinnerComponentType.nameAttribute = spinnerComponentType.Type.GetAttributeInfo("name");
            spinnerComponentType.activeAttribute = spinnerComponentType.Type.GetAttributeInfo("active");
            spinnerComponentType.rpsAttribute = spinnerComponentType.Type.GetAttributeInfo("rps");

            modelReferenceType.Type = getNodeType("gap", "modelReferenceType");
            modelReferenceType.uriAttribute = modelReferenceType.Type.GetAttributeInfo("uri");
            modelReferenceType.tagAttribute = modelReferenceType.Type.GetAttributeInfo("tag");

            locatorType.Type = getNodeType("gap", "locatorType");
            locatorType.transformAttribute = locatorType.Type.GetAttributeInfo("transform");
            locatorType.translateAttribute = locatorType.Type.GetAttributeInfo("translate");
            locatorType.rotateAttribute = locatorType.Type.GetAttributeInfo("rotate");
            locatorType.scaleAttribute = locatorType.Type.GetAttributeInfo("scale");
            locatorType.pivotAttribute = locatorType.Type.GetAttributeInfo("pivot");
            locatorType.transformationTypeAttribute = locatorType.Type.GetAttributeInfo("transformationType");
            locatorType.nameAttribute = locatorType.Type.GetAttributeInfo("name");
            locatorType.visibleAttribute = locatorType.Type.GetAttributeInfo("visible");
            locatorType.lockedAttribute = locatorType.Type.GetAttributeInfo("locked");
            locatorType.resourceChild = locatorType.Type.GetChildInfo("resource");
            locatorType.stmRefChild = locatorType.Type.GetChildInfo("stmRef");

            stateMachineRefType.Type = getNodeType("gap", "stateMachineRefType");
            stateMachineRefType.uriAttribute = stateMachineRefType.Type.GetAttributeInfo("uri");
            stateMachineRefType.flatPropertyTableChild = stateMachineRefType.Type.GetChildInfo("flatPropertyTable");

            flatPropertyTableType.Type = getNodeType("gap", "flatPropertyTableType");
            flatPropertyTableType.propertyChild = flatPropertyTableType.Type.GetChildInfo("property");

            propertyType.Type = getNodeType("gap", "propertyType");
            propertyType.scopeAttribute = propertyType.Type.GetAttributeInfo("scope");
            propertyType.typeAttribute = propertyType.Type.GetAttributeInfo("type");
            propertyType.absolutePathAttribute = propertyType.Type.GetAttributeInfo("absolutePath");
            propertyType.propertyNameAttribute = propertyType.Type.GetAttributeInfo("propertyName");
            propertyType.defaultValueAttribute = propertyType.Type.GetAttributeInfo("defaultValue");
            propertyType.valueAttribute = propertyType.Type.GetAttributeInfo("value");
            propertyType.minValueAttribute = propertyType.Type.GetAttributeInfo("minValue");
            propertyType.maxValueAttribute = propertyType.Type.GetAttributeInfo("maxValue");
            propertyType.descriptionAttribute = propertyType.Type.GetAttributeInfo("description");
            propertyType.categoryAttribute = propertyType.Type.GetAttributeInfo("category");
            propertyType.warningAttribute = propertyType.Type.GetAttributeInfo("warning");

            controlPointType.Type = getNodeType("gap", "controlPointType");
            controlPointType.transformAttribute = controlPointType.Type.GetAttributeInfo("transform");
            controlPointType.translateAttribute = controlPointType.Type.GetAttributeInfo("translate");
            controlPointType.rotateAttribute = controlPointType.Type.GetAttributeInfo("rotate");
            controlPointType.scaleAttribute = controlPointType.Type.GetAttributeInfo("scale");
            controlPointType.pivotAttribute = controlPointType.Type.GetAttributeInfo("pivot");
            controlPointType.transformationTypeAttribute = controlPointType.Type.GetAttributeInfo("transformationType");
            controlPointType.nameAttribute = controlPointType.Type.GetAttributeInfo("name");
            controlPointType.visibleAttribute = controlPointType.Type.GetAttributeInfo("visible");
            controlPointType.lockedAttribute = controlPointType.Type.GetAttributeInfo("locked");

            curveType.Type = getNodeType("gap", "curveType");
            curveType.transformAttribute = curveType.Type.GetAttributeInfo("transform");
            curveType.translateAttribute = curveType.Type.GetAttributeInfo("translate");
            curveType.rotateAttribute = curveType.Type.GetAttributeInfo("rotate");
            curveType.scaleAttribute = curveType.Type.GetAttributeInfo("scale");
            curveType.pivotAttribute = curveType.Type.GetAttributeInfo("pivot");
            curveType.transformationTypeAttribute = curveType.Type.GetAttributeInfo("transformationType");
            curveType.nameAttribute = curveType.Type.GetAttributeInfo("name");
            curveType.visibleAttribute = curveType.Type.GetAttributeInfo("visible");
            curveType.lockedAttribute = curveType.Type.GetAttributeInfo("locked");
            curveType.colorAttribute = curveType.Type.GetAttributeInfo("color");
            curveType.isClosedAttribute = curveType.Type.GetAttributeInfo("isClosed");
            curveType.stepsAttribute = curveType.Type.GetAttributeInfo("steps");
            curveType.interpolationTypeAttribute = curveType.Type.GetAttributeInfo("interpolationType");
            curveType.pointChild = curveType.Type.GetChildInfo("point");

            catmullRomType.Type = getNodeType("gap", "catmullRomType");
            catmullRomType.transformAttribute = catmullRomType.Type.GetAttributeInfo("transform");
            catmullRomType.translateAttribute = catmullRomType.Type.GetAttributeInfo("translate");
            catmullRomType.rotateAttribute = catmullRomType.Type.GetAttributeInfo("rotate");
            catmullRomType.scaleAttribute = catmullRomType.Type.GetAttributeInfo("scale");
            catmullRomType.pivotAttribute = catmullRomType.Type.GetAttributeInfo("pivot");
            catmullRomType.transformationTypeAttribute = catmullRomType.Type.GetAttributeInfo("transformationType");
            catmullRomType.nameAttribute = catmullRomType.Type.GetAttributeInfo("name");
            catmullRomType.visibleAttribute = catmullRomType.Type.GetAttributeInfo("visible");
            catmullRomType.lockedAttribute = catmullRomType.Type.GetAttributeInfo("locked");
            catmullRomType.colorAttribute = catmullRomType.Type.GetAttributeInfo("color");
            catmullRomType.isClosedAttribute = catmullRomType.Type.GetAttributeInfo("isClosed");
            catmullRomType.stepsAttribute = catmullRomType.Type.GetAttributeInfo("steps");
            catmullRomType.interpolationTypeAttribute = catmullRomType.Type.GetAttributeInfo("interpolationType");
            catmullRomType.pointChild = catmullRomType.Type.GetChildInfo("point");

            bezierType.Type = getNodeType("gap", "bezierType");
            bezierType.transformAttribute = bezierType.Type.GetAttributeInfo("transform");
            bezierType.translateAttribute = bezierType.Type.GetAttributeInfo("translate");
            bezierType.rotateAttribute = bezierType.Type.GetAttributeInfo("rotate");
            bezierType.scaleAttribute = bezierType.Type.GetAttributeInfo("scale");
            bezierType.pivotAttribute = bezierType.Type.GetAttributeInfo("pivot");
            bezierType.transformationTypeAttribute = bezierType.Type.GetAttributeInfo("transformationType");
            bezierType.nameAttribute = bezierType.Type.GetAttributeInfo("name");
            bezierType.visibleAttribute = bezierType.Type.GetAttributeInfo("visible");
            bezierType.lockedAttribute = bezierType.Type.GetAttributeInfo("locked");
            bezierType.colorAttribute = bezierType.Type.GetAttributeInfo("color");
            bezierType.isClosedAttribute = bezierType.Type.GetAttributeInfo("isClosed");
            bezierType.stepsAttribute = bezierType.Type.GetAttributeInfo("steps");
            bezierType.interpolationTypeAttribute = bezierType.Type.GetAttributeInfo("interpolationType");
            bezierType.pointChild = bezierType.Type.GetChildInfo("point");

            gameRootElement = getRootElement(NS, "game");
            prototypeRootElement = getRootElement(NS, "prototype");
            prefabRootElement = getRootElement(NS, "prefab");
            textureMetadataRootElement = getRootElement(NS, "textureMetadata");
            resourceMetadataRootElement = getRootElement(NS, "resourceMetadata");
        }

        public static class gameType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static ChildInfo gameObjectFolderChild;
            public static ChildInfo layersChild;
            public static ChildInfo bookmarksChild;
            public static ChildInfo gameReferenceChild;
            public static ChildInfo gridChild;
        }

        public static class gameObjectFolderType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo gameObjectChild;
            public static ChildInfo folderChild;
        }

        public static class gameObjectType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
        }

        public static class transformObjectType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
        }

        public static class layersType
        {
            public static DomNodeType Type;
            public static ChildInfo layerChild;
        }

        public static class layerType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static ChildInfo gameObjectReferenceChild;
            public static ChildInfo layerChild;
        }

        public static class gameObjectReferenceType
        {
            public static DomNodeType Type;
            public static AttributeInfo refAttribute;
        }

        public static class bookmarksType
        {
            public static DomNodeType Type;
            public static ChildInfo bookmarkChild;
        }

        public static class bookmarkType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static ChildInfo cameraChild;
            public static ChildInfo bookmarkChild;
        }

        public static class cameraType
        {
            public static DomNodeType Type;
            public static AttributeInfo eyeAttribute;
            public static AttributeInfo lookAtPointAttribute;
            public static AttributeInfo upVectorAttribute;
            public static AttributeInfo viewTypeAttribute;
            public static AttributeInfo yFovAttribute;
            public static AttributeInfo nearZAttribute;
            public static AttributeInfo farZAttribute;
            public static AttributeInfo focusRadiusAttribute;
        }

        public static class gameReferenceType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo refAttribute;
            public static AttributeInfo tagsAttribute;
        }

        public static class gridType
        {
            public static DomNodeType Type;
            public static AttributeInfo sizeAttribute;
            public static AttributeInfo subdivisionsAttribute;
            public static AttributeInfo heightAttribute;
            public static AttributeInfo snapAttribute;
            public static AttributeInfo visibleAttribute;
        }

        public static class prototypeType
        {
            public static DomNodeType Type;
            public static ChildInfo gameObjectChild;
        }

        public static class prefabType
        {
            public static DomNodeType Type;
            public static ChildInfo gameObjectChild;
        }

        public static class textureMetadataType
        {
            public static DomNodeType Type;
            public static AttributeInfo uriAttribute;
            public static AttributeInfo keywordsAttribute;
            public static AttributeInfo compressionSettingAttribute;
            public static AttributeInfo memoryLayoutAttribute;
            public static AttributeInfo mipMapAttribute;
            public static AttributeInfo colorSpaceAttribute;
        }

        public static class resourceMetadataType
        {
            public static DomNodeType Type;
            public static AttributeInfo uriAttribute;
            public static AttributeInfo keywordsAttribute;
        }

        public static class resourceReferenceType
        {
            public static DomNodeType Type;
            public static AttributeInfo uriAttribute;
        }

        public static class visibleTransformObjectType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
        }

        public static class transformObjectGroupType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo objectChild;
        }

        public static class gameObjectComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
        }

        public static class transformComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
            public static AttributeInfo translationAttribute;
            public static AttributeInfo rotationAttribute;
            public static AttributeInfo scaleAttribute;
        }

        public static class gameObjectWithComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo componentChild;
        }

        public static class objectOverrideType
        {
            public static DomNodeType Type;
            public static AttributeInfo objectNameAttribute;
            public static ChildInfo attributeOverrideChild;
        }

        public static class attributeOverrideType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo valueAttribute;
        }

        public static class prefabInstanceType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo prefabRefAttribute;
            public static ChildInfo objectChild;
            public static ChildInfo objectOverrideChild;
        }

        public static class renderComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
            public static AttributeInfo translationAttribute;
            public static AttributeInfo rotationAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo castShadowAttribute;
            public static AttributeInfo receiveShadowAttribute;
            public static AttributeInfo drawDistanceAttribute;
        }

        public static class meshComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
            public static AttributeInfo translationAttribute;
            public static AttributeInfo rotationAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo castShadowAttribute;
            public static AttributeInfo receiveShadowAttribute;
            public static AttributeInfo drawDistanceAttribute;
            public static AttributeInfo refAttribute;
        }

        public static class spinnerComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
            public static AttributeInfo rpsAttribute;
        }

        public static class modelReferenceType
        {
            public static DomNodeType Type;
            public static AttributeInfo uriAttribute;
            public static AttributeInfo tagAttribute;
        }

        public static class locatorType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo resourceChild;
            public static ChildInfo stmRefChild;
        }

        public static class stateMachineRefType
        {
            public static DomNodeType Type;
            public static AttributeInfo uriAttribute;
            public static ChildInfo flatPropertyTableChild;
        }

        public static class flatPropertyTableType
        {
            public static DomNodeType Type;
            public static ChildInfo propertyChild;
        }

        public static class propertyType
        {
            public static DomNodeType Type;
            public static AttributeInfo scopeAttribute;
            public static AttributeInfo typeAttribute;
            public static AttributeInfo absolutePathAttribute;
            public static AttributeInfo propertyNameAttribute;
            public static AttributeInfo defaultValueAttribute;
            public static AttributeInfo valueAttribute;
            public static AttributeInfo minValueAttribute;
            public static AttributeInfo maxValueAttribute;
            public static AttributeInfo descriptionAttribute;
            public static AttributeInfo categoryAttribute;
            public static AttributeInfo warningAttribute;
        }

        public static class controlPointType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
        }

        public static class curveType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo pointChild;
        }

        public static class catmullRomType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo pointChild;
        }

        public static class bezierType
        {
            public static DomNodeType Type;
            public static AttributeInfo transformAttribute;
            public static AttributeInfo translateAttribute;
            public static AttributeInfo rotateAttribute;
            public static AttributeInfo scaleAttribute;
            public static AttributeInfo pivotAttribute;
            public static AttributeInfo transformationTypeAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo pointChild;
        }

        public static ChildInfo gameRootElement;

        public static ChildInfo prototypeRootElement;

        public static ChildInfo prefabRootElement;

        public static ChildInfo textureMetadataRootElement;

        public static ChildInfo resourceMetadataRootElement;
    }
}

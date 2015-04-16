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
            gameType.fogEnabledAttribute = gameType.Type.GetAttributeInfo("fogEnabled");
            gameType.fogColorAttribute = gameType.Type.GetAttributeInfo("fogColor");
            gameType.fogRangeAttribute = gameType.Type.GetAttributeInfo("fogRange");
            gameType.fogDensityAttribute = gameType.Type.GetAttributeInfo("fogDensity");
            gameType.gameObjectFolderChild = gameType.Type.GetChildInfo("gameObjectFolder");
            gameType.layersChild = gameType.Type.GetChildInfo("layers");
            gameType.bookmarksChild = gameType.Type.GetChildInfo("bookmarks");
            gameType.gameReferenceChild = gameType.Type.GetChildInfo("gameReference");
            gameType.gridChild = gameType.Type.GetChildInfo("grid");
            gameType.placementsFolderChild = gameType.Type.GetChildInfo("placements");

            gameObjectFolderType.Type = getNodeType("gap", "gameObjectFolderType");
            gameObjectFolderType.nameAttribute = gameObjectFolderType.Type.GetAttributeInfo("name");
            gameObjectFolderType.visibleAttribute = gameObjectFolderType.Type.GetAttributeInfo("visible");
            gameObjectFolderType.lockedAttribute = gameObjectFolderType.Type.GetAttributeInfo("locked");
            gameObjectFolderType.gameObjectChild = gameObjectFolderType.Type.GetChildInfo("gameObject");
            gameObjectFolderType.folderChild = gameObjectFolderType.Type.GetChildInfo("folder");

            gameObjectType.Type = getNodeType("gap", "gameObjectType");
            gameObjectType.transform = new transformAttributes(gameObjectType.Type);
            gameObjectType.nameAttribute = gameObjectType.Type.GetAttributeInfo("name");
            gameObjectType.visibleAttribute = gameObjectType.Type.GetAttributeInfo("visible");
            gameObjectType.lockedAttribute = gameObjectType.Type.GetAttributeInfo("locked");
            gameObjectType.componentChild = gameObjectType.Type.GetChildInfo("component");

            gameObjectComponentType.Type = getNodeType("gap", "gameObjectComponentType");
            gameObjectComponentType.nameAttribute = gameObjectComponentType.Type.GetAttributeInfo("name");
            gameObjectComponentType.activeAttribute = gameObjectComponentType.Type.GetAttributeInfo("active");

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

            resourceMetadataType.Type = getNodeType("gap", "resourceMetadataType");
            resourceMetadataType.uriAttribute = resourceMetadataType.Type.GetAttributeInfo("uri");
            resourceMetadataType.keywordsAttribute = resourceMetadataType.Type.GetAttributeInfo("keywords");

            resourceReferenceType.Type = getNodeType("gap", "resourceReferenceType");
            resourceReferenceType.uriAttribute = resourceReferenceType.Type.GetAttributeInfo("uri");

            transformComponentType.Type = getNodeType("gap", "transformComponentType");
            transformComponentType.nameAttribute = transformComponentType.Type.GetAttributeInfo("name");
            transformComponentType.activeAttribute = transformComponentType.Type.GetAttributeInfo("active");
            transformComponentType.translationAttribute = transformComponentType.Type.GetAttributeInfo("translation");
            transformComponentType.rotationAttribute = transformComponentType.Type.GetAttributeInfo("rotation");
            transformComponentType.scaleAttribute = transformComponentType.Type.GetAttributeInfo("scale");

            gameObjectGroupType.Type = getNodeType("gap", "gameObjectGroupType");
            gameObjectGroupType.transform = new transformAttributes(gameObjectGroupType.Type);
            gameObjectGroupType.nameAttribute = gameObjectGroupType.Type.GetAttributeInfo("name");
            gameObjectGroupType.visibleAttribute = gameObjectGroupType.Type.GetAttributeInfo("visible");
            gameObjectGroupType.lockedAttribute = gameObjectGroupType.Type.GetAttributeInfo("locked");
            gameObjectGroupType.componentChild = gameObjectGroupType.Type.GetChildInfo("component");
            gameObjectGroupType.gameObjectChild = gameObjectGroupType.Type.GetChildInfo("gameObject");

            objectOverrideType.Type = getNodeType("gap", "objectOverrideType");
            objectOverrideType.objectNameAttribute = objectOverrideType.Type.GetAttributeInfo("objectName");
            objectOverrideType.attributeOverrideChild = objectOverrideType.Type.GetChildInfo("attributeOverride");

            attributeOverrideType.Type = getNodeType("gap", "attributeOverrideType");
            attributeOverrideType.nameAttribute = attributeOverrideType.Type.GetAttributeInfo("name");
            attributeOverrideType.valueAttribute = attributeOverrideType.Type.GetAttributeInfo("value");

            prefabInstanceType.Type = getNodeType("gap", "prefabInstanceType");
            prefabInstanceType.transform = new transformAttributes(prefabInstanceType.Type);
            prefabInstanceType.nameAttribute = prefabInstanceType.Type.GetAttributeInfo("name");
            prefabInstanceType.visibleAttribute = prefabInstanceType.Type.GetAttributeInfo("visible");
            prefabInstanceType.lockedAttribute = prefabInstanceType.Type.GetAttributeInfo("locked");
            prefabInstanceType.prefabRefAttribute = prefabInstanceType.Type.GetAttributeInfo("prefabRef");
            prefabInstanceType.componentChild = prefabInstanceType.Type.GetChildInfo("component");
            prefabInstanceType.gameObjectChild = prefabInstanceType.Type.GetChildInfo("gameObject");
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
            locatorType.transform = new transformAttributes(locatorType.Type);
            locatorType.nameAttribute = locatorType.Type.GetAttributeInfo("name");
            locatorType.visibleAttribute = locatorType.Type.GetAttributeInfo("visible");
            locatorType.lockedAttribute = locatorType.Type.GetAttributeInfo("locked");
            locatorType.componentChild = locatorType.Type.GetChildInfo("component");
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

            DirLight.Type = getNodeType("gap", "DirLight");
            DirLight.transform = new transformAttributes(DirLight.Type);
            DirLight.nameAttribute = DirLight.Type.GetAttributeInfo("name");
            DirLight.visibleAttribute = DirLight.Type.GetAttributeInfo("visible");
            DirLight.lockedAttribute = DirLight.Type.GetAttributeInfo("locked");
            DirLight.ambientAttribute = DirLight.Type.GetAttributeInfo("ambient");
            DirLight.diffuseAttribute = DirLight.Type.GetAttributeInfo("diffuse");
            DirLight.specularAttribute = DirLight.Type.GetAttributeInfo("specular");
            DirLight.directionAttribute = DirLight.Type.GetAttributeInfo("direction");
            DirLight.componentChild = DirLight.Type.GetChildInfo("component");

            BoxLight.Type = getNodeType("gap", "BoxLight");
            BoxLight.transform = new transformAttributes(BoxLight.Type);
            BoxLight.nameAttribute = BoxLight.Type.GetAttributeInfo("name");
            BoxLight.visibleAttribute = BoxLight.Type.GetAttributeInfo("visible");
            BoxLight.lockedAttribute = BoxLight.Type.GetAttributeInfo("locked");
            BoxLight.ambientAttribute = BoxLight.Type.GetAttributeInfo("ambient");
            BoxLight.diffuseAttribute = BoxLight.Type.GetAttributeInfo("diffuse");
            BoxLight.specularAttribute = BoxLight.Type.GetAttributeInfo("specular");
            BoxLight.directionAttribute = BoxLight.Type.GetAttributeInfo("direction");
            BoxLight.attenuationAttribute = BoxLight.Type.GetAttributeInfo("attenuation");
            BoxLight.componentChild = BoxLight.Type.GetChildInfo("component");

            PointLight.Type = getNodeType("gap", "PointLight");
            PointLight.transform = new transformAttributes(PointLight.Type);
            PointLight.nameAttribute = PointLight.Type.GetAttributeInfo("name");
            PointLight.visibleAttribute = PointLight.Type.GetAttributeInfo("visible");
            PointLight.lockedAttribute = PointLight.Type.GetAttributeInfo("locked");
            PointLight.ambientAttribute = PointLight.Type.GetAttributeInfo("ambient");
            PointLight.diffuseAttribute = PointLight.Type.GetAttributeInfo("diffuse");
            PointLight.specularAttribute = PointLight.Type.GetAttributeInfo("specular");
            PointLight.attenuationAttribute = PointLight.Type.GetAttributeInfo("attenuation");
            PointLight.rangeAttribute = PointLight.Type.GetAttributeInfo("range");
            PointLight.componentChild = PointLight.Type.GetChildInfo("component");

            controlPointType.Type = getNodeType("gap", "controlPointType");
            controlPointType.transform = new transformAttributes(controlPointType.Type);
            controlPointType.nameAttribute = controlPointType.Type.GetAttributeInfo("name");
            controlPointType.visibleAttribute = controlPointType.Type.GetAttributeInfo("visible");
            controlPointType.lockedAttribute = controlPointType.Type.GetAttributeInfo("locked");
            controlPointType.componentChild = controlPointType.Type.GetChildInfo("component");

            curveType.Type = getNodeType("gap", "curveType");
            curveType.transform = new transformAttributes(curveType.Type);
            curveType.nameAttribute = curveType.Type.GetAttributeInfo("name");
            curveType.visibleAttribute = curveType.Type.GetAttributeInfo("visible");
            curveType.lockedAttribute = curveType.Type.GetAttributeInfo("locked");
            curveType.colorAttribute = curveType.Type.GetAttributeInfo("color");
            curveType.isClosedAttribute = curveType.Type.GetAttributeInfo("isClosed");
            curveType.stepsAttribute = curveType.Type.GetAttributeInfo("steps");
            curveType.interpolationTypeAttribute = curveType.Type.GetAttributeInfo("interpolationType");
            curveType.componentChild = curveType.Type.GetChildInfo("component");
            curveType.pointChild = curveType.Type.GetChildInfo("point");

            catmullRomType.Type = getNodeType("gap", "catmullRomType");
            catmullRomType.transform = new transformAttributes(catmullRomType.Type);
            catmullRomType.nameAttribute = catmullRomType.Type.GetAttributeInfo("name");
            catmullRomType.visibleAttribute = catmullRomType.Type.GetAttributeInfo("visible");
            catmullRomType.lockedAttribute = catmullRomType.Type.GetAttributeInfo("locked");
            catmullRomType.colorAttribute = catmullRomType.Type.GetAttributeInfo("color");
            catmullRomType.isClosedAttribute = catmullRomType.Type.GetAttributeInfo("isClosed");
            catmullRomType.stepsAttribute = catmullRomType.Type.GetAttributeInfo("steps");
            catmullRomType.interpolationTypeAttribute = catmullRomType.Type.GetAttributeInfo("interpolationType");
            catmullRomType.componentChild = catmullRomType.Type.GetChildInfo("component");
            catmullRomType.pointChild = catmullRomType.Type.GetChildInfo("point");

            bezierType.Type = getNodeType("gap", "bezierType");
            bezierType.transform = new transformAttributes(bezierType.Type);
            bezierType.nameAttribute = bezierType.Type.GetAttributeInfo("name");
            bezierType.visibleAttribute = bezierType.Type.GetAttributeInfo("visible");
            bezierType.lockedAttribute = bezierType.Type.GetAttributeInfo("locked");
            bezierType.colorAttribute = bezierType.Type.GetAttributeInfo("color");
            bezierType.isClosedAttribute = bezierType.Type.GetAttributeInfo("isClosed");
            bezierType.stepsAttribute = bezierType.Type.GetAttributeInfo("steps");
            bezierType.interpolationTypeAttribute = bezierType.Type.GetAttributeInfo("interpolationType");
            bezierType.componentChild = bezierType.Type.GetChildInfo("component");
            bezierType.pointChild = bezierType.Type.GetChildInfo("point");

            skyDomeType.Type = getNodeType("gap", "skyDomeType");
            skyDomeType.transform = new transformAttributes(skyDomeType.Type);
            skyDomeType.nameAttribute = skyDomeType.Type.GetAttributeInfo("name");
            skyDomeType.visibleAttribute = skyDomeType.Type.GetAttributeInfo("visible");
            skyDomeType.lockedAttribute = skyDomeType.Type.GetAttributeInfo("locked");
            skyDomeType.cubeMapAttribute = skyDomeType.Type.GetAttributeInfo("cubeMap");
            skyDomeType.componentChild = skyDomeType.Type.GetChildInfo("component");

            shapeTestType.Type = getNodeType("gap", "shapeTestType");
            shapeTestType.transform = new transformAttributes(shapeTestType.Type);
            shapeTestType.nameAttribute = shapeTestType.Type.GetAttributeInfo("name");
            shapeTestType.visibleAttribute = shapeTestType.Type.GetAttributeInfo("visible");
            shapeTestType.lockedAttribute = shapeTestType.Type.GetAttributeInfo("locked");
            shapeTestType.colorAttribute = shapeTestType.Type.GetAttributeInfo("color");
            shapeTestType.emissiveAttribute = shapeTestType.Type.GetAttributeInfo("emissive");
            shapeTestType.specularAttribute = shapeTestType.Type.GetAttributeInfo("specular");
            shapeTestType.specularPowerAttribute = shapeTestType.Type.GetAttributeInfo("specularPower");
            shapeTestType.diffuseAttribute = shapeTestType.Type.GetAttributeInfo("diffuse");
            shapeTestType.normalAttribute = shapeTestType.Type.GetAttributeInfo("normal");
            shapeTestType.textureTransformAttribute = shapeTestType.Type.GetAttributeInfo("textureTransform");
            shapeTestType.componentChild = shapeTestType.Type.GetChildInfo("component");

            cubeTestType.Type = getNodeType("gap", "cubeTestType");
            cubeTestType.transform = new transformAttributes(cubeTestType.Type);
            cubeTestType.nameAttribute = cubeTestType.Type.GetAttributeInfo("name");
            cubeTestType.visibleAttribute = cubeTestType.Type.GetAttributeInfo("visible");
            cubeTestType.lockedAttribute = cubeTestType.Type.GetAttributeInfo("locked");
            cubeTestType.colorAttribute = cubeTestType.Type.GetAttributeInfo("color");
            cubeTestType.emissiveAttribute = cubeTestType.Type.GetAttributeInfo("emissive");
            cubeTestType.specularAttribute = cubeTestType.Type.GetAttributeInfo("specular");
            cubeTestType.specularPowerAttribute = cubeTestType.Type.GetAttributeInfo("specularPower");
            cubeTestType.diffuseAttribute = cubeTestType.Type.GetAttributeInfo("diffuse");
            cubeTestType.normalAttribute = cubeTestType.Type.GetAttributeInfo("normal");
            cubeTestType.textureTransformAttribute = cubeTestType.Type.GetAttributeInfo("textureTransform");
            cubeTestType.componentChild = cubeTestType.Type.GetChildInfo("component");

            TorusTestType.Type = getNodeType("gap", "TorusTestType");
            TorusTestType.transform = new transformAttributes(TorusTestType.Type);
            TorusTestType.nameAttribute = TorusTestType.Type.GetAttributeInfo("name");
            TorusTestType.visibleAttribute = TorusTestType.Type.GetAttributeInfo("visible");
            TorusTestType.lockedAttribute = TorusTestType.Type.GetAttributeInfo("locked");
            TorusTestType.colorAttribute = TorusTestType.Type.GetAttributeInfo("color");
            TorusTestType.emissiveAttribute = TorusTestType.Type.GetAttributeInfo("emissive");
            TorusTestType.specularAttribute = TorusTestType.Type.GetAttributeInfo("specular");
            TorusTestType.specularPowerAttribute = TorusTestType.Type.GetAttributeInfo("specularPower");
            TorusTestType.diffuseAttribute = TorusTestType.Type.GetAttributeInfo("diffuse");
            TorusTestType.normalAttribute = TorusTestType.Type.GetAttributeInfo("normal");
            TorusTestType.textureTransformAttribute = TorusTestType.Type.GetAttributeInfo("textureTransform");
            TorusTestType.componentChild = TorusTestType.Type.GetChildInfo("component");

            sphereTestType.Type = getNodeType("gap", "sphereTestType");
            sphereTestType.transform = new transformAttributes(sphereTestType.Type);
            sphereTestType.nameAttribute = sphereTestType.Type.GetAttributeInfo("name");
            sphereTestType.visibleAttribute = sphereTestType.Type.GetAttributeInfo("visible");
            sphereTestType.lockedAttribute = sphereTestType.Type.GetAttributeInfo("locked");
            sphereTestType.colorAttribute = sphereTestType.Type.GetAttributeInfo("color");
            sphereTestType.emissiveAttribute = sphereTestType.Type.GetAttributeInfo("emissive");
            sphereTestType.specularAttribute = sphereTestType.Type.GetAttributeInfo("specular");
            sphereTestType.specularPowerAttribute = sphereTestType.Type.GetAttributeInfo("specularPower");
            sphereTestType.diffuseAttribute = sphereTestType.Type.GetAttributeInfo("diffuse");
            sphereTestType.normalAttribute = sphereTestType.Type.GetAttributeInfo("normal");
            sphereTestType.textureTransformAttribute = sphereTestType.Type.GetAttributeInfo("textureTransform");
            sphereTestType.componentChild = sphereTestType.Type.GetChildInfo("component");

            coneTestType.Type = getNodeType("gap", "coneTestType");
            coneTestType.transform = new transformAttributes(coneTestType.Type);
            coneTestType.nameAttribute = coneTestType.Type.GetAttributeInfo("name");
            coneTestType.visibleAttribute = coneTestType.Type.GetAttributeInfo("visible");
            coneTestType.lockedAttribute = coneTestType.Type.GetAttributeInfo("locked");
            coneTestType.colorAttribute = coneTestType.Type.GetAttributeInfo("color");
            coneTestType.emissiveAttribute = coneTestType.Type.GetAttributeInfo("emissive");
            coneTestType.specularAttribute = coneTestType.Type.GetAttributeInfo("specular");
            coneTestType.specularPowerAttribute = coneTestType.Type.GetAttributeInfo("specularPower");
            coneTestType.diffuseAttribute = coneTestType.Type.GetAttributeInfo("diffuse");
            coneTestType.normalAttribute = coneTestType.Type.GetAttributeInfo("normal");
            coneTestType.textureTransformAttribute = coneTestType.Type.GetAttributeInfo("textureTransform");
            coneTestType.componentChild = coneTestType.Type.GetChildInfo("component");

            cylinderTestType.Type = getNodeType("gap", "cylinderTestType");
            cylinderTestType.transform = new transformAttributes(cylinderTestType.Type);
            cylinderTestType.nameAttribute = cylinderTestType.Type.GetAttributeInfo("name");
            cylinderTestType.visibleAttribute = cylinderTestType.Type.GetAttributeInfo("visible");
            cylinderTestType.lockedAttribute = cylinderTestType.Type.GetAttributeInfo("locked");
            cylinderTestType.colorAttribute = cylinderTestType.Type.GetAttributeInfo("color");
            cylinderTestType.emissiveAttribute = cylinderTestType.Type.GetAttributeInfo("emissive");
            cylinderTestType.specularAttribute = cylinderTestType.Type.GetAttributeInfo("specular");
            cylinderTestType.specularPowerAttribute = cylinderTestType.Type.GetAttributeInfo("specularPower");
            cylinderTestType.diffuseAttribute = cylinderTestType.Type.GetAttributeInfo("diffuse");
            cylinderTestType.normalAttribute = cylinderTestType.Type.GetAttributeInfo("normal");
            cylinderTestType.textureTransformAttribute = cylinderTestType.Type.GetAttributeInfo("textureTransform");
            cylinderTestType.componentChild = cylinderTestType.Type.GetChildInfo("component");

            planeTestType.Type = getNodeType("gap", "planeTestType");
            planeTestType.transform = new transformAttributes(planeTestType.Type);
            planeTestType.nameAttribute = planeTestType.Type.GetAttributeInfo("name");
            planeTestType.visibleAttribute = planeTestType.Type.GetAttributeInfo("visible");
            planeTestType.lockedAttribute = planeTestType.Type.GetAttributeInfo("locked");
            planeTestType.colorAttribute = planeTestType.Type.GetAttributeInfo("color");
            planeTestType.emissiveAttribute = planeTestType.Type.GetAttributeInfo("emissive");
            planeTestType.specularAttribute = planeTestType.Type.GetAttributeInfo("specular");
            planeTestType.specularPowerAttribute = planeTestType.Type.GetAttributeInfo("specularPower");
            planeTestType.diffuseAttribute = planeTestType.Type.GetAttributeInfo("diffuse");
            planeTestType.normalAttribute = planeTestType.Type.GetAttributeInfo("normal");
            planeTestType.textureTransformAttribute = planeTestType.Type.GetAttributeInfo("textureTransform");
            planeTestType.componentChild = planeTestType.Type.GetChildInfo("component");

            billboardTestType.Type = getNodeType("gap", "billboardTestType");
            billboardTestType.transform = new transformAttributes(billboardTestType.Type);
            billboardTestType.nameAttribute = billboardTestType.Type.GetAttributeInfo("name");
            billboardTestType.visibleAttribute = billboardTestType.Type.GetAttributeInfo("visible");
            billboardTestType.lockedAttribute = billboardTestType.Type.GetAttributeInfo("locked");
            billboardTestType.intensityAttribute = billboardTestType.Type.GetAttributeInfo("intensity");
            billboardTestType.diffuseAttribute = billboardTestType.Type.GetAttributeInfo("diffuse");
            billboardTestType.textureTransformAttribute = billboardTestType.Type.GetAttributeInfo("textureTransform");
            billboardTestType.componentChild = billboardTestType.Type.GetChildInfo("component");

            orcType.Type = getNodeType("gap", "orcType");
            orcType.transform = new transformAttributes(orcType.Type);
            orcType.nameAttribute = orcType.Type.GetAttributeInfo("name");
            orcType.visibleAttribute = orcType.Type.GetAttributeInfo("visible");
            orcType.lockedAttribute = orcType.Type.GetAttributeInfo("locked");
            orcType.weightAttribute = orcType.Type.GetAttributeInfo("weight");
            orcType.emotionAttribute = orcType.Type.GetAttributeInfo("emotion");
            orcType.goalsAttribute = orcType.Type.GetAttributeInfo("goals");
            orcType.colorAttribute = orcType.Type.GetAttributeInfo("color");
            orcType.toeColorAttribute = orcType.Type.GetAttributeInfo("toeColor");
            orcType.componentChild = orcType.Type.GetChildInfo("component");
            orcType.geometryChild = orcType.Type.GetChildInfo("geometry");
            orcType.animationChild = orcType.Type.GetChildInfo("animation");
            orcType.targetChild = orcType.Type.GetChildInfo("target");
            orcType.friendsChild = orcType.Type.GetChildInfo("friends");
            orcType.childrenChild = orcType.Type.GetChildInfo("children");

            gameRootElement = getRootElement(NS, "game");
            prototypeRootElement = getRootElement(NS, "prototype");
            prefabRootElement = getRootElement(NS, "prefab");
            textureMetadataRootElement = getRootElement(NS, "textureMetadata");
            resourceMetadataRootElement = getRootElement(NS, "resourceMetadata");

            // <<XLE
            placementsCellReferenceType.Type = getNodeType("gap", "placementsCellReference");
            placementsCellReferenceType.refAttribute = placementsCellReferenceType.Type.GetAttributeInfo("ref");
            placementsCellReferenceType.nameAttribute = placementsCellReferenceType.Type.GetAttributeInfo("name");
            placementsCellReferenceType.minsAttribute = placementsCellReferenceType.Type.GetAttributeInfo("mins");
            placementsCellReferenceType.maxsAttribute = placementsCellReferenceType.Type.GetAttributeInfo("maxs");

            placementsFolderType.Type = getNodeType("gap", "placementsFolderType");
            placementsFolderType.cellChild = placementsFolderType.Type.GetChildInfo("cell");

            placementsDocumentType.Type = getNodeType("gap", "placementsDocumentType");
            placementsDocumentType.nameAttribute = placementsDocumentType.Type.GetAttributeInfo("name");
            placementsDocumentType.placementsChild = placementsDocumentType.Type.GetChildInfo("placement");

            abstractPlacementObjectType.Type = getNodeType("gap", "abstractPlacementObjectType");
            abstractPlacementObjectType.transform = new transformAttributes(abstractPlacementObjectType.Type);
            
            placementObjectType.Type = getNodeType("gap", "placementObjectType");
            placementObjectType.modelChild = placementObjectType.Type.GetAttributeInfo("model");
            placementObjectType.materialChild = placementObjectType.Type.GetAttributeInfo("material");

            terrainType.Type = getNodeType("gap", "terrainType");

            terrainBaseTextureStrataType.Type = getNodeType("gap", "terrainBaseTextureStrataType");
            terrainBaseTextureType.Type = getNodeType("gap", "terrainBaseTextureType");
            terrainBaseTextureType.strataChild = terrainBaseTextureType.Type.GetChildInfo("strata");

            placementsDocumentRootElement = getRootElement(NS, "placementsDocument");
            // XLE>>
        }

        public static class gameType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo fogEnabledAttribute;
            public static AttributeInfo fogColorAttribute;
            public static AttributeInfo fogRangeAttribute;
            public static AttributeInfo fogDensityAttribute;
            public static ChildInfo gameObjectFolderChild;
            public static ChildInfo layersChild;
            public static ChildInfo bookmarksChild;
            public static ChildInfo gameReferenceChild;
            public static ChildInfo gridChild;
            
            // <<<XLE
            public static ChildInfo placementsFolderChild;
            // XLE>>
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

        public class transformAttributes
        {
            public AttributeInfo transformAttribute;
            public AttributeInfo translateAttribute;
            public AttributeInfo rotateAttribute;
            public AttributeInfo scaleAttribute;
            public AttributeInfo pivotAttribute;
            public AttributeInfo transformationTypeAttribute;

            public transformAttributes(DomNodeType type)
            {
                transformAttribute = type.GetAttributeInfo("transform");
                translateAttribute = type.GetAttributeInfo("translate");
                rotateAttribute = type.GetAttributeInfo("rotate");
                scaleAttribute = type.GetAttributeInfo("scale");
                pivotAttribute = type.GetAttributeInfo("pivot");
                transformationTypeAttribute = type.GetAttributeInfo("transformationType");
            }
        }

        public static class gameObjectType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo componentChild;
        }

        public static class gameObjectComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
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

        public static class transformComponentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo activeAttribute;
            public static AttributeInfo translationAttribute;
            public static AttributeInfo rotationAttribute;
            public static AttributeInfo scaleAttribute;
        }
        
        public static class gameObjectGroupType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo gameObjectChild;
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
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo prefabRefAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo gameObjectChild;
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
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo componentChild;
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

        public static class DirLight
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo ambientAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo directionAttribute;
            public static ChildInfo componentChild;
        }

        public static class BoxLight
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo ambientAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo directionAttribute;
            public static AttributeInfo attenuationAttribute;
            public static ChildInfo componentChild;
        }

        public static class PointLight
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo ambientAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo attenuationAttribute;
            public static AttributeInfo rangeAttribute;
            public static ChildInfo componentChild;
        }

        public static class controlPointType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static ChildInfo componentChild;
        }

        public static class curveType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo pointChild;
        }

        public static class catmullRomType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo pointChild;
        }

        public static class bezierType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo isClosedAttribute;
            public static AttributeInfo stepsAttribute;
            public static AttributeInfo interpolationTypeAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo pointChild;
        }

        public static class skyDomeType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo cubeMapAttribute;
            public static ChildInfo componentChild;
        }

        public static class shapeTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class cubeTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class TorusTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class sphereTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class coneTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class cylinderTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class planeTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo emissiveAttribute;
            public static AttributeInfo specularAttribute;
            public static AttributeInfo specularPowerAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo normalAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class billboardTestType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo intensityAttribute;
            public static AttributeInfo diffuseAttribute;
            public static AttributeInfo textureTransformAttribute;
            public static ChildInfo componentChild;
        }

        public static class orcType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo visibleAttribute;
            public static AttributeInfo lockedAttribute;
            public static AttributeInfo weightAttribute;
            public static AttributeInfo emotionAttribute;
            public static AttributeInfo goalsAttribute;
            public static AttributeInfo colorAttribute;
            public static AttributeInfo toeColorAttribute;
            public static ChildInfo componentChild;
            public static ChildInfo geometryChild;
            public static ChildInfo animationChild;
            public static ChildInfo targetChild;
            public static ChildInfo friendsChild;
            public static ChildInfo childrenChild;
        }

        public static ChildInfo gameRootElement;

        public static ChildInfo prototypeRootElement;

        public static ChildInfo prefabRootElement;

        public static ChildInfo textureMetadataRootElement;

        public static ChildInfo resourceMetadataRootElement;

        // <<XLE
        public static class placementsCellReferenceType
        {
            public static DomNodeType Type;
            public static AttributeInfo refAttribute;
            public static AttributeInfo nameAttribute;
            public static AttributeInfo minsAttribute;
            public static AttributeInfo maxsAttribute;
        }

        public static class placementsFolderType
        {
            public static DomNodeType Type;
            public static ChildInfo cellChild;
        }

        public static class placementsDocumentType
        {
            public static DomNodeType Type;
            public static AttributeInfo nameAttribute;
            public static ChildInfo placementsChild;
        }

        public static class abstractPlacementObjectType
        {
            public static DomNodeType Type;
            public static transformAttributes transform;
        }
        
        public static class placementObjectType
        {
            public static DomNodeType Type;
            public static AttributeInfo modelChild;
            public static AttributeInfo materialChild;
        }

        public static class terrainType
        {
            public static DomNodeType Type;
        }

        public static class terrainBaseTextureStrataType
        {
            public static DomNodeType Type;
        }

        public static class terrainBaseTextureType
        {
            public static DomNodeType Type;
            public static ChildInfo strataChild;
        }

        public static ChildInfo placementsDocumentRootElement;
        // XLE>>
    }
}

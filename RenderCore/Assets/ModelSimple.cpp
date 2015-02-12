// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelSimple.h"
#include "ModelRunTimeInternal.h"       // for TransformationMachine
#include "..\..\Utility\Streams\FileUtils.h"

namespace RenderCore { namespace Assets { namespace Simple 
{
    ScaffoldLevelOfDetail::ScaffoldLevelOfDetail() : _lodIndex(~unsigned(0x0)) {}
    ScaffoldLevelOfDetail::~ScaffoldLevelOfDetail() 
    {
        if (_transformMachine) {
            ((TransformationMachine*)Serialization::Block_GetFirstObject(_transformMachine.get()))->~TransformationMachine();
        }
    }

    ScaffoldLevelOfDetail::ScaffoldLevelOfDetail(ScaffoldLevelOfDetail&& moveFrom)
        : _transformMachine(std::move(moveFrom._transformMachine))
        , _lodIndex(moveFrom._lodIndex)
        , _meshCalls(std::move(moveFrom._meshCalls))
    {}

    ScaffoldLevelOfDetail& ScaffoldLevelOfDetail::operator=(ScaffoldLevelOfDetail&& moveFrom)
    {
        if (_transformMachine) {
            ((TransformationMachine*)Serialization::Block_GetFirstObject(_transformMachine.get()))->~TransformationMachine();
        }
        _transformMachine = std::move(moveFrom._transformMachine);
        _lodIndex = moveFrom._lodIndex;
        _meshCalls = std::move(moveFrom._meshCalls);
        return *this;
    }

    std::pair<Float3, Float3>   ModelScaffold::GetStaticBoundingBox(unsigned lodIndex) const
    {
            // (have to reopen the file, because the vertex data hasn't previously been loaded into memory)
        BasicFile file(_filename.c_str(), "rb");

        Float3 mins( FLT_MAX,  FLT_MAX,  FLT_MAX);
        Float3 maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        auto lodI = std::find_if(_lods.cbegin(), _lods.cend(),
            [=](const ScaffoldLevelOfDetail& i) { return i._lodIndex == lodIndex; });
        if (lodI == _lods.cend())
            return std::make_pair(mins, maxs);

        auto& lod = *lodI;

        auto* transMachine = (TransformationMachine*)Serialization::Block_GetFirstObject(lod._transformMachine.get());
        auto matCount = transMachine->GetOutputMatrixCount();
        auto localToModels = std::make_unique<Float4x4[]>(matCount);
        transMachine->GenerateOutputTransforms(localToModels.get(), matCount, nullptr);
        

            //  We need to look through each mesh call and find the vertices involved.
            //  Let's assume (to make this simpler) that every vertex in every mesh we come across
            //  will be used. That way we can just find the meshes
            //      (otherwise, we'd have to go through the draw calls, and look at the indices in the
            //      index buffers)
        for (auto m=lod._meshCalls.cbegin(); m!=lod._meshCalls.cend(); ++m) {
            auto mesh = std::find_if(_meshes.cbegin(), _meshes.cend(),
                [=](const ScaffoldMesh& mesh) { return mesh._meshId == m->_meshId; });
            if (mesh == _meshes.cend()) continue;

                // find the "position" data stream
            auto positions = std::find_if(mesh->_dataStreams.cbegin(), mesh->_dataStreams.cend(),
                [](const ScaffoldMesh::DataStream& dataStream) { return dataStream._semantic[0] == "POSITION"; });
            if (positions == mesh->_dataStreams.cend()) continue;

            auto streamData = std::make_unique<uint8[]>(positions->_streamSize);

                //  We only support 3d float vectors currently.
                //  todo -- we need some flexible dynamic casting tools so that we
                //          can more easily work with different input formats. At
                //          the least, 16 bit floats would be useful!
            assert(positions->_format[0] == Metal::NativeFormat::R32G32B32_FLOAT);

            auto localToModel = localToModels[m->_transformMarker];

            file.Seek(positions->_fileOffset, SEEK_SET);
            file.Read(streamData.get(), 1, positions->_streamSize);
            for (unsigned c=0; c<positions->_elementCount; ++c) {
                Float3 p = *(Float3*)PtrAdd(streamData.get(), c*positions->_elementSize);
                Float3 modelSpace = TransformPoint(localToModel, p);
                mins[0] = std::min(modelSpace[0], mins[0]);
                mins[1] = std::min(modelSpace[1], mins[1]);
                mins[2] = std::min(modelSpace[2], mins[2]);
                maxs[0] = std::max(modelSpace[0], maxs[0]);
                maxs[1] = std::max(modelSpace[1], maxs[1]);
                maxs[2] = std::max(modelSpace[2], maxs[2]);
            }
        }

        return std::make_pair(mins, maxs);
    }

    ModelScaffold::ModelScaffold() {}
    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom)
    : _lods(std::move(moveFrom._lods))
    , _meshes(std::move(moveFrom._meshes))
    , _materials(std::move(moveFrom._materials))
    {}

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom)
    {
        _lods = std::move(moveFrom._lods);
        _meshes = std::move(moveFrom._meshes);
        _materials = std::move(moveFrom._materials);
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {}

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MaterialScaffold::MaterialDefinition::MaterialDefinition()
    {
        _materialName = _shaderName = std::string();
        _diffuseColor = Float3(1.f, 1.f, 1.f);
        _specularColor = Float3(1.f, 1.f, 1.f);
        _flags = 0;
        _alphaThreshold = _opacity = 1.f;
    }


    auto MaterialScaffold::GetSubMaterial(const char name[]) const -> const MaterialDefinition*
    {
            //  Look for a material with this name in our list. Simple search
            //  \todo -- we could just store the hash of the material name 
        for (auto i = _subMaterials.cbegin(); i!=_subMaterials.cend(); ++i) {
            if (i->_materialName == name) {
                return AsPointer(i);
            }
        }
        return nullptr;
    }
    
    MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom)
    : _subMaterials(std::move(moveFrom._subMaterials))
    {}

    MaterialScaffold::MaterialScaffold() {}

    MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom)
    {
        _subMaterials = std::move(moveFrom._subMaterials);
        return *this;
    }

    MaterialScaffold::~MaterialScaffold()
    {}
    

}}}




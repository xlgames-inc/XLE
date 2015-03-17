// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelRunTime.h"
#include "ModelRunTimeInternal.h"
#include "Material.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{


    const ResolvedMaterial* MaterialScaffold::GetMaterial(MaterialGuid guid)
    {
        auto i = LowerBound(_data->_materials, guid);
        if (i!=_data->_materials.end() && i->first==guid){
            return &i->second;
        }
        return nullptr;
    }

    MaterialScaffold::MaterialScaffold(const ResChar filename[])
    {
            //  This work should be done in a pre-processing step. But we're 
            //  going to do it here for now.

        _data = std::make_unique<MaterialImmutableData>();

        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, filename);

        auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);

        Data data;
        data.Load((const char*)sourceFile.get(), int(fileSize));
        for (int c=0; c<data.Size(); ++c) {
            auto child = data.ChildAt(c);
            if (!child || !child->value) continue;

            MaterialGuid guid = XlAtoI64(child->value, nullptr, 16);
            RawMaterial rawMat(StringMeld<MaxPath>() << filename << ":" << child->value);
            auto i = LowerBound(_data->_materials, guid);
            if (i!=_data->_materials.begin() && i->first == guid) {
                LogWarning << "Hit guid collision while loading material file " << filename << ". Ignoring duplicated entry";
            } else {
                std::vector<::Assets::FileAndTime> deps;
                auto resolved = rawMat.Resolve(searchRules, &deps);
                _data->_materials.insert(i, std::make_pair(guid, std::move(resolved)));
            }
        }
    }

    MaterialScaffold::~MaterialScaffold()
    {
    }

}}


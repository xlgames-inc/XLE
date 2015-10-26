// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TransformationCommands.h"

#pragma warning(disable:4127)

namespace RenderCore { namespace Assets
{
    static unsigned CommandSize(TransformStackCommand cmd)
    {
        switch (cmd) {
        case TransformStackCommand::PushLocalToWorld:           return 0;
        case TransformStackCommand::PopLocalToWorld:            return 1;
        case TransformStackCommand::TransformFloat4x4_Static:   return 16;
        case TransformStackCommand::Translate_Static:           return 3;
        case TransformStackCommand::RotateX_Static:             return 1;
        case TransformStackCommand::RotateY_Static:             return 1;
        case TransformStackCommand::RotateZ_Static:             return 1;
        case TransformStackCommand::Rotate_Static:              return 4;
        case TransformStackCommand::UniformScale_Static:        return 1;
        case TransformStackCommand::ArbitraryScale_Static:      return 3;

        case TransformStackCommand::TransformFloat4x4_Parameter:
        case TransformStackCommand::Translate_Parameter:
        case TransformStackCommand::RotateX_Parameter:
        case TransformStackCommand::RotateY_Parameter:
        case TransformStackCommand::RotateZ_Parameter:
        case TransformStackCommand::Rotate_Parameter:
        case TransformStackCommand::UniformScale_Parameter:
        case TransformStackCommand::ArbitraryScale_Parameter:
            return 1;

        case TransformStackCommand::WriteOutputMatrix:
            return 1;

        default: return 0;
        }
    }

    T1(Iterator) static Iterator SkipUntilPop(Iterator i, Iterator end, signed& finalIdentLevel)
    {
        finalIdentLevel = 1;
        for (; i!=end;) {
            if (*i == (uint32)TransformStackCommand::PopLocalToWorld) {
                auto popCount = *(i+1);
                finalIdentLevel -= signed(popCount);
                if (finalIdentLevel <= 0)
                    return i;
            } else if (*i == (uint32)TransformStackCommand::PushLocalToWorld)
                ++finalIdentLevel;
            i += 1 + CommandSize((TransformStackCommand)*i);
        }
        return end;
    }

    static bool IsTransformCommand(TransformStackCommand cmd)
    {
        return 
                (cmd >= TransformStackCommand::TransformFloat4x4_Static && cmd <= TransformStackCommand::ArbitraryScale_Static)
            ||  (cmd >= TransformStackCommand::TransformFloat4x4_Parameter && cmd <= TransformStackCommand::ArbitraryScale_Parameter);
    }

    T1(Iterator) 
        static Iterator IsRedundantPush(Iterator i, Iterator end, bool& isRedundant)
    {
        // Scan forward... if the transform isn't modified at this level, or if
        // the matrix isn't used after the pop, then the push/pop is redundant
        assert(*i == (uint32)TransformStackCommand::PushLocalToWorld);
        ++i;

        bool foundTransformCmd = false;
        for (;i<end;) {
            auto cmd = TransformStackCommand(*i);
            if (IsTransformCommand(cmd)) {
                foundTransformCmd = true;
            } else if (cmd == TransformStackCommand::PushLocalToWorld) {
                ++i;
                signed finalIdentLevel = 0;
                i = SkipUntilPop(i, end, finalIdentLevel);
                if (finalIdentLevel < 0) {
                    isRedundant = (finalIdentLevel < -1) || !foundTransformCmd || (i+2) == end;
                    return i;
                }   
            } else if (cmd == TransformStackCommand::PopLocalToWorld) {
                auto popCount = *(i+1);
                isRedundant = (popCount > 1) || !foundTransformCmd || (i+2) == end;
                return i;
            }

            i += 1 + CommandSize((TransformStackCommand)*i);
        }
        isRedundant = true;
        return i;
    }

    static void RemoveRedundantPushes(std::vector<uint32>& cmdStream)
    {
            // First, we just want to convert series of pop operations into
            // a single pop.
        for (auto i=cmdStream.begin(); i!=cmdStream.end();) {
            if (*i == (uint32)TransformStackCommand::PopLocalToWorld) {
                if (    (cmdStream.end() - i) >= 4
                    &&  *(i+2) == (uint32)TransformStackCommand::PopLocalToWorld) {
                        // combine these 2 pops into a single pop command
                    auto newPopCount = *(i+1) + *(i+3);
                    i = cmdStream.erase(i, i+2);
                    *(i+1) = newPopCount;
                } else {
                    i += 2;
                }
            } else {
                i += 1 + CommandSize((TransformStackCommand)*i);
            }
        }

            // Now look for push operations that are redundant
        for (auto i=cmdStream.begin(); i!=cmdStream.end();) {
            if (*i == (uint32)TransformStackCommand::PushLocalToWorld) {
                    // let's scan forward to find the pop operation that matches
                    // this push...
                #if 0
                    signed finalIdentLevel = 0;
                    auto pop = SkipUntilPop(i+1, cmdStream.end(), finalIdentLevel);

                    // If finalIdentLevel < 0, it means our push was popped in a multi-pop sequence. 
                    // But it wasn't the last pop in that sequence. Therefore it was not used 
                    // after pop -- and so is redundant.
                    // Alternatively, if the pop is the very last thing in the command stream, it 
                    // is also redundant.
                    if (pop == cmdStream.end()) {
                        i = cmdStream.erase(i); // no pop found!
                    } else if (finalIdentLevel < 0) {
                        assert(*(pop+1) > 1);
                        (*(pop+1))--; // reduce the pop count
                        i = cmdStream.erase(i);
                    } else if ((pop+2) == cmdStream.end()) {
                        cmdStream.erase(pop, pop+2);    // erase both the push & the pop
                        i = cmdStream.erase(i);
                    } else ++i;
                #endif

                bool isRedundant = false;
                auto pop = IsRedundantPush(i, cmdStream.end(), isRedundant);
                if (isRedundant) {
                    if (pop < cmdStream.end()) {
                        auto& popCount = *(pop+1);
                        if (popCount > 1) {
                            popCount--;
                        } else {
                            cmdStream.erase(pop, pop+2);
                        }
                    }
                    i = cmdStream.erase(i);
                }
            } 

            i += 1 + CommandSize((TransformStackCommand)*i);
        }
    }

    enum MergeType { StaticTransform, OutputMatrix, Push, Pop, Blocker };
    static MergeType AsMergeType(TransformStackCommand cmd)
    {
        switch (cmd) {
        case TransformStackCommand::TransformFloat4x4_Static:
        case TransformStackCommand::Translate_Static:
        case TransformStackCommand::RotateX_Static:
        case TransformStackCommand::RotateY_Static:
        case TransformStackCommand::RotateZ_Static:
        case TransformStackCommand::Rotate_Static:
        case TransformStackCommand::UniformScale_Static:
        case TransformStackCommand::ArbitraryScale_Static:  return MergeType::StaticTransform;

        case TransformStackCommand::PushLocalToWorld:       return MergeType::Push;
        case TransformStackCommand::PopLocalToWorld:        return MergeType::Pop;
        case TransformStackCommand::WriteOutputMatrix:      return MergeType::OutputMatrix;

        default: return MergeType::Blocker;
        }
    }

    static const uint32* FindDownstreamInfluences(const uint32* i, const uint32* end, std::vector<const uint32*>& result, signed& finalIdentLevel)
    {
            // Search forward and find the commands that are going to directly 
            // effected by the transform before i
        for (;i<end;) {
            auto type = AsMergeType(TransformStackCommand(*i));
            if (type == MergeType::StaticTransform || type == MergeType::Blocker) {
                // Hitting a static transform blocks any further searches
                // We can just skip until we pop out of this block
                result.push_back(i);
                i = SkipUntilPop(i, end, finalIdentLevel);
                return i+1+CommandSize((TransformStackCommand)*i);
            } else if (type == MergeType::OutputMatrix) {
                result.push_back(i);
                i += 1 + CommandSize((TransformStackCommand)*i);
            } else if (type == MergeType::Pop) {
                auto popCount = *(i+1);
                finalIdentLevel = popCount-1;
                return i+1+CommandSize((TransformStackCommand)*i);
            } else if (type == MergeType::Push) {
                // Hitting a push operation means we have to branch.
                // Here, we must find all of the influences in the
                // pushed branch, and then continue on from the next
                // pop
                i = FindDownstreamInfluences(i+1, end, result, finalIdentLevel);
                if (finalIdentLevel < 0) {
                    ++finalIdentLevel;
                    return i;
                }
            }
        }
        finalIdentLevel = 1;
        return i;
    }

    static bool ShouldDoMerge(IteratorRange<const uint32**> influences)
    {
        const bool canMergeIntoOutputMatrix = true;
        signed commandAdjustment = -1;
        for (const auto& c:influences) {
            switch (AsMergeType(TransformStackCommand(*c))) {
            case MergeType::StaticTransform:
                    // This other transform might be merged away, also -- if it can be merged further.
                    // so let's consider it another dropped command
                --commandAdjustment;    
                break;
            case MergeType::Blocker:
                ++commandAdjustment;
                break;
            case MergeType::OutputMatrix:
                if (!canMergeIntoOutputMatrix) ++commandAdjustment;
                break;

            default:
                assert(0); // push & pop shouldn't be registered as influences
                break;
            }
        }
        return commandAdjustment < 0;
    }

    static bool ShouldDoSimpleMerge(TransformStackCommand lhs, TransformStackCommand rhs)
    {
        if (    lhs == TransformStackCommand::TransformFloat4x4_Static
            ||  rhs == TransformStackCommand::TransformFloat4x4_Static)
            return true;

        switch (lhs) {
        case TransformStackCommand::Translate_Static:
                // only merge into another translate
            return rhs == TransformStackCommand::Translate_Static;

        case TransformStackCommand::RotateX_Static:
        case TransformStackCommand::RotateY_Static:
        case TransformStackCommand::RotateZ_Static:
        case TransformStackCommand::Rotate_Static:
                // only merge into another rotate
            return (rhs == TransformStackCommand::RotateX_Static)
                || (rhs == TransformStackCommand::RotateY_Static)
                || (rhs == TransformStackCommand::RotateZ_Static)
                || (rhs == TransformStackCommand::Rotate_Static)
                ;

        case TransformStackCommand::UniformScale_Static:
        case TransformStackCommand::ArbitraryScale_Static:
                // only merge into another scale
            return (rhs == TransformStackCommand::UniformScale_Static)
                || (rhs == TransformStackCommand::ArbitraryScale_Static)
                ;
        }

        return false;
    }

    static Float4x4 PromoteToFloat4x4(const uint32* cmd)
    {
        switch (TransformStackCommand(*cmd)) {
        case TransformStackCommand::TransformFloat4x4_Static:
            return *(const Float4x4*)(cmd+1);

        case TransformStackCommand::Translate_Static:
            return AsFloat4x4(*(const Float3*)(cmd+1));

        case TransformStackCommand::RotateX_Static:
            return AsFloat4x4(*(const RotationX*)(cmd+1));

        case TransformStackCommand::RotateY_Static:
            return AsFloat4x4(*(const RotationY*)(cmd+1));

        case TransformStackCommand::RotateZ_Static:
            return AsFloat4x4(*(const RotationZ*)(cmd+1));

        case TransformStackCommand::Rotate_Static:
            return AsFloat4x4(*(const ArbitraryRotation*)(cmd+1));

        case TransformStackCommand::UniformScale_Static:
            return AsFloat4x4(*(const UniformScale*)(cmd+1));

        case TransformStackCommand::ArbitraryScale_Static:
            return AsFloat4x4(*(const ArbitraryScale*)(cmd+1));

        default:
            assert(0);
            return Identity<Float4x4>();
        }
    }

    static void DoTransformMerge(
        std::vector<uint32>& cmdStream, 
        std::vector<uint32>::iterator dst,
        std::vector<uint32>::iterator mergingCmd)
    {
        // If the transforms are of exactly the same type (and not Rotate_Static)
        // then we can merge into a final transform that is the same type.
        // Otherwise we should merge to Float4x4. In some cases, the final Final4x4
        // can be converted into a simplier transform... We will go back through
        // and optimize those cases later.
        auto typeDst = TransformStackCommand(*dst);
        auto typeMerging = TransformStackCommand(*mergingCmd);
        if (typeDst == TransformStackCommand::Translate_Static
            && typeMerging == TransformStackCommand::Translate_Static) {

            auto& dstTrans = *(Float3*)AsPointer(dst+1);
            auto& mergeTrans = *(Float3*)AsPointer(mergingCmd+1);
            dstTrans += mergeTrans;
        } else if ((typeDst == TransformStackCommand::RotateX_Static
            && typeMerging == TransformStackCommand::RotateX_Static)
            || (typeDst == TransformStackCommand::RotateY_Static
            && typeMerging == TransformStackCommand::RotateY_Static)
            || (typeDst == TransformStackCommand::RotateZ_Static
            && typeMerging == TransformStackCommand::RotateZ_Static)) {

            auto& dstTrans = *(float*)AsPointer(dst+1);
            auto& mergeTrans = *(float*)AsPointer(mergingCmd+1);
            dstTrans += mergeTrans;
        } else if (typeDst == TransformStackCommand::UniformScale_Static
            && typeMerging == TransformStackCommand::UniformScale_Static) {

            auto& dstTrans = *(float*)AsPointer(dst+1);
            auto& mergeTrans = *(float*)AsPointer(mergingCmd+1);
            dstTrans *= mergeTrans;
        } else if (typeDst == TransformStackCommand::ArbitraryScale_Static
            && typeMerging == TransformStackCommand::ArbitraryScale_Static) {

            auto& dstTrans = *(Float3*)AsPointer(dst+1);
            auto& mergeTrans = *(Float3*)AsPointer(mergingCmd+1);
            dstTrans[0] *= mergeTrans[0];
            dstTrans[1] *= mergeTrans[1];
            dstTrans[2] *= mergeTrans[2];
        } else if (typeDst == TransformStackCommand::TransformFloat4x4_Static
            && typeMerging == TransformStackCommand::TransformFloat4x4_Static) {

            auto& dstTrans = *(Float4x4*)AsPointer(dst+1);
            auto& mergeTrans = *(Float4x4*)AsPointer(mergingCmd+1);
            dstTrans = Combine(mergeTrans, dstTrans);
        } else {
            // Otherwise we need to promote both transforms into Float4x4, and we will push
            // a new Float4x4 transform into the space in "dst"
            auto dstTransform = PromoteToFloat4x4(AsPointer(dst));
            auto mergeTransform = PromoteToFloat4x4(AsPointer(dst));
            auto t = cmdStream.erase(dst+1, dst+CommandSize(TransformStackCommand(*dst)));
            assert(t==dst+1);
            *dst = (uint32)TransformStackCommand::TransformFloat4x4_Static;
            auto finalTransform = Combine(mergeTransform, dstTransform);
            cmdStream.insert(dst+1, (uint32*)&finalTransform, (uint32*)(&finalTransform + 1));
        }
    }

    static void MergeSequentialTransforms(std::vector<uint32>& cmdStream)
    {
        // Where we have multiple static transforms in a row, we can choose
        // to merge them together.
        // We can also merge static transforms into output matrices (where
        // this is marked as ok).
        // How this works depends on what comes immediately after the static
        // transform operation:
        //      (1) another static transform -- candidate for simple merge
        //      (2) parameter transform -- blocks merging
        //      (3) WriteOutputMatrix -- possibly merge into this output matrix
        //      (4) PushLocalToWorld -- creates a branching structure whereby
        //          the static transform is going to affect multiple future
        //          operations.
        // 
        // Consider the following command structure:
        // Here, the first transform can safely merged into 3 following transforms.
        // Since they are all transforms of the same type, there is a clear benefit
        // to doing this.
        // 
        // TransformFloat4x4_Static (diag:1, 1, 1, 1)
        // PushLocalToWorld
        //      TransformFloat4x4_Static (diag:1, 1, 1, 1)
        //      WriteOutputMatrix [1] (forge_wood)
        //      PopLocalToWorld (1)
        // PushLocalToWorld
        //      TransformFloat4x4_Static (diag:1, 1, 1, 1)
        //      WriteOutputMatrix [2] (forge_woll_brick)
        //      PopLocalToWorld (1)
        // PushLocalToWorld
        //      TransformFloat4x4_Static (diag:1, 1, 1, 1)
        //      WriteOutputMatrix [3] (forge_roof_wood)
        //      PopLocalToWorld (1)
        //
        // But, sometimes when a merge is possible, it might not be desirable.
        // Consider:
        // Translate_Static ... 
        // RotateX_Static ...
        // PushLocalToWorld
        //      TransformFloat4x4_Static (diag:1, 1, 1, 1)
        //      WriteOutputMatrix [1] (forge_wood)
        //      PopLocalToWorld (1)
        // PushLocalToWorld
        //      RotateZ_Static ...
        //      WriteOutputMatrix [2] (forge_woll_brick)
        //      PopLocalToWorld (1)
        // PushLocalToWorld
        //      UniformScale_Static ....
        //      WriteOutputMatrix [3] (forge_roof_wood)
        //      PopLocalToWorld (1)
        //
        // Here we have a more complex situation, because the transforms are all
        // different types. In some cases, merging may be preferable... In others,
        // the unmerged case might be best. There is no easy way to calculate the 
        // best combination of merges for cases like this. We could build up a list
        // of potential merges, and then analyse them all to find the best... Or we
        // could just upgrade them all into 4x4 matrices and merge them all.

        for (auto i=cmdStream.begin(); i!=cmdStream.end();) {
            auto type = AsMergeType(TransformStackCommand(*i));
            auto next = i + 1 + CommandSize(TransformStackCommand(*i));
            if (type == MergeType::StaticTransform) {
                    // Search forward & find influences
                std::vector<const uint32*> influences; signed finalIdentLevel = 0;
                FindDownstreamInfluences(
                    AsPointer(next), AsPointer(cmdStream.end()),
                    influences, finalIdentLevel);

                    // We need to decide whether to merge or not.
                    // If we merge, we must do something for each
                    // downstream influence -- (either a merge, or push in
                    // a new command).
                    // We will do 2 calculations to decide whether or not
                    // to merge
                    //  1)  In the case where we have 1 static transform influence,
                    //      and that transform isn't going to be merged... We will merge
                    //      the two transforms for only certain combinations of transform
                    //      types
                    //  2)  In other cases, we will merge only if it reduces the overall
                    //      command count.
                if (influences.empty()) {
                    // no influences means this transform is redundant... just remove it
                    i = cmdStream.erase(i, next);
                    continue;
                } 
                
                bool isSpecialCase = false;
                if (influences.size() == 1 && AsMergeType(TransformStackCommand(*influences[0])) == MergeType::StaticTransform) {
                        // we have a single static transform influence. Let's check the influences for
                        // the other transform.
                    std::vector<const uint32*> secondaryInfluences; 
                    FindDownstreamInfluences(
                        influences[0], AsPointer(cmdStream.end()),
                        secondaryInfluences, finalIdentLevel);
                    isSpecialCase = !ShouldDoMerge(MakeIteratorRange(secondaryInfluences));
                }

                bool doMerge = false;
                if (isSpecialCase) {
                    doMerge = ShouldDoSimpleMerge(TransformStackCommand(*i), TransformStackCommand(*influences[0]));
                } else {
                    doMerge = ShouldDoMerge(MakeIteratorRange(influences));
                }

                if (doMerge) {
                    auto iPos = std::distance(cmdStream.begin(), i);
                    auto nextPos = std::distance(cmdStream.begin(), next);

                        // If we've decided to do the merge, we need to walk through all of the influences
                        // and do something for each one (either a merge operation, or an insertion of another
                        // command). Let's walk through the influences in reverse order, to avoid screwing up
                        // our iterators immediately
                    for (auto r=influences.rbegin(); r!=influences.rend(); ++r) {
                        auto i2 = cmdStream.begin() + (*r - AsPointer(cmdStream.begin()));
                        auto type = AsMergeType(TransformStackCommand(**r));
                        if (type == MergeType::StaticTransform) {
                            DoTransformMerge(cmdStream, i2, i);
                        } else if (type == MergeType::Blocker) {
                            // this case always involves pushing a duplicate of the original command
                            cmdStream.insert(i2, i, next);
                        } else if (type == MergeType::OutputMatrix) {
                            // We must either record this transform to be merged into
                            // this output transform, or we have to insert a push into here
                            const bool canMergeIntoOutputMatrix = true;
                            if (canMergeIntoOutputMatrix) {
                            } else
                                cmdStream.insert(i2, i, next);
                        }
                    }

                        // remove the original...
                    i = cmdStream.erase(cmdStream.begin()+iPos, cmdStream.begin()+nextPos);
                    continue;
                }
            }

            i += 1 + CommandSize(TransformStackCommand(*i));
        }
    }

    std::vector<uint32> OptimizeTransformationMachine(IteratorRange<const uint32*> input)
    {
        // Create an optimzied version of the given transformation machine.
        // We want to parse through the command stream, and optimize out redundancies.
        // Here are the changes we want to make:
        //  (1) Series of static transforms (eg, rotate, then scale, then translate)
        //      should be combined into a single Transform4x4 command
        //  (2) If a "pop" is followed by another pop, it means that one of the "pushes"
        //      is redundant. In cases like this, we can remove the push. 
        //  (3) In some cases, we can merge a static transform with the actual geometry.
        //      These cases should result in removing both the transform command and the
        //      write output matrix command.
        //  (4) Where a push is followed immediately by a pop, we can remove both.
        //  (5) We can convert static transformations into equivalent simplier types
        //      (eg, replace a matrix 4x4 transforms with an equivalent translate transform)
        //
        // Note that the order in which we consider each optimisation will effect the final
        // result (because some optimisation will create new cases for other optimisations to
        // work). To make it easy, let's consider only one optimisation at a time.

        std::vector<uint32> result(input.cbegin(), input.cend());
        RemoveRedundantPushes(result);
        MergeSequentialTransforms(result);
        RemoveRedundantPushes(result);
        return std::move(result);
    }

    inline Float3 AsFloat3(const float input[])     { return Float3(input[0], input[1], input[2]); }

    template<bool UseDebugIterator>
        void GenerateOutputTransformsFree_Int(
            Float4x4                                    result[],
            size_t                                      resultCount,
            const TransformationParameterSet*           parameterSet,
            IteratorRange<const uint32*>                commandStream,
            const std::function<void(const Float4x4&, const Float4x4&)>&     debugIterator)
    {
            //
            //      Follow the commands in our command list, and output
            //      the resulting transformations.
            //

        Float4x4 workingStack[64]; // (fairly large space on the stack)
        Float4x4* workingTransform = workingStack;
        *workingTransform = Identity<Float4x4>();

        const float*    float1s = nullptr;
        const Float3*   float3s = nullptr;
        const Float4*   float4s = nullptr;
        const Float4x4* float4x4s = nullptr;
        size_t    float1Count, float3Count, float4Count, float4x4Count;
        if (parameterSet) {
            float1s         = parameterSet->GetFloat1Parameters();
            float3s         = parameterSet->GetFloat3Parameters();
            float4s         = parameterSet->GetFloat4Parameters();
            float4x4s       = parameterSet->GetFloat4x4Parameters();
            float1Count     = parameterSet->GetFloat1ParametersCount();
            float3Count     = parameterSet->GetFloat3ParametersCount();
            float4Count     = parameterSet->GetFloat4ParametersCount();
            float4x4Count   = parameterSet->GetFloat4x4ParametersCount();
        } else {
            float1Count = float3Count = float4Count = float4x4Count = 0;
        }
        (void)float1s; (void)float3s; (void)float4s; (void)float4x4s;

        for (auto i=commandStream.cbegin(); i!=commandStream.cend();) {
            auto commandIndex = *i++;
            switch ((TransformStackCommand)commandIndex) {
            case TransformStackCommand::PushLocalToWorld:
                if ((workingTransform+1) >= &workingStack[dimof(workingStack)]) {
                    Throw(::Exceptions::BasicLabel("Exceeded maximum stack depth in GenerateOutputTransforms"));
                }
                    
                if (constant_expression<UseDebugIterator>::result() && workingTransform != workingStack)
                    debugIterator(*(workingTransform-1), *workingTransform);

                *(workingTransform+1) = *workingTransform;
                ++workingTransform;
                break;

            case TransformStackCommand::PopLocalToWorld:
                {
                    auto popCount = *i++;
                    if (workingTransform < workingStack+popCount) {
                        Throw(::Exceptions::BasicLabel("Stack underflow in GenerateOutputTransforms"));
                    }

                    workingTransform -= popCount;
                }
                break;

            case TransformStackCommand::TransformFloat4x4_Static:
                    //
                    //      Parameter is a static single precision 4x4 matrix
                    //
                {
                    // i = AdvanceTo16ByteAlignment(i);
                    const Float4x4& transformMatrix = *reinterpret_cast<const Float4x4*>(AsPointer(i)); 
                    i += 16;
                    *workingTransform = Combine(transformMatrix, *workingTransform);
                }
                break;

            case TransformStackCommand::Translate_Static:
                // i = AdvanceTo16ByteAlignment(i);
                Combine_InPlace(AsFloat3(reinterpret_cast<const float*>(AsPointer(i))), *workingTransform);
                i += 3;
                break;

            case TransformStackCommand::RotateX_Static:
                Combine_InPlace(RotationX(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::RotateY_Static:
                Combine_InPlace(RotationY(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::RotateZ_Static:
                Combine_InPlace(RotationZ(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::Rotate_Static:
                // i = AdvanceTo16ByteAlignment(i);
                *workingTransform = Combine(MakeRotationMatrix(AsFloat3(reinterpret_cast<const float*>(AsPointer(i))), Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i+3)))), *workingTransform);
                i += 4;
                break;

            case TransformStackCommand::UniformScale_Static:
                Combine_InPlace(UniformScale(*reinterpret_cast<const float*>(AsPointer(i))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Static:
                Combine_InPlace(ArbitraryScale(AsFloat3(reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i+=3;
                break;

            case TransformStackCommand::TransformFloat4x4_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float4x4Count) {
                        *workingTransform = Combine(float4x4s[parameterIndex], *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for TransformFloat4x4_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::Translate_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float3Count) {
                        Combine_InPlace(float3s[parameterIndex], *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for Translate_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateX_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationX(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateX_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateY_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationY(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateY_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateZ_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationZ(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateZ_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::Rotate_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float4Count) {
                        *workingTransform = Combine(MakeRotationMatrix(Truncate(float4s[parameterIndex]), Deg2Rad(float4s[parameterIndex][3])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for Rotate_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::UniformScale_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(UniformScale(float1s[parameterIndex]), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for UniformScale_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::ArbitraryScale_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float3Count) {
                        Combine_InPlace(ArbitraryScale(float3s[parameterIndex]), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for ArbitraryScale_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::WriteOutputMatrix:
                    //
                    //      Dump the current working transform to the output array
                    //
                {
                    uint32 outputIndex = *i++;
                    if (outputIndex < resultCount) {
                        result[outputIndex] = *workingTransform;

                        if (constant_expression<UseDebugIterator>::result() && workingTransform != workingStack)
                            debugIterator(*(workingTransform-1), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad output matrix index (" << outputIndex << ")";
                    }
                }
                break;
            }
        }
    }


    void GenerateOutputTransformsFree(
        Float4x4                                    result[],
        size_t                                      resultCount,
        const TransformationParameterSet*           parameterSet,
        IteratorRange<const uint32*>                commandStream)
    {
        GenerateOutputTransformsFree_Int<false>(
            result, resultCount, parameterSet, commandStream, 
            std::function<void(const Float4x4&, const Float4x4&)>());
    }

    void GenerateOutputTransformsFree(
        Float4x4                                    result[],
        size_t                                      resultCount,
        const TransformationParameterSet*           parameterSet,
        IteratorRange<const uint32*>                commandStream,
        const std::function<void(const Float4x4&, const Float4x4&)>&     debugIterator)
    {
        GenerateOutputTransformsFree_Int<true>(
            result, resultCount, parameterSet, commandStream, debugIterator);
    }


        ///////////////////////////////////////////////////////

    static void MakeIndentBuffer(char buffer[], unsigned bufferSize, signed identLevel)
    {
        std::fill(buffer, &buffer[std::min(std::max(0,identLevel*2), signed(bufferSize-1))], ' ');
        buffer[std::min(std::max(0,identLevel*2), signed(bufferSize-1))] = '\0';
    }

    void TraceTransformationMachine(
        std::ostream&   stream,
        const uint32*   commandStreamBegin,
        const uint32*   commandStreamEnd,
        std::function<std::string(unsigned)> outputMatrixToName,
        std::function<std::string(TransformationParameterSet::Type::Enum, unsigned)> parameterToName)
    {
        stream << "Transformation machine size: (" << (commandStreamEnd - commandStreamBegin) * sizeof(uint32) << ") bytes" << std::endl;

        char indentBuffer[32];
        signed indentLevel = 2;
        MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);

        for (auto i=commandStreamBegin; i!=commandStreamEnd;) {
            auto commandIndex = *i++;
            switch ((TransformStackCommand)commandIndex) {
            case TransformStackCommand::PushLocalToWorld:
                stream << indentBuffer << "PushLocalToWorld" << std::endl;
                ++indentLevel;
                MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                break;

            case TransformStackCommand::PopLocalToWorld:
                {
                    auto popCount = *i++;
                    stream << indentBuffer << "PopLocalToWorld (" << popCount << ")" << std::endl;
                    indentLevel -= popCount;
                    MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                }
                break;

            case TransformStackCommand::TransformFloat4x4_Static:
                {
                    auto trans = *reinterpret_cast<const Float4x4*>(AsPointer(i));
                    stream << indentBuffer << "TransformFloat4x4_Static (diag:" 
                        << trans(0,0) << ", " << trans(1,1) << ", " << trans(2,2) << ", " << trans(3,3) << ")" << std::endl;
                    i += 16;
                }
                break;

            case TransformStackCommand::Translate_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    stream << indentBuffer << "Translate_Static (" << trans[0] << ", " << trans[1] << ", " << trans[2] << ")" << std::endl;
                    i += 3;
                }
                break;

            case TransformStackCommand::RotateX_Static:
                stream << indentBuffer << "RotateX_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateY_Static:
                stream << indentBuffer << "RotateY_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateZ_Static:
                stream << indentBuffer << "RotateZ_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::Rotate_Static:
                {
                    auto a = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    float r = *reinterpret_cast<const float*>(AsPointer(i+3));
                    stream << indentBuffer << "Rotate_Static (" << a[0] << ", " << a[1] << ", " << a[2] << ")(" << r << ")" << std::endl;
                    i += 4;
                }
                break;

            case TransformStackCommand::UniformScale_Static:
                stream << indentBuffer << "UniformScale_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    stream << indentBuffer << "ArbitraryScale_Static (" << trans[0] << ", " << trans[1] << ", " << trans[2] << ")" << std::endl;
                }
                i+=3;
                break;

            case TransformStackCommand::TransformFloat4x4_Parameter:
                stream << indentBuffer << "TransformFloat4x4_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float4x4, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::Translate_Parameter:
                stream << indentBuffer << "Translate_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float3, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateX_Parameter:
                stream << indentBuffer << "RotateX_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float1, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateY_Parameter:
                stream << indentBuffer << "RotateY_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float1, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateZ_Parameter:
                stream << indentBuffer << "RotateZ_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float1, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::Rotate_Parameter:
                stream << indentBuffer << "Rotate_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float4, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::UniformScale_Parameter:
                stream << indentBuffer << "UniformScale_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float1, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Parameter:
                stream << indentBuffer << "ArbitraryScale_Parameter [" << *i << "]";
                if (parameterToName)
                    stream << " (" << parameterToName(TransformationParameterSet::Type::Float3, *i) << ")";
                stream << std::endl;
                i++;
                break;

            case TransformStackCommand::WriteOutputMatrix:
                stream << indentBuffer << "WriteOutputMatrix [" << *i << "]";
                if (outputMatrixToName)
                    stream << " (" << outputMatrixToName(*i) << ")";
                stream << std::endl;
                i++;
                break;
            }

            assert(i <= commandStreamEnd);  // make sure we haven't jumped past the end marker
        }
    }


}}


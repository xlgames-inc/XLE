// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowUniforms.h"
#include "../Techniques/DrawableDelegates.h"
#include "../../Math/ProjectionMath.h"
#include "../../Math/Transformations.h"
#include "../../Math/Matrix.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace LightingEngine
{
	void BuildShadowConstantBuffers(
		CB_ArbitraryShadowProjection& arbitraryCBSource,
		CB_OrthoShadowProjection& orthoCBSource,
		const MultiProjection<MaxShadowTexturesPerLight>& desc)
	{
		auto frustumCount = desc._normalProjCount;

		XlZeroMemory(arbitraryCBSource);   // unused array elements must be zeroed out
		XlZeroMemory(orthoCBSource);       // unused array elements must be zeroed out

		if (desc._mode == ShadowProjectionMode::Arbitrary) {

			arbitraryCBSource._projectionCount = frustumCount;
			for (unsigned c=0; c<frustumCount; ++c) {
				arbitraryCBSource._worldToProj[c] = 
					Combine(  
						desc._fullProj[c]._viewMatrix, 
						desc._fullProj[c]._projectionMatrix);
				arbitraryCBSource._minimalProj[c] = desc._minimalProjection[c];
			}

		} else if (desc._mode == ShadowProjectionMode::Ortho) {

				//  We can still fill in the constant buffer
				//  for arbitrary projection. If we use the right
				//  shader, we shouldn't need this. But we can have it
				//  prepared for comparisons.
			arbitraryCBSource._projectionCount = frustumCount;
			auto baseWorldToProj = desc._definitionViewMatrix;

			float p22 = 1.f, p23 = 0.f;

			for (unsigned c=0; c<frustumCount; ++c) {

					// we don't really need to rebuild the projection
					// matrix here. It should already be calcated in 
					// _fullProj._projectionMatrix
				const auto& projMatrix = desc._fullProj[c]._projectionMatrix;
				assert(IsOrthogonalProjection(projMatrix));

				arbitraryCBSource._worldToProj[c] = Combine(baseWorldToProj, projMatrix);
				arbitraryCBSource._minimalProj[c] = ExtractMinimalProjection(projMatrix);

				orthoCBSource._cascadeScale[c][0] = projMatrix(0,0);
				orthoCBSource._cascadeScale[c][1] = projMatrix(1,1);
				orthoCBSource._cascadeTrans[c][0] = projMatrix(0,3);
				orthoCBSource._cascadeTrans[c][1] = projMatrix(1,3);

				if (c==0) {
					p22 = projMatrix(2,2);
					p23 = projMatrix(2,3);
				}

					// (unused parts)
				orthoCBSource._cascadeScale[c][2] = 1.f;
				orthoCBSource._cascadeTrans[c][2] = 0.f;
				orthoCBSource._cascadeScale[c][3] = 1.f;
				orthoCBSource._cascadeTrans[c][3] = 0.f;
			}

				// prevent matches on the remaining frustums...
			for (unsigned c=frustumCount; c<MaxShadowTexturesPerLight; ++c) {
				orthoCBSource._cascadeTrans[c][0] = std::numeric_limits<float>::max();
				orthoCBSource._cascadeTrans[c][1] = std::numeric_limits<float>::max();
			}

				//  Also fill in the constants for ortho projection mode
			orthoCBSource._projectionCount = frustumCount;
			orthoCBSource._minimalProjection = desc._minimalProjection[0];

				//  We merge in the transform for the z component
				//  Every cascade uses the same depth range, which means we only
				//  have to adjust the X and Y components for each cascade
			auto zComponentMerge = Identity<Float4x4>();
			zComponentMerge(2,2) = p22;
			zComponentMerge(2,3) = p23;
			orthoCBSource._worldToProj = AsFloat3x4(Combine(baseWorldToProj, zComponentMerge));

				// the special "near" cascade is reached via the main transform
			orthoCBSource._nearCascade = 
				AsFloat3x4(Combine(
					Inverse(AsFloat4x4(orthoCBSource._worldToProj)), 
					desc._specialNearProjection));
			orthoCBSource._nearMinimalProjection = desc._specialNearMinimalProjection;
		}
	}

	void PreparedShadowFrustum::InitialiseConstants(
		const MultiProjection<MaxShadowTexturesPerLight>& desc)
	{
		_frustumCount = desc._normalProjCount;
		_mode = desc._mode;
		_enableNearCascade = desc._useNearProj;

		BuildShadowConstantBuffers(_arbitraryCBSource, _orthoCBSource, desc);
	}

	PreparedShadowFrustum::PreparedShadowFrustum()
	: _frustumCount(0) 
	, _mode(ShadowProjectionMode::Arbitrary)
	{}

	PreparedShadowFrustum::PreparedShadowFrustum(PreparedShadowFrustum&& moveFrom) never_throws
	: _arbitraryCBSource(std::move(moveFrom._arbitraryCBSource))
	, _orthoCBSource(std::move(moveFrom._orthoCBSource))
	, _frustumCount(moveFrom._frustumCount)
	, _enableNearCascade(moveFrom._enableNearCascade)
	, _mode(moveFrom._mode)
	{}

	PreparedShadowFrustum& PreparedShadowFrustum::operator=(PreparedShadowFrustum&& moveFrom) never_throws
	{
		_arbitraryCBSource = std::move(moveFrom._arbitraryCBSource);
		_orthoCBSource = std::move(moveFrom._orthoCBSource);
		_frustumCount = moveFrom._frustumCount;
		_enableNearCascade = moveFrom._enableNearCascade;
		_mode = moveFrom._mode;
		return *this;
	}

	bool PreparedDMShadowFrustum::IsReady() const
	{
		return true;
	}

	PreparedDMShadowFrustum SetupPreparedDMShadowFrustum(const ShadowProjectionDesc& frustum)
	{
		auto projectionCount = std::min(frustum._projections.Count(), MaxShadowTexturesPerLight);
		if (!projectionCount)
			return PreparedDMShadowFrustum{};

		PreparedDMShadowFrustum preparedResult;
		preparedResult.InitialiseConstants(frustum._projections);
		preparedResult._resolveParameters._worldSpaceBias = frustum._worldSpaceResolveBias;
		preparedResult._resolveParameters._tanBlurAngle = frustum._tanBlurAngle;
		preparedResult._resolveParameters._minBlurSearch = frustum._minBlurSearch;
		preparedResult._resolveParameters._maxBlurSearch = frustum._maxBlurSearch;
		preparedResult._resolveParameters._shadowTextureSize = (float)std::min(frustum._shadowGeneratorDesc._width, frustum._shadowGeneratorDesc._height);
		XlZeroMemory(preparedResult._resolveParameters._dummy);

		return preparedResult;
	}

	CB_ShadowResolveParameters::CB_ShadowResolveParameters()
    {
        _worldSpaceBias = -0.03f;
        _tanBlurAngle = 0.00436f;		// tan(.25 degrees)
        _minBlurSearch = 0.5f;
        _maxBlurSearch = 25.f;
        _shadowTextureSize = 1024.f;
        XlZeroMemory(_dummy);
    }

	class ShadowUniformsDelegate : public Techniques::IShaderResourceDelegate
	{
	public:
		const UniformsStreamInterface& GetInterface() override
		{
			return _interf;
		}
	private:
		UniformsStreamInterface _interf;
	};

	std::shared_ptr<Techniques::IShaderResourceDelegate> CreateShadowUniformsDelegate()
	{
		return std::make_shared<ShadowUniformsDelegate>();
	}

}}

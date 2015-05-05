// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include "Matrix.h"

namespace XLEMath
{
	// This code based on code from Geometric Tools, LLC
	// Copyright (c) 1998-2010
	// Distributed under the Boost Software License, Version 1.0.
	// http://www.boost.org/LICENSE_1_0.txt
	// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
	//
	// File Version: 4.10.0 (2009/11/18)
	//
	//  see http://www.geometrictools.com/LibFoundation/NumericalAnalysis/Wm4Eigen.h
	//		http://www.geometrictools.com/LibFoundation/NumericalAnalysis/Wm4Eigen.cpp

	T1(PrimitiveType) class Eigen
	{
    public:
		explicit Eigen( int iSize );
		explicit Eigen( const Matrix4x4T<PrimitiveType> & rkM );

		// set the matrix for eigensolving
		PrimitiveType	  & operator()( int iRow, int iCol );
		Eigen			  & operator=( const Matrix4x4T<PrimitiveType> & rkM );
		
		// Get the eigenresults (eigenvectors are columns of eigenmatrix).  The
		// GetEigenvector calls involving Vector2 and Vector3 should only be
		// called if you know that the eigenmatrix is of the appropriate size.
		PrimitiveType						GetEigenvalue( int i ) const;
		const PrimitiveType				  * GetEigenvalues() const;
		void								GetEigenvector( int i, Vector2T<PrimitiveType>& rkV ) const;
		void								GetEigenvector( int i, Vector3T<PrimitiveType>& rkV ) const;
		Vector4T<PrimitiveType>				GetEigenvector( int i ) const;
		const Matrix4x4T<PrimitiveType>	  & GetEigenvectors() const;

		// solve eigensystem
		void		EigenStuff2();
		void		EigenStuff3();
		void		EigenStuffN();
		void		EigenStuff();

		// solve eigensystem, use decreasing sort on eigenvalues
		void		DecrSortEigenStuff2 ();
		void		DecrSortEigenStuff3 ();
		void		DecrSortEigenStuffN ();
		void		DecrSortEigenStuff  ();

		// solve eigensystem, use increasing sort on eigenvalues
		void		IncrSortEigenStuff2 ();
		void		IncrSortEigenStuff3 ();
		void		IncrSortEigenStuffN ();
		void		IncrSortEigenStuff  ();

	private: 
        int						    iSize;
		Matrix4x4T<PrimitiveType>	kMat;			/* by limiting our matrix and vector sizes here, we're restricting the dimensions of eigensystems we can solve */
		Vector4T<PrimitiveType>	    afDiag;	
		Vector4T<PrimitiveType>	    afSubd;

		// For odd size matrices, the Householder reduction involves an odd
		// number of reflections.  The product of these is a reflection.  The
		// QL algorithm uses rotations for further reductions.  The final
		// orthogonal matrix whose columns are the eigenvectors is a reflection,
		// so its determinant is -1.  For even size matrices, the Householder
		// reduction involves an even number of reflections whose product is a
		// rotation.  The final orthogonal matrix has determinant +1.  Many
		// algorithms that need an eigendecomposition want a rotation matrix.
		// We want to guarantee this is the case, so m_bRotation keeps track of
		// this.  The DecrSort and IncrSort further complicate the issue since
		// they swap columns of the orthogonal matrix, causing the matrix to
		// toggle between rotation and reflection.  The value m_bRotation must
		// be toggled accordingly.
		bool		bIsRotation;
		void		GuaranteeRotation ();

		// Householder reduction to tridiagonal form
		void		Tridiagonal2 ();
		void		Tridiagonal3 ();
		void		TridiagonalN ();

		// QL algorithm with implicit shifting, applies to tridiagonal matrices
		bool		QLAlgorithm ();

		// sort eigenvalues from largest to smallest
		void		DecreasingSort ();

		// sort eigenvalues from smallest to largest
		void		IncreasingSort ();
	};

	typedef Eigen<float> Eigenf;
	typedef Eigen<double> Eigend;

}

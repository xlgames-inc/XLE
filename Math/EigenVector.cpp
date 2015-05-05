// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EigenVector.h"
#include <assert.h>

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

	//----------------------------------------------------------------------------
	T1(PrimitiveType) Eigen<PrimitiveType>::Eigen (int _iSize)
	{
		assert(_iSize >= 2 && _iSize <= 4);
		iSize = _iSize;
		bIsRotation = false;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType) Eigen<PrimitiveType>::Eigen(const Matrix4x4T<PrimitiveType>& rkM)
		: kMat(rkM)
	{
		iSize = 4;
		bIsRotation = false;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	PrimitiveType& Eigen<PrimitiveType>::operator() (int iRow, int iCol)
	{
		return kMat(iRow, iCol);
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	Eigen<PrimitiveType>& Eigen<PrimitiveType>::operator= (const Matrix4x4T<PrimitiveType>& rkM)
	{
		kMat = rkM;
		return *this;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	PrimitiveType Eigen<PrimitiveType>::GetEigenvalue (int i) const
	{
		return afDiag[i];
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	const PrimitiveType* Eigen<PrimitiveType>::GetEigenvalues () const
	{
		return &afDiag[0];
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::GetEigenvector (int i, Vector2T<PrimitiveType>& rkV) const
	{
		assert(iSize == 2);
		if (iSize == 2)
		{
			for (int iRow = 0; iRow < iSize; iRow++)
			{
				rkV[iRow] = kMat(iRow, i);
			}
		}
		else
		{
			rkV = Zero<Vector2T<PrimitiveType>>();
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::GetEigenvector (int i, Vector3T<PrimitiveType>& rkV) const
	{
		assert(iSize == 3);
		if (iSize == 3)
		{
			for (int iRow = 0; iRow < iSize; iRow++)
			{
				rkV[iRow] = kMat(iRow, i);
			}
		}
		else
		{
			rkV = Zero<Vector3T<PrimitiveType>>();
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	Vector4T<PrimitiveType> Eigen<PrimitiveType>::GetEigenvector (int i) const
	{
        return Vector4T<PrimitiveType>(kMat(0, i), kMat(1, i), kMat(2, i), kMat(3, i));
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	const Matrix4x4T<PrimitiveType>& Eigen<PrimitiveType>::GetEigenvectors () const
	{
		return kMat;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::Tridiagonal2 ()
	{
		// matrix is already tridiagonal
		afDiag[0] = kMat(0,0);
		afDiag[1] = kMat(1,1);
		afSubd[0] = kMat(0,1);
		afSubd[1] = (PrimitiveType)0.0;
		kMat(0, 0) = (PrimitiveType)1.0;
		kMat(0, 1) = (PrimitiveType)0.0;
		kMat(1, 0) = (PrimitiveType)0.0;
		kMat(1, 1) = (PrimitiveType)1.0;

		bIsRotation = true;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::Tridiagonal3 ()
	{
		PrimitiveType fM00 = kMat(0, 0);
		PrimitiveType fM01 = kMat(0, 1);
		PrimitiveType fM02 = kMat(0, 2);
		PrimitiveType fM11 = kMat(1, 1);
		PrimitiveType fM12 = kMat(1, 2);
		PrimitiveType fM22 = kMat(2, 2);

		const PrimitiveType zeroTolerance = (PrimitiveType)0.000001;

		afDiag[0] = fM00;
		afSubd[2] = (PrimitiveType)0.0;
		if (XlAbs(fM02) > zeroTolerance)
		{
			PrimitiveType fLength = XlSqrt(fM01*fM01+fM02*fM02);
			PrimitiveType fInvLength = ((PrimitiveType)1.0)/fLength;
			fM01 *= fInvLength;
			fM02 *= fInvLength;
			PrimitiveType fQ = ((PrimitiveType)2.0)*fM01*fM12+fM02*(fM22-fM11);
			afDiag[1] = fM11+fM02*fQ;
			afDiag[2] = fM22-fM02*fQ;
			afSubd[0] = fLength;
			afSubd[1] = fM12-fM01*fQ;
			kMat(0, 0) = (PrimitiveType)1.0;
			kMat(0, 1) = (PrimitiveType)0.0;
			kMat(0, 2) = (PrimitiveType)0.0;
			kMat(1, 0) = (PrimitiveType)0.0;
			kMat(1, 1) = fM01;
			kMat(1, 2) = fM02;
			kMat(2, 0) = (PrimitiveType)0.0;
			kMat(2, 1) = fM02;
			kMat(2, 2) = -fM01;
			bIsRotation = false;
		}
		else
		{
			afDiag[1] = fM11;
			afDiag[2] = fM22;
			afSubd[0] = fM01;
			afSubd[1] = fM12;
			kMat(0, 0) = (PrimitiveType)1.0;
			kMat(0, 1) = (PrimitiveType)0.0;
			kMat(0, 2) = (PrimitiveType)0.0;
			kMat(1, 0) = (PrimitiveType)0.0;
			kMat(1, 1) = (PrimitiveType)1.0;
			kMat(1, 2) = (PrimitiveType)0.0;
			kMat(2, 0) = (PrimitiveType)0.0;
			kMat(2, 1) = (PrimitiveType)0.0;
			kMat(2, 2) = (PrimitiveType)1.0;
			bIsRotation = true;
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::TridiagonalN ()
	{
		int i0, i1, i2, i3;

		for (i0 = iSize-1, i3 = iSize-2; i0 >= 1; i0--, i3--)
		{
			PrimitiveType fH = (PrimitiveType)0.0, fScale = (PrimitiveType)0.0;

			if (i3 > 0)
			{
				for (i2 = 0; i2 <= i3; i2++)
				{
					fScale += XlAbs(kMat(i0, i2));
				}
				if (fScale == (PrimitiveType)0.0)
				{
					afSubd[i0] = kMat(i0, i3);
				}
				else
				{
					PrimitiveType fInvScale = ((PrimitiveType)1.0)/fScale;
					for (i2 = 0; i2 <= i3; i2++)
					{
						kMat(i0, i2) *= fInvScale;
						fH += kMat(i0, i2)*kMat(i0, i2);
					}
					PrimitiveType fF = kMat(i0, i3);
					PrimitiveType fG = XlSqrt(fH);
					if (fF > (PrimitiveType)0.0)
					{
						fG = -fG;
					}
					afSubd[i0] = fScale*fG;
					fH -= fF*fG;
					kMat(i0, i3) = fF-fG;
					fF = (PrimitiveType)0.0;
					PrimitiveType fInvH = ((PrimitiveType)1.0)/fH;
					for (i1 = 0; i1 <= i3; i1++)
					{
						kMat(i1, i0) = kMat(i0, i1)*fInvH;
						fG = (PrimitiveType)0.0;
						for (i2 = 0; i2 <= i1; i2++)
						{
							fG += kMat(i1, i2)*kMat(i0, i2);
						}
						for (i2 = i1+1; i2 <= i3; i2++)
						{
							fG += kMat(i2, i1)*kMat(i0, i2);
						}
						afSubd[i1] = fG*fInvH;
						fF += afSubd[i1]*kMat(i0, i1);
					}
					PrimitiveType fHalfFdivH = ((PrimitiveType)0.5)*fF*fInvH;
					for (i1 = 0; i1 <= i3; i1++)
					{
						fF = kMat(i0, i1);
						fG = afSubd[i1] - fHalfFdivH*fF;
						afSubd[i1] = fG;
						for (i2 = 0; i2 <= i1; i2++)
						{
							kMat(i1, i2) -= fF*afSubd[i2] +
								fG*kMat(i0, i2);
						}
					}
				}
			}
			else
			{
				afSubd[i0] = kMat(i0, i3);
			}

			afDiag[i0] = fH;
		}

		afDiag[0] = (PrimitiveType)0.0;
		afSubd[0] = (PrimitiveType)0.0;
		for (i0 = 0, i3 = -1; i0 <= iSize-1; i0++, i3++)
		{
			if (afDiag[i0] != (PrimitiveType)0.0)
			{
				for (i1 = 0; i1 <= i3; i1++)
				{
					PrimitiveType fSum = (PrimitiveType)0.0;
					for (i2 = 0; i2 <= i3; i2++)
					{
						fSum += kMat(i0, i2)*kMat(i2, i1);
					}
					for (i2 = 0; i2 <= i3; i2++)
					{
						kMat(i2, i1) -= fSum*kMat(i2, i0);
					}
				}
			}
			afDiag[i0] = kMat(i0, i0);
			kMat(i0, i0) = (PrimitiveType)1.0;
			for (i1 = 0; i1 <= i3; i1++)
			{
				kMat(i1, i0) = (PrimitiveType)0.0;
				kMat(i0, i1) = (PrimitiveType)0.0;
			}
		}

		// re-ordering if Eigen::QLAlgorithm is used subsequently
		for (i0 = 1, i3 = 0; i0 < iSize; i0++, i3++)
		{
			afSubd[i3] = afSubd[i0];
		}
		afSubd[iSize-1] = (PrimitiveType)0.0;

		bIsRotation = ((iSize % 2) == 0);
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	bool Eigen<PrimitiveType>::QLAlgorithm ()
	{
		const int iMaxIter = 32;

		for (int i0 = 0; i0 < iSize; i0++)
		{
			int i1;
			for (i1 = 0; i1 < iMaxIter; i1++)
			{
				int i2;
				for (i2 = i0; i2 <= iSize-2; i2++)
				{
					PrimitiveType fTmp = XlAbs(afDiag[i2]) +
						XlAbs(afDiag[i2+1]);

					if (XlAbs(afSubd[i2]) + fTmp == fTmp)
					{
						break;
					}
				}
				if (i2 == i0)
				{
					break;
				}

				PrimitiveType fG = (afDiag[i0+1] - afDiag[i0])/(((PrimitiveType)2.0) *
					afSubd[i0]);
				PrimitiveType fR = XlSqrt(fG*fG+(PrimitiveType)1.0);
				if ( fG < (PrimitiveType)0.0 )
				{
					fG = afDiag[i2]-afDiag[i0]+afSubd[i0]/(fG-fR);
				}
				else
				{
					fG = afDiag[i2]-afDiag[i0]+afSubd[i0]/(fG+fR);
				}
				PrimitiveType fSin = (PrimitiveType)1.0, fCos = (PrimitiveType)1.0, fP = (PrimitiveType)0.0;
				for (int i3 = i2-1; i3 >= i0; i3--)
				{
					PrimitiveType fF = fSin*afSubd[i3];
					PrimitiveType fB = fCos*afSubd[i3];
					if (XlAbs(fF) >= XlAbs(fG))
					{
						fCos = fG/fF;
						fR = XlSqrt(fCos*fCos+(PrimitiveType)1.0);
						afSubd[i3+1] = fF*fR;
						fSin = ((PrimitiveType)1.0)/fR;
						fCos *= fSin;
					}
					else
					{
						fSin = fF/fG;
						fR = XlSqrt(fSin*fSin+(PrimitiveType)1.0);
						afSubd[i3+1] = fG*fR;
						fCos = ((PrimitiveType)1.0)/fR;
						fSin *= fCos;
					}
					fG = afDiag[i3+1]-fP;
					fR = (afDiag[i3]-fG)*fSin+((PrimitiveType)2.0)*fB*fCos;
					fP = fSin*fR;
					afDiag[i3+1] = fG+fP;
					fG = fCos*fR-fB;

					for (int i4 = 0; i4 < iSize; i4++)
					{
						fF = kMat(i4, i3+1);
						kMat(i4, i3+1) = fSin*kMat(i4, i3)+fCos*fF;
						kMat(i4, i3) = fCos*kMat(i4, i3)-fSin*fF;
					}
				}
				afDiag[i0] -= fP;
				afSubd[i0] = fG;
				afSubd[i2] = (PrimitiveType)0.0;
			}
			if (i1 == iMaxIter)
			{
				return false;
			}
		}

		return true;
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::DecreasingSort ()
	{
		// sort eigenvalues in decreasing order, e[0] >= ... >= e[iSize-1]
		for (int i0 = 0, i1; i0 <= iSize-2; i0++)
		{
			// locate maximum eigenvalue
			i1 = i0;
			PrimitiveType fMax = afDiag[i1];
			int i2;
			for (i2 = i0+1; i2 < iSize; i2++)
			{
				if (afDiag[i2] > fMax)
				{
					i1 = i2;
					fMax = afDiag[i1];
				}
			}

			if (i1 != i0)
			{
				// swap eigenvalues
				afDiag[i1] = afDiag[i0];
				afDiag[i0] = fMax;

				// swap eigenvectors
				for (i2 = 0; i2 < iSize; i2++)
				{
					PrimitiveType fTmp = kMat(i2, i0);
					kMat(i2, i0) = kMat(i2, i1);
					kMat(i2, i1) = fTmp;
					bIsRotation = !bIsRotation;
				}
			}
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::IncreasingSort ()
	{
		// sort eigenvalues in increasing order, e[0] <= ... <= e[iSize-1]
		for (int i0 = 0, i1; i0 <= iSize-2; i0++)
		{
			// locate minimum eigenvalue
			i1 = i0;
			PrimitiveType fMin = afDiag[i1];
			int i2;
			for (i2 = i0+1; i2 < iSize; i2++)
			{
				if (afDiag[i2] < fMin)
				{
					i1 = i2;
					fMin = afDiag[i1];
				}
			}

			if (i1 != i0)
			{
				// swap eigenvalues
				afDiag[i1] = afDiag[i0];
				afDiag[i0] = fMin;

				// swap eigenvectors
				for (i2 = 0; i2 < iSize; i2++)
				{
					PrimitiveType fTmp = kMat(i2, i0);
					kMat(i2, i0) = kMat(i2, i1);
					kMat(i2, i1) = fTmp;
					bIsRotation = !bIsRotation;
				}
			}
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::GuaranteeRotation ()
	{
		if (!bIsRotation)
		{
			// change sign on the first column
			for (int iRow = 0; iRow < iSize; iRow++)
			{
				kMat(iRow, 0) = -kMat(iRow, 0);
			}
		}
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::EigenStuff2 ()
	{
		Tridiagonal2();
		QLAlgorithm();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::EigenStuff3 ()
	{
		Tridiagonal3();
		QLAlgorithm();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::EigenStuffN ()
	{
		TridiagonalN();
		QLAlgorithm();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::EigenStuff ()
	{
		switch (iSize)
		{
			case 2:  Tridiagonal2();  break;
			case 3:  Tridiagonal3();  break;
			default: TridiagonalN();  break;
		}
		QLAlgorithm();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::DecrSortEigenStuff2 ()
	{
		Tridiagonal2();
		QLAlgorithm();
		DecreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::DecrSortEigenStuff3 ()
	{
		Tridiagonal3();
		QLAlgorithm();
		DecreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::DecrSortEigenStuffN ()
	{
		TridiagonalN();
		QLAlgorithm();
		DecreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::DecrSortEigenStuff ()
	{
		switch (iSize)
		{
			case 2:  Tridiagonal2();  break;
			case 3:  Tridiagonal3();  break;
			default: TridiagonalN();  break;
		}
		QLAlgorithm();
		DecreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::IncrSortEigenStuff2 ()
	{
		Tridiagonal2();
		QLAlgorithm();
		IncreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::IncrSortEigenStuff3 ()
	{
		Tridiagonal3();
		QLAlgorithm();
		IncreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::IncrSortEigenStuffN ()
	{
		TridiagonalN();
		QLAlgorithm();
		IncreasingSort();
		GuaranteeRotation();
	}
	//----------------------------------------------------------------------------
	T1(PrimitiveType)
	void Eigen<PrimitiveType>::IncrSortEigenStuff ()
	{
		switch (iSize)
		{
			case 2:  Tridiagonal2();  break;
			case 3:  Tridiagonal3();  break;
			default: TridiagonalN();  break;
		}
		QLAlgorithm();
		IncreasingSort();
		GuaranteeRotation();
	}



	template class Eigen<float>;
	template class Eigen<double>;
}



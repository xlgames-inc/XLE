// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)



/////////////////// /////////////////// /////////////////// /////////////////// 
    //      Utilities for order independent transparency resolve

void Swap(inout SortingElement lhs, inout SortingElement rhs)
{
	SortingElement x = lhs;
	lhs = rhs;
	rhs = x;
}

#if LIMIT_LAYER_COUNT==1
	static const uint FixedSampleCount = 6;
#else
	static const uint FixedSampleCount = 12;
#endif

void BubbleSort(inout SortingElement sortingBuffer[FixedSampleCount], uint startingLeft, uint startingRight)
{
	[loop] for (uint q=0; q<(startingRight-startingLeft+1); q++) {

		[loop] for (uint c=startingLeft; c<startingRight; c++) {
			if (Less(sortingBuffer[c], sortingBuffer[c+1])) {
				Swap(sortingBuffer[c], sortingBuffer[c+1]);
			}
		}

	}
}

uint Partition(inout SortingElement sortingBuffer[FixedSampleCount], uint left, uint right, uint pivotIndex)
{
	SortingElement pivotValue = sortingBuffer[pivotIndex];
	Swap(sortingBuffer[pivotIndex], sortingBuffer[right]);
	uint storeIndex = left;
	for (uint i=left; i<right; ++i) {
		if (!Less(sortingBuffer[i], pivotValue)) {
			Swap(sortingBuffer[i], sortingBuffer[storeIndex]);
			++storeIndex;
		}
	}
	Swap(sortingBuffer[storeIndex], sortingBuffer[right]);
	return storeIndex;
}

SortingElement FindPivot(SortingElement sortingBuffer[FixedSampleCount], uint left, uint right)
{
		//		Is all this work helpful to find the pivot?
		//		would it be best just to return the 'middle'
		//		every time, even if the pivot is not as optimal?
	uint middle = (left+right)/2;
	// return middle;
	SortingElement a = sortingBuffer[left];
	SortingElement b = sortingBuffer[middle];
	SortingElement c = sortingBuffer[right];

	if (Less(a, b)) {
		if (Less(c, a)) {
			return left;
		} else if (Less(c,b)) {
			return right;
		} else {
			return middle;
		}
	} else {
		// b < a
		if (Less(c, b)) {
			return middle;
		} else if (Less(c, a)) {
			return right;
		} else {
			return left;
		}
	}
}

bool Quicksort(inout SortingElement sortingBuffer[FixedSampleCount], uint startingLeft, uint startingRight)
{
		//
		//	Sort from largest depth value to smallest 
		//		depth value...
		//
	const uint SortingStackMaxLength = 6;
	uint2 sortingStack[SortingStackMaxLength];
	uint stackSize = 1;
	sortingStack[0] = uint2(startingLeft, startingRight);

	bool result = true;

		//	Recursive functions aren't allowed...
		//		so we have to set up a stack and a loop.
	[loop] do {
		uint left = sortingStack[stackSize-1].x;
		uint right = sortingStack[stackSize-1].y;

		int diff = right-left;
		[branch] if (diff > 2) {

			SortingElement pivot = FindPivot(sortingBuffer, left, right);
			uint newPivotIndex = Partition(sortingBuffer, left, right, pivot);
			
			#if defined(_DEBUG)
				for (uint c=left; c<newPivotIndex; ++c) {
					if (Less(sortingBuffer[c], sortingBuffer[newPivotIndex])) {
						result = false;
					}
				}

				for (uint c2=newPivotIndex+1; c2<=right; ++c2) {
					if (!Less(sortingBuffer[c2], sortingBuffer[newPivotIndex])) {
						result = false;
					}
				}
			#endif

			sortingStack[stackSize-1].y = newPivotIndex-1;
			if ((stackSize+1)<=SortingStackMaxLength) {
				sortingStack[stackSize].x = newPivotIndex+1;
				sortingStack[stackSize].y = right;
				++stackSize;
			} else {
				#if defined(_DEBUG)
					result = false;
				#endif
			}

		} else if (diff == 2) {

			--stackSize;

				// 3 elements... basic bubble sort style solution
			if (Less(sortingBuffer[left], sortingBuffer[left+1])) {
				Swap(sortingBuffer[left], sortingBuffer[left+1]);
			}
			if (Less(sortingBuffer[left+1], sortingBuffer[left+2])) {
				Swap(sortingBuffer[left+1], sortingBuffer[left+2]);
				if (Less(sortingBuffer[left], sortingBuffer[left+1])) {
					Swap(sortingBuffer[left], sortingBuffer[left+1]);
				}
			}
			// BubbleSort(sortingBuffer, left, right);

		} else if (diff == 1) {

			--stackSize;

				// 2 elements, easy sort
			if (Less(sortingBuffer[left], sortingBuffer[right])) {
				Swap(sortingBuffer[left], sortingBuffer[right]);
			}

		} else {
			--stackSize;
		}
	} while (stackSize > 0);

	return result;
}

void InsertBefore_SlideLeft(inout SortingElement sortingBuffer[FixedSampleCount], uint sampleCount, uint insertLocation, SortingElement insertItem)
{
	[branch] if (sampleCount < FixedSampleCount) {
		for (uint c=min(sampleCount, FixedSampleCount)-1; c>insertLocation; c--) {
			sortingBuffer[c] = sortingBuffer[c-1];
		}
	} else {
		for (uint c=0; c<insertLocation; c++) {
			sortingBuffer[c] = sortingBuffer[c+1];
		}
	}
	sortingBuffer[insertLocation] = insertItem;
}

void SortedInsert_LimitedBuffer(inout SortingElement sortingBuffer[FixedSampleCount], SortingElement insertItem)
{
		// insert assuming buffer is already fill to "FixedSampleCount" items
	if (Less(insertItem, sortingBuffer[0]) && !Less(insertItem, sortingBuffer[1])) {
		sortingBuffer[0] = insertItem;
		return;
	}

	for (uint c2=1; c2<FixedSampleCount; ++c2) {
		if (!Less(insertItem, sortingBuffer[c2])) {
			InsertBefore_SlideLeft(sortingBuffer, FixedSampleCount, c2-1, insertItem);
			return;
		}
	}
	for (uint c3=0; c3<FixedSampleCount-1; c3++) {
		sortingBuffer[c3] = sortingBuffer[c3+1];
	}
	sortingBuffer[FixedSampleCount-1] = insertItem;
}

void InsertBefore(inout SortingElement sortingBuffer[FixedSampleCount], uint sampleCount, uint insertLocation, SortingElement insertItem)
{
		// (potentially expensive loop in massively parrallel world...?)
	for (uint c=min(sampleCount, FixedSampleCount-1); c>insertLocation; c--) {
		sortingBuffer[c] = sortingBuffer[c-1];
	}
	sortingBuffer[insertLocation] = insertItem;
}

void SortedInsert(inout SortingElement sortingBuffer[FixedSampleCount], inout uint sampleCount, SortingElement insertItem)
{
		//	There are a lot of branches here... How efficient can this be in a pixel shader?
	for (uint c=0; c<sampleCount; ++c) {
		if (!Less(insertItem, sortingBuffer[c])) {
			InsertBefore(sortingBuffer, sampleCount, c, insertItem);
			sampleCount = min(sampleCount+1, FixedSampleCount);
			return;
		}
	}

	if (sampleCount < FixedSampleCount) {
		sortingBuffer[sampleCount] = insertItem;
		++sampleCount;
	}

		//	Do a binary search for the insertion position
//	uint2 searchArea = uint2(0, sampleCount-1);
//	[loop] while (true) {
//
//			// If we've narrowed down the search area, insert the item directly
//		if (searchArea.x == searchArea.y) {
//			uint finalLocation;
//			if (sortingBuffer[searchArea.x].y < insertItem.y) {
//				finalLocation = searchArea.x+1;
//				if (finalLocation == FixedSampleCount) {
//					return;
//				}
//			} else {
//				finalLocation = searchArea.x;
//			}
//			InsertBefore(sortingBuffer, sampleCount, finalLocation, insertItem);
//			sampleCount = min(sampleCount+1, FixedSampleCount);
//			return;
//		}
//
//		uint test = (searchArea.x+searchArea.y)/2;
//		if (sortingBuffer[test].y < insertItem.y) {
//			searchArea = uint2(test+1, searchArea.y);
//		} else {
//			searchArea = uint2(searchArea.x, test-1);
//		}
//	}
}


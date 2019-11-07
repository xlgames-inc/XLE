#include "FiniteResourceHeap.h"

namespace SceneEngine
{
    template<typename Key>
        void FiniteResourceHeap<Key>::ProcessRequests(IteratorRange<RequestResult*> results,
                                                      IteratorRange<const Key*> requests) {

            unsigned missingRequests[requests.size()];
            unsigned missingRequestsCount = 0;

            unsigned frozenCount = 0;

            #if defined(_DEBUG)
                bool frozenBlocks[_blockStates.size()];
                for (unsigned c=0; c<unsigned(_blockStates.size()); ++c) frozenBlocks[c] = false;
            #endif

            for (unsigned c=0; c<requests.size(); ++c) {
                auto k = requests[c];
                auto i = std::find_if(_blockStates.begin(), _blockStates.end(),
                    [k](const Block& b) { return (b._state != Block::State::Empty) && (b._key == k); });
                if (i!=_blockStates.end()) {
                    auto blockStateIndex = (unsigned)std::distance(_blockStates.begin(), i);
                    results[c]._index = blockStateIndex;
                    results[c]._result = (i->_state == Block::State::Occupied) ? RequestResult::Result::Available : RequestResult::Result::CurrentlyTransferring;
                    results[c]._previousKey = k;
                    _lruQueue.BringToFront(blockStateIndex);

                    #if defined(_DEBUG)
                        assert(!frozenBlocks[blockStateIndex]);
                        frozenBlocks[blockStateIndex] = true;
                    #endif

                    ++frozenCount;
                } else {
                    missingRequests[missingRequestsCount++] = c;
                }
            }
            
            assert(frozenCount <= unsigned(_blockStates.size()));
            auto maxEvict = unsigned(_blockStates.size()) - frozenCount;
            unsigned c=0;
            for (; c<std::min(missingRequestsCount, maxEvict); ++c) {
                auto oldest = _lruQueue.GetOldestValue();
                if (_blockStates[oldest]._state == Block::State::Transfer)
                    break;  // hit something still transferring. We can't evict while transferring

                results[missingRequests[c]]._previousKey = (_blockStates[oldest]._state != Block::State::Empty) ? _blockStates[oldest]._key : _dummyKey;
                _blockStates[oldest]._state = Block::State::Transfer;
                _blockStates[oldest]._key = requests[missingRequests[c]];
                results[missingRequests[c]]._index = oldest;
                results[missingRequests[c]]._result = RequestResult::Result::InitiateTransfer;

                _lruQueue.BringToFront(oldest);
            }
            
            for (; c<missingRequestsCount; ++c) {
                results[missingRequests[c]]._result = RequestResult::Result::Overflow;
                results[missingRequests[c]]._index = ~0u;
                results[missingRequests[c]]._previousKey = _dummyKey;
            }
        }
    
    template<typename Key>
        void FiniteResourceHeap<Key>::CancelTransfer(unsigned idx) {
            assert(_blockStates[idx]._state == Block::State::Transfer);
            _blockStates[idx]._state = Block::State::Empty;
            _lruQueue.SendToBack(idx);
        }
    
    template<typename Key>
        void FiniteResourceHeap<Key>::CompleteTransfer(unsigned idx) {
            assert(_blockStates[idx]._state == Block::State::Transfer);
            _blockStates[idx]._state = Block::State::Occupied;
        }
    
    template<typename Key>
        void FiniteResourceHeap<Key>::Evict(unsigned idx) {
            assert(_blockStates[idx]._state == Block::State::Occupied);
            _blockStates[idx]._state = Block::State::Empty;
            _lruQueue.SendToBack(idx);
        }
    
    template<typename Key>
        FiniteResourceHeap<Key>::FiniteResourceHeap(unsigned count, const Key& dummyKey)
        : _lruQueue(count)
        , _dummyKey(dummyKey) {
            _blockStates.resize(count, Block { Block::State::Empty, dummyKey });
            for (unsigned c=0; c<count; ++c)
                _lruQueue.BringToFront(c);
        }
    
    template<typename Key>
        FiniteResourceHeap<Key>::FiniteResourceHeap() {}
    
    template<typename Key>
        FiniteResourceHeap<Key>::~FiniteResourceHeap() {}

    template class FiniteResourceHeap<Int2>;
    template class FiniteResourceHeap<Int3>;
    template class FiniteResourceHeap<uint64_t>;
}


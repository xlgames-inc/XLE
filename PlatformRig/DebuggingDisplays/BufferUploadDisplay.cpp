// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BufferUploadDisplay.h"
#include "../../RenderOverlays/OverlayUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/StringUtils.h"
#include <assert.h>

#pragma warning(disable:4127)       // warning C4127: conditional expression is constant

namespace PlatformRig { namespace Overlays
{
    static void DrawButton(IOverlayContext* context, const char name[], const Rect&buttonRect, Interactables&interactables, InterfaceState& interfaceState)
    {
        InteractableId id = InteractableId_Make(name);
        DrawButtonBasic(context, buttonRect, name, FormatButton(interfaceState, id));
        interactables.Register(Interactables::Widget(buttonRect, id));
    }

    BufferUploadDisplay::GPUMetrics::GPUMetrics()
    {
        _slidingAverageCostMS = 0.f;
        _slidingAverageBytesPerSecond = 0;
    }

    BufferUploadDisplay::FrameRecord::FrameRecord()
    {
        _frameId = 0x0;
        _gpuCost = 0.f;
        _commandListStart = _commandListEnd = ~unsigned(0x0);
    }

    BufferUploadDisplay::BufferUploadDisplay(BufferUploads::IManager* manager)
    : _manager(manager)
    {
        _graphMinValueHistory = _graphMaxValueHistory = 0.f;
        XlZeroMemory(_accumulatedCreateCount);
        XlZeroMemory(_accumulatedCreateBytes);
        XlZeroMemory(_accumulatedUploadCount);
        XlZeroMemory(_accumulatedUploadBytes);
        _graphsMode = 0;
        _mostRecentGPUFrequency = 0;
        _lastUploadBeginTime = 0;
        _mostRecentGPUCost = 0.f;
        _mostRecentGPUFrameId = 0;
        _lockedFrameId = ~unsigned(0x0);
        _drawHistory = false;

        #if defined(WIN32) || defined(WIN64)
            int64 timerFrequency;
            QueryPerformanceFrequency((LARGE_INTEGER*)&timerFrequency);
            _reciprocalTimerFrequency = 1./double(timerFrequency);
        #endif

        assert(s_gpuListenerDisplay==0);
        s_gpuListenerDisplay = this;
    }

    BufferUploadDisplay::~BufferUploadDisplay()
    {
        assert(s_gpuListenerDisplay==this);
        s_gpuListenerDisplay = NULL;
    }

    static const char* AsString(BufferUploads::UploadDataType::Enum value)
    {
        using namespace BufferUploads::UploadDataType;
        switch (value) {
        case Texture:   return "Texture";
        case Vertex:    return "Vertex";
        case Index:     return "Index";
        default: return "<<unknown>>";
        }
    };

    static std::string BuildDescription(const BufferUploads::BufferDesc& desc)
    {
        using namespace BufferUploads;
        char buffer[2048];
        if (desc._type == BufferDesc::Type::Texture) {
            const TextureDesc& tDesc = desc._textureDesc;
            xl_snprintf(buffer, dimof(buffer), "[%s] Tex(%s) (%4ix%4i) mips:(%i)", 
                desc._name, (tDesc._dimensionality==TextureDesc::Dimensionality::T2D)?"  2D":"Cube",
                tDesc._width, tDesc._height, tDesc._mipCount);
        } else if (desc._type == BufferDesc::Type::LinearBuffer) {
            if (desc._bindFlags & BindFlag::VertexBuffer) {
                xl_snprintf(buffer, dimof(buffer), "[%s] VB (%6.2fkb)", 
                    desc._name, desc._linearBufferDesc._sizeInBytes/1024.f);
            } else if (desc._bindFlags & BindFlag::IndexBuffer) {
                xl_snprintf(buffer, dimof(buffer), "[%s] IB (%6.2fkb)", 
                    desc._name, desc._linearBufferDesc._sizeInBytes/1024.f);
            }
        } else {
            xl_snprintf(buffer, dimof(buffer), "Unknown");
        }
        return std::string(buffer);
    }

    static unsigned& GetFrameID() 
    { 
        static unsigned s_frameId = 0;
        return s_frameId; 
    }

    namespace GraphTabs
    {
        enum Enum
        {
            Uploads, 
            CreatesMB, CreatesCount, DeviceCreatesCount, 
            Latency, PendingBuffers, CommandListCount, 
            GPUCost, GPUBytesPerSecond, AveGPUCost, 
            ThreadActivity, BatchedCopy, FramePriorityStall
        };
        static const char* Names[] = {
            "Uploads (MB)", "Creates (MB)", "Creates (count)", "Device creates (count)", "Latency (ms)", "Pending Buffers (MB)", "Command List Count", "GPU Cost", "GPU bytes/second", "Ave GPU cost", "Thread Activity", "Batched copy", "Frame Priority Stalls"
        };
    }

    static const unsigned s_MaxGraphSegments = 256;

    void    BufferUploadDisplay::Render(IOverlayContext* context, Layout& layout, Interactables& interactables, InterfaceState& interfaceState)
    {
        using namespace BufferUploads;
        CommandListMetrics mostRecentResults;
        unsigned commandListCount = 0;

            //      Keep popping metrics from the upload manager until we stop getting valid ones            
        BufferUploads::IManager* manager = _manager;
        if (manager) {
            for (;;) {
                CommandListMetrics metrics = manager->PopMetrics();
                if (!metrics._commitTime) {
                    break;
                }
                mostRecentResults = metrics;
                _recentHistory.push_back(metrics);
                AddCommandListToFrame(metrics._frameId, unsigned(_recentHistory.size()-1));
                for (unsigned c=0; c<BufferUploads::UploadDataType::Max; ++c) {
                    _accumulatedCreateCount[c] += metrics._countCreations[c];
                    _accumulatedCreateBytes[c] += metrics._bytesCreated[c];
                    _accumulatedUploadCount[c] += metrics._countUploaded[c];
                    _accumulatedUploadBytes[c] += metrics._bytesUploaded[c] + metrics._bytesUploadedDuringCreation[c];
                }
                ++commandListCount;
            }
        }

        if (!mostRecentResults._commitTime && _recentHistory.size()) {
            mostRecentResults = _recentHistory[_recentHistory.size()-1];
        }

        {
            ScopedLock(_gpuEventsBufferLock);
            ProcessGPUEvents_MT(AsPointer(_gpuEventsBuffer.begin()), AsPointer(_gpuEventsBuffer.end()));
            _gpuEventsBuffer.erase(_gpuEventsBuffer.begin(), _gpuEventsBuffer.end());
        }

            //      Present these frame by frame results visually.
            //      But also show information about the recent history (retired textures, etc)
        Rect topThird    = layout.AllocateFullWidthFraction(.25f+(_drawHistory?0.f:.4f));
        Rect buttons     = layout.AllocateFullWidthFraction(.05f);
        Rect middleThird = layout.AllocateFullWidthFraction(.2f);
        Rect bottomThird = layout.AllocateFullWidthFraction(.1f+(_drawHistory?.4f:.0f));

            //      Top part is a few graphs of the bytes uploaded/frame (or other statistics)
        {
            Layout topArea(topThird);
            unsigned graphCount = (_graphsMode<=GraphTabs::PendingBuffers)?UploadDataType::Max:1;
            for (unsigned c=0; c<graphCount; ++c) {
                Rect graphArea = topArea.AllocateFullHeightFraction(1.f/float(graphCount));
                DrawRectangleOutline(context, graphArea);

                //  Copy the recent history into an array of floats, so we can graph the results...
                float valuesBuffer[s_MaxGraphSegments];
                size_t valuesCount = 0;
                for (std::deque<FrameRecord>::const_reverse_iterator i =_frames.rbegin(); i!=_frames.rend(); ++i) {
                    if (valuesCount>=dimof(valuesBuffer)) {
                        break;
                    }
                    ++valuesCount;
                    using namespace GraphTabs;
                    float& value = valuesBuffer[dimof(valuesBuffer)-valuesCount];
                    value = 0.f;
                    if (graphCount == UploadDataType::Max) {
                        if (_graphsMode == Latency) { // latency (ms)
                            TimeMarker transactionLatencySum = 0;
                            unsigned transactionLatencyCount = 0;
                            for (unsigned cl=i->_commandListStart; cl<i->_commandListEnd; ++cl) {
                                BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                                for (unsigned i2=0; i2<commandList.RetirementCount(); ++i2) {
                                    const AssemblyLineRetirement& retirement = commandList.Retirement(i2);
                                    transactionLatencySum += retirement._retirementTime - retirement._requestTime;
                                    ++transactionLatencyCount;
                                }
                            }

                            float averageTransactionLatency = transactionLatencyCount?float(double(transactionLatencySum/TimeMarker(transactionLatencyCount)) * _reciprocalTimerFrequency):0.f;
                            value = averageTransactionLatency;
                        } else if (_graphsMode == PendingBuffers) { // pending buffers
                            if (i->_commandListStart!=i->_commandListEnd) {
                                value = _recentHistory[i->_commandListEnd-1]._assemblyLineMetrics._queuedBytes[c] / (1024.f*1024.f);
                            }
                        } else {
                            for (unsigned cl=i->_commandListStart; cl<i->_commandListEnd; ++cl) {
                                BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                                if (_graphsMode == Uploads) { // bytes uploaded
                                    value += (commandList._bytesUploaded[c] + commandList._bytesUploadedDuringCreation[c]) / (1024.f*1024.f);
                                } else if (_graphsMode == CreatesMB) { // creations (bytes)
                                    value += commandList._bytesCreated[c] / (1024.f*1024.f);
                                } else if (_graphsMode == CreatesCount) { // creations (count)
                                    value += commandList._countCreations[c];
                                } else if (_graphsMode == DeviceCreatesCount) {
                                    value += commandList._countDeviceCreations[c];
                                } else if (_graphsMode == FramePriorityStall) {
                                    value += float(commandList._framePriorityStallTime * _reciprocalTimerFrequency * 1000.f);
                                }
                            }
                        }
                    } else {
                        if (_graphsMode == CommandListCount) {
                            value = float(i->_commandListEnd-i->_commandListStart);
                        } else if (_graphsMode == GPUCost) {
                            value = i->_gpuCost;
                        } else if (_graphsMode == GPUBytesPerSecond) {
                            value = i->_gpuMetrics._slidingAverageBytesPerSecond / (1024.f * 1024.f);
                        } else if (_graphsMode == AveGPUCost) {
                            value = i->_gpuMetrics._slidingAverageCostMS;
                        } else if (_graphsMode == ThreadActivity) {
                            TimeMarker processingTimeSum = 0, waitTimeSum = 0;
                            for (unsigned cl=i->_commandListStart; cl<i->_commandListEnd; ++cl) {
                                BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                                processingTimeSum += commandList._processingEnd - commandList._processingStart;
                                waitTimeSum += commandList._waitTime;
                            }
                            value = (float(processingTimeSum))?(100.f * (1.0f-(waitTimeSum/float(processingTimeSum)))):0.f;
                        } else if (_graphsMode == BatchedCopy) {
                            value = 0;
                            for (unsigned cl=i->_commandListStart; cl<i->_commandListEnd; ++cl) {
                                BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                                value += commandList._batchedCopyBytes;
                            }
                        }
                    }
                }

                Rect sectionNameRect( 
                    Coord2(graphArea._topLeft[0], graphArea._topLeft[1]),
                    Coord2(graphArea._bottomRight[0], LinearInterpolate(graphArea._topLeft[1], graphArea._bottomRight[1], 0.2f)) );
                if (graphCount == UploadDataType::Max) {
                    DrawFormatText(
                        context, sectionNameRect, 2.5f, nullptr, ColorB(0xffffffffu), "%s (%s)", 
                        AsString(UploadDataType::Enum(c)), GraphTabs::Names[_graphsMode]);
                } else {
                    DrawText(context, sectionNameRect, 2.5f, nullptr, ColorB(0xffffffffu), GraphTabs::Names[_graphsMode]);
                }

				if (valuesCount > 0) {
					float mostRecentValue = valuesBuffer[dimof(valuesBuffer) - valuesCount];
					Rect valueRect(
						Coord2(graphArea._topLeft[0], graphArea._topLeft[1]),
						Coord2(sectionNameRect._bottomRight[0], LinearInterpolate(graphArea._topLeft[1], graphArea._bottomRight[1], 0.4f)));
					DrawFormatText(context, valueRect, 2.f, nullptr, ColorB(0xffffffffu), "%6.3f", mostRecentValue);
				}

                Rect realGraphRect(
                    Coord2(graphArea._topLeft[0], LinearInterpolate(graphArea._topLeft[1], graphArea._bottomRight[1], 0.333f)),
                    Coord2(graphArea._bottomRight[0], graphArea._bottomRight[1]) );

                DrawHistoryGraph(   context, realGraphRect, &valuesBuffer[dimof(valuesBuffer)-valuesCount], (unsigned)valuesCount, (unsigned)dimof(valuesBuffer), 
                                    _graphMinValueHistory, _graphMaxValueHistory);

                if (_graphsMode == GraphTabs::GPUCost) {
                        // GPU cost graph should also have the total bytes uploaded draw in
                    size_t valuesCount = 0;
                    for (std::deque<FrameRecord>::const_reverse_iterator i =_frames.rbegin(); i!=_frames.rend(); ++i) {
                        if (valuesCount>=dimof(valuesBuffer)) {
                            break;
                        }
                        ++valuesCount;
                        valuesBuffer[dimof(valuesBuffer)-valuesCount] = 0;
                        for (unsigned cl=i->_commandListStart; cl<i->_commandListEnd; ++cl) {
                            BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                            for (unsigned c2=0; c2<UploadDataType::Max; ++c2) {
                                valuesBuffer[dimof(valuesBuffer)-valuesCount] += (commandList._bytesUploaded[c2] + commandList._bytesUploadedDuringCreation[c2]) / (1024.f*1024.f);
                            }
                        }
                    }

                    DrawHistoryGraph_ExtraLine( context, realGraphRect, &valuesBuffer[dimof(valuesBuffer)-valuesCount], (unsigned)valuesCount, (unsigned)dimof(valuesBuffer), 
                                                _graphMinValueHistory, _graphMaxValueHistory);
                }

                {
                    const InteractableId framePicker = InteractableId_Make("FramePicker");
                    size_t newValuesCount = 0;
                    for (std::deque<FrameRecord>::const_reverse_iterator i =_frames.rbegin(); i!=_frames.rend(); ++i) {
                        if (newValuesCount>=dimof(valuesBuffer)) {
                            break;
                        }
                        int graphPartIndex = int(dimof(valuesBuffer)-newValuesCount-1);
                        Rect graphPart( Coord2(LinearInterpolate(graphArea._topLeft[0], graphArea._bottomRight[0], (graphPartIndex)/float(dimof(valuesBuffer))), graphArea._topLeft[1]),
                                        Coord2(LinearInterpolate(graphArea._topLeft[0], graphArea._bottomRight[0], (graphPartIndex+1)/float(dimof(valuesBuffer))), graphArea._bottomRight[1]));
                        InteractableId id = framePicker + newValuesCount;
                        if (interfaceState.HasMouseOver(id)) {
                            DrawRectangle(context, graphPart, ColorB(0x3f7f7f7fu));
                        } else if (i->_frameId == _lockedFrameId) {
                            DrawRectangle(context, graphPart, ColorB(0x3f7f3f7fu));
                        }
                        interactables.Register(Interactables::Widget(graphPart, id));
                        ++newValuesCount;
                    }
                }
            }
        }

            //      a few buttons in the middle to control the data in the graphs
        {
            Layout buttonsArea(buttons);
            for (unsigned c=0; c<dimof(GraphTabs::Names); ++c) {
                Rect buttonRect = buttonsArea.AllocateFullHeightFraction( 1.f/float(dimof(GraphTabs::Names)) );
                DrawButton(context, GraphTabs::Names[c], buttonRect, interactables, interfaceState);
            }
        }

        const Coord rowHeight    = 6;
        const Coord rowHeight2   = 12;

            //      Middle part is some written statics
        {
            Layout middleArea(middleThird);
            Layout columnZero(middleArea.AllocateFullHeightFraction(.333f));
            Layout columnOne (middleArea.AllocateFullHeightFraction(.333f));
            Layout columnTwo (middleArea.AllocateFullHeightFraction(.333f));

            GPUMetrics gpuMetrics = CalculateGPUMetrics();

                //////
            TimeMarker transactionLatencySum = 0;
            unsigned transactionLatencyCount = 0;
            TimeMarker commandListLatencySum = 0;
            unsigned commandListLatencyCount = 0;
            for (   std::deque<CommandListMetrics>::const_reverse_iterator i =_recentHistory.rbegin();i!=_recentHistory.rend(); ++i) {
                for (unsigned i2=0; i2<i->RetirementCount(); ++i2) {
                    const AssemblyLineRetirement& retire = i->Retirement(i2);
                    transactionLatencySum += retire._retirementTime - retire._requestTime;
                    ++transactionLatencyCount;
                }
                commandListLatencySum += i->_commitTime - i->_resolveTime;
                ++commandListLatencyCount;
            }

            TimeMarker processingTimeSum = 0, waitTimeSum = 0;
            unsigned wakeCountSum = 0;
            
            size_t validFrameIndex = _frames.size()-1;
            for (std::deque<FrameRecord>::reverse_iterator i=_frames.rbegin(); i!=_frames.rend(); ++i, --validFrameIndex) {
                if (i->_gpuCost != 0.f && i->_commandListStart != i->_commandListEnd) {
                    break;
                }
            }
            if (validFrameIndex < _frames.size()) {
                for (unsigned cl=_frames[validFrameIndex]._commandListStart; cl<_frames[validFrameIndex]._commandListEnd; ++cl) {
                    BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                    processingTimeSum += commandList._processingEnd - commandList._processingStart;
                    waitTimeSum += commandList._waitTime;
                    wakeCountSum += commandList._wakeCount;
                }
            }

            float averageTransactionLatency = transactionLatencyCount?float(double(transactionLatencySum/TimeMarker(transactionLatencyCount)) * _reciprocalTimerFrequency):0.f;
            float averageCommandListLatency = commandListLatencyCount?float(double(commandListLatencySum/TimeMarker(commandListLatencyCount)) * _reciprocalTimerFrequency):0.f;

            DrawFormatText(context, columnZero.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Ave latency: (%6.2f)ms", averageTransactionLatency * 1000.f);
            DrawFormatText(context, columnZero.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Creates: T(%i)V(%i)I(%i)", 
                mostRecentResults._countCreations[UploadDataType::Texture], mostRecentResults._countCreations[UploadDataType::Vertex], mostRecentResults._countCreations[UploadDataType::Index]);
            DrawFormatText(context, columnZero.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Command list latency: (%6.2f)ms", averageCommandListLatency * 1000.f);
            DrawFormatText(context, columnZero.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "GPU theoretical MB/second: (%6.2f)MB", gpuMetrics._slidingAverageBytesPerSecond/float(1024.f*1024.f));
            DrawFormatText(context, columnZero.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "GPU ave cost: (%6.2f)ms", gpuMetrics._slidingAverageCostMS);

                //////
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "AccumulatedValues");
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Creates: (T%i) (V%i) (I%i)", 
                _accumulatedCreateCount[UploadDataType::Texture], _accumulatedCreateCount[UploadDataType::Vertex], _accumulatedCreateCount[UploadDataType::Index]);
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Creates (MB): (T%8.3f) (V%8.3f) (I%8.3f)", 
                _accumulatedCreateBytes[UploadDataType::Texture] / (1024.f*1024.f), _accumulatedCreateBytes[UploadDataType::Vertex] / (1024.f*1024.f), _accumulatedCreateBytes[UploadDataType::Index] / (1024.f*1024.f));
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Uploads: (T%i) (V%i) (I%i)", 
                _accumulatedUploadCount[UploadDataType::Texture], _accumulatedUploadCount[UploadDataType::Vertex], _accumulatedUploadCount[UploadDataType::Index]);
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Uploads (MB): (T%8.3f) (V%8.3f) (I%8.3f)", 
                _accumulatedUploadBytes[UploadDataType::Texture] / (1024.f*1024.f), _accumulatedUploadBytes[UploadDataType::Vertex] / (1024.f*1024.f), _accumulatedUploadBytes[UploadDataType::Index] / (1024.f*1024.f));
            DrawFormatText(context, columnOne.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Thread activity (%6.3f)%% (%i)", 
                (float(processingTimeSum))?(100.f * (1.0f-(waitTimeSum/float(processingTimeSum)))):0.f, wakeCountSum);


                //////
            DrawFormatText(context, columnTwo.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Pending creates: (%i/%i)", mostRecentResults._assemblyLineMetrics._queuedCreates, mostRecentResults._assemblyLineMetrics._queuedPeakCreates);
            DrawFormatText(context, columnTwo.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Pending uploads: (%i/%i)", mostRecentResults._assemblyLineMetrics._queuedUploads, mostRecentResults._assemblyLineMetrics._queuedPeakUploads);
            DrawFormatText(context, columnTwo.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Pending staging creates: (%i/%i)", mostRecentResults._assemblyLineMetrics._queuedStagingCreates, mostRecentResults._assemblyLineMetrics._queuedPeakStagingCreates);
            DrawFormatText(context, columnTwo.AllocateFullWidth(rowHeight2), 2.f, nullptr, ColorB(0xffffffffu), "Transaction count: (%i/%i/%i)", mostRecentResults._assemblyLineMetrics._transactionCount, mostRecentResults._assemblyLineMetrics._temporaryTransactionsAllocated, mostRecentResults._assemblyLineMetrics._longTermTransactionsAllocated);
        }


            //      Bottom part is a list of recent retirements
        {
            Layout bottomArea(bottomThird);
            Layout columns[4] = {
                Layout(bottomArea.AllocateFullHeightFraction(1.0f/float(dimof(columns)))),
                Layout(bottomArea.AllocateFullHeightFraction(1.0f/float(dimof(columns)))),
                Layout(bottomArea.AllocateFullHeightFraction(1.0f/float(dimof(columns)))),
                Layout(bottomArea.AllocateFullHeightFraction(1.0f/float(dimof(columns))))
            };
            for (unsigned c=0; c<dimof(columns); ++c) {
                DrawRectangleOutline(context, columns[c].GetMaximumSize());
            }
            {
                Rect buttonRect = columns[0].AllocateFullWidth(rowHeight);
                InteractableId id = InteractableId_Make("ShowUploadHistory");
                DrawText(context, buttonRect, 1.25f, nullptr, interfaceState.HasMouseOver(id)?ColorB(0xff000000u):ColorB(0xffffffffu), _drawHistory?"Hide":"Show");
                interactables.Register(Interactables::Widget(buttonRect, id));
            }
            if (_drawHistory) {
                unsigned columnIndex = 0;
                unsigned frameCounter = 0;
                for (std::deque<FrameRecord>::const_reverse_iterator i=_frames.rbegin(); (i!=_frames.rend())&&(columnIndex<dimof(columns)); ++i, ++frameCounter) {
                    if (_lockedFrameId != ~unsigned(0x0) && i->_frameId != _lockedFrameId) {
                        continue;
                    }
                    for (unsigned cl=i->_commandListEnd-1; (cl>=i->_commandListStart)&&(columnIndex<dimof(columns)); --cl) {
                        const BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                        for (unsigned i2=0; i2<commandList.RetirementCount(); ++i2) {
                            Rect rect = columns[columnIndex].AllocateFullWidth(rowHeight);
                            if (!(IsGood(rect) && rect._bottomRight[1] < bottomThird._bottomRight[1] && rect._topLeft[1] >= bottomThird._topLeft[1])) {
                                ++columnIndex;
                                if (columnIndex >= dimof(columns)) {
                                    break;
                                }
                                rect = columns[columnIndex].AllocateFullWidth(rowHeight);
                            }

                            const AssemblyLineRetirement& retire = commandList.Retirement(i2);
                            DrawFormatText(context, rect, 1.f, nullptr, (frameCounter&0x1)?ColorB(0xff7f7fffu):ColorB(0xffffffffu), "%s Latency:%6.2f", 
                                BuildDescription(retire._desc).c_str(), float(double(retire._retirementTime-retire._requestTime)*_reciprocalTimerFrequency*1000.));
                        }
                    }
                }
            }
        }
    }

    bool    BufferUploadDisplay::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        if (interfaceState.TopMostId()) {
            if (input.IsRelease_LButton()) {
                InteractableId topMostWidget = interfaceState.TopMostId();
                for (unsigned c=0; c<dimof(GraphTabs::Names); ++c) {
                    if (topMostWidget == InteractableId_Make(GraphTabs::Names[c])) {
                        _graphsMode = c;
                        return true;
                    }
                }

                if (topMostWidget == InteractableId_Make("ShowUploadHistory")) {
                    _drawHistory = !_drawHistory;
                    return true;
                }

                const InteractableId framePicker = InteractableId_Make("FramePicker");
                if (topMostWidget >= framePicker && topMostWidget < (framePicker+s_MaxGraphSegments)) {
                    unsigned graphIndex = unsigned(topMostWidget - framePicker);
                    _lockedFrameId = _frames[std::max(0,signed(_frames.size())-signed(graphIndex)-1)]._frameId;
                    return true;
                }

                return false;
            }
        }
        return false;
    }

    void    BufferUploadDisplay::AddCommandListToFrame(unsigned frameId, unsigned commandListIndex)
    {
        for (std::deque<FrameRecord>::reverse_iterator i=_frames.rbegin(); i!=_frames.rend(); ++i) {
            if (i->_frameId == frameId) {
                if (i->_commandListStart == ~unsigned(0x0)) {
                    i->_commandListStart = commandListIndex;
                    i->_commandListEnd = commandListIndex+1;
                } else {
                    assert(commandListIndex == i->_commandListEnd || commandListIndex == (i->_commandListEnd-1));
                    i->_commandListEnd = std::max(i->_commandListEnd, commandListIndex+1);
                }
                i->_gpuMetrics = CalculateGPUMetrics();
                return;
            } else if (i->_frameId < frameId) {
                    //      We went too far and didn't find this frame... We'll have to insert it in as a new frame.
                FrameRecord newFrame;
                newFrame._frameId = frameId;
                newFrame._commandListStart = commandListIndex;
                newFrame._commandListEnd = commandListIndex+1;
                std::deque<FrameRecord>::iterator newItem = _frames.insert(i.base(),newFrame);
                newItem->_gpuMetrics = CalculateGPUMetrics();
                return;
            }
        }

        FrameRecord newFrame;
        newFrame._frameId = frameId;
        newFrame._commandListStart = commandListIndex;
        newFrame._commandListEnd = commandListIndex+1;
        _frames.push_back(newFrame);
        _frames[_frames.size()-1]._gpuMetrics = CalculateGPUMetrics();
    }

    void    BufferUploadDisplay::AddGPUToCostToFrame(unsigned frameId, float gpuCost)
    {
        for (std::deque<FrameRecord>::reverse_iterator i=_frames.rbegin(); i!=_frames.rend(); ++i) {
            if (i->_frameId == frameId) {
                i->_gpuCost += gpuCost;
                i->_gpuMetrics = CalculateGPUMetrics();
                return;
            } else if (i->_frameId < frameId) {
                // we went too far and didn't find this frame... We'll have to insert it in as a new frame.
                FrameRecord newFrame;
                newFrame._frameId = frameId;
                newFrame._gpuCost = gpuCost;
                std::deque<FrameRecord>::iterator newItem = _frames.insert(i.base(),newFrame);
                newItem->_gpuMetrics = CalculateGPUMetrics();
                return;
            }
        }

        FrameRecord newFrame;
        newFrame._frameId = frameId;
        newFrame._gpuCost = gpuCost;
        _frames.push_back(newFrame);
        _frames[_frames.size()-1]._gpuMetrics = CalculateGPUMetrics();
    }

    BufferUploadDisplay::GPUMetrics   BufferUploadDisplay::CalculateGPUMetrics()
    {
                //////
                //      calculate the GPU upload speed... How much GPU time should we expect to consume per mb uploaded,
                //      based on a sliding average
        BufferUploadDisplay::GPUMetrics result;
        result._slidingAverageBytesPerSecond = 0;
        result._slidingAverageCostMS = 0.f;

        unsigned framesCountWithValidGPUCost = (unsigned)_frames.size();
        for (std::deque<FrameRecord>::reverse_iterator i=_frames.rbegin(); i!=_frames.rend(); ++i, --framesCountWithValidGPUCost) {
            if (i->_gpuCost != 0.f && i->_commandListStart != i->_commandListEnd) {
                break;
            }
        }

        const unsigned samples = std::min(unsigned(framesCountWithValidGPUCost), 256u);
        std::deque<FrameRecord>::const_iterator gI = _frames.begin() + (_frames.size()-samples);
        float totalGPUCost = 0.f; unsigned totalBytesUploaded = 0;
        for (unsigned c=0; c<samples; ++c, ++gI) {
            float gpuCost = gI->_gpuCost; unsigned bytesUploaded = 0;
            for (unsigned cl=gI->_commandListStart; cl<gI->_commandListEnd; ++cl) {
                BufferUploads::CommandListMetrics& commandList = _recentHistory[cl];
                for (unsigned c2=0; c2<BufferUploads::UploadDataType::Max; ++c2) {
                    bytesUploaded += commandList._bytesUploaded[c2] + commandList._bytesUploadedDuringCreation[c2];
                }
            }
            totalGPUCost += gpuCost;
            totalBytesUploaded += bytesUploaded;
        }
        if (totalGPUCost) {
            result._slidingAverageBytesPerSecond = unsigned(totalBytesUploaded / (totalGPUCost/1000.f));
        }
        if (samples) {
            result._slidingAverageCostMS = totalGPUCost / float(samples);
        }
        return result;
    }

    void    BufferUploadDisplay::ProcessGPUEvents(const void* eventsBufferStart, const void* eventsBufferEnd)
    {
        ScopedLock(_gpuEventsBufferLock);
        size_t oldSize = _gpuEventsBuffer.size();
        size_t eventsBufferSize = ptrdiff_t(eventsBufferEnd) - ptrdiff_t(eventsBufferStart);
        _gpuEventsBuffer.resize(_gpuEventsBuffer.size() + eventsBufferSize);
        memcpy(&_gpuEventsBuffer[oldSize], eventsBufferStart, eventsBufferSize);
    }

    void    BufferUploadDisplay::ProcessGPUEvents_MT(const void* eventsBufferStart, const void* eventsBufferEnd)
    {
        const void * evnt = eventsBufferStart;
        while (evnt < eventsBufferEnd) {
            uint32 eventType = (uint32)*((const size_t*)evnt); evnt = PtrAdd(evnt, sizeof(size_t));
            if (eventType == ~uint32(0x0)) {
                size_t frameId = *((const size_t*)evnt); evnt = PtrAdd(evnt, sizeof(size_t));
                GPUTime frequency = *((const uint64*)evnt); evnt = PtrAdd(evnt, sizeof(uint64));
                _mostRecentGPUFrequency = frequency;
                _mostRecentGPUFrameId = (unsigned)frameId;
            } else {
                const char* eventName = *((const char**)evnt); evnt = PtrAdd(evnt, sizeof(const char*));
                assert((size_t(evnt)%sizeof(uint64))==0);
                uint64 timeValue = *((const uint64*)evnt); evnt = PtrAdd(evnt, sizeof(uint64));

                if (eventName && !XlCompareStringI(eventName, "GPU_UPLOAD")) {
                    if (eventType == 0) {
                        _lastUploadBeginTime = timeValue;
                    } else {
                        if (_lastUploadBeginTime) {
                            _mostRecentGPUCost = float(double(timeValue - _lastUploadBeginTime) / double(_mostRecentGPUFrequency) * 1000.);

                                //      write this result into the GPU time for any frames that need it...
                            AddGPUToCostToFrame(_mostRecentGPUFrameId, _mostRecentGPUCost);
                        }
                    }
                }
            }
        }
    }

    BufferUploadDisplay* BufferUploadDisplay::s_gpuListenerDisplay = 0;
    void BufferUploadDisplay::GPUEventListener(const void* eventsBufferStart, const void* eventsBufferEnd)
    {
        if (s_gpuListenerDisplay) {
            s_gpuListenerDisplay->ProcessGPUEvents(eventsBufferStart, eventsBufferEnd);
        }
    }


        ////////////////////////////////////////////////////////////////////

    ResourcePoolDisplay::ResourcePoolDisplay(BufferUploads::IManager* manager)
    {
        _manager = manager;
        _filter = 0;
        _detailsIndex = 0;
        _graphMin = _graphMax = 0.f;
    }

    ResourcePoolDisplay::~ResourcePoolDisplay()
    {
    }

    namespace ResourcePoolDisplayTabs
    {
        static const char* Names[] = { "Index Buffers", "Vertex Buffers", "Staging Textures" };
    }

    bool    ResourcePoolDisplay::Filter(const BufferUploads::BufferDesc& desc)
    {
        if (_filter == 0 && (desc._bindFlags&BufferUploads::BindFlag::IndexBuffer)) return true;
        if (_filter == 1 && (desc._bindFlags&BufferUploads::BindFlag::VertexBuffer)) return true;
        if (_filter == 2 && (desc._type == BufferUploads::BufferDesc::Type::Texture)) return true;
        return false;
    }

    static const InteractableId ResourcePoolDisplayGraph = InteractableId_Make("ResourcePoolDisplayGraph");

    void    ResourcePoolDisplay::Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState)
    {
        using namespace BufferUploads;
        IManager* manager = _manager;
        if (manager) {
            PoolSystemMetrics metrics = manager->CalculatePoolMetrics();

                /////////////////////////////////////////////////////////////////////////////
            const std::vector<PoolMetrics>& metricsVector = (_filter==2)?metrics._stagingPools:metrics._resourcePools;
            unsigned maxSize = 0, count = 0;
            for (std::vector<PoolMetrics>::const_iterator i=metricsVector.begin();i!=metricsVector.end(); ++i) {
                if (Filter(i->_desc)) {
                    maxSize = std::max(maxSize, i->_peakSize);
                    ++count;
                }
            }

                /////////////////////////////////////////////////////////////////////////////
            layout.AllocateFullWidth(128);  // leave some space at the top
            Layout buttonsLayout(layout.AllocateFullWidth(32));
            for (unsigned c=0; c<dimof(ResourcePoolDisplayTabs::Names); ++c) {
                DrawButton(context, ResourcePoolDisplayTabs::Names[c], buttonsLayout.AllocateFullHeightFraction(1.f/float(dimof(ResourcePoolDisplayTabs::Names))), interactables, interfaceState);
            }

            if (count) {
                    /////////////////////////////////////////////////////////////////////////////
                Rect barChartRect = layout.AllocateFullWidth(400);
                Layout barsLayout(barChartRect);
                barsLayout._paddingBetweenAllocations = 4;
                const unsigned barWidth = (barChartRect.Width() - (count-1) * barsLayout._paddingBetweenAllocations - 2*barsLayout._paddingInternalBorder) / count;

                static ColorB rectColor(96, 192, 170, 128);
                static ColorB peakMarkerColor(192, 64, 64, 128);
                static ColorB textColour(192, 192, 192, 128);
            
                    /////////////////////////////////////////////////////////////////////////////
                unsigned c=0;
                const PoolMetrics* detailsMetrics = NULL;
                for (std::vector<PoolMetrics>::const_iterator i=metricsVector.begin(); i!=metricsVector.end(); ++i) {
                    if (Filter(i->_desc)) {
                        float A = i->_currentSize / float(maxSize);
                        float B = i->_peakSize / float(maxSize);
                        Rect fullRect = barsLayout.AllocateFullHeight(barWidth);
                        Rect colouredRect(Coord2(fullRect._topLeft[0], LinearInterpolate(fullRect._topLeft[1], fullRect._bottomRight[1], 1.f-A)), fullRect._bottomRight);
                        DrawRectangle(context, colouredRect, rectColor);
                        DrawRectangle(context, Rect(    Coord2(fullRect._topLeft[0], LinearInterpolate(fullRect._topLeft[1], fullRect._bottomRight[1], 1.f-B)),
                                                        Coord2(fullRect._bottomRight[0], LinearInterpolate(fullRect._topLeft[1], fullRect._bottomRight[1], 1.f-B)+2)), peakMarkerColor);

                        Rect textRect(colouredRect._topLeft, Coord2(colouredRect._bottomRight[0], colouredRect._topLeft[1]+10));
                        if (i->_peakSize) {
                            const BufferDesc& desc = i->_desc;
                            if (desc._type == BufferDesc::Type::LinearBuffer) {
                                if (desc._bindFlags & BindFlag::IndexBuffer) {
                                    DrawFormatText(context, textRect, 1.f, nullptr, textColour, "IB %6.2fk", desc._linearBufferDesc._sizeInBytes / 1024.f);
                                } else if (desc._bindFlags & BindFlag::VertexBuffer) {
                                    DrawFormatText(context, textRect, 1.f, nullptr, textColour, "VB %6.2fk", desc._linearBufferDesc._sizeInBytes / 1024.f);
                                } else {
                                    DrawFormatText(context, textRect, 1.f, nullptr, textColour, "B %6.2fk", desc._linearBufferDesc._sizeInBytes / 1024.f);
                                }
                            } else if (desc._type == BufferDesc::Type::Texture) {
                                DrawFormatText(context, textRect, 1.f, nullptr, textColour, "Tex %ix%i", desc._textureDesc._width, desc._textureDesc._height);
                            }
                            textRect._topLeft[1] += 16; textRect._bottomRight[1] += 16;
                            if (i->_currentSize) {
                                DrawFormatText(context, textRect, 1.f, nullptr, textColour, "%i (%6.3fMB)", 
                                    i->_currentSize, (i->_currentSize * manager->ByteCount(i->_desc)) / (1024.f*1024.f));
                            }
                        }

                        InteractableId id = ResourcePoolDisplayGraph+c;
                        if (_detailsIndex==c) {
                            detailsMetrics = &(*i);
                        }
                        interactables.Register(Interactables::Widget(fullRect, id));
                        ++c;
                    }
                }

                if (detailsMetrics) {
                    _detailsHistory.push_back(*detailsMetrics);
                    Rect textRect = layout.AllocateFullWidth(32);
                    DrawFormatText(context, textRect, 2.f, nullptr, textColour, "Real size: %6.2fMB, Created size: %6.2fMB, Padding overhead: %6.2fMB, Count: %i",
                        detailsMetrics->_totalRealSize/(1024.f*1024.f), detailsMetrics->_totalCreateSize/(1024.f*1024.f), (detailsMetrics->_totalCreateSize-detailsMetrics->_totalRealSize)/(1024.f*1024.f),
                        detailsMetrics->_totalCreateCount);

                    Rect historyRect = layout.AllocateFullWidth(200);
                    float historyValues[256];
                    unsigned historyCount = 0;
                    for (std::vector<PoolMetrics>::const_reverse_iterator i=_detailsHistory.rbegin(); i!=_detailsHistory.rend() && historyCount < dimof(historyValues); ++i, ++historyCount) {
                        historyValues[historyCount] = float(i->_recentReleaseCount);
                    }
                    DrawHistoryGraph(context, historyRect, historyValues, historyCount, dimof(historyValues), _graphMin, _graphMax);
                }
            }
        }
    }

    bool    ResourcePoolDisplay::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        if (interfaceState.TopMostId()) {
            if (input.IsRelease_LButton()) {
                InteractableId topMostWidget = interfaceState.TopMostId();
                if (topMostWidget == InteractableId_Make(ResourcePoolDisplayTabs::Names[0])) {
                    _filter = 0;
                    return true;
                } else if (topMostWidget == InteractableId_Make(ResourcePoolDisplayTabs::Names[1])) {
                    _filter = 1;
                    return true;
                } else if (topMostWidget == InteractableId_Make(ResourcePoolDisplayTabs::Names[2])) {
                    _filter = 2;
                    return true;
                } else if (topMostWidget >= ResourcePoolDisplayGraph && topMostWidget < ResourcePoolDisplayGraph+100) {
                    _detailsIndex = unsigned(topMostWidget-ResourcePoolDisplayGraph);
                    _detailsHistory.clear();
                    return true;
                }
            }
        }
        return false;
    }



        ////////////////////////////////////////////////////////////////////

    static const unsigned FramesOfWarmth = 60;

    void    BatchingDisplay::Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState)
    {
        using namespace BufferUploads;
        IManager* manager = _manager;
        if (manager) {
            PoolSystemMetrics poolMetrics = manager->CalculatePoolMetrics();
            const BatchingSystemMetrics& metrics = poolMetrics._batchingSystemMetrics;

            layout.AllocateFullWidth(32);  // leave some space at the top
            static ColorB textColour(192, 192, 192, 128);
            static ColorB unallocatedLineColour(192, 192, 192, 128);

            unsigned allocatedSpace = 0, unallocatedSpace = 0;
            unsigned largestFreeBlock = 0;
            unsigned largestHeapSize = 0;
            unsigned totalBlockCount = 0;
            for (std::vector<BatchedHeapMetrics>::const_iterator i=metrics._heaps.begin(); i!=metrics._heaps.end(); ++i) {
                allocatedSpace += i->_allocatedSpace;
                unallocatedSpace += i->_unallocatedSpace;
                largestFreeBlock = std::max(largestFreeBlock, i->_largestFreeBlock);
                largestHeapSize = std::max(largestHeapSize, i->_heapSize);
                totalBlockCount += i->_referencedCountedBlockCount;
            }

            {
                DrawFormatText(context, layout.AllocateFullWidth(16), 1.5f, nullptr, textColour, "Heap count: %i / Total allocated: %7.3fMb / Total unallocated: %7.3fMb",
                    metrics._heaps.size(), allocatedSpace/(1024.f*1024.f), unallocatedSpace/(1024.f*1024.f));
                DrawFormatText(context, layout.AllocateFullWidth(16), 1.5f, nullptr, textColour, "Largest free block: %7.3fKb / Average unallocated: %7.3fKb",
                    largestFreeBlock/1024.f, unallocatedSpace/(float(metrics._heaps.size())*1024.f));
                DrawFormatText(context, layout.AllocateFullWidth(16), 1.5f, nullptr, textColour, "Block count: %i / Ave block size: %7.3fKb",
                    totalBlockCount, allocatedSpace/float(totalBlockCount*1024.f));
            }

            unsigned currentFrameId = GetFrameID();

            {
                const unsigned lineHeight = 4;
                Rect outsideRect = layout.AllocateFullWidth(DebuggingDisplay::Coord(metrics._heaps.size()*lineHeight + layout._paddingInternalBorder*2));
                Rect heapAllocationDisplay = Layout(outsideRect).AllocateFullWidthFraction(100.f);

                DrawRectangleOutline(context, outsideRect);

                std::vector<Float3> lines;
                std::vector<ColorB> lineColors;
                lines.reserve(metrics._heaps.size()*lineHeight*2*10);
                lineColors.reserve(metrics._heaps.size()*lineHeight*10);

                float X = heapAllocationDisplay.Width() / float(largestHeapSize);
                unsigned y = heapAllocationDisplay._topLeft[1];
                
                for (std::vector<BatchedHeapMetrics>::const_iterator i=metrics._heaps.begin(); i!=metrics._heaps.end(); ++i) {
                    unsigned heapIndex = (unsigned)std::distance(metrics._heaps.begin(), i);

                    unsigned lastStart = 0;
                    const bool drawAllocated = true;
                    for (std::vector<unsigned>::const_iterator i2=i->_markers.begin(); (i2+1)<i->_markers.end(); i2+=2) {
                        unsigned start, end;
                        if (drawAllocated) {
                            start = lastStart;
                            end = *i2;
                        } else {
                            start = *i2;
                            end = *(i2+1);
                        }
                        if (start != end) {
                            float warmth = CalculateWarmth(heapIndex, start, end, drawAllocated);
                            ColorB col = ColorB::FromNormalized(warmth, 0.f, 1.0f-warmth);
                            for (unsigned c=0; c<lineHeight; ++c) {
                                const Coord x = Coord(start*X + heapAllocationDisplay._topLeft[0]);
                                lines.push_back(AsPixelCoords(Coord2(x, y+c)));
                                lines.push_back(AsPixelCoords(Coord2(std::max(x+1, Coord(end*X + heapAllocationDisplay._topLeft[0])), y+c)));
                                lineColors.push_back(col);
                                lineColors.push_back(col);
                            }
                        }
                        lastStart = *(i2+1);
                    }

                    y += lineHeight;
                }

                if (!lines.empty()) {
                    context->DrawLines(ProjectionMode::P2D, AsPointer(lines.begin()), (uint32)lines.size(), AsPointer(lineColors.begin()));
                }
            }

            _lastFrameMetrics = metrics;

                //      extinquish cooling spans
            for (std::vector<WarmSpan>::iterator i=_warmSpans.begin(); i!=_warmSpans.end();) {
                if (i->_frameStart <= (currentFrameId-FramesOfWarmth)) {
                    i = _warmSpans.erase(i);
                } else {
                    ++i;
                }
            }
        }
    }

    float BatchingDisplay::CalculateWarmth(unsigned heapIndex, unsigned begin, unsigned end, bool allocatedMode)
    {
        const unsigned currentFrameId = GetFrameID();
        for (std::vector<WarmSpan>::const_iterator i=_warmSpans.begin(); i!=_warmSpans.end(); ++i) {
            if (i->_heapIndex == heapIndex && i->_begin == begin && i->_end == end) {
                return 1.f-std::min((currentFrameId-i->_frameStart)/float(FramesOfWarmth), 1.f);
            }
        }

        const bool thereLastFrame = FindSpan(heapIndex, begin, end, allocatedMode);
        if (!thereLastFrame) {
            WarmSpan warmSpan;
            warmSpan._heapIndex = heapIndex;
            warmSpan._begin = begin;
            warmSpan._end = end;
            warmSpan._frameStart = currentFrameId;
            _warmSpans.push_back(warmSpan);
            return 1.f;
        }

        return 0.f;
    }

    bool BatchingDisplay::FindSpan(unsigned heapIndex, unsigned begin, unsigned end, bool allocatedMode)
    {
        if (heapIndex >= _lastFrameMetrics._heaps.size()) {
            return false;
        }

        unsigned lastStart = 0;
        for (std::vector<unsigned>::const_iterator i2=_lastFrameMetrics._heaps[heapIndex]._markers.begin(); (i2+1)<_lastFrameMetrics._heaps[heapIndex]._markers.end(); i2+=2) {
            unsigned spanBegin, spanEnd;
            if (allocatedMode) {
                spanBegin = lastStart;
                spanEnd = *i2;
            } else {
                spanBegin = *i2;
                spanEnd = *(i2+1);
            }
            if (begin == spanBegin && end == spanEnd) {
                return true;
            }
            lastStart = *(i2+1);
        }
        return false;
    }

    bool    BatchingDisplay::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        return false;
    }

    BatchingDisplay::BatchingDisplay(BufferUploads::IManager* manager)
    : _manager(manager)
    {
    }

    BatchingDisplay::~BatchingDisplay()
    {

    }
}}





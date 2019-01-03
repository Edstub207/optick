#include "Core.h"
#include "ProfilerServer.h"

#include "Trace.h"
#include "SymbolEngine.h"

#include <algorithm>
#include <unordered_set>


extern "C" Brofiler::EventData* NextEvent()
{
	if (Brofiler::EventStorage* storage = Brofiler::Core::storage)
	{
		return &storage->NextEvent();
	}

	return nullptr;
}


namespace Brofiler
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
OutputDataStream& operator<<(OutputDataStream& stream, const TagData<T>& ob)
{
	return stream << ob.timestamp << ob.description->index << ob.data;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VS TODO: Replace with random access iterator for MemoryPool
template<class T, uint32 SIZE>
void SortMemoryPool(MemoryPool<T, SIZE>& memoryPool)
{
	size_t count = memoryPool.Size();
	if (count == 0)
		return;

	std::vector<T> memoryArray;
	memoryArray.resize(count);
	memoryPool.ToArray(&memoryArray[0]);

	std::sort(memoryArray.begin(), memoryArray.end());

	memoryPool.Clear(true);

	for (const T& item : memoryArray)
		memoryPool.Add(item);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription* EventDescription::Create(const char* eventName, const char* fileName, const unsigned long fileLine, const unsigned long eventColor /*= Color::Null*/, const unsigned long filter /*= 0*/)
{
	return EventDescriptionBoard::Get().CreateDescription(eventName, fileName, fileLine, eventColor, filter);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription* EventDescription::CreateShared(const char* eventName, const char* fileName, const unsigned long fileLine, const unsigned long eventColor /*= Color::Null*/, const unsigned long filter /*= 0*/)
{
	return EventDescriptionBoard::Get().CreateSharedDescription(eventName, fileName, fileLine, eventColor, filter);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription::EventDescription() : name(""), file(""), line(0), color(0)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription& EventDescription::operator=(const EventDescription&)
{
	BRO_FAILED("It is pointless to copy EventDescription. Please, check you logic!"); return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventData* Event::Start(const EventDescription& description)
{
	EventData* result = nullptr;

	if (EventStorage* storage = Core::storage)
	{
		result = &storage->NextEvent();
		result->description = &description;
		result->Start();
	}
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Stop(EventData& data)
{
	if (EventStorage* storage = Core::storage)
	{
		data.Stop();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BRO_INLINE PushEvent(EventStorage* pStorage, const EventDescription* description, int64_t timestampStart)
{
	if (EventStorage* storage = pStorage)
	{
		EventData& result = storage->NextEvent();
		result.description = description;
		result.start = timestampStart;
		storage->pushPopEventStack[storage->pushPopEventStackIndex++] = &result;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BRO_INLINE PopEvent(EventStorage* pStorage, int64_t timestampFinish)
{
	if (EventStorage* storage = pStorage)
		if (storage->pushPopEventStackIndex > 0)
			storage->pushPopEventStack[--storage->pushPopEventStackIndex]->finish = timestampFinish;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Push(const char* name)
{
	if (EventStorage* storage = Core::storage)
	{
		EventDescription* desc = EventDescription::CreateShared(name);
		Push(*desc);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Push(const EventDescription& description)
{
	PushEvent(Core::storage, &description, Brofiler::GetHighPrecisionTime());
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Pop()
{
	PopEvent(Core::storage, Brofiler::GetHighPrecisionTime());
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Add(EventStorage* storage, const EventDescription* description, int64_t timestampStart, int64_t timestampFinish)
{
	EventData& data = storage->eventBuffer.Add();
	data.description = description;
	data.start = timestampStart;
	data.finish = timestampFinish;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Push(EventStorage* storage, const EventDescription* description, int64_t timestampStart)
{
	PushEvent(storage, description, timestampStart);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Event::Pop(EventStorage* storage, int64_t timestampFinish)
{
	PopEvent(storage, timestampFinish);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventData* GPUEvent::Start(const EventDescription& description)
{
	EventData* result = nullptr;

	if (EventStorage* storage = Core::storage)
		result = storage->gpuStorage.Start(description);

	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GPUEvent::Stop(EventData& data)
{
	if (EventStorage* storage = Core::storage)
		storage->gpuStorage.Stop(data);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FiberSyncData::AttachToThread(EventStorage* storage, uint64_t threadId)
{
	if (storage)
	{
		FiberSyncData& data = storage->fiberSyncBuffer.Add();
		data.Start();
		data.finish = EventTime::INVALID_TIMESTAMP;
		data.threadId = threadId;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FiberSyncData::DetachFromThread(EventStorage* storage)
{
	if (storage)
	{
		if (FiberSyncData* syncData = storage->fiberSyncBuffer.Back())
		{
			syncData->Stop();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, float val)
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagFloatBuffer.Add(TagFloat(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, int32_t val)
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagS32Buffer.Add(TagS32(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, uint32_t val)
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagU32Buffer.Add(TagU32(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, uint64_t val)
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagU64Buffer.Add(TagU64(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, float val[3])
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagPointBuffer.Add(TagPoint(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Tag::Attach(const EventDescription& description, const char* val)
{
	if (EventStorage* storage = Core::storage)
	{
		storage->tagStringBuffer.Add(TagString(description, val));
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream & operator<<(OutputDataStream &stream, const EventDescription &ob)
{
	byte flags = 0;
	return stream << ob.name << ob.file << ob.line << ob.filter << ob.color << (float)0.0f << flags;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const EventTime& ob)
{
	return stream << ob.start << ob.finish;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const EventData& ob)
{
	return stream << (EventTime)(ob) << (ob.description ? ob.description->index : (uint32)-1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const SyncData& ob)
{
	return stream << (EventTime)(ob) << ob.core << ob.reason << ob.newThreadId;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const FiberSyncData& ob)
{
	return stream << (EventTime)(ob) << ob.threadId;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Category::Category(const EventDescription& description) : Event(description)
{
	if (data)
	{
		if (EventStorage* storage = Core::storage)
		{
			storage->RegisterCategory(*data);
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::mutex& GetBoardLock()
{
	// Initialize as static local variable to prevent problems with static initialization order
	static std::mutex lock;
	return lock;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescriptionBoard& EventDescriptionBoard::Get()
{
	return instance;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EventDescriptionList& EventDescriptionBoard::GetEvents() const
{
	return boardDescriptions;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription* EventDescriptionBoard::CreateDescription(const char* name, const char* file /*= nullptr*/, uint32_t line /*= 0*/, uint32_t color /*= Color::Null*/, uint32_t filter /*= 0*/)
{
	std::lock_guard<std::mutex> lock(GetBoardLock());

	size_t index = boardDescriptions.Size();

	EventDescription& desc = boardDescriptions.Add();
	desc.index = (uint32)index;
	desc.name = name;
	desc.file = file;
	desc.line = line;
	desc.color = color;
	desc.filter = filter;

	return &desc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventDescription* EventDescriptionBoard::CreateSharedDescription(const char* name, const char* file /*= nullptr*/, uint32_t line /*= 0*/, uint32_t color /*= Color::Null*/, uint32_t filter /*= 0*/)
{
	BroStringHash nameHash(name);

	std::lock_guard<std::mutex> lock(sharedLock);

	std::pair<DescriptionMap::iterator, bool> cached = sharedDescriptions.insert({ nameHash, nullptr });

	if (cached.second)
	{
		const char* nameCopy = sharedNames.Add(name, strlen(name) + 1, false);
		cached.first->second = CreateDescription(nameCopy, file, line, color, filter);
	}

	return cached.first->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator << (OutputDataStream& stream, const EventDescriptionBoard& ob)
{
	std::lock_guard<std::mutex> lock(GetBoardLock());
	stream << ob.GetEvents();
	return stream;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Brofiler::EventDescriptionBoard EventDescriptionBoard::instance;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProcessDescription::ProcessDescription(const char* processName, ProcessID pid, uint64 key) : name(processName), processID(pid), uniqueKey(key)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThreadDescription::ThreadDescription(const char* threadName, ThreadID tid, ProcessID pid, int32 _maxDepth /*= 1*/, int32 _priority /*= 0*/, uint32 _mask /*= 0*/)
	: name(threadName), threadID(tid), processID(pid), maxDepth(_maxDepth), priority(_priority), mask(_mask)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int64_t GetHighPrecisionTime()
{
	return GetTime();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int64_t GetHighPrecisionFrequency()
{
	return GetFrequency();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream & operator<<(OutputDataStream &stream, const SysCallData &ob)
{
	return stream << (const EventData&)ob << ob.threadID << ob.id;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SysCallData& SysCallCollector::Add()
{
	return syscallPool.Add();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SysCallCollector::Clear()
{
	syscallPool.Clear(false);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SysCallCollector::Serialize(OutputDataStream& stream)
{
	stream << syscallPool;

	if (!syscallPool.IsEmpty())
	{
		syscallPool.Clear(false);
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CallstackCollector::Add(const CallstackDesc& desc)
{
	if (uint64* storage = callstacksPool.TryAdd(desc.count + 3))
	{
		storage[0] = desc.threadID;
		storage[1] = desc.timestamp;
		storage[2] = desc.count;

		for (uint64 i = 0; i < desc.count; ++i)
		{
			storage[3 + i] = desc.callstack[desc.count - i - 1];
		}
	}
	else
	{
		uint64& item0 = callstacksPool.Add();
		uint64& item1 = callstacksPool.Add();
		uint64& item2 = callstacksPool.Add();

		item0 = desc.threadID;
		item1 = desc.timestamp;
		item2 = desc.count;

		for (uint64 i = 0; i < desc.count; ++i)
		{
			callstacksPool.Add() = desc.callstack[desc.count - i - 1];
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CallstackCollector::Clear()
{
	callstacksPool.Clear(false);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CallstackCollector::SerializeSymbols(OutputDataStream& stream)
{
	typedef std::unordered_set<uint64> SymbolSet;
	SymbolSet symbolSet;

	for (CallstacksPool::const_iterator it = callstacksPool.begin(); it != callstacksPool.end();)
	{
		CallstacksPool::const_iterator startIt = it;
		BRO_UNUSED(startIt);

		uint64 threadID = *it;
		BRO_UNUSED(threadID);
		++it; //Skip ThreadID
		uint64 timestamp = *it;
		BRO_UNUSED
		(timestamp);
		++it; //Skip Timestamp
		uint64 count = *it;
		count = (count & 0xFF);
		++it; //Skip Count

		bool isBadAddrFound = false;

		for (uint64 i = 0; i < count; ++i)
		{
			uint64 address = *it;
			++it;

			if (address == 0)
			{
				isBadAddrFound = true;
			}

			if (!isBadAddrFound)
			{
				symbolSet.insert(address);
			}
		}
	}

	SymbolEngine* symEngine = Core::Get().symbolEngine;

	std::vector<const Symbol*> symbols;
	symbols.reserve(symbolSet.size());

	size_t callstacksCount = symbolSet.size();
	size_t callstackIndex = 0;


	Core::Get().DumpProgress("Resolving addresses... ");

	if (symEngine)
	{
		for (auto it = symbolSet.begin(); it != symbolSet.end(); ++it)
		{
			callstackIndex++;

			uint64 address = *it;
			if (const Symbol* symbol = symEngine->GetSymbol(address))
			{
				symbols.push_back(symbol);
			}
		}
	}

	stream << symbols;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CallstackCollector::SerializeCallstacks(OutputDataStream& stream)
{
	stream << callstacksPool;

	if (!callstacksPool.IsEmpty())
	{
		callstacksPool.Clear(false);
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CallstackCollector::IsEmpty() const
{
	return callstacksPool.IsEmpty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream & operator<<(OutputDataStream &stream, const SwitchContextDesc &ob)
{
	return stream << ob.timestamp << ob.oldThreadId << ob.newThreadId << ob.cpuId << ob.reason;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SwitchContextCollector::Add(const SwitchContextDesc& desc)
{
	switchContextPool.Add() = desc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SwitchContextCollector::Clear()
{
	switchContextPool.Clear(false);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SwitchContextCollector::Serialize(OutputDataStream& stream)
{
	stream << switchContextPool;

	if (!switchContextPool.IsEmpty())
	{
		switchContextPool.Clear(false);
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(BRO_PLATFORM_WINDOWS)
#define CPUID(INFO, ID) __cpuid(INFO, ID)
#include <intrin.h> 
#elif defined(BRO_PLATFORM_POSIX) || defined(BRO_PLATFORM_OSX)
#include <cpuid.h>
#define CPUID(INFO, ID) __cpuid(ID, INFO[0], INFO[1], INFO[2], INFO[3])
#else
#error Platform is not supported!
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string GetCPUName()
{
	int cpuInfo[4] = { -1 };
	char cpuBrandString[0x40] = { 0 };
	CPUID(cpuInfo, 0x80000000);
	unsigned nExIds = cpuInfo[0];
	for (unsigned i = 0x80000000; i <= nExIds; ++i)
	{
		CPUID(cpuInfo, i);
		if (i == 0x80000002)
			memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
		else if (i == 0x80000003)
			memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
		else if (i == 0x80000004)
			memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));
	}
	return std::string(cpuBrandString);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpProgress(const char* message)
{
	progressReportedLastTimestampMS = GetTimeMilliSeconds();

	OutputDataStream stream;
	stream << message;

	Server::Get().Send(DataResponse::ReportProgress, stream);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpEvents(EventStorage& entry, const EventTime& timeSlice, ScopeData& scope)
{
	if (!entry.eventBuffer.IsEmpty())
	{
		const EventData* rootEvent = nullptr;

		entry.eventBuffer.ForEach([&](const EventData& data)
		{
			if (data.finish >= data.start && data.start >= timeSlice.start && timeSlice.finish >= data.finish)
			{
				if (!rootEvent)
				{
					rootEvent = &data;
					scope.InitRootEvent(*rootEvent);
				} 
				else if (rootEvent->finish < data.finish)
				{
					scope.Send();

					rootEvent = &data;
					scope.InitRootEvent(*rootEvent);
				}
				else
				{
					scope.AddEvent(data);
				}
			}
		});

		scope.Send();

		entry.eventBuffer.Clear(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpTags(EventStorage& entry, ScopeData& scope)
{
	if (!entry.tagFloatBuffer.IsEmpty() ||
		!entry.tagS32Buffer.IsEmpty() ||
		!entry.tagU32Buffer.IsEmpty() ||
		!entry.tagU64Buffer.IsEmpty() ||
		!entry.tagPointBuffer.IsEmpty() ||
		!entry.tagStringBuffer.IsEmpty())
	{
		OutputDataStream tagStream;
		tagStream << scope.header.boardNumber << scope.header.threadNumber;
		tagStream  
			<< (uint32)0
			<< entry.tagFloatBuffer
			<< entry.tagU32Buffer
			<< entry.tagS32Buffer
			<< entry.tagU64Buffer
			<< entry.tagPointBuffer
			<< (uint32)0
			<< (uint32)0
			<< entry.tagStringBuffer;
		Server::Get().Send(DataResponse::TagsPack, tagStream);

		entry.ClearTags(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpThread(ThreadEntry& entry, const EventTime& timeSlice, ScopeData& scope)
{
	// We need to sort events for all the custom thread storages
	if (entry.description.threadID == INVALID_THREAD_ID)
		entry.Sort();

	// Events
	DumpEvents(entry.storage, timeSlice, scope);
	DumpTags(entry.storage, scope);
	BRO_ASSERT(entry.storage.fiberSyncBuffer.IsEmpty(), "Fiber switch events in native threads?");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpFiber(FiberEntry& entry, const EventTime& timeSlice, ScopeData& scope)
{
	// Events
	DumpEvents(entry.storage, timeSlice, scope);

	if (!entry.storage.fiberSyncBuffer.IsEmpty())
	{
		OutputDataStream fiberSynchronizationStream;
		fiberSynchronizationStream << scope.header.boardNumber;
		fiberSynchronizationStream << scope.header.fiberNumber;
		fiberSynchronizationStream << entry.storage.fiberSyncBuffer;
		Server::Get().Send(DataResponse::FiberSynchronizationData, fiberSynchronizationStream);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpFrames(uint32 mode)
{
	if (frames.empty() || threads.empty())
		return;

	++boardNumber;

	DumpProgress("Generating summary...");
	if (stateCallback != nullptr)
	{
		stateCallback(BRO_DUMP_CAPTURE);
	}
	GenerateCommonSummary();
	DumpSummary();

	DumpProgress("Collecting Frame Events...");

	EventTime timeSlice;
	timeSlice.start = frames.front().start;
	timeSlice.finish = frames.back().finish;

	DumpBoard(mode, timeSlice);

	ScopeData threadScope;
	threadScope.header.boardNumber = boardNumber;
	threadScope.header.fiberNumber = -1;

	if (gpuProfiler)
		gpuProfiler->Dump(mode);

	for (size_t i = 0; i < threads.size(); ++i)
	{
		threadScope.header.threadNumber = (uint32)i;
		DumpThread(*threads[i], timeSlice, threadScope);
	}

	ScopeData fiberScope;
	fiberScope.header.boardNumber = (uint32)boardNumber;
	fiberScope.header.threadNumber = -1;
	for (size_t i = 0; i < fibers.size(); ++i)
	{
		fiberScope.header.fiberNumber = (uint32)i;
		DumpFiber(*fibers[i], timeSlice, fiberScope);
	}

	frames.clear();
	CleanupThreadsAndFibers();

	{
		DumpProgress("Serializing SwitchContexts");
		OutputDataStream switchContextsStream;
		switchContextsStream << boardNumber;
		switchContextCollector.Serialize(switchContextsStream);
		Server::Get().Send(DataResponse::SynchronizationData, switchContextsStream);
	}

	{
		DumpProgress("Serializing SysCalls");
		OutputDataStream callstacksStream;
		callstacksStream << boardNumber;
		syscallCollector.Serialize(callstacksStream);
		Server::Get().Send(DataResponse::SyscallPack, callstacksStream);
	}

	if (!callstackCollector.IsEmpty())
	{
		DumpProgress("Resolving callstacks");
		OutputDataStream symbolsStream;
		symbolsStream << boardNumber;
		callstackCollector.SerializeSymbols(symbolsStream);
		Server::Get().Send(DataResponse::CallstackDescriptionBoard, symbolsStream);

		DumpProgress("Serializing callstacks");
		OutputDataStream callstacksStream;
		callstacksStream << boardNumber;
		callstackCollector.SerializeCallstacks(callstacksStream);
		Server::Get().Send(DataResponse::CallstackPack, callstacksStream);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpSummary()
{
	OutputDataStream stream;

	// Board Number
	stream << boardNumber;

	// Frames
	double frequency = (double)GetFrequency();
	stream << (uint32_t)frames.size();
	for (const EventTime& frame : frames)
	{
		double frameTimeMs = 1000.0 * (frame.finish - frame.start) / frequency;
		stream << (float)frameTimeMs;
	}

	// Summary
	stream << (uint32_t)summary.size();
	for (size_t i = 0; i < summary.size(); ++i)
		stream << summary[i].first << summary[i].second;
	summary.clear();

	// Attachments
	stream << (uint32_t)attachments.size();
	for (const Attachment& att : attachments)
		stream << (uint32_t)att.type << att.name << att.data;
	attachments.clear();

	// Send
	Server::Get().Send(DataResponse::SummaryPack, stream);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::CleanupThreadsAndFibers()
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);

	for (ThreadList::iterator it = threads.begin(); it != threads.end();)
	{
		if (!(*it)->isAlive)
		{
			Memory::Delete(*it);
			it = threads.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void Core::DumpBoard(uint32 mode, EventTime timeSlice)
{
	uint32 mainThreadIndex = 0;

	for (size_t i = 0; i < threads.size(); ++i)
		if (threads[i]->description.threadID == mainThreadID)
			mainThreadIndex = (uint32)i;

	OutputDataStream boardStream;

	boardStream << boardNumber;
	boardStream << GetFrequency();
	boardStream << (uint64)0; // Origin
	boardStream << (uint32)0; // Precision
	boardStream << timeSlice;
	boardStream << threads;
	boardStream << fibers;
	boardStream << mainThreadIndex;
	boardStream << EventDescriptionBoard::Get();
	boardStream << (uint32)0; // Tags
	boardStream << (uint32)0; // Run
	boardStream << (uint32)0; // Filters
	boardStream << (uint32)0; // ThreadDescs
	boardStream << mode; // Mode
	boardStream << processDescs;
	boardStream << threadDescs;
	boardStream << (uint32)GetProcessID();
	boardStream << (uint32)std::thread::hardware_concurrency();
	Server::Get().Send(DataResponse::FrameDescriptionBoard, boardStream);

	// Cleanup
	processDescs.clear();
	threadDescs.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::GenerateCommonSummary()
{
	AttachSummary("CPU", GetCPUName().c_str());
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Core::Core() : progressReportedLastTimestampMS(0), isActive(false), stateCallback(nullptr), boardNumber(0), gpuProfiler(nullptr), frameNumber(0)
{
	mainThreadID = GetThreadID();
	schedulerTrace = Trace::Get();
	symbolEngine = SymbolEngine::Get();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Core::Update()
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);
	
	if (isActive)
	{
		if (!frames.empty())
			frames.back().Stop();

		if (IsTimeToReportProgress())
			DumpCapturingProgress();		
	}

	UpdateEvents();

	if (isActive)
	{
		frames.push_back(EventTime());
		frames.back().Start();
	}

	return ++frameNumber;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::UpdateEvents()
{
	Server::Get().Update();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::ReportSwitchContext(const SwitchContextDesc& desc)
{
	switchContextCollector.Add(desc);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::ReportStackWalk(const CallstackDesc& desc)
{
	callstackCollector.Add(desc);
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::Activate( bool active )
{
	if (isActive != active)
	{
		isActive = active;

		for(auto it = threads.begin(); it != threads.end(); ++it)
		{
			ThreadEntry* entry = *it;
			entry->Activate(active);
		}

		if (active)
		{
			CaptureStatus::Type status = CaptureStatus::FAILED;

			if (schedulerTrace)
			{
				status = schedulerTrace->Start(Trace::ALL, threads);
				// Let's retry with more narrow setup
				if (status != CaptureStatus::OK)
					status = schedulerTrace->Start(Trace::SWITCH_CONTEXTS, threads);
			}

			if (gpuProfiler)
				gpuProfiler->Start(0);

			SendHandshakeResponse(status);
		}
		else
		{
			if (schedulerTrace)
				schedulerTrace->Stop();

			if (gpuProfiler)
				gpuProfiler->Stop(0);
		}

		if (stateCallback != nullptr)
			stateCallback(isActive ? BRO_START_CAPTURE : BRO_STOP_CAPTURE);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::DumpCapturingProgress()
{
	std::stringstream stream;

	if (isActive)
		stream << "Capturing Frame " << (uint32)frames.size() << std::endl;

	DumpProgress(stream.str().c_str());
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::IsTimeToReportProgress() const
{
	return GetTimeMilliSeconds() > progressReportedLastTimestampMS + 200;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::SendHandshakeResponse(CaptureStatus::Type status)
{
	OutputDataStream stream;
	stream << (uint32)status;
	Server::Get().Send(DataResponse::Handshake, stream);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::IsRegistredThread(ThreadID id)
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);

	for (ThreadList::iterator it = threads.begin(); it != threads.end(); ++it)
	{
		ThreadEntry* entry = *it;
		if (entry->description.threadID == id)
		{
			return true;
		}
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThreadEntry* Core::RegisterThread(const ThreadDescription& description, EventStorage** slot)
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);

	ThreadEntry* entry = Memory::New<ThreadEntry>(description, slot);
	threads.push_back(entry);

	if (isActive && slot != nullptr)
		*slot = &entry->storage;

	return entry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::UnRegisterThread(ThreadID threadID)
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);

	for (ThreadList::iterator it = threads.begin(); it != threads.end(); ++it)
	{
		ThreadEntry* entry = *it;
		if (entry->description.threadID == threadID && entry->isAlive)
		{
			if (!isActive)
			{
				Memory::Delete(entry);
				threads.erase(it);
				return true;
			}
			else
			{
				entry->isAlive = false;
				return true;
			}
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::RegisterFiber(const FiberDescription& description, EventStorage** slot)
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);
	FiberEntry* entry = Memory::New<FiberEntry>(description);
	fibers.push_back(entry);
	entry->storage.isFiberStorage = true;
	*slot = &entry->storage;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::RegisterProcessDescription(const ProcessDescription& description)
{
	processDescs.push_back(description);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::RegisterThreadDescription(const ThreadDescription& description)
{
	threadDescs.push_back(description);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::SetStateChangedCallback(BroStateCallback cb)
{
	stateCallback = cb;
	return stateCallback != nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::AttachSummary(const char* key, const char* value)
{
	summary.push_back(make_pair(std::string(key), std::string(value)));
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Core::AttachFile(BroFile::Type type, const char* name, const uint8_t* data, uint32_t size)
{
	attachments.push_back(Attachment(type, name));
	Attachment& attachment = attachments.back();
	attachment.data = std::vector<uint8_t>(data, data + size);
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Core::InitGPUProfiler(GPUProfiler* profiler)
{
	BRO_ASSERT(gpuProfiler == nullptr, "Can't reinitialize GPU profiler! Not supported yet!");
	Memory::Delete<GPUProfiler>(gpuProfiler);
	gpuProfiler = profiler;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Core::~Core()
{
	std::lock_guard<std::recursive_mutex> lock(coreLock);

	for (ThreadList::iterator it = threads.begin(); it != threads.end(); ++it)
	{
		Memory::Delete(*it);
	}
	threads.clear();

	for (FiberList::iterator it = fibers.begin(); it != fibers.end(); ++it)
	{
		Memory::Delete(*it);
	}
	fibers.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<ThreadEntry*>& Core::GetThreads() const
{
	return threads;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BRO_THREAD_LOCAL EventStorage* Core::storage = nullptr;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Core Core::notThreadSafeInstance;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ScopeHeader::ScopeHeader() : boardNumber(0), threadNumber(0)
{

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const ScopeHeader& header)
{
	return stream << header.boardNumber << header.threadNumber << header.fiberNumber << header.event;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const ScopeData& ob)
{
	return stream << ob.header << ob.categories << ob.events;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const ThreadDescription& description)
{
	return stream << description.threadID << description.processID << description.name << description.maxDepth << description.priority << description.mask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const ThreadEntry* entry)
{
	return stream << entry->description;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const FiberDescription& description)
{
	return stream << description.id;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const FiberEntry* entry)
{
	return stream << entry->description;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const ProcessDescription& description)
{
	return stream << description.processID << description.name << description.uniqueKey;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool SetStateChangedCallback(BroStateCallback cb)
{
	return Core::Get().SetStateChangedCallback(cb);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool AttachSummary(const char* key, const char* value)
{
	return Core::Get().AttachSummary(key, value);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool AttachFile(BroFile::Type type, const char* name, const uint8_t* data, uint32_t size)
{
	return Core::Get().AttachFile(type, name, data, size);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator<<(OutputDataStream& stream, const BroPoint& ob)
{
	return stream << ob.x << ob.y << ob.z;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API uint32_t NextFrame()
{
	return Core::NextFrame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool IsActive()
{
	return Core::Get().isActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API EventStorage** GetEventStorageSlotForCurrentThread()
{
	return &Core::Get().storage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool IsFiberStorage(EventStorage* fiberStorage)
{
	return fiberStorage->isFiberStorage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool RegisterThread(const char* name)
{
	return Core::Get().RegisterThread(ThreadDescription(name, GetThreadID(), GetProcessID()), &Core::storage) != nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool RegisterThread(const wchar_t* name)
{
	const int THREAD_NAME_LENGTH = 128;
	char mbName[THREAD_NAME_LENGTH];
	size_t numConverted = 0;
	wcstombs_s(&numConverted, mbName, name, THREAD_NAME_LENGTH);
	return Core::Get().RegisterThread(ThreadDescription(mbName, GetThreadID(), GetProcessID()), &Core::storage) != nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool UnRegisterThread()
{
	return Core::Get().UnRegisterThread(GetThreadID());
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API bool RegisterFiber(uint64 fiberId, EventStorage** slot)
{
	return Core::Get().RegisterFiber(FiberDescription(fiberId), slot);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API EventStorage* RegisterStorage(const char* name)
{
	ThreadEntry* entry = Core::Get().RegisterThread(ThreadDescription(name, INVALID_THREAD_ID, GetProcessID(), false), nullptr);
	return entry ? &entry->storage : nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API void GpuFlip(void* swapChain)
{
	if (GPUProfiler* gpuProfiler = Core::Get().gpuProfiler)
		gpuProfiler->Flip(swapChain);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROFILER_API GPUContext SetGpuContext(GPUContext context)
{
	if (EventStorage* storage = Core::storage)
	{
		GPUContext prevContext = storage->gpuStorage.context;
		storage->gpuStorage.context = context;
		return prevContext;
	}
	return GPUContext();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventStorage::EventStorage(): isFiberStorage(false), pushPopEventStackIndex(0)
{
	 
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThreadEntry::Activate(bool isActive)
{
	if (!isAlive)
		return;

	if (isActive)
		storage.Clear(true);

	if (threadTLS != nullptr)
	{
		*threadTLS = isActive ? &storage : nullptr;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThreadEntry::Sort()
{
	SortMemoryPool(storage.eventBuffer);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsSleepOnlyScope(const ScopeData& scope)
{
	if (!scope.categories.empty())
		return false;

	const std::vector<EventData>& events = scope.events;
	for(auto it = events.begin(); it != events.end(); ++it)
	{
		const EventData& data = *it;

		if (data.description->color != Color::White)
		{
			return false;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ScopeData::Send()
{
	if (!events.empty() || !categories.empty())
	{
		if (!IsSleepOnlyScope(*this))
		{
			OutputDataStream frameStream;
			frameStream << *this;
			Server::Get().Send(DataResponse::EventFrame, frameStream);
		}
	}

	Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ScopeData::Clear()
{
	events.clear();
	categories.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventStorage::GPUStorage::Clear(bool preserveMemory)
{
	for (int i = 0; i < MAX_GPU_NODES; ++i)
		for (int j = 0; j < GPU_QUEUE_COUNT; ++j)
			gpuBuffer[i][j].Clear(preserveMemory);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventData* EventStorage::GPUStorage::Start(const EventDescription &desc)
{
	if (GPUProfiler* gpuProfiler = Core::Get().gpuProfiler)
	{
		EventData& result = gpuBuffer[context.node][context.queue].Add();
		result.description = &desc;
		result.start = EventTime::INVALID_TIMESTAMP;
		result.finish = EventTime::INVALID_TIMESTAMP;
		gpuProfiler->QueryTimestamp(context.cmdBuffer, &result.start);
		return &result;
	}
	return nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventStorage::GPUStorage::Stop(EventData& data)
{
	if (GPUProfiler* gpuProfiler = Core::Get().gpuProfiler)
	{
		gpuProfiler->QueryTimestamp(context.cmdBuffer, &data.finish);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

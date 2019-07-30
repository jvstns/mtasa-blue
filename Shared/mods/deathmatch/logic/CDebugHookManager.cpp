/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        CDebugHookManager.h
 *
 *  Multi Theft Auto is available from http://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#ifdef MTA_CLIENT
    #define DECLARE_PROFILER_SECTION_CDebugHookManager
    #include "profiler/SharedUtil.Profiler.h"
    #define g_pGame g_pClientGame
#else
    #define DECLARE_PROFILER_SECTION(tag)
#endif

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::CDebugHookManager
//
//
//
///////////////////////////////////////////////////////////////
CDebugHookManager::CDebugHookManager()
{
#ifndef MTA_CLIENT
    m_MaskArgumentsMap["dbConnect"] = {1, 2, 3};               // type, 1=HOST, 2=USERNAME, 3=PASSWORD, options
    m_MaskArgumentsMap["logIn"] = {2};                         // player, account, 2=PASSWORD
    m_MaskArgumentsMap["addAccount"] = {1};                    // name, 1=PASSWORD
    m_MaskArgumentsMap["getAccount"] = {1};                    // name, 1=PASSWORD
    m_MaskArgumentsMap["setAccountPassword"] = {1};            // account, 1=PASSWORD
#endif
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::~CDebugHookManager
//
//
//
///////////////////////////////////////////////////////////////
CDebugHookManager::~CDebugHookManager()
{
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::GetHookInfoListForType
//
//
//
///////////////////////////////////////////////////////////////
std::vector<SDebugHookCallInfo>& CDebugHookManager::GetHookInfoListForType(EDebugHookType hookType)
{
    switch (hookType)
    {
        case EDebugHookType::PRE_EVENT:
            return m_PreEventHookList;
        case EDebugHookType::POST_EVENT:
            return m_PostEventHookList;
        case EDebugHookType::PRE_FUNCTION:
            return m_PreFunctionHookList;
        case EDebugHookType::POST_FUNCTION:
            return m_PostFunctionHookList;
        case EDebugHookType::PRE_EVENT_FUNCTION:
            return m_PreEventFunctionHookList;
        case EDebugHookType::POST_EVENT_FUNCTION:
            return m_PostEventFunctionHookList;
        case EDebugHookType::MAX_DEBUG_HOOK_TYPE:
        default:
            dassert(hookType == EDebugHook::POST_FUNCTION);
            return m_PostFunctionHookList;
    }
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::AddDebugHook
//
// Returns true if hook was added
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::AddDebugHook(EDebugHookType hookType, const CLuaFunctionRef& functionRef, const std::vector<SString>& allowedNameList)
{
    std::vector<SDebugHookCallInfo>& hookInfoList = GetHookInfoListForType(hookType);
    for (std::vector<SDebugHookCallInfo>::iterator iter = hookInfoList.begin(); iter != hookInfoList.end(); ++iter)
    {
        if ((*iter).functionRef == functionRef)
            return false;
    }

    SDebugHookCallInfo info;
    info.functionRef = functionRef;
    info.pLuaMain = g_pGame->GetLuaManager()->GetVirtualMachine(functionRef.GetLuaVM());
    if (!info.pLuaMain)
        return false;

    for (uint i = 0; i < allowedNameList.size(); i++)
        MapInsert(info.allowedNameMap, allowedNameList[i]);

    hookInfoList.push_back(info);
    return true;
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::RemoveDebugHook
//
// Returns true if hook was removed
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::RemoveDebugHook(EDebugHookType hookType, const CLuaFunctionRef& functionRef)
{
    CLuaMain* pLuaMain = g_pGame->GetLuaManager()->GetVirtualMachine(functionRef.GetLuaVM());

    std::vector<SDebugHookCallInfo>& hookInfoList = GetHookInfoListForType(hookType);
    for (std::vector<SDebugHookCallInfo>::iterator iter = hookInfoList.begin(); iter != hookInfoList.end(); ++iter)
    {
        if ((*iter).pLuaMain == pLuaMain && (*iter).functionRef == functionRef)
        {
            hookInfoList.erase(iter);
            return true;
        }
    }

    return false;
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnLuaMainDestroy
//
// When a Lua VM is stopped
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::OnLuaMainDestroy(CLuaMain* pLuaMain)
{
    for (uint hookType = EDebugHook::PRE_EVENT; hookType < EDebugHook::MAX_DEBUG_HOOK_TYPE; hookType++)
    {
        std::vector<SDebugHookCallInfo>& hookInfoList = GetHookInfoListForType((EDebugHookType)hookType);
        for (uint i = 0; i < hookInfoList.size();)
        {
            if (hookInfoList[i].pLuaMain == pLuaMain)
                ListRemoveIndex(hookInfoList, i);
            else
                i++;
        }
    }
}

///////////////////////////////////////////////////////////////
//
// GetDebugInfo
//
// Get current Lua source file and line number
//
///////////////////////////////////////////////////////////////
void GetDebugInfo(lua_State* luaVM, lua_Debug& debugInfo, const char*& szFilename, int& iLineNumber)
{
    if (luaVM && lua_getstack(luaVM, 1, &debugInfo))
    {
        lua_getinfo(luaVM, "nlS", &debugInfo);

        // Make sure this function isn't defined in a string
        if (debugInfo.source[0] == '@')
        {
            szFilename = debugInfo.source;
            iLineNumber = debugInfo.currentline != -1 ? debugInfo.currentline : debugInfo.linedefined;
        }
        else
        {
            szFilename = debugInfo.short_src;
        }

        // Remove path
        if (const char* szNext = strrchr(szFilename, '\\'))
            szFilename = szNext + 1;
        if (const char* szNext = strrchr(szFilename, '/'))
            szFilename = szNext + 1;
    }
}

///////////////////////////////////////////////////////////////
//
// GetMapEventDebugInfo
//
// Get current Lua source file and line number
//
///////////////////////////////////////////////////////////////
void GetMapEventDebugInfo(CMapEvent* pMapEvent, const char*& szFilename, int& iLineNumber)
{
    CLuaMain* pLuaMain = pMapEvent->GetVM();

    if (!pLuaMain)
        return;

    lua_State* luaVM = pLuaMain->GetVirtualMachine();

    if (!luaVM)
        return;

    const CLuaFunctionRef& iLuaFunction = pMapEvent->GetLuaFunction();
    lua_Debug              debugInfo;
    lua_getref(luaVM, iLuaFunction.ToInt());

    if (lua_getinfo(luaVM, ">lS", &debugInfo))
    {
        // Make sure this function isn't defined in a string
        if (debugInfo.source[0] == '@')
        {
            szFilename = debugInfo.source;
            iLineNumber = debugInfo.currentline != -1 ? debugInfo.currentline : debugInfo.linedefined;
        }
        else
        {
            szFilename = debugInfo.short_src;
        }

        // Remove path
        if (const char* szNext = strrchr(szFilename, '\\'))
            szFilename = szNext + 1;

        if (const char* szNext = strrchr(szFilename, '/'))
            szFilename = szNext + 1;
    }
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPreFunction
//
// Called before an MTA function is called
// Returns false if function call should be skipped
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::OnPreFunction(lua_CFunction f, lua_State* luaVM, bool bAllowed)
{
    DECLARE_PROFILER_SECTION(OnPreFunction)

    if (m_PreFunctionHookList.empty())
        return true;

    CLuaCFunction* pFunction = CLuaCFunctions::GetFunction(f);
    if (!pFunction)
        return true;

    const SString& strName = pFunction->GetName();
    bool           bNameMustBeExplicitlyAllowed = MustNameBeExplicitlyAllowed(strName);

    // Check if named function is pre hooked
    if (!IsNameAllowed(strName, m_PreFunctionHookList, bNameMustBeExplicitlyAllowed))
        return true;

    CLuaArguments NewArguments;
    GetFunctionCallHookArguments(NewArguments, strName, luaVM, bAllowed);

    return CallHook(strName, m_PreFunctionHookList, NewArguments, bNameMustBeExplicitlyAllowed);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPostFunction
//
// Called after an MTA function is called
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::OnPostFunction(lua_CFunction f, lua_State* luaVM)
{
    DECLARE_PROFILER_SECTION(OnPostFunction)

    if (m_PostFunctionHookList.empty())
        return;

    CLuaCFunction* pFunction = CLuaCFunctions::GetFunction(f);
    if (!pFunction)
        return;

    const SString& strName = pFunction->GetName();
    bool           bNameMustBeExplicitlyAllowed = MustNameBeExplicitlyAllowed(strName);

    // Check if named function is post hooked
    if (!IsNameAllowed(strName, m_PostFunctionHookList, bNameMustBeExplicitlyAllowed))
        return;

    CLuaArguments NewArguments;
    GetFunctionCallHookArguments(NewArguments, strName, luaVM, true);

    CallHook(strName, m_PostFunctionHookList, NewArguments, bNameMustBeExplicitlyAllowed);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::GetFunctionCallHookArguments
//
// Get call hook arguments for OnPre/PostFunction
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::GetFunctionCallHookArguments(CLuaArguments& NewArguments, const SString& strName, lua_State* luaVM, bool bAllowed)
{
    // Get file/line number
    const char* szFilename = "";
    int         iLineNumber = 0;
    lua_Debug   debugInfo;
    GetDebugInfo(luaVM, debugInfo, szFilename, iLineNumber);

    CLuaMain*  pSourceLuaMain = g_pGame->GetScriptDebugging()->GetTopLuaMain();
    CResource* pSourceResource = pSourceLuaMain ? pSourceLuaMain->GetResource() : NULL;

    if (pSourceResource)
        NewArguments.PushResource(pSourceResource);
    else
        NewArguments.PushNil();
    NewArguments.PushString(strName);
    NewArguments.PushBoolean(bAllowed);
    NewArguments.PushString(szFilename);
    NewArguments.PushNumber(iLineNumber);

    CLuaArguments FunctionArguments;
    FunctionArguments.ReadArguments(luaVM);
    MaybeMaskArgumentValues(strName, FunctionArguments);
    NewArguments.PushArguments(FunctionArguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPreEvent
//
// Called before a Lua event is triggered
// Returns false if event should be skipped
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::OnPreEvent(const char* szName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller)
{
    if (m_PreEventHookList.empty())
        return true;

    // Check if named event is pre hooked
    if (!IsNameAllowed(szName, m_PreEventHookList))
        return true;

    CLuaArguments NewArguments;
    GetEventCallHookArguments(NewArguments, szName, Arguments, pSource, pCaller);

    return CallHook(szName, m_PreEventHookList, NewArguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPostEvent
//
// Called after a Lua event is triggered
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::OnPostEvent(const char* szName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller)
{
    if (m_PostEventHookList.empty())
        return;

    // Check if named event is post hooked
    if (!IsNameAllowed(szName, m_PostEventHookList))
        return;

    CLuaArguments NewArguments;
    GetEventCallHookArguments(NewArguments, szName, Arguments, pSource, pCaller);

    CallHook(szName, m_PostEventHookList, NewArguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::GetEventCallHookArguments
//
// Get call hook arguments for OnPre/PostEvent
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::GetEventCallHookArguments(CLuaArguments& NewArguments, const SString& strName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller)
{
    CLuaMain*  pSourceLuaMain = g_pGame->GetScriptDebugging()->GetTopLuaMain();
    CResource* pSourceResource = pSourceLuaMain ? pSourceLuaMain->GetResource() : NULL;

    // Get file/line number
    const char* szFilename = "";
    int         iLineNumber = 0;
    lua_Debug   debugInfo;
    lua_State*  luaVM = pSourceLuaMain ? pSourceLuaMain->GetVM() : NULL;
    if (luaVM)
        GetDebugInfo(luaVM, debugInfo, szFilename, iLineNumber);

    if (pSourceResource)
        NewArguments.PushResource(pSourceResource);
    else
        NewArguments.PushNil();
    NewArguments.PushString(strName);
    NewArguments.PushElement(pSource);
    NewArguments.PushElement(pCaller);
    NewArguments.PushString(szFilename);
    NewArguments.PushNumber(iLineNumber);
    NewArguments.PushArguments(Arguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPreEventFunction
//
// Called before a Lua event function is called
// Returns false if function call should be skipped
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::OnPreEventFunction(const char* szName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller, CMapEvent* pMapEvent)
{
    if (m_PreEventFunctionHookList.empty())
        return true;

    // Check if named event function is pre hooked
    if (!IsNameAllowed(szName, m_PreEventFunctionHookList))
        return true;

    CLuaArguments NewArguments;
    GetEventFunctionCallHookArguments(NewArguments, szName, Arguments, pSource, pCaller, pMapEvent);

    return CallHook(szName, m_PreEventFunctionHookList, NewArguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::OnPostEventFunction
//
// Called after a Lua event function is called
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::OnPostEventFunction(const char* szName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller, CMapEvent* pMapEvent)
{
    if (m_PostEventFunctionHookList.empty())
        return;

    // Check if named event function is post hooked
    if (!IsNameAllowed(szName, m_PostEventFunctionHookList))
        return;

    CLuaArguments NewArguments;
    GetEventFunctionCallHookArguments(NewArguments, szName, Arguments, pSource, pCaller, pMapEvent);

    CallHook(szName, m_PostEventFunctionHookList, NewArguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::GetEventFunctionCallHookArguments
//
// Get call hook arguments for OnPre/PostEventFunction
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::GetEventFunctionCallHookArguments(CLuaArguments& NewArguments, const SString& strName, const CLuaArguments& Arguments, CElement* pSource, CPlayer* pCaller, CMapEvent* pMapEvent)
{
    CLuaMain*  pEventLuaMain = g_pGame->GetScriptDebugging()->GetTopLuaMain();
    CResource* pEventResource = pEventLuaMain ? pEventLuaMain->GetResource() : NULL;

    // Get file/line number for event
    const char* szEventFilename = "";
    int         iEventLineNumber = 0;
    lua_Debug   eventDebugInfo;
    lua_State*  eventLuaVM = pEventLuaMain ? pEventLuaMain->GetVM() : NULL;
    if (eventLuaVM)
        GetDebugInfo(eventLuaVM, eventDebugInfo, szEventFilename, iEventLineNumber);

    // Get file/line number for function
    const char* szFunctionFilename = "";
    int         iFunctionLineNumber = 0;
    GetMapEventDebugInfo(pMapEvent, szFunctionFilename, iFunctionLineNumber);

    CLuaMain*  pFunctionLuaMain = pMapEvent->GetVM();
    CResource* pFunctionResource = pFunctionLuaMain ? pFunctionLuaMain->GetResource() : NULL;

    // resource eventResource, string eventName, element eventSource, element eventClient, string eventFilename, int eventLineNumber,
    if (pEventResource)
        NewArguments.PushResource(pEventResource);
    else
        NewArguments.PushNil();

    NewArguments.PushString(strName);
    NewArguments.PushElement(pSource);
    NewArguments.PushElement(pCaller);
    NewArguments.PushString(szEventFilename);
    NewArguments.PushNumber(iEventLineNumber);

    // resource functionResource, string functionFilename, int functionLineNumber, ...args
    if (pFunctionResource)
        NewArguments.PushResource(pFunctionResource);
    else
        NewArguments.PushNil();

    NewArguments.PushString(szFunctionFilename);
    NewArguments.PushNumber(iFunctionLineNumber);
    NewArguments.PushArguments(Arguments);
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::IsNameAllowed
//
// Returns true if there is a debughook which handles the name
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::IsNameAllowed(const char* szName, const std::vector<SDebugHookCallInfo>& eventHookList, bool bNameMustBeExplicitlyAllowed)
{
    for (uint i = 0; i < eventHookList.size(); i++)
    {
        const SDebugHookCallInfo& info = eventHookList[i];

        if (info.allowedNameMap.empty() && !bNameMustBeExplicitlyAllowed)
            return true;            // All names allowed

        if (MapContains(info.allowedNameMap, szName))
            return true;            // Name allowed
    }
    return false;
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::MustNameBeExplicitlyAllowed
//
// Don't trace add/removeDebugHook unless requested
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::MustNameBeExplicitlyAllowed(const SString& strName)
{
    return strName == "addDebugHook" || strName == "removeDebugHook";
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::MaybeMaskArgumentValues
//
// Mask security sensitive argument values
//
///////////////////////////////////////////////////////////////
void CDebugHookManager::MaybeMaskArgumentValues(const SString& strFunctionName, CLuaArguments& FunctionArguments)
{
    auto* pArgIndexList = MapFind(m_MaskArgumentsMap, strFunctionName);
    if (pArgIndexList)
    {
        for (uint uiIndex : *pArgIndexList)
        {
            CLuaArgument* pArgument = FunctionArguments[uiIndex];
            if (pArgument)
                pArgument->ReadString("***");
        }
    }
}

///////////////////////////////////////////////////////////////
//
// CDebugHookManager::CallHook
//
// Return false if function/event should be skipped
//
///////////////////////////////////////////////////////////////
bool CDebugHookManager::CallHook(const char* szName, const std::vector<SDebugHookCallInfo>& eventHookList, const CLuaArguments& Arguments,
                                 bool bNameMustBeExplicitlyAllowed)
{
    static bool bRecurse = false;
    if (bRecurse)
        return true;
    bRecurse = true;
    bool bSkip = false;

    for (uint i = 0; i < eventHookList.size(); i++)
    {
        const SDebugHookCallInfo& info = eventHookList[i];

        if (!info.allowedNameMap.empty() || bNameMustBeExplicitlyAllowed)
        {
            if (!MapContains(info.allowedNameMap, szName))
                continue;
        }

        lua_State* pState = info.pLuaMain->GetVirtualMachine();

        if (!pState)
            continue;

        // Save script MTA globals in case hook messes with them
        lua_getglobal(pState, "source");
        CLuaArgument OldSource(pState, -1);
        lua_pop(pState, 1);

        lua_getglobal(pState, "this");
        CLuaArgument OldThis(pState, -1);
        lua_pop(pState, 1);

        lua_getglobal(pState, "sourceResource");
        CLuaArgument OldResource(pState, -1);
        lua_pop(pState, 1);

        lua_getglobal(pState, "sourceResourceRoot");
        CLuaArgument OldResourceRoot(pState, -1);
        lua_pop(pState, 1);

        lua_getglobal(pState, "eventName");
        CLuaArgument OldEventName(pState, -1);
        lua_pop(pState, 1);

        lua_getglobal(pState, "client");
        CLuaArgument OldClient(pState, -1);
        lua_pop(pState, 1);

        CLuaArguments returnValues;
        Arguments.Call(info.pLuaMain, info.functionRef, &returnValues);
        // Note: info could be invalid now

        // Check for skip option
        if (returnValues.Count())
        {
            CLuaArgument* returnedValue = *returnValues.IterBegin();
            if (returnedValue->GetType() == LUA_TSTRING)
            {
                if (returnedValue->GetString() == "skip")
                    bSkip = true;
            }
        }

        // Reset the globals on that VM
        OldSource.Push(pState);
        lua_setglobal(pState, "source");

        OldThis.Push(pState);
        lua_setglobal(pState, "this");

        OldResource.Push(pState);
        lua_setglobal(pState, "sourceResource");

        OldResourceRoot.Push(pState);
        lua_setglobal(pState, "sourceResourceRoot");

        OldEventName.Push(pState);
        lua_setglobal(pState, "eventName");

        OldClient.Push(pState);
        lua_setglobal(pState, "client");
    }

    bRecurse = false;
    return !bSkip;
}

/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientModel.h
 *  PURPOSE:     Model handling class
 *
 *****************************************************************************/

#include "StdInc.h"
#include "game/CStreaming.h"

CClientModel::CClientModel(CClientManager* pManager, int iModelID, eClientModelType eModelType)
{
    m_pManager = pManager;
    m_iModelID = iModelID;
    m_eModelType = eModelType;
}

CClientModel::~CClientModel(void)
{
    Deallocate();
}

bool CClientModel::Allocate(ushort usParentID)
{
    m_bAllocatedByUs = true;

    CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_iModelID, true);

    // Allocate only on free IDs
    if (pModelInfo->IsValid())
        return false;

    // Avoid hierarchy
    CModelInfo* pParentModelInfo = g_pGame->GetModelInfo(usParentID, true);

    if (pParentModelInfo->GetParentID())
        return false;

    switch (m_eModelType)
    {
        case eClientModelType::PED:
            pModelInfo->MakePedModel("PSYCHO");
            return true;
        case eClientModelType::OBJECT:
            if (g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
            {
                pModelInfo->MakeObjectModel(usParentID);
                return true;
            }
            break;
        case eClientModelType::CLUMP:
            if (g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
            {
                pModelInfo->MakeClumpModel(usParentID);
                return true;
            }
            break;
        case eClientModelType::TIMED_OBJECT:
            if (g_pClientGame->GetObjectManager()->IsValidModel(usParentID))
            {
                pModelInfo->MakeTimedObjectModel(usParentID);
                return true;
            }
            break;
        case eClientModelType::VEHICLE:
            if (g_pClientGame->GetVehicleManager()->IsValidModel(usParentID))
            {
                pModelInfo->MakeVehicleAutomobile(usParentID);
                return true;
            }
            break;
        default:
            return false;
    }
    return false;
}

bool CClientModel::Deallocate()
{
    if (!m_bAllocatedByUs)
        return false;
    CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_iModelID, true);
    if (!pModelInfo || !pModelInfo->IsValid())
        return false;

    SetParentResource(nullptr);

    switch (m_eModelType)
    {
        case eClientModelType::PED:
        case eClientModelType::OBJECT:
        case eClientModelType::VEHICLE:
            return DeallocateDFF(pModelInfo);
        case eClientModelType::TXD:
            return DeallocateTXD(pModelInfo);
        default:
            return false;
    }
}

bool CClientModel::DeallocateDFF(CModelInfo* pModelInfo)
{
    auto unloadModelsAndCallEvents = [&](auto iterBegin, auto iterEnd, unsigned short usParentID, auto setElementModelLambda) {
        for (auto iter = iterBegin; iter != iterEnd; iter++)
        {
            auto& element = **iter;

            if (element.GetModel() != m_iModelID)
                continue;

            if (element.IsStreamedIn())
                element.StreamOutForABit();

            setElementModelLambda(element);

            CLuaArguments Arguments;
            Arguments.PushNumber(m_iModelID);
            Arguments.PushNumber(usParentID);
            element.CallEvent("onClientElementModelChange", Arguments, true);
        }
    };

    switch (m_eModelType)
    {
        case eClientModelType::PED:
        {
            // If some ped is using this ID, change him to CJ
            CClientPedManager* pPedManager = g_pClientGame->GetManager()->GetPedManager();

            unloadModelsAndCallEvents(pPedManager->IterBegin(), pPedManager->IterEnd(), 0, [](auto& element) { element.SetModel(0); });
            break;
        }
        case eClientModelType::CLUMP:
        case eClientModelType::OBJECT:
        case eClientModelType::TIMED_OBJECT:
        {
            const auto&    objects = &g_pClientGame->GetManager()->GetObjectManager()->GetObjects();
            unsigned short usParentID = g_pGame->GetModelInfo(m_iModelID)->GetParentID();

            unloadModelsAndCallEvents(objects->begin(), objects->end(), usParentID, [=](auto& element) { element.SetModel(usParentID); });

            // Restore pickups with custom model
            CClientPickupManager* pPickupManager = g_pClientGame->GetManager()->GetPickupManager();

            unloadModelsAndCallEvents(pPickupManager->IterBegin(), pPickupManager->IterEnd(), usParentID, [=](auto& element) { element.SetModel(usParentID); });

            // Restore COL
            g_pClientGame->GetManager()->GetColModelManager()->RestoreModel(m_iModelID);
            break;
        }
        case eClientModelType::VEHICLE:
        {
            CClientVehicleManager* pVehicleManager = g_pClientGame->GetManager()->GetVehicleManager();
            unsigned short         usParentID = g_pGame->GetModelInfo(m_iModelID)->GetParentID();

            unloadModelsAndCallEvents(pVehicleManager->IterBegin(), pVehicleManager->IterEnd(), usParentID,
                                      [=](auto& element) { element.SetModelBlocking(usParentID, 255, 255); });
            break;
        }
    }

    // Restore DFF/TXD
    g_pClientGame->GetManager()->GetDFFManager()->RestoreModel(m_iModelID);

    // Remove model info
    pModelInfo->DeallocateModel();

    return true;
}

bool CClientModel::AllocateTXD(std::string &strTxdName)
{
    uint uiSlotID = g_pGame->GetPools()->AllocateTextureDictonarySlot(m_iModelID - MAX_MODEL_DFF_ID, strTxdName);
    if (uiSlotID != -1)
    {
        m_bAllocatedByUs = true;
        return true;
    }
    return false;
}

bool CClientModel::DeallocateTXD(CModelInfo* pModelInfo)
{
    uint uiTextureDictonarySlotID = pModelInfo->GetModel() - MAX_MODEL_DFF_ID;

    for (uint uiModelID = 0; uiModelID < MAX_MODEL_DFF_ID; uiModelID++)
    {
        CModelInfo* pModelInfo = g_pGame->GetModelInfo(uiModelID, true);

        if (pModelInfo->GetTextureDictionaryID() == uiTextureDictonarySlotID)
            pModelInfo->SetTextureDictionaryID(0);
    }

    g_pGame->GetPools()->RemoveTextureDictonarySlot(uiTextureDictonarySlotID);
    g_pGame->GetStreaming()->SetStreamingInfo(pModelInfo->GetModel(), 0, 0, 0, -1);

    return true;
}

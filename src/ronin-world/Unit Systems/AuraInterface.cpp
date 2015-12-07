/***
 * Demonstrike Core
 */

#include "StdAfx.h"

AuraInterface::AuraInterface()
{
    m_modifierMask.SetCount(SPELL_AURA_TOTAL);
}

AuraInterface::~AuraInterface()
{

}

void AuraInterface::Init(Unit* unit)
{
    m_Unit = unit;
}

void AuraInterface::DeInit()
{
    m_Unit = NULL;
    m_auras.clear();
}

void AuraInterface::Update(uint32 diff)
{
    // Do not use iterators because update can invalidate them when they're removed
    for(uint8 x = 0; x < TOTAL_AURAS; ++x)
    {
        if(m_auras.find(x) == m_auras.end())
            continue;
        m_auras.at(x)->Update(diff);
    }
}

void AuraInterface::OnChangeLevel(uint32 newLevel)
{
    // On target level change recalculate modifiers where caster is unit
    for(std::map<uint8, Aura*>::iterator itr = m_auras.begin(); itr != m_auras.end(); itr++)
    {
        // For now, we'll only update auras we've casted on ourself
        if(m_Unit->GetGUID() != itr->second->GetCasterGUID())
            continue;

        itr->second->OnTargetChangeLevel(newLevel, m_Unit->GetGUID());
    }
}

uint8 AuraInterface::GetFreeSlot(bool ispositive)
{
    uint8 begin = ispositive ? 0 : MAX_POSITIVE_AURAS, end = ispositive ? MAX_POSITIVE_AURAS : MAX_AURAS;
    for (uint8 i = begin; i < end; i++)
    {
        if(m_auras.find(i) == m_auras.end())
        {
            // Return the index
            return i;
        }
    }

    return 0xFF;
}

void AuraInterface::OnAuraRemove(Aura* aura, uint8 aura_slot)
{
    std::map<uint8, Aura*>::iterator itr;
    if(aura_slot > TOTAL_AURAS)
    {
        for(itr = m_auras.begin(); itr != m_auras.end(); itr++)
        {
            if(itr->second == aura)
            {   // Completely unnecessary.
                m_auras.erase(itr);
                break;
            }
        }
    }
    else
    {
        if((itr = m_auras.find(aura_slot)) != m_auras.end())
        {
            if(itr->second == aura)
                m_auras.erase(itr);
        }
        m_auras.erase(aura_slot);
    }
}

bool AuraInterface::IsDazed()
{
    for(uint8 x = 0; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->MechanicsType == MECHANIC_ENSNARED)
                return true;

            for(uint32 y = 0; y < 3; y++)
            {
                if(m_auras.at(x)->GetSpellProto()->EffectMechanic[y]==MECHANIC_ENSNARED)
                    return true;
            }
        }
    }

    return false;
}

bool AuraInterface::IsPoisoned()
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
            if( m_auras.at(x)->GetSpellProto()->isSpellPoisonType() )
                return true;
    }

    return false;
}

void AuraInterface::UpdateDuelAuras()
{
    for( uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; ++x )
        if( m_auras.find(x) != m_auras.end())
            if(m_auras.at(x)->WasCastInDuel())
                RemoveAuraBySlot(x);
}

void AuraInterface::BuildAllAuraUpdates()
{
    for( uint8 x = 0; x < MAX_AURAS; ++x )
        if( m_auras.find(x) != m_auras.end() )
            m_auras.at(x)->BuildAuraUpdate();
}

bool AuraInterface::BuildAuraUpdateAllPacket(WorldPacket* data)
{
    if(!m_auras.size())
        return false;

    bool res = false;
    for (uint8 i = 0; i < MAX_AURAS; i++)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            res = true;
            m_auras.at(i)->BuildAuraUpdatePacket(data);
        }
    }
    return res;
}

void AuraInterface::SpellStealAuras(Unit* caster, int32 MaxSteals)
{
    Aura* aur = NULL;
    int32 spells_to_steal = MaxSteals > 1 ? MaxSteals : 1;
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            aur = m_auras.at(x);
            if(aur != NULL && aur->GetSpellId() != 15007 && !aur->IsPassive() && aur->IsPositive()) //Nothing can dispel resurrection sickness
            {
                if(aur->GetSpellProto()->DispelType == DISPEL_MAGIC && aur->GetDuration() > 0)
                {
                    WorldPacket data(SMSG_SPELLDISPELLOG, 16);
                    data << caster->GetGUID();
                    data << m_Unit->GetGUID();
                    data << uint32(1);
                    data << aur->GetSpellId();
                    caster->SendMessageToSet(&data,true);

                    Aura* aura = new Aura(aur->GetSpellProto(), caster, caster);
                    aura->AddStackSize(aur->getStackSize()-1);

                    // copy the mods across
                    for( uint32 m = 0; m < aur->GetModCount(); ++m )
                    {
                        Modifier *mod = aur->GetMod(m);
                        aura->AddMod(mod->m_type, mod->m_baseAmount, mod->m_miscValue[0], mod->m_miscValue[1], mod->i);
                    }

                    caster->AddAura(aura);
                    RemoveAuraBySlot(x);
                    if( --spells_to_steal <= 0 )
                        break; //exit loop now
                }
            }
        }
    }
}

void AuraInterface::UpdateShapeShiftAuras(uint32 oldSS, uint32 newSS)
{
    // TODO: Passive auras should not be removed, but deactivated.
    for( uint8 x = 0; x < TOTAL_AURAS; x++ )
    {
        if( m_auras.find(x) != m_auras.end() )
        {
            uint32 reqss = m_auras.at(x)->GetSpellProto()->RequiredShapeShift;
            if( reqss != 0 && m_auras.at(x)->IsPositive() )
            {
                if( oldSS > 0 && oldSS != 28)
                {
                    if( ( ((uint32)1 << (oldSS-1)) & reqss ) && // we were in the form that required it
                        !( ((uint32)1 << (newSS-1) & reqss) ) )         // new form doesnt have the right form
                    {
                        RemoveAuraBySlot(x);
                        continue;
                    }
                }
            }

            if( m_Unit->getClass() == DRUID )
            {
                for (uint8 y = 0; y < 3; y++ )
                {
                    switch( m_auras.at(x)->GetSpellProto()->EffectApplyAuraName[y])
                    {
                    case SPELL_AURA_MOD_ROOT: //Root
                    case SPELL_AURA_MOD_DECREASE_SPEED: //Movement speed
                    case SPELL_AURA_MOD_CONFUSE:  //Confuse (polymorph)
                        {
                            RemoveAuraBySlot(x);
                        }break;
                    default:
                        break;
                    }

                    if( m_auras.find(x) == m_auras.end() )
                        break;
                }
            }
        }
    }
}

void AuraInterface::AttemptDispel(Unit* caster, int32 Mechanic, bool hostile)
{
    SpellEntry *p = NULL;
    if( hostile )
    {
        for( uint8 x = 0; x < MAX_POSITIVE_AURAS; x++ )
        {
            if( m_auras.find(x) != m_auras.end() )
            {
                if(m_auras.at(x)->IsPositive())
                {
                    p = m_auras.at(x)->GetSpellProto();
                    if( Spell::HasMechanic(p, Mechanic) )
                    {
                        m_auras.at(x)->AttemptDispel( caster );
                    }
                }
            }
        }
    }
    else
    {
        for( uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++ )
        {
            if( m_auras.find(x) != m_auras.end() )
            {
                if(!m_auras.at(x)->IsPositive())
                {
                    p = m_auras.at(x)->GetSpellProto();
                    if( Spell::HasMechanic(p, Mechanic) )
                    {
                        m_auras.at(x)->AttemptDispel( caster );
                    }
                }
            }
        }
    }
}

void AuraInterface::MassDispel(Unit* caster, uint32 index, SpellEntry* Dispelling, uint32 MaxDispel, uint8 start, uint8 end)
{
    ASSERT(start < TOTAL_AURAS && end < TOTAL_AURAS);
    WorldPacket data(SMSG_SPELLDISPELLOG, 16);

    Aura* aur = NULL;
    for(uint8 x = start; x < end; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            aur = m_auras.at(x);

            //Nothing can dispel resurrection sickness;
            if(aur != NULL && !aur->IsPassive() && !(aur->GetSpellProto()->isUnstoppableForce2()))
            {
                int32 resistchance = 0;
                if(Unit* caster = aur->GetUnitCaster())
                {
                    caster->SM_FIValue(SMT_RESIST_DISPEL, &resistchance, aur->GetSpellProto()->SpellGroupType);

                    if( !Rand(resistchance) )
                    {
                        if(Dispelling->DispelType == DISPEL_ALL)
                        {
                            data.clear();
                            data << caster->GetGUID();
                            data << m_Unit->GetGUID();
                            data << (uint32)1;//probably dispel type
                            data << aur->GetSpellId();
                            caster->SendMessageToSet(&data,true);
                            aur->AttemptDispel( caster );
                            if(!--MaxDispel)
                                return;
                        }
                        else if(aur->GetSpellProto()->DispelType == Dispelling->EffectMiscValue[index])
                        {
                            data.clear();
                            data << caster->GetGUID();
                            data << m_Unit->GetGUID();
                            data << (uint32)1;
                            data << aur->GetSpellId();
                            caster->SendMessageToSet(&data,true);
                            aur->AttemptDispel( caster );
                            if(!--MaxDispel)
                                return;
                        }
                    }
                    else if( !--MaxDispel )
                        return;
                }
            }
        }
    }
}

void AuraInterface::RemoveAllAurasWithDispelType(uint32 DispelType)
{
    for( uint8 x = 0; x < MAX_AURAS; x++ )
    {
        if(m_auras.find(x) != m_auras.end())
            if(m_auras.at(x)->GetSpellProto()->DispelType == DispelType)
                RemoveAuraBySlot(x);
    }
}

void AuraInterface::RemoveAllAurasWithAttributes(uint8 index, uint32 attributeFlag)
{
    for(uint8 x=0;x<MAX_AURAS;x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto() && (m_auras.at(x)->GetSpellProto()->Attributes[index] & attributeFlag))
            {
                RemoveAuraBySlot(x);
            }
        }
    }
}

void AuraInterface::RemoveAllAurasOfSchool(uint32 School, bool Positive, bool Immune)
{
    for(uint8 x = 0; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(!Immune && (m_auras.at(x)->GetSpellProto()->isUnstoppableForce2()))
                continue;
            if(m_auras.at(x)->GetSpellProto()->School == School && (!m_auras.at(x)->IsPositive() || Positive))
                RemoveAuraBySlot(x);
        }
    }
}

void AuraInterface::RemoveAllAurasByInterruptFlagButSkip(uint32 flag, uint32 skip)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) == m_auras.end())
            continue;

        Aura *a = m_auras.at(x);
        if((a->GetSpellProto()->AuraInterruptFlags & flag) && (a->GetSpellProto()->Id != skip))
            RemoveAuraBySlot(x);
    }
}

uint32 AuraInterface::GetSpellIdFromAuraSlot(uint32 slot)
{
    if(m_auras.find(slot) != m_auras.end())
        return m_auras.at(slot)->GetSpellId();
    return 0;
}

AuraCheckResponse AuraInterface::AuraCheck(SpellEntry *info, WoWGuid casterGuid)
{
    AuraCheckResponse resp;

    // no error for now
    resp.Error = AURA_CHECK_RESULT_NONE;
    resp.Misc  = 0;

    // look for spells with same namehash
    bool stronger = false;
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if( m_auras.find(x) == m_auras.end() )
            continue;
        if(casterGuid == m_auras.at(x)->GetCasterGUID())
            continue; // We can skip auras cast by ourself

        SpellEntry *currInfo = m_auras.at(x)->GetSpellProto();
        if(info->NameHash == currInfo->NameHash || info->isSpellBuffType() && currInfo->isSpellSameBuffType(info))
        {
            if(info->RankNumber < currInfo->RankNumber)
                stronger = true;
            else if(info->maxstack > 1 && m_auras.at(x)->getStackSize() > 1)
                stronger = true;
            else
            {
                resp.Error = AURA_CHECK_RESULT_LOWER_BUFF_PRESENT;
                resp.Misc = x;
            }
        }

        if( stronger )
        {
            resp.Error = AURA_CHECK_RESULT_HIGHER_BUFF_PRESENT;
            break;
        }
    }

    return resp; // return it back to our caller
}

uint32 AuraInterface::GetAuraSpellIDWithNameHash(uint32 name_hash)
{
    for(uint8 x = 0; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == name_hash)
            {
                return m_auras.at(x)->GetSpellProto()->Id;
            }
        }
    }

    return 0;
}

bool AuraInterface::HasAura(uint32 spellid)
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellid)
            {
                return true;
            }
        }
    }

    return false;
}

bool AuraInterface::HasAuraVisual(uint32 visualid)
{
    for(uint8 x = 0; x < TOTAL_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->SpellVisual[0] == visualid || m_auras.at(x)->GetSpellProto()->SpellVisual[1] == visualid)
            {
                return true;
            }
        }
    }

    return false;
}

bool AuraInterface::HasActiveAura(uint32 spellid)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellid)
            {
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::HasNegativeAura(uint32 spell_id)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->Id == spell_id)
            {
                return true;
            }
        }
    }

    return false;
}

bool AuraInterface::HasAuraWithMechanic(uint32 mechanic)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetMechanic() == mechanic)
            {
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::HasActiveAura(uint32 spellid, WoWGuid guid)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellid && (!guid || m_auras.at(x)->GetCasterGUID() == guid))
            {
                return true;
            }
        }
    }

    return false;
}

bool AuraInterface::HasPosAuraWithMechanic(uint32 mechanic)
{
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if( m_auras.find(x) != m_auras.end() )
        {
            if( m_auras.at(x)->GetMechanic() == mechanic )
            {
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::HasNegAuraWithMechanic(uint32 mechanic)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if( m_auras.find(x) != m_auras.end() )
        {
            if( m_auras.at(x)->GetMechanic() == mechanic )
            {
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::HasNegativeAuraWithNameHash(uint32 name_hash)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; ++x)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == name_hash)
            {
                return true;
            }
        }
    }

    return false;
}

bool AuraInterface::HasCombatStatusAffectingAuras(WoWGuid checkGuid)
{
    for(uint8 i = MAX_POSITIVE_AURAS; i < MAX_AURAS; i++)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            if(checkGuid == m_auras.at(i)->GetCasterGUID() && m_auras.at(i)->IsCombatStateAffecting())
                return true;
        }
    }
    return false;
}

bool AuraInterface::HasAurasOfNameHashWithCaster(uint32 namehash, WoWGuid casterguid)
{
    for(uint8 i = 0; i < MAX_AURAS; i++)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            if(casterguid)
            {
                if(casterguid == m_auras.at(i)->GetCasterGUID() && m_auras.at(i)->GetSpellProto()->NameHash == namehash)
                    return true;
            }
            else
            {
                if(m_auras.at(i)->GetSpellProto()->NameHash == namehash)
                    return true;
            }
        }
    }
    return false;
}

bool AuraInterface::OverrideSimilarAuras(Unit *caster, Aura *aur)
{
    uint32 maxStack = aur->GetSpellProto()->maxstack;
    if( maxStack && m_Unit->IsPlayer() && castPtr<Player>(m_Unit)->stack_cheat )
        maxStack = 0xFF;

    std::set<uint8> m_aurasToRemove;
    SpellEntry *info = aur->GetSpellProto();
    for( uint8 x = 0; x < MAX_AURAS; x++ )
    {
        if(m_auras.find(x) == m_auras.end())
            continue;

        Aura* curAura = m_auras.at(x);
        if(curAura == NULL || aur == curAura || curAura->IsDeleted())
            continue;
        SpellEntry *currSP = curAura->GetSpellProto();
        if(info->NameHash == currSP->NameHash)
        {
            if(aur->IsPositive() == curAura->IsPositive())
            {
                if(info->always_apply)
                    m_aurasToRemove.insert(x);
                else if( curAura->GetCasterGUID() == aur->GetCasterGUID() )
                {
                    if(info->procCharges || maxStack > 1)
                    {
                        // target already has this aura. Update duration, time left, procCharges
                        curAura->ResetExpirationTime();
                        curAura->UpdateModifiers();
                        // If we have proc charges, reset the proc charges
                        if(info->procCharges) curAura->SetProcCharges(aur->GetMaxProcCharges(aur->GetUnitCaster()));
                        else curAura->AddStackSize(1);   // increment stack size
                        return false;
                    }
                    m_aurasToRemove.insert(x);
                }
                else
                {
                    if( currSP->RankNumber <= info->RankNumber )
                    {
                        m_aurasToRemove.insert(x);
                        continue;
                    }
                    else if(info->isSpellBuffType() && info->isSpellSameBuffType(currSP))
                    {
                        if(maxStack > 1 && curAura->getStackSize() > 1)
                            return false;
                        m_aurasToRemove.insert(x);
                    }
                    else if(info->isUnique)
                    {
                        // Unique spells need to rebound, otherwise we can just apply
                        return false;
                    }
                }
            }
            else if(curAura->IsPositive())
                m_aurasToRemove.insert(x);
            else return false;
        }
        else
        {
            if(info->isSpellBuffType() && currSP->isSpellBuffType() && info->isSpellSameBuffType(currSP))
            {
                if( currSP->isSpellAuraBuff() )
                {
                    if( curAura->GetUnitCaster() != aur->GetUnitCaster() )
                        continue;
                }
                m_aurasToRemove.insert(x);
            }
            else if( info->isSpellPoisonType() && currSP->isSpellPoisonType() && info->isSpellSameBuffType(currSP) )
            {
                if( currSP->RankNumber < info->RankNumber || maxStack == 0)
                {
                    m_aurasToRemove.insert(x);
                    continue;
                }
                else if( currSP->RankNumber > info->RankNumber )
                {
                    m_aurasToRemove.insert(x);
                    break;
                }
            }
        }
    }

    for(std::set<uint8>::iterator itr = m_aurasToRemove.begin(); itr != m_aurasToRemove.end(); itr++)
        RemoveAuraBySlot(*itr);
    m_aurasToRemove.clear();
    return true;
}

void AuraInterface::AddAura(Aura* aur, uint8 slot)
{
    Unit* pCaster = NULL;
    if(aur->GetUnitTarget() != NULL)
        pCaster = aur->GetUnitCaster();
    else if( m_Unit->GetGUID() == aur->GetCasterGUID() )
        pCaster = m_Unit;
    else if( m_Unit->GetMapMgr() && aur->GetCasterGUID())
        pCaster = m_Unit->GetMapMgr()->GetUnit( aur->GetCasterGUID());
    if(pCaster == NULL)
        return;
    if(!aur->IsPassive() && !OverrideSimilarAuras(pCaster, aur))
    {
        RemoveAura(aur);
        return;
    }

    ////////////////////////////////////////////////////////
    if( aur->GetAuraSlot() != 0xFF && aur->GetAuraSlot() < TOTAL_AURAS && m_auras.find(aur->GetAuraSlot()) != m_auras.end() )
        RemoveAuraBySlot(aur->GetAuraSlot());
    else
    {
        aur->SetAuraSlot(slot);
        if(slot != 0xFF && slot < TOTAL_AURAS && m_auras.find(slot) != m_auras.end())
            RemoveAuraBySlot(slot);
    }

    Unit* target = aur->GetUnitTarget();
    if(target == NULL)
        return; // Should never happen.

    if(aur->IsPassive())
    {
        for(uint8 x = MAX_AURAS; x < TOTAL_AURAS; x++)
        {
            if(m_auras.find(x) == m_auras.end())
            {
                m_auras.insert(std::make_pair(x, aur));
                aur->SetAuraSlot(x);
                break;
            }
        }

        if(aur->GetAuraSlot() == 0xFF)
        {
            sLog.Debug("Unit","AddAura error in passive aura. removing. SpellId: %u", aur->GetSpellProto()->Id);
            RemoveAura(aur);
            return;
        }
    }
    else if(aur->GetAuraSlot() == 0xFF || m_auras.find(aur->GetAuraSlot()) != m_auras.end())
    {
        if(!aur->AddAuraVisual())
        {
            for(uint8 x = MAX_AURAS; x < TOTAL_AURAS; x++)
            {
                if(m_auras.find(x) == m_auras.end())
                {
                    m_auras.insert(std::make_pair(x, aur));
                    aur->SetAuraSlot(x);
                    break;
                }
            }
        }

        if(aur->GetAuraSlot() == 0xFF)
        {
            sLog.Debug("Unit","AddAura error in active aura. removing. SpellId: %u", aur->GetSpellProto()->Id);
            RemoveAura(aur);
            return;
        }
    }
    else
    {
        aur->BuildAuraUpdate();
        m_auras.insert(std::make_pair(aur->GetAuraSlot(), aur));
    }

    aur->ApplyModifiers(true);

    // Reaction from enemy AI
    if( !aur->IsPositive() && CanAgroHash( aur->GetSpellProto()->NameHash ) )
    {
        if(pCaster != NULL && m_Unit->isAlive())
        {
            pCaster->CombatStatus.OnDamageDealt(castPtr<Unit>(this), 1);

            if(m_Unit->IsCreature())
                m_Unit->GetAIInterface()->AttackReaction(pCaster, 1, aur->GetSpellId());
        }
    }

    if (aur->GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_ON_INVINCIBLE)
    {
        if( pCaster != NULL )
        {
            pCaster->RemoveStealth();
            pCaster->RemoveInvisibility();
            pCaster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_ICE_BLOCK, false);
            pCaster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_DIVINE_SHIELD, false);
            pCaster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_BLESSING_OF_PROTECTION, false);
        }
    }
}

void AuraInterface::RemoveAura(Aura* aur)
{
    if(aur == NULL)
        return;

    for(uint8 x = 0; x < TOTAL_AURAS; x++)
        if(m_auras.find(x) != m_auras.end())
            if(m_auras.at(x) == aur)
                m_auras.erase(x); // Null it every time we find it.
    aur->Remove(); // Call remove once.
}

void AuraInterface::RemoveAuraBySlot(uint8 Slot)
{
    if(m_auras.find(Slot) != m_auras.end())
    {
        m_auras.at(Slot)->Remove();
        m_auras.erase(Slot);
    }
}

bool AuraInterface::RemoveAuras(uint32 * SpellIds)
{
    if(!SpellIds || *SpellIds == 0)
        return false;

    bool res = false;
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            for(uint32 y = 0; SpellIds[y] != 0; ++y)
            {
                if(m_auras.at(x)->GetSpellId()==SpellIds[y])
                {
                    RemoveAuraBySlot(x);
                    res = true;
                }
            }
        }
    }
    return res;
}

void AuraInterface::RemoveAuraNoReturn(uint32 spellId)
{   //this can be speed up, if we know passive \pos neg
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if( m_auras.at(x)->GetSpellId() == spellId )
            {
                RemoveAuraBySlot(x);
                return;
            }
        }
    }
    return;
}

bool AuraInterface::RemovePositiveAura(uint32 spellId)
{
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if( m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellId)
            {
                RemoveAuraBySlot(x);
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::RemoveNegativeAura(uint32 spellId)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if( m_auras.find(x) != m_auras.end())
        {
            if( m_auras.at(x)->GetSpellId() == spellId )
            {
                RemoveAuraBySlot(x);
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::RemoveAuraByNameHash(uint32 namehash)
{
    return RemoveAuraPosByNameHash(namehash) || RemoveAuraNegByNameHash(namehash);
}

bool AuraInterface::RemoveAuraPosByNameHash(uint32 namehash)
{
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                RemoveAuraBySlot(x);
                return true;
            }
        }
    }
    return false;
}

bool AuraInterface::RemoveAuraNegByNameHash(uint32 namehash)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                RemoveAuraBySlot(x);
                return true;
            }
        }
    }
    return false;
}

void AuraInterface::RemoveAuraBySlotOrRemoveStack(uint8 Slot)
{
    if(m_auras.find(Slot) != m_auras.end())
    {
        if(m_auras.at(Slot)->getStackSize() > 1)
        {
            m_auras.at(Slot)->RemoveStackSize(1);
            return;
        }
        m_auras.at(Slot)->Remove();
        m_auras.erase(Slot);
    }
}

bool AuraInterface::RemoveAura(uint32 spellId, WoWGuid guid )
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellId && (!guid || m_auras.at(x)->GetCasterGUID() == guid))
            {
                RemoveAuraBySlot(x);
                return true;
            }
        }
    }
    return false;
}

void AuraInterface::RemoveAllAuras()
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
            RemoveAuraBySlot(x);
    }
}

void AuraInterface::RemoveAllExpiringAuras()
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->DurationIndex == 0)
                continue;
            if(m_auras.at(x)->GetSpellProto()->isDeathPersistentAura())
                continue;
            RemoveAuraBySlot(x);
        }
    }
}

void AuraInterface::RemoveAllNegativeAuras()
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->isDeathPersistentAura())
                continue;
            RemoveAuraBySlot(x);
        }
    }
}

void AuraInterface::RemoveAllNonPassiveAuras()
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
            RemoveAuraBySlot(x);
    }
}

void AuraInterface::RemoveAllAreaAuras(WoWGuid skipguid)
{
    for (uint8 i = 0; i < MAX_POSITIVE_AURAS; ++i)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            if (m_auras.at(i)->IsAreaAura() && m_auras.at(i)->GetUnitCaster() && (!m_auras.at(i)->GetUnitCaster()
                || (m_auras.at(i)->GetUnitCaster()->IsPlayer() && (!skipguid || skipguid != m_auras.at(i)->GetCasterGUID()))))
                RemoveAuraBySlot(i);
        }
    }
}

bool AuraInterface::RemoveAllAurasFromGUID(WoWGuid guid)
{
    bool res = false;
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetCasterGUID() == guid)
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

//ex:to remove morph spells
void AuraInterface::RemoveAllAurasOfType(uint32 auratype)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            SpellEntry *proto = NULL;
            proto = m_auras.at(x)->GetSpellProto();
            if(proto != NULL && proto->EffectApplyAuraName[0] == auratype || proto->EffectApplyAuraName[1] == auratype || proto->EffectApplyAuraName[2] == auratype)
                RemoveAura(m_auras.at(x)->GetSpellId());//remove all morph auras containig to this spell (like wolf motph also gives speed)
        }
    }
}

bool AuraInterface::RemoveAllPosAurasFromGUID(WoWGuid guid)
{
    bool res = false;
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetCasterGUID() == guid)
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

bool AuraInterface::RemoveAllNegAurasFromGUID(WoWGuid guid)
{
    bool res = false;
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetCasterGUID() == guid)
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

bool AuraInterface::RemoveAllAurasByNameHash(uint32 namehash, bool passive)
{
    bool res = false;
    uint8 max = (passive ? TOTAL_AURAS : MAX_AURAS);
    for(uint8 x = 0; x < max; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                res = true;
                RemoveAuraBySlot(x);
            }
        }
    }
    return res;
}

void AuraInterface::RemoveAllAurasExpiringWithPet()
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
        if(m_auras.find(x) != m_auras.end())
            if(m_auras.at(x)->GetSpellProto()->isSpellExpiringWithPet())
                RemoveAuraBySlot(x);
}

void AuraInterface::RemoveAllAurasByInterruptFlag(uint32 flag)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) == m_auras.end())
            continue;
        //some spells do not get removed all the time only at specific intervals
        if(m_auras.at(x)->GetSpellProto()->AuraInterruptFlags & flag)
            RemoveAuraBySlot(x);
    }
}

void AuraInterface::RemoveAllAurasWithAuraName(uint32 auraName)
{
    for(uint8 i = 0; i < TOTAL_AURAS; i++)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            for(uint32 x = 0; x < 3; x++)
            {
                if( m_auras.at(i)->GetSpellProto()->EffectApplyAuraName[x] == auraName )
                {
                    RemoveAuraBySlot(i);
                    break;
                }
            }
        }
    }
}

void AuraInterface::RemoveAllAurasWithSpEffect(uint32 EffectId)
{
    for(uint8 i = 0; i < TOTAL_AURAS; i++)
    {
        if(m_auras.find(i) != m_auras.end())
        {
            for(uint32 x = 0; x < 3; x++)
            {
                if( m_auras.at(i)->GetSpellProto()->Effect[x] == EffectId )
                {
                    RemoveAuraBySlot(i);
                    break;
                }
            }
        }
    }
}

bool AuraInterface::RemoveAllPosAurasByNameHash(uint32 namehash)
{
    bool res = false;
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS;x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

bool AuraInterface::RemoveAllNegAurasByNameHash(uint32 namehash)
{
    bool res = false;
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

bool AuraInterface::RemoveAllAuras(uint32 spellId, WoWGuid guid)
{
    bool res = false;
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellId && (!guid || m_auras.at(x)->GetCasterGUID() == guid) )
            {
                RemoveAuraBySlot(x);
                res = true;
            }
        }
    }
    return res;
}

uint32 AuraInterface::GetAuraCountWithFamilyNameAndSkillLine(uint32 spellFamily, uint32 SkillLine)
{
    uint32 count = 0;
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if (m_auras.at(x)->GetSpellProto()->SpellFamilyName == spellFamily)
            {
                SkillLineAbilityEntry *sk = objmgr.GetSpellSkill(m_auras.at(x)->GetSpellId());
                if(sk && sk->skilline == SkillLine)
                {
                    count++;
                }
            }
        }
    }
    return count;
}

/* bool Unit::RemoveAllAurasByMechanic (renamed from MechanicImmunityMassDispel)
- Removes all auras on this unit that are of a specific mechanic.
- Useful for things like.. Apply Aura: Immune Mechanic, where existing (de)buffs are *always supposed* to be removed.
- I'm not sure if this goes here under unit.
* Arguments:
    - uint32 MechanicType
        *

* Returns;
    - False if no buffs were dispelled, true if more than 0 were dispelled.
*/
bool AuraInterface::RemoveAllAurasByMechanic( uint32 MechanicType, int32 MaxDispel, bool HostileOnly )
{
    //sLog.outString( "Unit::MechanicImmunityMassDispel called, mechanic: %u" , MechanicType );
    uint32 DispelCount = 0;
    for(uint8 x = ( HostileOnly ? MAX_POSITIVE_AURAS : 0 ) ; x < MAX_AURAS ; x++ ) // If HostileOnly = 1, then we use aura slots 40-86 (hostile). Otherwise, we use 0-86 (all)
    {
        if(MaxDispel > 0)
            if(DispelCount >= (uint32)MaxDispel)
                return true;

        if(m_auras.find(x) != m_auras.end())
        {
            if( Spell::HasMechanic(m_auras.at(x)->GetSpellProto(), MechanicType) ) // Remove all mechanics of type MechanicType (my english goen boom)
            {
                // TODO: Stop moving if fear was removed.
                RemoveAuraBySlot(x);
                DispelCount ++;
            }
            else if( MechanicType == MECHANIC_ENSNARED ) // if got immunity for slow, remove some that are not in the mechanics
            {
                for( int i = 0; i < 3; i++ )
                {
                    // SNARE + ROOT
                    if( m_auras.at(x)->GetSpellProto()->EffectApplyAuraName[i] == SPELL_AURA_MOD_DECREASE_SPEED || m_auras.at(x)->GetSpellProto()->EffectApplyAuraName[i] == SPELL_AURA_MOD_ROOT )
                    {
                        RemoveAuraBySlot(x);
                        break;
                    }
                }
            }
        }
    }
    return ( DispelCount != 0 );
}

Aura* AuraInterface::FindAuraBySlot(uint8 auraSlot)
{
    if(m_auras.find(auraSlot) == m_auras.end())
        return NULL;
    return m_auras.at(auraSlot);
}

Aura* AuraInterface::FindAura(uint32 spellId, WoWGuid guid)
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId() == spellId && (!guid || m_auras.at(x)->GetCasterGUID() == guid))
            {
                return m_auras.at(x);
            }
        }
    }
    return NULL;
}

Aura* AuraInterface::FindPositiveAuraByNameHash(uint32 namehash)
{
    for(uint8 x = 0; x < MAX_POSITIVE_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                return m_auras.at(x);
            }
        }
    }
    return NULL;
}

Aura* AuraInterface::FindNegativeAuraByNameHash(uint32 namehash)
{
    for(uint8 x = MAX_POSITIVE_AURAS; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash)
            {
                return m_auras.at(x);
            }
        }
    }
    return NULL;
}

Aura* AuraInterface::FindActiveAura(uint32 spellId, WoWGuid guid)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellId()==spellId && (!guid || m_auras.at(x)->GetCasterGUID() == guid))
            {
                return m_auras.at(x);
            }
        }
    }
    return NULL;
}

Aura* AuraInterface::FindActiveAuraWithNameHash(uint32 namehash, WoWGuid guid)
{
    for(uint8 x = 0; x < MAX_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->NameHash == namehash && (!guid || m_auras.at(x)->GetCasterGUID() == guid))
            {
                return m_auras.at(x);
            }
        }
    }
    return NULL;
}

void AuraInterface::EventDeathAuraRemoval()
{
    for(uint8 x = 0; x < TOTAL_AURAS; x++)
    {
        if(m_auras.find(x) != m_auras.end())
        {
            if(m_auras.at(x)->GetSpellProto()->isDeathPersistentAura())
                continue;

            RemoveAuraBySlot(x);
        }
    }
}

void AuraInterface::UpdateModifier(uint32 auraSlot, uint8 index, Modifier *mod, bool apply)
{
    m_modifierMask.SetBit(mod->m_type);

    std::pair<uint32, uint32> mod_index = std::make_pair(auraSlot, index);
    if(apply)
    {
        ModifierHolder *modHolder = NULL;
        if(m_modifierHolders.find(auraSlot) != m_modifierHolders.end())
            modHolder = m_modifierHolders.at(auraSlot);
        else if(m_auras.find(auraSlot) != m_auras.end())
        {
            modHolder = new ModifierHolder(auraSlot,m_auras.at(auraSlot)->GetSpellProto());
            m_modifierHolders.insert(std::make_pair(auraSlot, modHolder));
        }
        if(modHolder == NULL || modHolder->mod[index] == mod)
            return;

        m_modifiersByModType[mod->m_type].insert(std::make_pair(mod_index, mod));
        modHolder->mod[index] = mod;
    }
    else if(m_modifierHolders.find(auraSlot) != m_modifierHolders.end())
    {
        m_modifiersByModType[mod->m_type].erase(mod_index);

        ModifierHolder *modHolder = m_modifierHolders.at(auraSlot);
        modHolder->mod[index] = NULL;
        for(uint8 i=0;i<3;i++)
            if(modHolder->mod[i])
                return;
        m_modifierHolders.erase(auraSlot);
        delete modHolder;
    }

    if(mod->m_type == SPELL_AURA_ADD_FLAT_MODIFIER || mod->m_type == SPELL_AURA_ADD_PCT_MODIFIER)
        UpdateSpellGroupModifiers(apply, mod);
}

void AuraInterface::UpdateSpellGroupModifiers(bool apply, Modifier *mod)
{
    assert(mod->m_miscValue[0] < SPELL_MODIFIERS);
    std::pair<uint8, uint8> index = std::make_pair(uint8(mod->m_miscValue[0] & 0x7F), uint8(mod->m_type == SPELL_AURA_ADD_PCT_MODIFIER ? 1 : 0));
    std::map<uint8, int32> groupModMap = m_spellGroupModifiers[index];

    uint32 count = 0;
    WorldPacket *data = NULL;
    if(m_Unit->IsPlayer())
    {
        data = new WorldPacket(SMSG_SET_FLAT_SPELL_MODIFIER+index.second, 20);
        *data << uint32(1) << count << uint8(index.first);
    }
    for(uint32 bit = 0, intbit = 0; bit < SPELL_GROUPS; ++bit, ++intbit)
    {
        if(bit && (bit%32 == 0)) ++intbit;
        if( ( 1 << bit%32 ) & mod->m_spellInfo->EffectSpellClassMask[mod->i][intbit] )
        {
            if(apply) groupModMap[bit] += mod->m_amount;
            else groupModMap[bit] -= mod->m_amount;
            if(data) *data << uint8(bit) << groupModMap[bit];
            count++;
        }
    }

    if(data)
    {
        data->put<uint32>(4, count);
        castPtr<Player>(m_Unit)->SendPacket(data);
        delete data;
    }
}

uint32 AuraInterface::get32BitOffsetAndGroup(uint32 value, uint8 &group)
{
    group = uint8(float2int32(floor(float(value)/32.f)));
    return value%32;
}

void AuraInterface::SM_FIValue( uint32 modifier, int32* v, uint32* group )
{
    assert(modifier < SPELL_MODIFIERS);
    std::pair<uint8, uint8> index = std::make_pair(modifier, 0);
    std::map<uint8, int32> groupModMap = m_spellGroupModifiers[index];
    if(groupModMap.size() == 0)
        return;

    uint32 flag;
    uint8 groupOffset;
    for(std::map<uint8, int32>::iterator itr = groupModMap.begin(); itr != groupModMap.end(); itr++)
    {
        flag = (uint32(1)<<get32BitOffsetAndGroup(itr->second, groupOffset));
        if(flag & group[groupOffset]) (*v) += itr->second;
    }
}

void AuraInterface::SM_FFValue( uint32 modifier, float* v, uint32* group )
{
    assert(modifier < SPELL_MODIFIERS);
    std::pair<uint8, uint8> index = std::make_pair(modifier, 0);
    std::map<uint8, int32> groupModMap = m_spellGroupModifiers[index];
    if(groupModMap.size() == 0)
        return;

    uint32 flag;
    uint8 groupOffset;
    for(std::map<uint8, int32>::iterator itr = groupModMap.begin(); itr != groupModMap.end(); itr++)
    {
        flag = (uint32(1)<<get32BitOffsetAndGroup(itr->second, groupOffset));
        if(flag & group[groupOffset]) (*v) += itr->second;
    }
}

void AuraInterface::SM_PIValue( uint32 modifier, int32* v, uint32* group )
{
    assert(modifier < SPELL_MODIFIERS);
    std::pair<uint8, uint8> index = std::make_pair(modifier, 1);
    std::map<uint8, int32> groupModMap = m_spellGroupModifiers[index];
    if(groupModMap.size() == 0)
        return;

    uint32 flag;
    uint8 groupOffset;
    for(std::map<uint8, int32>::iterator itr = groupModMap.begin(); itr != groupModMap.end(); itr++)
    {
        flag = (uint32(1)<<get32BitOffsetAndGroup(itr->second, groupOffset));
        if(flag & group[groupOffset]) (*v) += float2int32(floor((float(*v)*float(itr->second))/100.f));
    }
}

void AuraInterface::SM_PFValue( uint32 modifier, float* v, uint32* group )
{
    assert(modifier < SPELL_MODIFIERS);
    std::pair<uint8, uint8> index = std::make_pair(modifier, 1);
    std::map<uint8, int32> groupModMap = m_spellGroupModifiers[index];
    if(groupModMap.size() == 0)
        return;

    uint32 flag;
    uint8 groupOffset;
    for(std::map<uint8, int32>::iterator itr = groupModMap.begin(); itr != groupModMap.end(); itr++)
    {
        flag = (uint32(1)<<get32BitOffsetAndGroup(itr->second, groupOffset));
        if(flag & group[groupOffset]) (*v) += ((*v)*float(itr->second))/100.f;
    }
}
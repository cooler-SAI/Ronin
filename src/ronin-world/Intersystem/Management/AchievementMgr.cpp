
#include "stdafx.h"

initialiseSingleton( AchievementMgr );

AchievementMgr::AchievementMgr()
{

}

AchievementMgr::~AchievementMgr()
{

}

void AchievementMgr::ParseAchievements()
{
    // Process achievement criteria into maps
    for(uint32 i = 0; i < dbcAchievementCriteria.GetNumRows(); i++)
    {
        AchievementCriteriaEntry *entry = dbcAchievementCriteria.LookupRow(i);
        if(entry == NULL)
            continue;
        AchievementEntry *achievement = dbcAchievement.LookupEntry(entry->referredAchievement);
        if(achievement == NULL || achievement->flags & ACHIEVEMENT_FLAG_GUILD)
            continue;
        m_criteriaByType.insert(std::make_pair(entry->requiredType, entry));
        m_criteriaByAchievement.insert(std::make_pair(entry->referredAchievement, entry));
    }

    // Process achievements into maps and set data
    for(uint32 i = 0; i < dbcAchievement.GetNumRows(); i++)
    {
        AchievementEntry *achievement = dbcAchievement.LookupRow(i);
        if(achievement == NULL)
            continue;

        // Process achievement reference lists
        if(achievement->refAchievement == 0)
        {
            if(achievement->Previous_achievement)
            {
                AchievementEntry *entry = dbcAchievement.LookupEntry(achievement->Previous_achievement);
                while(entry->Previous_achievement && entry->Previous_achievement != entry->ID)
                    entry = dbcAchievement.LookupEntry(entry->Previous_achievement);
                m_achievementsByReferrence.insert(std::make_pair(entry->ID, achievement));
            } else m_achievementsByReferrence.insert(std::make_pair(achievement->ID, achievement));
        } else if(AchievementEntry *ref = dbcAchievement.LookupEntry(achievement->refAchievement))
            m_achievementsByReferrence.insert(std::make_pair(ref->ID, achievement));
    }
}

void AchievementMgr::AllocatePlayerData(WoWGuid guid)
{
    if(m_playerAchieveData.find(guid) != m_playerAchieveData.end())
        return;
    m_playerAchieveData.insert(std::make_pair(guid, new AchieveDataContainer(guid)));
}

void AchievementMgr::CleanupPlayerData(WoWGuid guid)
{
    if(m_playerAchieveData.find(guid) == m_playerAchieveData.end())
        return;
    AchieveDataContainer *container = m_playerAchieveData.at(guid);
    m_playerAchieveData.erase(guid);
    delete container;
}

void AchievementMgr::PlayerFinishedLoading(WoWGuid guid)
{
    if(m_playerAchieveData.find(guid) == m_playerAchieveData.end())
        return;
    m_playerAchieveData.at(guid)->_loading = false;
}

void AchievementMgr::LoadAchievementData(WoWGuid guid, PlayerInfo *info, QueryResult *result)
{
    uint32 loadedPoints = 0;
    if(result)
    {
        AchieveDataContainer *container = m_playerAchieveData.at(guid);
        do
        {
            Field *fields = result->Fetch();
            uint32 achievement = fields[1].GetUInt32();
            AchievementEntry *entry = dbcAchievement.LookupEntry(achievement);
            if(entry == NULL)
                continue;

            time_t finishTime = fields[2].GetUInt64();
            container->m_completedAchievements.insert(std::make_pair(achievement, finishTime));
            loadedPoints += entry->points;
        }while(result->NextRow());
    }

    if(loadedPoints != info->achievementPoints)
        info->achievementPoints = loadedPoints;
}

void AchievementMgr::LoadCriteriaData(WoWGuid guid, QueryResult *result)
{
    if(result == NULL)
        return;

    AchieveDataContainer *container = m_playerAchieveData.at(guid);
    do
    {
        Field *fields = result->Fetch();
        uint32 criteria = fields[1].GetUInt32();
        AchievementCriteriaEntry *entry = dbcAchievementCriteria.LookupEntry(criteria);
        if(entry == NULL)
            continue;

        CriteriaData *data = new CriteriaData();
        data->criteriaCounter = fields[2].GetUInt64();
        data->timerData[0] = fields[3].GetUInt64();
        data->timerData[1] = fields[4].GetUInt64();
        container->m_criteriaProgress.insert(std::make_pair(criteria, data));
    }while(result->NextRow());
}

void AchievementMgr::SaveAchievementData(WoWGuid guid, QueryBuffer *buff)
{
    // Clear out the table
    if(buff) buff->AddQuery("DELETE FROM character_achievements WHERE guid = %u;", guid.getLow());
    else CharacterDatabase.Execute("DELETE FROM character_achievements WHERE guid = %u;", guid.getLow());

    // Append anything we need to save
    AchieveDataContainer *container = m_playerAchieveData.at(guid);
    if(container == NULL)
        return;

    std::stringstream ss;
    for(auto it = container->m_completedAchievements.begin(); it != container->m_completedAchievements.end(); it++)
    {
        if(ss.str().length())
            ss << ", ";

        ss << "(" << guid.getLow()
        << ", " << uint32(it->first)
        << ", " << uint64(it->second);
        ss << ")";
    }

    if(ss.str().length())
    {
        if(buff) buff->AddQuery("REPLACE INTO character_achievements VALUES %s;", ss.str().c_str());
        else CharacterDatabase.Execute("REPLACE INTO character_achievements VALUES %s;", ss.str().c_str());
    }
}

void AchievementMgr::SaveCriteriaData(WoWGuid guid, QueryBuffer *buff)
{
    // Clear out the table
    if(buff) buff->AddQuery("DELETE FROM character_criteria_data WHERE guid = %u;", guid.getLow());
    else CharacterDatabase.Execute("DELETE FROM character_criteria_data WHERE guid = %u;", guid.getLow());

    // Append anything we need to save
    AchieveDataContainer *container = m_playerAchieveData.at(guid);
    if(container == NULL)
        return;

    std::stringstream ss;
    for(auto it = container->m_criteriaProgress.begin(); it != container->m_criteriaProgress.end(); it++)
    {
        if(ss.str().length())
            ss << ", ";

        ss << "(" << guid.getLow()
        << ", " << uint32(it->first)
        << ", " << uint64(it->second->criteriaCounter)
        << ", " << uint64(it->second->timerData[0])
        << ", " << uint64(it->second->timerData[1]);
        ss << ")";
    }

    if(ss.str().length())
    {
        if(buff) buff->AddQuery("REPLACE INTO character_criteria_data VALUES %s;", ss.str().c_str());
        else CharacterDatabase.Execute("REPLACE INTO character_criteria_data VALUES %s;", ss.str().c_str());
    }
}

void AchievementMgr::BuildAchievementData(WoWGuid guid, WorldPacket *data)
{
    AchieveDataContainer *container = m_playerAchieveData.at(guid);
    ByteBuffer criteriaData;
    data->WriteBits(container->m_criteriaProgress.size(), 21);
    for(auto it = container->m_criteriaProgress.begin(); it != container->m_criteriaProgress.end(); it++)
    {
        WoWGuid counter(it->second->criteriaCounter);
        // Append our bit data
        data->WriteBit(guid[4]);
        data->WriteBit(counter[3]);
        data->WriteBit(guid[5]);
        data->WriteGuidBitString(2, counter, 0, 6);
        data->WriteGuidBitString(2, guid, 3, 0);
        data->WriteBit(counter[4]);
        data->WriteBit(guid[2]);
        data->WriteBit(counter[7]);
        data->WriteBit(guid[7]);
        data->WriteBits(it->second->flag, 2);
        data->WriteBit(guid[6]);
        data->WriteGuidBitString(3, counter, 2, 1, 5);
        data->WriteBit(guid[1]);

        // Append byte data
        criteriaData.WriteByteSeq(guid[3]);
        criteriaData.WriteSeqByteString(2, counter, 5, 6);
        criteriaData.WriteSeqByteString(2, guid, 4, 6);
        criteriaData.WriteByteSeq(counter[2]);

        criteriaData << uint32(UNIXTIME - it->second->timerData[1]); // Timer 2
        criteriaData.WriteByteSeq(guid[2]);
        criteriaData << uint32(it->first);

        criteriaData.WriteByteSeq(guid[5]);
        criteriaData.WriteSeqByteString(4, counter, 0, 3, 1, 4);
        criteriaData.WriteSeqByteString(2, guid, 0, 7);
        criteriaData.WriteByteSeq(counter[7]);

        criteriaData << uint32(UNIXTIME - it->second->timerData[0]); // Timer 1
        criteriaData << RONIN_UTIL::secsToTimeBitFields(UNIXTIME);

        criteriaData.WriteByteSeq(guid[1]);
    }

    data->WriteBits(container->m_completedAchievements.size(), 23);
    data->FlushBits();
    // Append our criteria bytes
    data->append(criteriaData.contents(), criteriaData.size());
    // Append achievements earned and completion dates
    for(auto it = container->m_completedAchievements.begin(); it != container->m_completedAchievements.end(); it++)
    {
        *data << uint32(it->first);
        *data << RONIN_UTIL::secsToTimeBitFields(it->second);
    }
}

bool AchievementMgr::IsValidAchievement(Player *plr, AchievementEntry *entry)
{
    if(entry->factionFlag != ACHIEVEMENT_FACTION_FLAG_ANY)
    {
        if(plr->GetTeam() == HORDE && entry->factionFlag == ACHIEVEMENT_FACTION_FLAG_ALLIANCE)
            return false;
        if(plr->GetTeam() == ALLIANCE && entry->factionFlag == ACHIEVEMENT_FACTION_FLAG_HORDE)
            return false;
    }

    switch(entry->ID)
    {
        // Class world firsts
    case 458: if(plr->getClass() != ROGUE) return false; break;
    case 459: if(plr->getClass() != WARRIOR) return false; break;
    case 460: if(plr->getClass() != MAGE) return false; break;
    case 461: if(plr->getClass() != DEATHKNIGHT) return false; break;
    case 462: if(plr->getClass() != HUNTER) return false; break;
    case 463: if(plr->getClass() != WARLOCK) return false; break;
    case 464: if(plr->getClass() != PRIEST) return false; break;
    case 465: if(plr->getClass() != PALADIN) return false; break;
    case 466: if(plr->getClass() != DRUID) return false; break;
    case 467: if(plr->getClass() != SHAMAN) return false; break;
        // Race world firsts
    case 1404: if(plr->getRace() != RACE_GNOME) return false; break;
    case 1405: if(plr->getRace() != RACE_BLOODELF) return false; break;
    case 1406: if(plr->getRace() != RACE_DRAENEI) return false; break;
    case 1407: if(plr->getRace() != RACE_DWARF) return false; break;
    case 1408: if(plr->getRace() != RACE_HUMAN) return false; break;
    case 1410: if(plr->getRace() != RACE_ORC) return false; break;
    case 1409: if(plr->getRace() != RACE_NIGHTELF) return false; break;
    case 1411: if(plr->getRace() != RACE_TAUREN) return false; break;
    case 1412: if(plr->getRace() != RACE_TROLL) return false; break;
    case 1413: if(plr->getRace() != RACE_UNDEAD) return false; break;
    }

    return true;
}

void AchievementMgr::UpdateCriteriaValue(Player *plr, uint32 criteriaType, uint32 mod, uint32 misc1, uint32 misc2)
{
    if(m_playerAchieveData.find(plr->GetGUID()) == m_playerAchieveData.end())
        return;
    AchieveDataContainer *container = m_playerAchieveData.at(plr->GetGUID());
    if(container->_loading)
        return;
    CriteriaStorageBounds cbounds = m_criteriaByType.equal_range(criteriaType);
    if(cbounds.first == cbounds.second)
        return;

    std::vector<std::pair<uint64, AchievementCriteriaEntry*>> processedCriteria;
    for(CriteriaStorage::iterator itr = cbounds.first; itr != cbounds.second; itr++)
    {
        AchievementCriteriaEntry *criteria = itr->second;
        if(criteria == NULL || !_ValidateCriteriaRequirements(plr, criteria, misc1, misc2))
            continue;

        // Update criteria value here
        if(container->m_criteriaProgress.find(criteria->ID) == container->m_criteriaProgress.end())
            container->m_criteriaProgress.insert(std::make_pair(criteria->ID, new CriteriaData()));
        CriteriaData *data = container->m_criteriaProgress.at(criteria->ID);
        // store previous criteria value
        uint64 previous = data->criteriaCounter;
        data->criteriaCounter = mod; // TODO: Process wether to add or set criteria
        data->timerData[1] = UNIXTIME;
        // Achievement handling
        if(criteria->referredAchievement == 0)
            continue;
        // Push the criteria into the processed batch before we update the counter
        processedCriteria.push_back(std::make_pair(previous, criteria));
    }

    for(std::vector<std::pair<uint64, AchievementCriteriaEntry*>>::iterator itr = processedCriteria.begin(); itr != processedCriteria.end(); itr++)
    {
        uint64 previous = (*itr).first;
        AchievementCriteriaEntry *criteria = (*itr).second;
        // Get our list of achievements along the reference line and process them
        AchievementStorageBounds abounds = m_achievementsByReferrence.equal_range(criteria->referredAchievement);
        if(abounds.first == abounds.second)
            continue;

        CriteriaData *data = container->m_criteriaProgress.at(criteria->ID);
        // The referrenced achievement is always at the front(or should be?) and in the storage
        for(AchievementStorage::iterator itr2 = abounds.first; itr2 != abounds.second; itr2++)
        {
            AchievementEntry *entry = itr2->second;
            if(entry == NULL || !IsValidAchievement(plr, entry) || entry->flags & ACHIEVEMENT_FLAG_COUNTER)
                continue;
            if(data->criteriaCounter >= previous)
            {
                if(!_FinishedCriteria(container, criteria, entry))
                    continue;
                if(_CheckAchievementRequirements(container, entry))
                    EarnAchievement(plr, entry->ID);
            } else if(!_CheckAchievementRequirements(container, entry))
                RemoveAchievement(plr, entry->ID);
        }
    }
}

void AchievementMgr::EarnAchievement(Player *plr, uint32 achievementId)
{
    if(m_playerAchieveData.find(plr->GetGUID()) == m_playerAchieveData.end())
        return;
    AchieveDataContainer *container = m_playerAchieveData.at(plr->GetGUID());
    if(container->_loading)
        return;
    AchievementEntry *entry = dbcAchievement.LookupEntry(achievementId);
    if(entry == NULL || !IsValidAchievement(plr, entry))
        return;
    std::map<uint32, time_t> *achievements = &container->m_completedAchievements;
    if(achievements->find(achievementId) != achievements->end())
        return;
    achievements->insert(std::make_pair(achievementId, time_t(UNIXTIME)));
    // Increment cached achievement points
    plr->m_playerInfo->achievementPoints += entry->points;
    if(!plr->IsInWorld())
        return;

    // Send our achievement earned packet
    WorldPacket data(SMSG_ACHIEVEMENT_EARNED, 8 + 4 + 4 + 4);
    data << plr->GetGUID().asPacked();
    data << uint32(achievementId);
    data << RONIN_UTIL::secsToTimeBitFields(UNIXTIME);
    data << uint32(0);
    plr->SendMessageToSet(&data, true, true, 50.f);
}

void AchievementMgr::RemoveAchievement(Player *plr, uint32 achievementId)
{
    if(m_playerAchieveData.find(plr->GetGUID()) == m_playerAchieveData.end())
        return;
    AchievementEntry *entry = dbcAchievement.LookupEntry(achievementId);
    AchieveDataContainer *container = m_playerAchieveData.at(plr->GetGUID());
    if(container->_loading || entry == NULL)
        return;
    std::map<uint32, time_t> *achievements = &container->m_completedAchievements;
    if(achievements->find(achievementId) == achievements->end())
        return;

    achievements->erase(achievementId);
    // Remove cached achievement points
    plr->m_playerInfo->achievementPoints -= entry->points;
    if(!plr->IsInWorld())
        return;
    plr->GetSession()->OutPacket(SMSG_ACHIEVEMENT_DELETED, 4, &achievementId);
}

bool AchievementMgr::_ValidateCriteriaRequirements(Player *plr, AchievementCriteriaEntry *entry, uint32 misc1, uint32 misc2)
{

    return true;
}

bool AchievementMgr::_FinishedCriteria(AchieveDataContainer *container, AchievementCriteriaEntry *criteria, AchievementEntry *achievement)
{
    // Can't complete achievements that don't exist
    if(achievement == NULL || achievement->flags & ACHIEVEMENT_FLAG_COUNTER)
        return false;
    // Check realm first status
    if (achievement->flags & (ACHIEVEMENT_FLAG_REALM_FIRST_REACH | ACHIEVEMENT_FLAG_REALM_FIRST_KILL))
        if (m_realmFirstCompleted.find(achievement->ID) != m_realmFirstCompleted.end()) // See if we've already completed it
            return false;
    if(container->m_criteriaProgress.find(criteria->ID) == container->m_criteriaProgress.end())
        return false;
    uint64 criteriaCounter = container->m_criteriaProgress.at(criteria->ID)->criteriaCounter;
    return criteriaCounter >= criteria->getMaxCounter() || (achievement->flags & ACHIEVEMENT_FLAG_REQ_COUNT && criteriaCounter);
}

bool AchievementMgr::_CheckAchievementRequirements(AchieveDataContainer *container, AchievementEntry *achievement)
{
    // for achievement with referenced achievement criterias get from referenced and counter from self
    CriteriaStorageBounds bounds = m_criteriaByAchievement.equal_range(achievement->refAchievement ? achievement->refAchievement : achievement->ID);
    if(bounds.first == bounds.second)
        return false;

    uint64 count = 0;
    // For SUMM achievements, we have to count the progress of each criteria of the achievement.
    // Oddly, the target count is NOT countained in the achievement, but in each individual criteria
    if (achievement->flags & ACHIEVEMENT_FLAG_SUMM)
    {
        std::map<uint32, CriteriaData*> *criteriaMap = &container->m_criteriaProgress;
        for (CriteriaStorage::iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            AchievementCriteriaEntry *criteria = itr->second;
            if(criteriaMap->find(criteria->ID) == criteriaMap->end())
                continue;

            count += criteriaMap->at(criteria->ID)->criteriaCounter;
            // for counters, field4 contains the main count requirement
            if (count >= criteria->raw.count)
                return true;
        }
        return false;
    }

    // Default case - need complete all or
    bool completed_all = true;
    for (CriteriaStorage::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        AchievementCriteriaEntry *criteria = itr->second;
        if (!_FinishedCriteria(container, criteria, achievement))
            completed_all = false;
        else ++count;

        // completed as have req. count of completed criterias
        if (achievement->count > 0 && achievement->count <= count)
            return true;
    }

    // all criterias completed requirement
    if (completed_all && achievement->count == 0)
        return true;

    return false;
}
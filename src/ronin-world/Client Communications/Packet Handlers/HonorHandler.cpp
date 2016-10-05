/***
 * Demonstrike Core
 */

#include "StdAfx.h"

void WorldSession::HandleSetVisibleRankOpcode(WorldPacket& recv_data)
{
    uint32 ChosenRank;
    recv_data >> ChosenRank;
    if( ChosenRank == 0xFFFFFFFF )
        _player->SetUInt32Value( PLAYER_CHOSEN_TITLE, 0 );
    else if( _player->HasKnownTitleByIndex(ChosenRank) )
        _player->SetUInt32Value( PLAYER_CHOSEN_TITLE, ChosenRank );
}

void HonorHandler::AddHonorPointsToPlayer(Player* pPlayer, uint32 uAmount)
{
    if(pPlayer->m_bg && pPlayer->m_bg->IsArena())
        return;

    RecalculateHonorFields(pPlayer);
}

int32 HonorHandler::CalculateHonorPoints(uint32 AttackerLevel, uint32 VictimLevel)
{
    uint32 k_grey = 0;

    if(AttackerLevel > 5 && AttackerLevel < 40)
        k_grey = AttackerLevel - 5 - float2int32(std::floor(((float)AttackerLevel) / 10.0f));
    else
        k_grey = AttackerLevel - 1 - float2int32(std::floor(((float)AttackerLevel) / 5.0f));

    if(VictimLevel <= k_grey)
        return 0;

    float honor_points = sWorld.getRate(RATE_HONOR)*(-0.53177f + (0.59357f * exp((AttackerLevel + 23.54042f) / 26.07859f)));
    return float2int32(floor(honor_points));
}

void HonorHandler::OnPlayerKilled( Player* pPlayer, Player* pVictim )
{
    if( pVictim->HasAura(PLAYER_HONORLESS_TARGET_SPELL) )
        return;

    if( pPlayer->m_bg )
    {
        if( castPtr<Player>( pVictim )->GetBGTeam() == pPlayer->GetBGTeam() )
            return;

        // patch 2.4, players killed >50 times in battlegrounds won't be worth honor for the rest of that bg
        if( castPtr<Player>(pVictim)->m_bgScore.Deaths >= 50 )
            return;
    }
    else
    {
        if( pPlayer->GetTeam() == castPtr<Player>( pVictim )->GetTeam() )
            return;
    }

    // Calculate points
    int32 Points = 0;
    if(pPlayer != pVictim)
        Points = CalculateHonorPoints(pPlayer->getLevel(), pVictim->getLevel());

    if( Points > 0 )
    {
        if( pPlayer->m_bg )
        {
            // hackfix for battlegrounds (since the groups there are disabled, we need to do this manually)
            std::vector<Player*  > toadd;
            uint8 t = pPlayer->GetBGTeam();
            toadd.reserve(15);      // shouldnt have more than this
            pPlayer->m_bg->Lock();
            std::set<Player*  > * s = &pPlayer->m_bg->m_players[t];

            for(std::set<Player*  >::iterator itr = s->begin(); itr != s->end(); itr++)
                if((*itr) == pPlayer || (*itr)->isInRange(pPlayer, 40.0f))
                    toadd.push_back(*itr);

            if( toadd.size() > 0 )
            {
                uint32 pts = Points / (uint32)toadd.size();
                for(std::vector<Player*  >::iterator vtr = toadd.begin(); vtr != toadd.end(); ++vtr)
                {
                    AddHonorPointsToPlayer(*vtr, pts);
                    pPlayer->m_bg->HookOnHK(*vtr);
                    TRIGGER_INSTANCE_EVENT( pPlayer->GetMapInstance(), OnPlayerHonorKill )( pPlayer );
                    if(pVictim)
                    {
                        // Send PVP credit
                        WorldPacket data(SMSG_PVP_CREDIT, 12);
                        data << pts << pVictim->GetGUID() << uint32(castPtr<Player>(pVictim)->GetPVPRank());
                        (*vtr)->GetSession()->SendPacket(&data);
                    }
                }
            }

            pPlayer->m_bg->Unlock();
        }
        else if(pPlayer->GetGroup())
        {
            Group *pGroup = pPlayer->GetGroup();
            Player* gPlayer = NULL;
            int32 GroupPoints;
            pGroup->Lock();
            GroupPoints = (Points / (pGroup->MemberCount() ? pGroup->MemberCount() : 1));
            GroupMembersSet::iterator gitr;
            for(uint32 k = 0; k < pGroup->GetSubGroupCount(); k++)
            {
                for(gitr = pGroup->GetSubGroup(k)->GetGroupMembersBegin(); gitr != pGroup->GetSubGroup(k)->GetGroupMembersEnd(); ++gitr)
                {
                    gPlayer = (*gitr)->m_loggedInPlayer;

                    if(gPlayer && (gPlayer == pPlayer || gPlayer->isInRange(pPlayer,100.0f))) // visible range
                    {
                        if(gPlayer->m_bg)
                            gPlayer->m_bg->HookOnHK(gPlayer);

                        TRIGGER_INSTANCE_EVENT( pPlayer->GetMapInstance(), OnPlayerHonorKill )( pPlayer );
                        AddHonorPointsToPlayer(gPlayer, GroupPoints);
                        if(pVictim)
                        {
                            // Send PVP credit
                            WorldPacket data(SMSG_PVP_CREDIT, 12);
                            data << GroupPoints << pVictim->GetGUID() << uint32(castPtr<Player>(pVictim)->GetPVPRank());
                            gPlayer->GetSession()->SendPacket(&data);
                        }

                        //patch by emsy
                        // If we are in Halaa
                        if(gPlayer->GetZoneId() == 3518)
                        {
                            // Add Halaa Battle Token
                            SpellEntry * pvp_token_spell = dbcSpell.LookupEntry(gPlayer->GetTeam()? 33004 : 33005);
                            gPlayer->CastSpell(gPlayer, pvp_token_spell, true);
                        } // If we are in Hellfire Peninsula
                        else if(gPlayer->GetZoneId() == 3483)
                        {
                            // Add Mark of Thrallmar/Honor Hold
                            SpellEntry * pvp_token_spell = dbcSpell.LookupEntry(gPlayer->GetTeam()? 32158 : 32155);
                            gPlayer->CastSpell(gPlayer, pvp_token_spell, true);
                        }
                    }

                }
            }
            pGroup->Unlock();
        }
        else
        {
            AddHonorPointsToPlayer(pPlayer, Points);

            if(pPlayer->m_bg)
                pPlayer->m_bg->HookOnHK(pPlayer);
            TRIGGER_INSTANCE_EVENT( pPlayer->GetMapInstance(), OnPlayerHonorKill )( pPlayer );
            if(pVictim)
            {
                // Send PVP credit
                WorldPacket data(SMSG_PVP_CREDIT, 12);
                data << Points << pVictim->GetGUID() << uint32(castPtr<Player>(pVictim)->GetPVPRank());
                pPlayer->GetSession()->SendPacket(&data);
            }

            //patch by emsy
            // If we are in Halaa
            if(pPlayer->GetZoneId() == 3518)
            {
                // Add Halaa Battle Token
                SpellEntry * halaa_spell = dbcSpell.LookupEntry(pPlayer->GetTeam()? 33004 : 33005);
                pPlayer->CastSpell(pPlayer, halaa_spell, true);
            } // If we are in Hellfire Peninsula
            else if(pPlayer->GetZoneId() == 3483)
            {
                // Add Mark of Thrallmar/Honor Hold
                SpellEntry * pvp_token_spell = dbcSpell.LookupEntry(pPlayer->GetTeam() ? 32158 : 32155);
                pPlayer->CastSpell(pPlayer, pvp_token_spell, true);
            }
        }
    }
}

void HonorHandler::RecalculateHonorFields(Player* pPlayer)
{
    // Currency tab - (Blizz Placeholders)

}

bool ChatHandler::HandleAddKillCommand(const char* args, WorldSession* m_session)
{
    uint32 KillAmount = args ? atol(args) : 1;
    Player* plr = getSelectedChar(m_session, true);
    if(plr == 0)
        return true;

    BlueSystemMessage(m_session, "Adding %u kills to player %s.", KillAmount, plr->GetName());
    GreenSystemMessage(plr->GetSession(), "You have had %u honor kills added to your character.", KillAmount);

    plr->RecalculateHonor();
    return true;
}

bool ChatHandler::HandleAddHonorCommand(const char* args, WorldSession* m_session)
{
    uint32 HonorAmount = args ? atol(args) : 1;
    Player* plr = getSelectedChar(m_session, true);
    if(plr == 0)
        return true;

    BlueSystemMessage(m_session, "Adding %u honor to player %s.", HonorAmount, plr->GetName());
    GreenSystemMessage(plr->GetSession(), "You have had %u honor points added to your character.", HonorAmount);

    HonorHandler::AddHonorPointsToPlayer(plr, HonorAmount);
    return true;
}

bool ChatHandler::HandlePVPCreditCommand(const char* args, WorldSession* m_session)
{
    uint32 Rank, Points;
    if(sscanf(args, "%u %u", &Rank, &Points) != 2)
    {
        RedSystemMessage(m_session, "Command must be in format <rank> <points>.");
        return true;
    }
    Points *= 10;
    uint64 Guid = m_session->GetPlayer()->GetSelection();
    if(Guid == 0)
    {
        RedSystemMessage(m_session, "A selection of a unit or player is required.");
        return true;
    }

    BlueSystemMessage(m_session, "Building packet with Rank %u, Points %u, GUID %llu.", Rank, Points, Guid);

    WorldPacket data(SMSG_PVP_CREDIT, 12);
    data << Points << Guid << Rank;
    m_session->SendPacket(&data);
    return true;
}

bool ChatHandler::HandleGlobalHonorDailyMaintenanceCommand(const char* args, WorldSession* m_session)
{
    return false;
}

bool ChatHandler::HandleNextDayCommand(const char* args, WorldSession* m_session)
{
    return false;
}

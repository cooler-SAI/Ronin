/***
 * Demonstrike Core
 */

#include "StdAfx.h"

#define FACTION_FLAG_HIDDEN 4
#define FACTION_FLAG_INVISIBLE_FORCED 8
#define FACTION_FLAG_PEACE_FORCED 16
#define FACTION_FLAG_AT_WAR  2
#define FACTION_FLAG_VISIBLE 1

Standing Player::GetReputationRankFromStanding(int32 Standing_)
{
    if( Standing_ >= 42000 )
        return STANDING_EXALTED;
    else if( Standing_ >= 21000 )
        return STANDING_REVERED;
    else if( Standing_ >= 9000 )
        return STANDING_HONORED;
    else if( Standing_ >= 3000 )
        return STANDING_FRIENDLY;
    else if( Standing_ >= 0 )
        return STANDING_NEUTRAL;
    else if( Standing_ > -3000 )
        return STANDING_UNFRIENDLY;
    else if( Standing_ > -6000 )
        return STANDING_HOSTILE;
    return STANDING_HATED;
}

RONIN_INLINE void SetFlagAtWar(uint8 & flag)
{
    if(flag & FACTION_FLAG_AT_WAR)
        return;

    flag |= FACTION_FLAG_AT_WAR;
}

RONIN_INLINE void UnsetFlagAtWar(uint8 & flag)
{
    if(!(flag & FACTION_FLAG_AT_WAR))
        return;

    flag &= ~FACTION_FLAG_AT_WAR;
}

RONIN_INLINE void SetFlagVisible(uint8 & flag)
{
    if(flag & FACTION_FLAG_VISIBLE)
        return;

    flag |= FACTION_FLAG_VISIBLE;
}

RONIN_INLINE void SetFlagPeaceForced(uint8 & flag)
{
    if(flag & FACTION_FLAG_PEACE_FORCED)
        return;

    flag |= FACTION_FLAG_PEACE_FORCED;
}

RONIN_INLINE void SetForcedInvisible(uint8 & flag)
{
    if(flag & FACTION_FLAG_INVISIBLE_FORCED)
        return;

    flag |= FACTION_FLAG_INVISIBLE_FORCED;
}

RONIN_INLINE void UnsetFlagVisible(uint8 & flag)
{
    if(!(flag & FACTION_FLAG_VISIBLE))
        return;

    flag &= ~FACTION_FLAG_VISIBLE;
}

RONIN_INLINE bool AtWar(uint8 flag) { return (flag & FACTION_FLAG_AT_WAR) ? true : false; }
RONIN_INLINE bool Visible(uint8 flag) { return (flag & FACTION_FLAG_VISIBLE) ? true : false; }

RONIN_INLINE bool RankChanged(int32 Standing, int32 Change)
{
    if(Player::GetReputationRankFromStanding(Standing) != Player::GetReputationRankFromStanding(Standing + Change))
        return true;
    else
        return false;
}

RONIN_INLINE bool RankChangedFlat(int32 Standing, int32 NewStanding)
{
    if(Player::GetReputationRankFromStanding(Standing) != Player::GetReputationRankFromStanding(NewStanding))
        return true;
    else
        return false;
}

void Player::smsg_InitialFactions()
{
    WorldPacket data( SMSG_INITIALIZE_FACTIONS, 644 );
    data << uint32( 128 );
    FactionReputation * rep;
    for ( uint32 i = 0; i < 128; ++i )
    {
        rep = reputationByListId[i];
        if ( rep == NULL )
            data << uint8(0) << uint32(0);
        else data << rep->flag << rep->CalcStanding();
    }
    m_session->SendPacket(&data);
}

uint32 Player::GetInitialFactionId()
{
    PlayerCreateInfo * pci = objmgr.GetPlayerCreateInfo(getRace(), getClass());
    if( pci )
        return pci->factiontemplate;
    else return 35;
}

void Player::_InitialReputation()
{
    FactionEntry * f;
    for ( uint32 i = 0; i < dbcFaction.GetNumRows(); i++ )
    {
        f = dbcFaction.LookupRow( i );
        AddNewFaction( f, 0, true );
    }
}

int32 Player::GetStanding(uint32 Faction)
{
    ReputationMap::iterator itr = m_reputation.find(Faction);
    if(itr == m_reputation.end()) return 0;
    else return itr->second->standing;
}

int32 Player::GetBaseStanding(uint32 Faction)
{
    ReputationMap::iterator itr = m_reputation.find(Faction);
    if(itr == m_reputation.end()) return 0;
    else return itr->second->baseStanding;
}

void Player::SetStanding(uint32 Faction, int32 Value)
{
    ReputationMap::iterator itr = m_reputation.find(Faction);
    FactionEntry *faction = dbcFaction.LookupEntry(Faction);
    if(faction == NULL) return;

    if(itr == m_reputation.end())
    {
        if( !AddNewFaction( faction, Value, false ) )
            return;
        itr = m_reputation.find( Faction );
        OnModStanding( faction, itr->second );
    }
    else
    {
        // Assign it.
        if ( RankChangedFlat( itr->second->standing, Value ) )
        {
            itr->second->standing = Value;
        } else itr->second->standing = Value;
        
        OnModStanding( faction, itr->second );
    }
}

Standing Player::GetStandingRank(uint32 Faction)
{
    return Standing(GetReputationRankFromStanding(GetStanding(Faction)));
}

bool Player::IsHostileBasedOnReputation(FactionEntry *faction)
{
    if( faction->RepListId < 0 || faction->RepListId >= 128 )
        return false;

    FactionReputation * rep = reputationByListId[faction->RepListId];
    if ( rep == NULL )
        return false;

    // forced reactions take precedence
    std::map<uint32, uint32>::iterator itr = m_forcedReactions.find( faction->ID );
    if( itr != m_forcedReactions.end() )
        return ( itr->second <= STANDING_HOSTILE );

    return GetReputationRankFromStanding( rep->standing ) <= STANDING_HOSTILE;
}

void Player::ModStanding(uint32 Faction, int32 Value)
{
    ReputationMap::iterator itr = m_reputation.find(Faction);
    FactionEntry* faction = dbcFaction.LookupEntry(Faction);
    if (faction == NULL || faction->RepListId < 0)
        return;

    if(itr == m_reputation.end())
    {
        if (AddNewFaction(faction, 0, true)) 
            return;
        itr = m_reputation.find( Faction );
        OnModStanding( faction, itr->second );
    }
    else
    {
        int32 newValue = itr->second->standing + Value;
        itr->second->standing = newValue < -42000 ? -42000 : newValue > 42999 ? 42999 : newValue;
        OnModStanding(faction, itr->second);
    }
}

void Player::SetAtWar(uint32 Faction, bool Set)
{
    if( Faction >= 128 )
        return;

    FactionReputation * rep = reputationByListId[Faction];
    if(!rep)
        return;

    if(GetReputationRankFromStanding(rep->standing) <= STANDING_HOSTILE && !Set) // At this point we have to be at war.
        return;

    if(rep->flag & 0x4 || rep->flag & 16 )
        return;

    if(Set && !AtWar(rep->flag))
        SetFlagAtWar(rep->flag);
    else if(!Set && AtWar(rep->flag))
        UnsetFlagAtWar(rep->flag);
}

bool Player::IsAtWar(uint32 factionId)
{
    if( factionId >= 128 )
        return false;
    FactionReputation *rep = reputationByListId[factionId];
    if(rep == NULL)
        return false;
    return AtWar(rep->flag);
}

void WorldSession::HandleSetAtWarOpcode(WorldPacket& recv_data)
{
    uint32 id;
    uint8 state;
    recv_data >> id >> state;

    /*uint32 faction_id = (id >= 128) ? 0 : INITIALIZE_FACTIONS[id];
    if(faction_id == 0) return;

    if(state & FACTION_FLAG_AT_WAR)
        _player->SetAtWar(faction_id, true);
    else
        _player->SetAtWar(faction_id, false);*/

    if(state == 1)
        _player->SetAtWar(id, true);
    else _player->SetAtWar(id, false);
}

void Player::Reputation_OnKilledUnit(Unit* pUnit, bool InnerLoop)
{
    // add rep for on kill
    if ( pUnit->GetTypeId() != TYPEID_UNIT || pUnit->IsPet() || pUnit->GetFaction() == NULL )
        return;

    Group * m_Group = GetGroup();

    // Why would this be accessed if the group didn't exist?
    if ( !InnerLoop && m_Group != NULL )
    {
        /* loop the rep for group members */
        m_Group->getLock().Acquire();
        GroupMembersSet::iterator it;
        for ( uint32 i = 0; i < m_Group->GetSubGroupCount(); i++ )
        {
            for ( it = m_Group->GetSubGroup(i)->GetGroupMembersBegin(); it != m_Group->GetSubGroup(i)->GetGroupMembersEnd(); ++it )
            {
                if ( (*it)->m_loggedInPlayer && (*it)->m_loggedInPlayer->isInRange( this, 100.0f ) )
                    (*it)->m_loggedInPlayer->Reputation_OnKilledUnit( pUnit, true );
            }
        }
        m_Group->getLock().Release();
        return;
    }

    uint32 team = GetTeam();
    ReputationModifier * modifier = objmgr.GetReputationModifier( pUnit->GetEntry(), pUnit->GetFactionID() );
    if( modifier != 0 )
    {
        // Apply this data.
        for( std::vector<ReputationMod>::iterator itr = modifier->mods.begin(); itr != modifier->mods.end(); itr++ )
        {
            if ( !(*itr).faction[team] )
                continue;

            /* rep limit? */
            if ( !IS_INSTANCE( GetMapId() ) || ( IS_INSTANCE( GetMapId() ) && iInstanceType != MODE_5PLAYER_HEROIC) )
            {
                if ( (*itr).replimit )
                {
                    if ( GetStanding( (*itr).faction[team] ) >= (int32)(*itr).replimit )
                        continue;
                }
            }
            ModStanding( itr->faction[team], float2int32( itr->value * sWorld.getRate( RATE_KILLREPUTATION ) ) );
        }
    }
    else
    {
        if ( IS_INSTANCE( GetMapId() ) && objmgr.HandleInstanceReputationModifiers( this, pUnit ) )
            return;

        FactionEntry *faction = pUnit->GetFaction();
        if ( faction == NULL || faction->RepListId < 0 )
            return;

        int32 change = int32( -5.0f * sWorld.getRate( RATE_KILLREPUTATION ) );
        ModStanding( faction->ID, change );
    }
}

void Player::Reputation_OnTalk(FactionEntry *faction)
{
    // set faction visible if not visible
    if(!faction || faction->RepListId < 0)
        return;

    FactionReputation * rep = reputationByListId[faction->RepListId];
    if(!rep)
        return;

    if(!Visible(rep->flag))
    {
        SetFlagVisible(rep->flag);
        if(IsInWorld())
            m_session->OutPacket(SMSG_SET_FACTION_VISIBLE, 4, &faction->RepListId);
    }
}

bool Player::AddNewFaction( FactionEntry *faction, int32 standing, bool base )
{
    if ( faction == NULL || faction->RepListId < 0 )
        return false;

    uint32 RaceMask = getRaceMask();
    uint32 ClassMask = getClassMask();
    for ( uint32 i = 0; i < 4; i++ )
    {
        if( ( faction->RaceMask[i] & RaceMask || ( faction->RaceMask[i] == 0 && faction->ClassMask[i] != 0 ) ) &&
            ( faction->ClassMask[i] & ClassMask || faction->ClassMask[i] == 0 ) )
        {
            FactionReputation * rep = new FactionReputation;
            rep->flag = uint8(faction->repFlags[i]);
            rep->baseStanding = faction->baseRepValue[i];
            rep->standing = ( base ) ? faction->baseRepValue[i] : standing;
            m_reputation[faction->ID] = rep;
            reputationByListId[faction->RepListId] = rep;
            return true;
        }
    }
    return false;
}

void Player::OnModStanding( FactionEntry *faction, FactionReputation * rep )
{
    if(!Visible(rep->flag))
    {
        SetFlagVisible(rep->flag);
        if(IsInWorld())
            m_session->OutPacket(SMSG_SET_FACTION_VISIBLE, 4, &faction->RepListId);
    }

    if(GetReputationRankFromStanding(rep->standing) <= STANDING_HOSTILE && !AtWar(rep->flag))
        SetFlagAtWar(rep->flag);

    if ( Visible( rep->flag ) && IsInWorld() )
    {
        WorldPacket data( SMSG_SET_FACTION_STANDING, 17 );
        data << uint32( 0 ) ;
        data << uint8( 1 ) ; //count 
        data << uint32( rep->flag ) << faction->RepListId << rep->CalcStanding();
        m_session->SendPacket( &data );
    }
}
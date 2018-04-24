/*
 * Originally written by Xinef - Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: http://github.com/azerothcore/azerothcore-wotlk/LICENSE-AGPL
*/

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "naxxramas.h"
#include "Player.h"

enum Says
{
    SAY_AGGRO                   = 0,
    SAY_SLAY                    = 1,
    SAY_TAUNT                   = 2,
    SAY_DEATH                   = 3
};

enum Spells
{
    SPELL_SPELL_DISRUPTION          = 29310,
    SPELL_DECREPIT_FEVER_10         = 29998,
    SPELL_DECREPIT_FEVER_25         = 55011,
    SPELL_PLAGUE_CLOUD              = 29350,
};

enum Events
{
    EVENT_SPELL_SPELL_DISRUPTION    = 1,
    EVENT_SPELL_DECEPIT_FEVER       = 2,
    EVENT_ERUPT_SECTION             = 3,
    EVENT_SWITCH_PHASE              = 4,
    EVENT_SAFETY_DANCE              = 5,
};

enum Misc
{
    PHASE_SLOW_DANCE                = 0,
    PHASE_FAST_DANCE                = 1,
};

class boss_heigan : public CreatureScript
{
public:
    boss_heigan() : CreatureScript("boss_heigan") { }

    CreatureAI* GetAI(Creature* pCreature) const
    {
        return new boss_heiganAI (pCreature);
    }

    struct boss_heiganAI : public ScriptedAI
    {
        boss_heiganAI(Creature *c) : ScriptedAI(c)
        {
            pInstance = me->GetInstanceScript();
        }

        InstanceScript* pInstance;
        EventMap events;
        uint8 currentPhase;
        uint8 currentSection;
        bool moveRight;

        void Reset()
        {
            events.Reset();
            currentPhase = 0;
            currentSection = 3;
            moveRight = true;

            if (pInstance)
            {
                pInstance->SetData(EVENT_HEIGAN, NOT_STARTED);
                if (GameObject* go = me->GetMap()->GetGameObject(pInstance->GetData64(DATA_HEIGAN_ENTER_GATE)))
                    go->SetGoState(GO_STATE_ACTIVE);
            }
        }

        void KilledUnit(Unit* who)
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (!urand(0,3))
                Talk(SAY_SLAY);

            if (pInstance)
                pInstance->SetData(DATA_IMMORTAL_FAIL, 0);
        }

        void JustDied(Unit*  /*Killer*/)
        {
            Talk(SAY_DEATH);
            if (pInstance)
                pInstance->SetData(EVENT_HEIGAN, DONE);
        }

        void EnterCombat(Unit * /*who*/)
        {
            me->SetInCombatWithZone();
            Talk(SAY_AGGRO);
            if (pInstance)
            {
                pInstance->SetData(EVENT_HEIGAN, IN_PROGRESS);
                if (GameObject* go = me->GetMap()->GetGameObject(pInstance->GetData64(DATA_HEIGAN_ENTER_GATE)))
                    go->SetGoState(GO_STATE_READY);
            }

            StartFightPhase(PHASE_SLOW_DANCE);
        }

        void StartFightPhase(uint8 phase)
        {
            currentSection = 3;
            currentPhase = phase;
            events.Reset();
            if (phase == PHASE_SLOW_DANCE)
            {
                events.ScheduleEvent(EVENT_SPELL_SPELL_DISRUPTION, 0);
                events.ScheduleEvent(EVENT_SPELL_DECEPIT_FEVER, 12000);
                events.ScheduleEvent(EVENT_ERUPT_SECTION, 10000);
                events.ScheduleEvent(EVENT_SWITCH_PHASE, 90000);
            }
            else // if (phase == PHASE_FAST_DANCE)
            {
                me->MonsterTextEmote("%s teleports and begins to channel a spell!", 0, true);
                // teleport
                float x, y, z, o;
                me->GetHomePosition(x, y, z, o);
                me->NearTeleportTo(x, y, z, o);

                me->CastSpell(me, SPELL_PLAGUE_CLOUD, false);
                events.ScheduleEvent(EVENT_ERUPT_SECTION, 4000);
                events.ScheduleEvent(EVENT_SWITCH_PHASE, 45000);
            }
            events.ScheduleEvent(EVENT_SAFETY_DANCE, 5000);
        }

        bool IsInRoom(Unit* who)
        {
            if (who->GetPositionX() > 2826 || who->GetPositionX() < 2723 || who->GetPositionY() > -3641 || who->GetPositionY() < -3736)
            {
                if (who->GetGUID() == me->GetGUID())
                    EnterEvadeMode();
                return false;
            }

            return true;
        }

        void UpdateAI(uint32 diff)
        {
            if (!IsInRoom(me))
                return;

            if (!UpdateVictim())
                return;

            events.Update(diff);
            //if (me->HasUnitState(UNIT_STATE_CASTING))
            //  return;

            switch (events.GetEvent())
            {
                case EVENT_SPELL_SPELL_DISRUPTION:
                    me->CastSpell(me, SPELL_SPELL_DISRUPTION, false);
                    events.RepeatEvent(10000);
                    break;
                case EVENT_SPELL_DECEPIT_FEVER:
                    me->CastSpell(me, RAID_MODE(SPELL_DECREPIT_FEVER_10, SPELL_DECREPIT_FEVER_25), false);
                    events.RepeatEvent(20000);
                    break;
                case EVENT_SWITCH_PHASE:
                    StartFightPhase(currentPhase == PHASE_SLOW_DANCE ? PHASE_FAST_DANCE : PHASE_SLOW_DANCE);
                    // no pop, there is reset in start fight
                    break;
                case EVENT_ERUPT_SECTION:
                    if (pInstance)
                    {
                        pInstance->SetData(DATA_HEIGAN_ERUPTION, currentSection);
                        if (currentSection == 3)
                            moveRight = false;
                        else if (currentSection == 0)
                            moveRight = true;

                        moveRight ? currentSection++ : currentSection--;
                    }

                    if (currentPhase == PHASE_SLOW_DANCE && !urand(0,3))
                        Talk(SAY_TAUNT);

                    events.RepeatEvent(currentPhase == PHASE_SLOW_DANCE ? 10000 : 4000);
                    break;
                case EVENT_SAFETY_DANCE:
                {
                    Map::PlayerList const& pList = me->GetMap()->GetPlayers();
                    for(Map::PlayerList::const_iterator itr = pList.begin(); itr != pList.end(); ++itr)
                    {
                        if (IsInRoom(itr->GetSource()) && !itr->GetSource()->IsAlive())
                        {
                            events.PopEvent();
                            pInstance->SetData(DATA_DANCE_FAIL, 0);
                            pInstance->SetData(DATA_IMMORTAL_FAIL, 0);
                            return;
                        }

                    }
                    events.RepeatEvent(5000);
                    return;
                }
            }

            DoMeleeAttackIfReady();
        }
    };
};

void AddSC_boss_heigan()
{
    new boss_heigan();
}

#include "AnimalPlayerAbilities.hpp"

#include "game/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/frontend/GUI.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

#include <cmath>

namespace YimMenu::Features
{
	namespace
	{
		constexpr float kBirdFlySpeed = 0.35f;

		bool g_ScriptRunning = false;

		void DisableAnimalFlee(int pedHandle)
		{
			static constexpr int fleeParams[] = {104, 105, 10, 146, 113, 114, 115, 116, 117, 118, 119, 111, 107};
			for (const auto param : fleeParams)
				FLOCK::SET_ANIMAL_TUNING_FLOAT_PARAM(pedHandle, param, 0.0f);
		}

		void ConfigureCombatAnimal(int pedHandle)
		{
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(pedHandle, false);
			PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 46, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 5, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(pedHandle, 1, true);
			PED::SET_PED_COMBAT_ABILITY(pedHandle, 2);
			PED::SET_PED_COMBAT_MOVEMENT(pedHandle, 2);
			PED::SET_PED_COMBAT_RANGE(pedHandle, 2);
			PED::SET_PED_FLEE_ATTRIBUTES(pedHandle, 0, false);
			PED::SET_PED_CAN_RAGDOLL(pedHandle, true);
		}

		bool IsValidAttackTarget(int pedHandle, int selfHandle)
		{
			if (!pedHandle || pedHandle == selfHandle)
				return false;

			if (!ENTITY::DOES_ENTITY_EXIST(pedHandle) || !ENTITY::IS_ENTITY_A_PED(pedHandle))
				return false;

			if (PED::IS_PED_DEAD_OR_DYING(pedHandle, true))
				return false;

			return true;
		}

		int GetAimedPedTarget(int playerId, int selfHandle)
		{
			int target = 0;
			if (PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(playerId, &target) && IsValidAttackTarget(target, selfHandle))
				return target;

			if (PLAYER::GET_PLAYER_TARGET_ENTITY(playerId, &target) && IsValidAttackTarget(target, selfHandle))
				return target;

			return 0;
		}

		void TickAnimalAttack(Ped ped)
		{
			const int pedHandle  = ped.GetHandle();
			const int playerId   = Self::GetPlayer().GetId();
			const int target     = GetAimedPedTarget(playerId, pedHandle);

			if (!target)
				return;

			const bool wantsAttack = PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK)
			    || PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MELEE_ATTACK)
			    || PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK2);

			if (!wantsAttack)
				return;

			if (PED::IS_PED_IN_COMBAT(pedHandle, target))
				return;

			ped.ForceControl();
			ConfigureCombatAnimal(pedHandle);

			if (ENTITY::_GET_IS_BIRD(pedHandle))
			{
				TASK::TASK_COMBAT_PED(pedHandle, target, 0, 16);
				return;
			}

			TASK::TASK_COMBAT_ANIMAL_CHARGE_PED(pedHandle, target, true, 0, 0, 0, 0);
		}

		void TickBirdFlight(Ped ped)
		{
			if (!ENTITY::_GET_IS_BIRD(ped.GetHandle()))
				return;

			if (GUI::IsOpen())
				return;

			const int pedHandle = ped.GetHandle();
			ped.ForceControl();

			PED::SET_PED_GRAVITY(pedHandle, false);
			ENTITY::SET_ENTITY_HAS_GRAVITY(pedHandle, false);

			rage::fvector3 vel{};

			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_SPRINT))
				vel.z += kBirdFlySpeed;
			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_DUCK))
				vel.z -= kBirdFlySpeed;
			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_UP_ONLY))
				vel.y += kBirdFlySpeed;
			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_DOWN_ONLY))
				vel.y -= kBirdFlySpeed;
			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_LEFT_ONLY))
				vel.x -= kBirdFlySpeed;
			if (PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MOVE_RIGHT_ONLY))
				vel.x += kBirdFlySpeed;

			if (vel.x == 0.f && vel.y == 0.f && vel.z == 0.f)
				return;

			const auto location = ped.GetPosition();
			const auto offset   = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(
			    pedHandle,
			    vel.x,
			    vel.y,
			    vel.z);

			ped.SetVelocity({});
			ped.SetPosition({offset.x, offset.y, offset.z});

			const auto rot = CAM::GET_GAMEPLAY_CAM_ROT(2);
			ped.SetRotation({0.0f, rot.y, rot.z});
		}

		void RestoreGravityIfNeeded(Ped ped)
		{
			const int pedHandle = ped.GetHandle();
			if (ENTITY::_GET_IS_BIRD(pedHandle))
				return;

			PED::SET_PED_GRAVITY(pedHandle, true);
			ENTITY::SET_ENTITY_HAS_GRAVITY(pedHandle, true);
		}

		void TickAnimalAbilities()
		{
			auto ped = Self::GetPed();
			if (!ped || !ped.IsValid() || !ped.IsAnimal())
			{
				if (ped && ped.IsValid())
					RestoreGravityIfNeeded(ped);
				return;
			}

			ConfigureCombatAnimal(ped.GetHandle());
			TickBirdFlight(ped);
			TickAnimalAttack(ped);
		}

		void StartAnimalAbilitiesScript()
		{
			if (g_ScriptRunning)
				return;

			g_ScriptRunning = true;
			ScriptMgr::AddScript(std::make_unique<Script>([] {
				while (true)
				{
					TickAnimalAbilities();
					ScriptMgr::Yield(0ms);
				}
			}));
		}
	}

	void AnimalPlayerAbilities::Init()
	{
		StartAnimalAbilitiesScript();
	}

	void AnimalPlayerAbilities::ConfigurePlayerAsAnimal(Ped ped)
	{
		if (!ped || !ped.IsValid() || !ped.IsAnimal())
			return;

		const int pedHandle = ped.GetHandle();
		ped.ForceControl();

		FLOCK::_SET_ANIMAL_IS_WILD(pedHandle, false);
		DisableAnimalFlee(pedHandle);
		ConfigureCombatAnimal(pedHandle);
		PED::SET_PED_KEEP_TASK(pedHandle, true);

		if (ENTITY::_GET_IS_BIRD(pedHandle))
		{
			PED::SET_PED_GRAVITY(pedHandle, false);
			ENTITY::SET_ENTITY_HAS_GRAVITY(pedHandle, false);
		}
	}
}

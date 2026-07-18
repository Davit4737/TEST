#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu::Features
{
	namespace
	{
		bool IsValidAttackTarget(Entity target, Entity self)
		{
			if (!target || target == self)
				return false;

			if (!ENTITY::DOES_ENTITY_EXIST(target) || !ENTITY::IS_ENTITY_A_PED(target))
				return false;

			if (PED::IS_PED_DEAD_OR_DYING(target, true))
				return false;

			return true;
		}

		Entity GetAimedTarget(int playerId, Entity self)
		{
			Entity target = 0;
			if (PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(playerId, &target) && IsValidAttackTarget(target, self))
				return target;

			if (PLAYER::GET_PLAYER_TARGET_ENTITY(playerId, &target) && IsValidAttackTarget(target, self))
				return target;

			return 0;
		}
	}

	// When you SET_PLAYER_MODEL to an animal, INPUT_MELEE_ATTACK does nothing no
	// matter what flags you clear: the player's melee task is built around the
	// human moveset/skeleton, and animal peds don't have that moveset to play. The
	// only thing that actually makes an animal ped attack is forcing the game's own
	// animal combat task onto whatever you're aiming/locked onto.
	class AnimalAttack : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		static void ConfigureCombatAnimal(Ped handle)
		{
			PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(handle, false);
			PED::SET_PED_COMBAT_ATTRIBUTES(handle, 46, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(handle, 5, true);
			PED::SET_PED_COMBAT_ATTRIBUTES(handle, 1, true);
			PED::SET_PED_COMBAT_ABILITY(handle, 2);
			PED::SET_PED_COMBAT_MOVEMENT(handle, 2);
			PED::SET_PED_COMBAT_RANGE(handle, 2);
			PED::SET_PED_FLEE_ATTRIBUTES(handle, 0, false);
			PED::SET_PED_CAN_RAGDOLL(handle, true);
		}

		virtual void OnTick() override
		{
			auto ped = Self::GetPed();

			if (!ped)
				return;

			const bool isBird = PED::_GET_IS_BIRD(ped.GetHandle());

			// only touch animal/bird models, leave Arthur/John alone
			if (!ped.IsAnimal() && !isBird)
				return;

			const auto handle   = ped.GetHandle();
			const auto playerId = Self::GetPlayer().GetId();

			const auto target = GetAimedTarget(playerId, handle);
			if (!target)
				return;

			const bool wantsAttack = PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK)
			    || PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_MELEE_ATTACK)
			    || PAD::IS_CONTROL_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK2);

			if (!wantsAttack)
				return;

			if (PED::IS_PED_IN_COMBAT(handle, target))
				return;

			ped.ForceControl();
			ConfigureCombatAnimal(handle);

			if (isBird)
				TASK::TASK_COMBAT_PED(handle, target, 0, 16);
			else
				TASK::TASK_COMBAT_ANIMAL_CHARGE_PED(handle, target, true, 0, 0, 0, 0);
		}
	};

	static AnimalAttack _AnimalAttack{"animalattack", "Animal Attack", "Makes your animal or bird player model attack whatever you're aiming at"};
}

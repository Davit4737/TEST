#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

namespace YimMenu::Features
{
	namespace
	{
		// these are raw game handles (ints), not the YimMenu::Entity wrapper -- the
		// natives read/write plain int handles and the wrapper deletes operator int
		bool IsValidAttackTarget(int target, int self)
		{
			if (!target || target == self)
				return false;

			if (!ENTITY::DOES_ENTITY_EXIST(target) || !ENTITY::IS_ENTITY_A_PED(target))
				return false;

			if (PED::IS_PED_DEAD_OR_DYING(target, true))
				return false;

			return true;
		}

		int GetAttackTarget(int playerId, int self, const rage::fvector3& pos)
		{
			int target = 0;

			// if we somehow do have a lock-on / free-aim target, prefer it
			if (PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(playerId, &target) && IsValidAttackTarget(target, self))
				return target;

			if (PLAYER::GET_PLAYER_TARGET_ENTITY(playerId, &target) && IsValidAttackTarget(target, self))
				return target;

			// animals have no weapon and no aim reticle, so the two calls above almost
			// always come back empty -- fall back to grabbing the nearest ped around us
			int closest = 0;
			if (PED::GET_CLOSEST_PED(pos.x, pos.y, pos.z, 25.0f, true, true, &closest, false, false, false, -1) && IsValidAttackTarget(closest, self))
				return closest;

			return 0;
		}
	}

	// When you SET_PLAYER_MODEL to an animal, INPUT_MELEE_ATTACK does nothing no
	// matter what flags you clear: the player's melee task is built around the
	// human moveset/skeleton, and animal peds don't have that moveset to play. The
	// only thing that actually makes an animal ped attack is forcing the game's own
	// animal combat task onto a target. And since animals can't aim or lock on, we
	// pick that target ourselves -- the nearest ped -- when you press attack.
	class AnimalAttack : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		static void ConfigureCombatAnimal(int handle)
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

			const bool isBird = ENTITY::_GET_IS_BIRD(ped.GetHandle());

			// only touch animal/bird models, leave Arthur/John alone
			if (!ped.IsAnimal() && !isBird)
				return;

			const int handle   = ped.GetHandle();
			const int playerId = Self::GetPlayer().GetId();

			// only act on a fresh attack-button press, so we don't re-issue the task
			// every single frame while the button is held
			const bool wantsAttack = PAD::IS_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK)
			    || PAD::IS_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_MELEE_ATTACK)
			    || PAD::IS_CONTROL_JUST_PRESSED(0, (Hash)NativeInputs::INPUT_ATTACK2);

			if (!wantsAttack)
				return;

			const auto pos    = ped.GetPosition();
			const int  target = GetAttackTarget(playerId, handle, pos);
			if (!target)
				return;

			ped.ForceControl();
			ConfigureCombatAnimal(handle);

			if (isBird)
				TASK::TASK_COMBAT_PED(handle, target, 0, 16);
			else
				TASK::TASK_COMBAT_ANIMAL_CHARGE_PED(handle, target, true, 0, 0, 0, 0);
		}
	};

	static AnimalAttack _AnimalAttack{"animalattack", "Animal Attack", "As an animal/bird player model, press attack to charge the nearest ped"};
}

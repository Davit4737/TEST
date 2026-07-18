#include "core/commands/FloatCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Enums.hpp"
#include "game/rdr/Natives.hpp"

#include <cmath>

namespace YimMenu::Features
{
	static constexpr NativeInputs flightControls[] = {
	    NativeInputs::INPUT_SPRINT,
	    NativeInputs::INPUT_MOVE_UP_ONLY,
	    NativeInputs::INPUT_MOVE_DOWN_ONLY,
	    NativeInputs::INPUT_MOVE_LEFT_ONLY,
	    NativeInputs::INPUT_MOVE_RIGHT_ONLY,
	    NativeInputs::INPUT_DUCK,
	    NativeInputs::INPUT_JUMP,
	};

	static FloatCommand _BirdFlightSpeed{"birdflightspeed", "Flight Speed", "Features", 1.0f, 40.0f, 12.0f};

	// This is NOT the game's flight task -- RDR2 has no player-drivable one for bird
	// peds. A plain SET_ENTITY_VELOCITY does not stick on the local player ped: its
	// own locomotion task recomputes velocity every tick and overwrites it (see the
	// abandoned SetVelocity branch in Noclip.cpp). So like Noclip, we step the ped's
	// position directly each tick instead of trying to push it with velocity.
	class BirdFlight : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		Ped m_Ped{nullptr};
		bool m_Applied = false;

		void RestorePed(Ped ped)
		{
			if (!ped)
				return;

			ENTITY::SET_ENTITY_HAS_GRAVITY(ped.GetHandle(), true);
			PED::SET_PED_GRAVITY(ped.GetHandle(), true);
			PED::SET_PED_CAN_RAGDOLL(ped.GetHandle(), true);
		}

		virtual void OnTick() override
		{
			auto ped = Self::GetPed();

			if (!ped)
				return;

			// animal models, plus birds specifically in case they aren't classed as animals --
			// flying Arthur looks like a bug, not a feature
			if (!ped.IsAnimal() && !PED::_GET_IS_BIRD(ped.GetHandle()))
				return;

			// clean up if we swapped ped since last tick
			if (m_Ped != ped)
			{
				RestorePed(m_Ped);
				m_Ped     = ped;
				m_Applied = false;
			}

			const auto handle = ped.GetHandle();

			if (!m_Applied)
			{
				ENTITY::SET_ENTITY_HAS_GRAVITY(handle, false);
				PED::SET_PED_GRAVITY(handle, false);
				PED::SET_PED_CAN_RAGDOLL(handle, false);
				m_Applied = true;
			}

			for (const auto& control : flightControls)
				PAD::DISABLE_CONTROL_ACTION(0, static_cast<int>(control), true);

			rage::fvector3 vel{};

			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_MOVE_UP_ONLY))
				vel.y += 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_MOVE_DOWN_ONLY))
				vel.y -= 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_MOVE_LEFT_ONLY))
				vel.x -= 1.0f;
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_MOVE_RIGHT_ONLY))
				vel.x += 1.0f;
			// Space -- climb
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_JUMP))
				vel.z += 1.0f;
			// Ctrl -- dive
			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_DUCK))
				vel.z -= 1.0f;

			// point the bird where the camera is looking so it banks into turns
			const auto camRot = CAM::GET_GAMEPLAY_CAM_ROT(2);
			ped.SetRotation({camRot.x * 0.5f, 0.0f, camRot.z});

			// kill any velocity the ped's own locomotion task tried to apply this tick
			ped.SetVelocity({});

			if (vel.x == 0.f && vel.y == 0.f && vel.z == 0.f)
				return; // hover in place

			float speed = _BirdFlightSpeed.GetState();

			if (PAD::IS_DISABLED_CONTROL_PRESSED(0, (int)NativeInputs::INPUT_SPRINT))
				speed *= 2.0f;

			const float length = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
			if (length > 0.0001f)
			{
				vel.x /= length;
				vel.y /= length;
				vel.z /= length;
			}

			// distance to cover this tick, scaled by real elapsed time so speed is framerate-independent
			const float step = speed * MISC::GET_FRAME_TIME();

			// GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS turns the local-space step into a world-space
			// target point; teleporting there each tick is what actually moves the ped (see comment above)
			const auto offset = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(handle, vel.x * step, vel.y * step, vel.z * step);
			ped.SetPosition({offset.x, offset.y, offset.z});
		}

		virtual void OnDisable() override
		{
			RestorePed(m_Ped ? m_Ped : Self::GetPed());
			m_Ped     = Ped(nullptr);
			m_Applied = false;
		}
	};

	static BirdFlight _BirdFlight{"birdflight", "Bird Flight", "Fly when your player model is a bird or other animal"};
}

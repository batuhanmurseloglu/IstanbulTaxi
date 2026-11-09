#include "Vehicle/TaxiWheels.h"

namespace
{
	void ApplyCommonTaxiWheel(UChaosVehicleWheel& W)
	{
		W.WheelRadius = 31.5f;            // 195/65 R15
		W.WheelWidth = 19.5f;
		W.WheelMass = 18.0f;
		W.FrictionForceMultiplier = 2.5f; // base grip; surface system scales this per wheel
		W.CorneringStiffness = 800.0f;    // a bit lazy -> forgiving, readable slides
		W.SideSlipModifier = 1.0f;
		W.SlipThreshold = 20.0f;
		W.SkidThreshold = 20.0f;

		// Soft "worn taxi" suspension
		W.SuspensionMaxRaise = 9.0f;
		W.SuspensionMaxDrop = 11.0f;
		W.SuspensionDampingRatio = 0.35f;
		W.SpringRate = 170.0f;
		W.SpringPreload = 60.0f;
		W.SuspensionSmoothing = 4;
		W.RollbarScaling = 0.15f;         // allow visible body roll

		W.MaxBrakeTorque = 1500.0f;
		W.bABSEnabled = true;
		W.bTractionControlEnabled = true;
	}
}

UTaxiWheelFront::UTaxiWheelFront()
{
	ApplyCommonTaxiWheel(*this);
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 36.0f;
	bAffectedByEngine = true;
	bAffectedByHandbrake = false;
	bAffectedByBrake = true;
}

UTaxiWheelRear::UTaxiWheelRear()
{
	ApplyCommonTaxiWheel(*this);
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	MaxSteerAngle = 0.0f;
	bAffectedByEngine = false;
	bAffectedByHandbrake = true;
	bAffectedByBrake = true;
	MaxBrakeTorque = 1000.0f;
	MaxHandBrakeTorque = 3000.0f;
}

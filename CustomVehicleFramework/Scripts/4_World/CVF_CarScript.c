modded class CarScript
{
	protected bool m_CVF_RuntimeChecked = false;
	protected bool m_CVF_IsManaged = false;
	protected string m_CVF_VehicleType = "";
	protected ref CVF_ResolvedVehicleConfig m_CVF_Config;

	override void EEInit()
	{
		super.EEInit();
		CVF_InitRuntimeConfig();
	}

	override void OnUpdate(float dt)
	{
		super.OnUpdate(dt);

		if (!m_CVF_RuntimeChecked)
			CVF_InitRuntimeConfig();

		if (!m_CVF_IsManaged)
			return;

		if (!g_Game || !g_Game.IsServer())
			return;

		CVF_ApplyRuntimeTuning(dt);
	}

	protected void CVF_InitRuntimeConfig()
	{
		if (m_CVF_RuntimeChecked)
			return;

		m_CVF_RuntimeChecked = true;

		if (!g_Game || !g_Game.IsServer())
			return;

		m_CVF_VehicleType = GetType();

		CVF_ResolvedVehicleConfig resolved;
		if (GetCVFConfigManager().GetResolvedConfigFor(m_CVF_VehicleType, resolved) && resolved.HasRuntimeOverrides())
		{
			m_CVF_Config = resolved;
			m_CVF_IsManaged = true;
			CVF_Logger.Log("Managing vehicle: " + m_CVF_VehicleType);
		}
	}

	protected void CVF_ApplyRuntimeTuning(float dt)
	{
		if (!m_CVF_Config)
			return;

		float throttle = GetThrottle();
		float steering = GetSteering();
		float brake = GetBrake();

		float tunedThrottle = Math.Clamp(throttle * m_CVF_Config.ThrottleMultiplier, 0.0, 1.0);
		float tunedSteering = Math.Clamp(steering * m_CVF_Config.SteeringMultiplier, -1.0, 1.0);
		float tunedBrake = Math.Clamp(brake * m_CVF_Config.BrakeMultiplier, 0.0, 1.0);

		if (tunedThrottle != throttle)
			SetThrottle(tunedThrottle);
		if (tunedSteering != steering)
			SetSteering(tunedSteering);
		if (tunedBrake != brake)
			SetBrake(tunedBrake);

		vector velocity = GetVelocity(this);
		float speedMs = velocity.Length();
		float speedKmh = speedMs * 3.6;

		vector transform[4];
		GetTransform(transform);
		vector forward = transform[2];
		vector up = transform[1];

		bool hasGroundContact = CVF_HasWheelContact();

		if (hasGroundContact && tunedThrottle > 0.01 && m_CVF_Config.ExtraDriveForce > 0.0)
		{
			if (m_CVF_Config.MaxSpeedKmh <= 0.0 || speedKmh < m_CVF_Config.MaxSpeedKmh)
			{
				vector driveForce = forward * (tunedThrottle * m_CVF_Config.ExtraDriveForce);
				dBodyApplyForce(this, driveForce);
			}
		}

		if (hasGroundContact && tunedBrake > 0.01 && m_CVF_Config.ExtraBrakeForce > 0.0 && speedMs > 0.1)
		{
			vector brakeForce = -velocity.Normalized() * (tunedBrake * m_CVF_Config.ExtraBrakeForce);
			dBodyApplyForce(this, brakeForce);
		}

		if (m_CVF_Config.MaxSpeedKmh > 0.0 && speedKmh > m_CVF_Config.MaxSpeedKmh && speedMs > 0.1)
		{
			float overspeed = speedKmh - m_CVF_Config.MaxSpeedKmh;
			vector limiterForce = -velocity.Normalized() * (overspeed * 60.0);
			dBodyApplyForce(this, limiterForce);
		}

		if (m_CVF_Config.DragResistance > 0.0 && speedMs > 0.1)
		{
			vector dragForce = -velocity.Normalized() * (speedMs * speedMs * m_CVF_Config.DragResistance);
			dBodyApplyForce(this, dragForce);
		}

		if (hasGroundContact && m_CVF_Config.SteeringYawAssist > 0.0 && Math.AbsFloat(tunedSteering) > 0.01 && speedKmh > 1.0)
		{
			float assistScale = Math.Clamp(1.0 - (speedKmh / 180.0), 0.15, 1.0);
			vector steeringTorque = up * (-tunedSteering * m_CVF_Config.SteeringYawAssist * assistScale);
			dBodyApplyTorque(this, steeringTorque);
		}

		if (m_CVF_Config.StabilityAssist > 0.0 && speedKmh > 1.0)
		{
			vector angularVelocity = dBodyGetAngularVelocity(this);
			vector dampingTorque = -angularVelocity * m_CVF_Config.StabilityAssist;
			dBodyApplyTorque(this, dampingTorque);
		}
	}

	protected bool CVF_HasWheelContact()
	{
		int wheelCount = WheelCount();
		if (wheelCount <= 0)
			return true;

		for (int i = 0; i < wheelCount; i++)
		{
			if (WheelHasContact(i))
				return true;
		}

		return false;
	}
}

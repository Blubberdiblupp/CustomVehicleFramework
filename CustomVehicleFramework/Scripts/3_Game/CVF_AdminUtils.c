class CVF_AdminUtils
{
	static void ReloadConfigForNewSpawns()
	{
		if (!g_Game || !g_Game.IsServer())
		{
			CVF_Logger.Error("ReloadConfigForNewSpawns can only be called on server.");
			return;
		}

		GetCVFConfigManager().ReloadConfigForNewSpawns();
	}

	static void PrintVehicleList()
	{
		CVF_ConfigManager configMgr = GetCVFConfigManager();
		configMgr.LoadConfig();

		CVF_Logger.Log("=== CVF Vehicle Overrides ===");
		for (int i = 0; i < configMgr.m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = configMgr.m_Config.VehicleOverrides.Get(i);
			if (!overrideData)
				continue;

			string status = "[GLOBAL]";
			if (overrideData.HasAnyOverrides())
				status = "[CUSTOM]";
			CVF_Logger.Log(status + " " + overrideData.ClassName);
		}
		CVF_Logger.Log("================================");
	}

	static void PrintVehicleDetails(string className)
	{
		CVF_ResolvedVehicleConfig resolved;
		if (!GetCVFConfigManager().GetResolvedConfigFor(className, resolved))
		{
			CVF_Logger.Error("Vehicle not found in CVF config: " + className);
			return;
		}

		CVF_Logger.Log("=== CVF Resolved: " + className + " ===");
		CVF_Logger.Log("MaxSpeedKmh=" + resolved.MaxSpeedKmh.ToString());
		CVF_Logger.Log("ThrottleMultiplier=" + resolved.ThrottleMultiplier.ToString());
		CVF_Logger.Log("ExtraDriveForce=" + resolved.ExtraDriveForce.ToString());
		CVF_Logger.Log("SteeringMultiplier=" + resolved.SteeringMultiplier.ToString());
		CVF_Logger.Log("SteeringYawAssist=" + resolved.SteeringYawAssist.ToString());
		CVF_Logger.Log("BrakeMultiplier=" + resolved.BrakeMultiplier.ToString());
		CVF_Logger.Log("ExtraBrakeForce=" + resolved.ExtraBrakeForce.ToString());
		CVF_Logger.Log("DragResistance=" + resolved.DragResistance.ToString());
		CVF_Logger.Log("StabilityAssist=" + resolved.StabilityAssist.ToString());
		CVF_Logger.Log("MaxSteeringAngle(native)=" + resolved.MaxSteeringAngle.ToString());
		CVF_Logger.Log("EngineRPMRedline(native)=" + resolved.EngineRPMRedline.ToString());
		CVF_Logger.Log("TorqueCurve count(native)=" + resolved.EngineTorqueCurve.Count().ToString());
		CVF_Logger.Log("GearboxType(native)=" + resolved.GearboxType);
		CVF_Logger.Log("GearboxReverse(native)=" + resolved.GearboxReverse.ToString());
		CVF_Logger.Log("GearboxRatios count(native)=" + resolved.GearboxRatios.Count().ToString());
		CVF_Logger.Log("================================");
	}

	static void PrintGlobalSettings()
	{
		CVF_ConfigManager configMgr = GetCVFConfigManager();
		configMgr.LoadConfig();

		CVF_GlobalSettings global = configMgr.m_Config.GlobalSettings;
		CVF_Logger.Log("=== CVF Global Settings ===");
		CVF_Logger.Log("MaxSpeedKmh=" + global.MaxSpeedKmh.ToString());
		CVF_Logger.Log("ThrottleMultiplier=" + global.ThrottleMultiplier.ToString());
		CVF_Logger.Log("ExtraDriveForce=" + global.ExtraDriveForce.ToString());
		CVF_Logger.Log("SteeringMultiplier=" + global.SteeringMultiplier.ToString());
		CVF_Logger.Log("SteeringYawAssist=" + global.SteeringYawAssist.ToString());
		CVF_Logger.Log("BrakeMultiplier=" + global.BrakeMultiplier.ToString());
		CVF_Logger.Log("ExtraBrakeForce=" + global.ExtraBrakeForce.ToString());
		CVF_Logger.Log("DragResistance=" + global.DragResistance.ToString());
		CVF_Logger.Log("StabilityAssist=" + global.StabilityAssist.ToString());
		CVF_Logger.Log("================================");
	}

	static void AddVehicleToOverrides(string className)
	{
		if (!g_Game || !g_Game.IsServer())
		{
			CVF_Logger.Error("Can only add vehicles on server.");
			return;
		}

		CVF_ConfigManager configMgr = GetCVFConfigManager();
		configMgr.LoadConfig();

		for (int i = 0; i < configMgr.m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride existing = configMgr.m_Config.VehicleOverrides.Get(i);
			if (existing && existing.ClassName == className)
			{
				CVF_Logger.Warning("Vehicle already exists: " + className);
				return;
			}
		}

		CVF_VehicleOverride newOverride = new CVF_VehicleOverride(className);
		configMgr.m_Config.VehicleOverrides.Insert(newOverride);
		configMgr.SaveConfig();
		CVF_Logger.Log("Added vehicle with global/fallback values: " + className);
	}

	static void RemoveVehicleFromOverrides(string className)
	{
		if (!g_Game || !g_Game.IsServer())
		{
			CVF_Logger.Error("Can only remove vehicles on server.");
			return;
		}

		CVF_ConfigManager configMgr = GetCVFConfigManager();
		configMgr.LoadConfig();

		for (int i = 0; i < configMgr.m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride existing = configMgr.m_Config.VehicleOverrides.Get(i);
			if (existing && existing.ClassName == className)
			{
				configMgr.m_Config.VehicleOverrides.Remove(i);
				configMgr.SaveConfig();
				CVF_Logger.Log("Removed vehicle: " + className);
				return;
			}
		}

		CVF_Logger.Warning("Vehicle not found: " + className);
	}
}

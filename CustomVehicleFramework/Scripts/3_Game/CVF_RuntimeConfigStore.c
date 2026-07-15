class CVF_RuntimeConfigStore
{
	private static ref map<string, ref CVF_GeneratedRuntimeConfig> s_RuntimeVehicles = new map<string, ref CVF_GeneratedRuntimeConfig>;
	private static bool s_HasVerifiedPackage = false;
	private static string s_ServerId = "";
	private static int s_PayloadHash = 0;

	static void InstallVerifiedPackage(CVF_GeneratedPackage package, int payloadHash)
	{
		ClearVerifiedPackage();
		if (!package || !package.RuntimeVehicles)
			return;

		for (int i = 0; i < package.RuntimeVehicles.Count(); i++)
		{
			CVF_GeneratedRuntimeConfig runtimeVehicle = package.RuntimeVehicles.Get(i);
			if (!runtimeVehicle)
				continue;

			string classKey = runtimeVehicle.ClassName;
			classKey.ToLower();
			s_RuntimeVehicles.Set(classKey, runtimeVehicle);
		}

		s_HasVerifiedPackage = true;
		s_ServerId = package.ServerId;
		s_PayloadHash = payloadHash;
		CVF_Logger.Log("Installed verified client runtime package. ServerId=" + s_ServerId + " Hash=" + payloadHash.ToString() + " Vehicles=" + s_RuntimeVehicles.Count().ToString());
	}

	static void ClearVerifiedPackage()
	{
		s_RuntimeVehicles.Clear();
		s_HasVerifiedPackage = false;
		s_ServerId = "";
		s_PayloadHash = 0;
	}

	static bool HasVerifiedPackage()
	{
		return s_HasVerifiedPackage;
	}

	static bool GetResolvedConfigFor(string className, out CVF_ResolvedVehicleConfig resolved)
	{
		resolved = null;
		if (!s_HasVerifiedPackage)
			return false;

		string classKey = className;
		classKey.ToLower();
		CVF_GeneratedRuntimeConfig runtimeVehicle;
		if (!s_RuntimeVehicles.Find(classKey, runtimeVehicle))
		{
			string currentClass = className;
			for (int parentIndex = 0; parentIndex < 50; parentIndex++)
			{
				string parentClass;
				if (!g_Game || !g_Game.ConfigGetBaseName("CfgVehicles " + currentClass, parentClass) || parentClass == "" || parentClass == currentClass)
					break;

				string parentKey = parentClass;
				parentKey.ToLower();
				if (s_RuntimeVehicles.Find(parentKey, runtimeVehicle))
				{
					s_RuntimeVehicles.Set(classKey, runtimeVehicle);
					break;
				}
				currentClass = parentClass;
			}
		}

		if (!runtimeVehicle)
			return false;

		resolved = new CVF_ResolvedVehicleConfig();
		resolved.MaxSpeedKmh = runtimeVehicle.MaxSpeedKmh;
		resolved.ThrottleMultiplier = runtimeVehicle.ThrottleMultiplier;
		resolved.ExtraDriveForce = runtimeVehicle.ExtraDriveForce;
		resolved.SteeringMultiplier = runtimeVehicle.SteeringMultiplier;
		resolved.SteeringYawAssist = runtimeVehicle.SteeringYawAssist;
		resolved.BrakeMultiplier = runtimeVehicle.BrakeMultiplier;
		resolved.ExtraBrakeForce = runtimeVehicle.ExtraBrakeForce;
		resolved.DragResistance = runtimeVehicle.DragResistance;
		resolved.StabilityAssist = runtimeVehicle.StabilityAssist;
		return true;
	}
}

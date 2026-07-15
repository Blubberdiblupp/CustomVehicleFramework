class CVF_ConfigRenderer
{
	static bool IsLoadedAddonClass(string addonName)
	{
		if (!g_Game || addonName == "")
			return false;
		return g_Game.ConfigGetType("CfgPatches " + addonName) == CT_CLASS;
	}

	static bool IsSupportedVehicleClass(string className)
	{
		if (!g_Game || className == "" || !g_Game.ConfigIsExisting("CfgVehicles " + className))
			return false;

		string basePath = "CfgVehicles " + className;
		string lowerName = className;
		lowerName.ToLower();
		if (lowerName.IndexOf("wreck") != -1 || lowerName.IndexOf("ruined") != -1)
			return false;

		string scopePath = basePath + " scope";
		if (g_Game.ConfigIsExisting(scopePath) && g_Game.ConfigGetInt(scopePath) <= 0)
			return false;

		string currentClass = className;
		for (int i = 0; i < 50 && currentClass != ""; i++)
		{
			if (currentClass == "CarScript")
				return true;

			string parentClass;
			if (!g_Game.ConfigGetBaseName("CfgVehicles " + currentClass, parentClass))
				break;
			currentClass = parentClass;
		}

		string modulePath = basePath + " SimulationModule";
		if (g_Game.ConfigIsExisting(modulePath))
		{
			if (g_Game.ConfigIsExisting(modulePath + " Steering") || g_Game.ConfigIsExisting(modulePath + " Engine") || g_Game.ConfigIsExisting(modulePath + " Gearbox"))
				return true;
		}

		string simulationPath = basePath + " simulation";
		if (!g_Game.ConfigIsExisting(simulationPath))
			return false;

		string simulation;
		g_Game.ConfigGetText(simulationPath, simulation);
		simulation.ToLower();
		return simulation.IndexOf("car") != -1;
	}

	static bool ValidatePackage(CVF_GeneratedPackage package, bool requireLocalClasses, bool requireLocalAddons, out string error)
	{
		error = "";
		if (!package)
		{
			error = "Package is null.";
			return false;
		}

		if (package.ProtocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION)
		{
			error = "Unsupported protocol version.";
			return false;
		}

		if (package.ConfigVersion != CVF_Constants.CONFIG_VERSION)
		{
			error = "Unsupported config version.";
			return false;
		}

		if (!CVF_SharedUtils.IsSafeToken(package.ServerId))
		{
			error = "Unsafe or empty server ID.";
			return false;
		}

		if (!package.RequiredAddons || package.RequiredAddons.Count() == 0 || package.RequiredAddons.Count() > 8192)
		{
			error = "Invalid required addon count.";
			return false;
		}

		bool hasCoreAddon = false;
		ref map<string, bool> knownAddons = new map<string, bool>;
		for (int addonIndex = 0; addonIndex < package.RequiredAddons.Count(); addonIndex++)
		{
			string addonName = package.RequiredAddons.Get(addonIndex);
			if (!CVF_SharedUtils.IsSafeAddonName(addonName) || addonName == "CustomVehicleFramework_GeneratedOverrides" || addonName == "CustomVehicleFramework_ClientGeneratedOverrides" || addonName == "CustomVehicleFramework_Overrides" || addonName == "CustomVehicleFramework_Overrides_Deprecated")
			{
				error = "Unsafe required addon name: " + addonName;
				return false;
			}
			if (requireLocalAddons && !IsLoadedAddonClass(addonName))
			{
				error = "Required CfgPatches entry is missing or is not an addon class: " + addonName;
				return false;
			}
			if (addonName == "CustomVehicleFramework")
				hasCoreAddon = true;

			bool addonIgnored;
			if (knownAddons.Find(addonName, addonIgnored))
			{
				error = "Duplicate required addon: " + addonName;
				return false;
			}
			knownAddons.Set(addonName, true);

		}

		if (!hasCoreAddon)
		{
			error = "The package does not depend on the CVF core addon.";
			return false;
		}

		if (!package.Vehicles || package.Vehicles.Count() > 4096)
		{
			error = "Invalid vehicle count.";
			return false;
		}

		ref map<string, bool> knownClasses = new map<string, bool>;
		for (int i = 0; i < package.Vehicles.Count(); i++)
		{
			CVF_GeneratedVehicleConfig vehicle = package.Vehicles.Get(i);
			if (!ValidateVehicle(vehicle, requireLocalClasses, error))
				return false;

			string classKey = vehicle.ClassName;
			classKey.ToLower();
			bool ignored;
			if (knownClasses.Find(classKey, ignored))
			{
				error = "Duplicate vehicle class: " + vehicle.ClassName;
				return false;
			}
			knownClasses.Set(classKey, true);
		}

		if (!package.RuntimeVehicles || package.RuntimeVehicles.Count() > 4096)
		{
			error = "Invalid runtime vehicle count.";
			return false;
		}

		ref map<string, bool> knownRuntimeClasses = new map<string, bool>;
		for (int runtimeIndex = 0; runtimeIndex < package.RuntimeVehicles.Count(); runtimeIndex++)
		{
			CVF_GeneratedRuntimeConfig runtimeVehicle = package.RuntimeVehicles.Get(runtimeIndex);
			if (!ValidateRuntimeVehicle(runtimeVehicle, requireLocalClasses, error))
				return false;

			string runtimeClassKey = runtimeVehicle.ClassName;
			runtimeClassKey.ToLower();
			bool runtimeIgnored;
			if (knownRuntimeClasses.Find(runtimeClassKey, runtimeIgnored))
			{
				error = "Duplicate runtime vehicle class: " + runtimeVehicle.ClassName;
				return false;
			}
			knownRuntimeClasses.Set(runtimeClassKey, true);
		}

		return true;
	}

	static bool WritePackage(CVF_GeneratedPackage package, string path, int payloadHash, int payloadChars, bool clientTarget, out string error)
	{
		if (!ValidatePackage(package, true, !clientTarget, error))
			return false;

		string addonClass = "CustomVehicleFramework_GeneratedOverrides";
		string probeClass = "GeneratedOverrideProbe";
		if (clientTarget)
		{
			addonClass = "CustomVehicleFramework_ClientGeneratedOverrides";
			probeClass = "ClientGeneratedOverrideProbe";
		}

		FileHandle file = OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			error = "Could not open generated config for writing: " + path;
			return false;
		}

		FPrintln(file, "// Generated by CustomVehicleFramework. Do not edit this file.");
		FPrintln(file, "class CfgPatches");
		FPrintln(file, "{");
		FPrintln(file, "    class " + addonClass);
		FPrintln(file, "    {");
		FPrintln(file, "        units[] = {};");
		FPrintln(file, "        weapons[] = {};");
		FPrintln(file, "        requiredVersion = 0.1;");
		WriteRequiredAddons(file, package.RequiredAddons, clientTarget);
		FPrintln(file, "    };");
		FPrintln(file, "};");
		FPrintln(file, "");
		FPrintln(file, "class CfgCustomVehicleFramework");
		FPrintln(file, "{");
		FPrintln(file, "    class " + probeClass);
		FPrintln(file, "    {");
		FPrintln(file, "        cvfProtocol = " + package.ProtocolVersion.ToString() + ";");
		FPrintln(file, "        cvfPayloadHash = " + payloadHash.ToString() + ";");
		FPrintln(file, "        cvfPayloadChars = " + payloadChars.ToString() + ";");
		FPrintln(file, "        cvfServerId = \"" + package.ServerId + "\";");
		FPrintln(file, "    };");
		FPrintln(file, "};");
		FPrintln(file, "");
		FPrintln(file, "class CfgVehicles");
		FPrintln(file, "{");
		FPrintln(file, "");
		WriteParentDeclarations(file, package.Vehicles);

		for (int i = 0; i < package.Vehicles.Count(); i++)
			WriteVehicle(file, package.Vehicles.Get(i));
		FPrintln(file, "};");

		CloseFile(file);
		return true;
	}

	static bool VerifyLoadedPackage(CVF_GeneratedPackage package, out string error)
	{
		if (!ValidatePackage(package, true, false, error))
			return false;

		for (int i = 0; i < package.Vehicles.Count(); i++)
		{
			CVF_GeneratedVehicleConfig vehicle = package.Vehicles.Get(i);
			string modulePath = "CfgVehicles " + vehicle.ClassName + " SimulationModule";
			if (!VerifyOptionalFloat(modulePath + " Steering maxSteeringAngle", vehicle.MaxSteeringAngle, error)) return false;
			if (!VerifyOptionalArray(modulePath + " Steering increaseSpeed", vehicle.SteeringIncreaseSpeed, error)) return false;
			if (!VerifyOptionalArray(modulePath + " Steering decreaseSpeed", vehicle.SteeringDecreaseSpeed, error)) return false;
			if (!VerifyOptionalArray(modulePath + " Steering centeringSpeed", vehicle.SteeringCenteringSpeed, error)) return false;
			if (!VerifyOptionalFloat(modulePath + " Engine rpmIdle", vehicle.EngineRPMIdle, error)) return false;
			if (!VerifyOptionalFloat(modulePath + " Engine rpmMin", vehicle.EngineRPMMin, error)) return false;
			if (!VerifyOptionalFloat(modulePath + " Engine rpmClutch", vehicle.EngineRPMClutch, error)) return false;
			if (!VerifyOptionalFloat(modulePath + " Engine rpmRedline", vehicle.EngineRPMRedline, error)) return false;
			if (!VerifyOptionalArray(modulePath + " Engine torqueCurve", vehicle.EngineTorqueCurve, error)) return false;
			if (!VerifyOptionalText(modulePath + " Gearbox type", vehicle.GearboxType, error)) return false;
			if (!VerifyOptionalFloat(modulePath + " Gearbox reverse", vehicle.GearboxReverse, error)) return false;
			if (!VerifyOptionalArray(modulePath + " Gearbox ratios", vehicle.GearboxRatios, error)) return false;
		}

		error = "";
		return true;
	}

	static bool VerifyLoadedProbe(CVF_GeneratedPackage package, int payloadHash, int payloadChars, bool clientTarget, out string error)
	{
		error = "";
		if (!package || !g_Game)
		{
			error = "Package or game config is unavailable.";
			return false;
		}

		string probePath = CVF_Constants.CONFIG_PROBE_PATH;
		if (clientTarget)
			probePath = CVF_Constants.CLIENT_CONFIG_PROBE_PATH;
		if (!g_Game.ConfigIsExisting(probePath + " cvfProtocol"))
		{
			error = "The generated CVF config probe is not loaded.";
			return false;
		}

		string loadedServerId;
		g_Game.ConfigGetText(probePath + " cvfServerId", loadedServerId);
		int loadedProtocol = g_Game.ConfigGetInt(probePath + " cvfProtocol");
		int loadedHash = g_Game.ConfigGetInt(probePath + " cvfPayloadHash");
		int loadedChars = g_Game.ConfigGetInt(probePath + " cvfPayloadChars");
		if (loadedProtocol != package.ProtocolVersion || loadedServerId != package.ServerId || loadedHash != payloadHash || loadedChars != payloadChars)
		{
			error = "Loaded CVF probe differs from the server package.";
			return false;
		}
		return true;
	}

	static bool BuildLoadedNativeFingerprint(CVF_GeneratedPackage package, out int fingerprintHash, out int fingerprintChars, out string error)
	{
		fingerprintHash = 0;
		fingerprintChars = 0;
		error = "";
		if (!ValidatePackage(package, true, false, error))
			return false;

		ref array<string> managedClasses = new array<string>;
		ref map<string, bool> knownClasses = new map<string, bool>;
		for (int nativeIndex = 0; nativeIndex < package.Vehicles.Count(); nativeIndex++)
		{
			string nativeClass = package.Vehicles.Get(nativeIndex).ClassName;
			knownClasses.Set(nativeClass, true);
			managedClasses.Insert(nativeClass);
		}
		for (int runtimeIndex = 0; runtimeIndex < package.RuntimeVehicles.Count(); runtimeIndex++)
		{
			string runtimeClass = package.RuntimeVehicles.Get(runtimeIndex).ClassName;
			bool ignored;
			if (!knownClasses.Find(runtimeClass, ignored))
			{
				knownClasses.Set(runtimeClass, true);
				managedClasses.Insert(runtimeClass);
			}
		}
		managedClasses.Sort();

		string fingerprint = "CVF_NATIVE_V1|" + managedClasses.Count().ToString() + "|";
		for (int i = 0; i < managedClasses.Count(); i++)
		{
			string className = managedClasses.Get(i);
			string modulePath = "CfgVehicles " + className + " SimulationModule";
			fingerprint = fingerprint + "CLASS=" + className + "|";
			fingerprint = fingerprint + FingerprintFloat("steering.max", modulePath + " Steering maxSteeringAngle");
			fingerprint = fingerprint + FingerprintArray("steering.increase", modulePath + " Steering increaseSpeed");
			fingerprint = fingerprint + FingerprintArray("steering.decrease", modulePath + " Steering decreaseSpeed");
			fingerprint = fingerprint + FingerprintArray("steering.center", modulePath + " Steering centeringSpeed");
			fingerprint = fingerprint + FingerprintFloat("throttle.reaction", modulePath + " Throttle reactionTime");
			fingerprint = fingerprint + FingerprintFloat("throttle.default", modulePath + " Throttle defaultThrust");
			fingerprint = fingerprint + FingerprintFloat("throttle.gentle", modulePath + " Throttle gentleThrust");
			fingerprint = fingerprint + FingerprintFloat("throttle.turbo", modulePath + " Throttle turboCoef");
			fingerprint = fingerprint + FingerprintFloat("throttle.gentleCoef", modulePath + " Throttle gentleCoef");
			fingerprint = fingerprint + FingerprintArray("brake.pressure", modulePath + " Brake pressureBySpeed");
			fingerprint = fingerprint + FingerprintFloat("brake.gentle", modulePath + " Brake gentleCoef");
			fingerprint = fingerprint + FingerprintFloat("brake.minimum", modulePath + " Brake minPressure");
			fingerprint = fingerprint + FingerprintFloat("brake.reaction", modulePath + " Brake reactionTime");
			fingerprint = fingerprint + FingerprintFloat("brake.driverless", modulePath + " Brake driverless");
			fingerprint = fingerprint + FingerprintFloat("aero.area", modulePath + " Aerodynamics frontalArea");
			fingerprint = fingerprint + FingerprintFloat("aero.drag", modulePath + " Aerodynamics dragCoefficient");
			fingerprint = fingerprint + FingerprintFloat("aero.downforce", modulePath + " Aerodynamics downforceCoefficient");
			fingerprint = fingerprint + FingerprintArray("aero.offset", modulePath + " Aerodynamics downforceOffset");
			fingerprint = fingerprint + FingerprintText("drive", modulePath + " drive");
			fingerprint = fingerprint + FingerprintArray("engine.torque", modulePath + " Engine torqueCurve");
			fingerprint = fingerprint + FingerprintFloat("engine.inertia", modulePath + " Engine inertia");
			fingerprint = fingerprint + FingerprintFloat("engine.friction", modulePath + " Engine frictionTorque");
			fingerprint = fingerprint + FingerprintFloat("engine.rolling", modulePath + " Engine rollingFriction");
			fingerprint = fingerprint + FingerprintFloat("engine.viscous", modulePath + " Engine viscousFriction");
			fingerprint = fingerprint + FingerprintFloat("engine.idle", modulePath + " Engine rpmIdle");
			fingerprint = fingerprint + FingerprintFloat("engine.minimum", modulePath + " Engine rpmMin");
			fingerprint = fingerprint + FingerprintFloat("engine.clutch", modulePath + " Engine rpmClutch");
			fingerprint = fingerprint + FingerprintFloat("engine.redline", modulePath + " Engine rpmRedline");
			fingerprint = fingerprint + FingerprintFloat("clutch.torque", modulePath + " Clutch maxTorqueTransfer");
			fingerprint = fingerprint + FingerprintFloat("clutch.uncouple", modulePath + " Clutch uncoupleTime");
			fingerprint = fingerprint + FingerprintFloat("clutch.couple", modulePath + " Clutch coupleTime");
			fingerprint = fingerprint + FingerprintText("gearbox.type", modulePath + " Gearbox type");
			fingerprint = fingerprint + FingerprintFloat("gearbox.reverse", modulePath + " Gearbox reverse");
			fingerprint = fingerprint + FingerprintArray("gearbox.ratios", modulePath + " Gearbox ratios");
		}

		fingerprintChars = fingerprint.Length();
		fingerprintHash = CVF_SharedUtils.HashText(fingerprint);
		return fingerprintChars > 0 && fingerprintHash != 0;
	}

	private static bool ValidateVehicle(CVF_GeneratedVehicleConfig vehicle, bool requireLocalClass, out string error)
	{
		if (!vehicle || !CVF_SharedUtils.IsSafeIdentifier(vehicle.ClassName) || !CVF_SharedUtils.IsSafeIdentifier(vehicle.ParentClass) || vehicle.ParentClass == vehicle.ClassName)
		{
			error = "Unsafe vehicle class or parent class name.";
			return false;
		}

		if (requireLocalClass && !IsSupportedVehicleClass(vehicle.ClassName))
		{
			error = "Vehicle class is not loaded locally or is not a supported car: " + vehicle.ClassName;
			return false;
		}
		if (requireLocalClass)
		{
			string localParent;
			if (!g_Game.ConfigGetBaseName("CfgVehicles " + vehicle.ClassName, localParent) || localParent != vehicle.ParentClass)
			{
				error = "Vehicle parent class differs for " + vehicle.ClassName + ". Server=" + vehicle.ParentClass + " Local=" + localParent;
				return false;
			}
		}

		if (!ValidateOptionalFloat(vehicle.MaxSteeringAngle, 0.0, 180.0))
		{
			error = "Invalid steering angle for " + vehicle.ClassName;
			return false;
		}

		if (!ValidateOptionalFloat(vehicle.EngineRPMIdle, 0.0, 100000.0) || !ValidateOptionalFloat(vehicle.EngineRPMMin, 0.0, 100000.0) || !ValidateOptionalFloat(vehicle.EngineRPMClutch, 0.0, 100000.0) || !ValidateOptionalFloat(vehicle.EngineRPMRedline, 0.0, 100000.0))
		{
			error = "Invalid engine RPM value for " + vehicle.ClassName;
			return false;
		}

		if (!ValidateOptionalFloat(vehicle.GearboxReverse, 0.0, 1000.0))
		{
			error = "Invalid reverse ratio for " + vehicle.ClassName;
			return false;
		}

		if (vehicle.GearboxType != "" && vehicle.GearboxType != "GEARBOX_MANUAL" && vehicle.GearboxType != "GEARBOX_AUTOMATIC")
		{
			error = "Unsupported gearbox type for " + vehicle.ClassName;
			return false;
		}

		if (!ValidatePairArray(vehicle.SteeringIncreaseSpeed, 128, 0.0, 10000000.0) || !ValidatePairArray(vehicle.SteeringDecreaseSpeed, 128, 0.0, 10000000.0) || !ValidatePairArray(vehicle.SteeringCenteringSpeed, 128, 0.0, 10000000.0) || !ValidatePairArray(vehicle.EngineTorqueCurve, 256, -10000000.0, 10000000.0))
		{
			error = "Invalid curve array for " + vehicle.ClassName;
			return false;
		}

		if (!ValidateArray(vehicle.GearboxRatios, 64, 0.000001, 1000.0))
		{
			error = "Invalid gearbox ratios for " + vehicle.ClassName;
			return false;
		}

		return true;
	}

	private static bool ValidateRuntimeVehicle(CVF_GeneratedRuntimeConfig vehicle, bool requireLocalClass, out string error)
	{
		if (!vehicle || !CVF_SharedUtils.IsSafeIdentifier(vehicle.ClassName))
		{
			error = "Unsafe runtime vehicle class name.";
			return false;
		}
		if (requireLocalClass && !IsSupportedVehicleClass(vehicle.ClassName))
		{
			error = "Runtime vehicle class is not loaded locally or is not a supported car: " + vehicle.ClassName;
			return false;
		}

		if (!CVF_SharedUtils.IsFiniteInRange(vehicle.MaxSpeedKmh, 0.0, 10000.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.ThrottleMultiplier, 0.0, 100.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.SteeringMultiplier, 0.0, 100.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.BrakeMultiplier, 0.0, 100.0))
		{
			error = "Invalid runtime multiplier for " + vehicle.ClassName;
			return false;
		}
		if (!CVF_SharedUtils.IsFiniteInRange(vehicle.ExtraDriveForce, 0.0, 100000000.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.SteeringYawAssist, 0.0, 100000000.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.ExtraBrakeForce, 0.0, 100000000.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.DragResistance, 0.0, 1000000.0) || !CVF_SharedUtils.IsFiniteInRange(vehicle.StabilityAssist, 0.0, 100000000.0))
		{
			error = "Invalid runtime force for " + vehicle.ClassName;
			return false;
		}
		return true;
	}

	private static bool ValidateOptionalFloat(float value, float minimum, float maximum)
	{
		if (CVF_Constants.IsFallback(value))
			return true;
		return CVF_SharedUtils.IsFiniteInRange(value, minimum, maximum);
	}

	private static string FingerprintFloat(string key, string path)
	{
		if (!g_Game || !g_Game.ConfigIsExisting(path))
			return key + "=<missing>|";
		return key + "=" + g_Game.ConfigGetFloat(path).ToString() + "|";
	}

	private static string FingerprintArray(string key, string path)
	{
		if (!g_Game || !g_Game.ConfigIsExisting(path))
			return key + "=<missing>|";

		ref array<float> values = new array<float>;
		g_Game.ConfigGetFloatArray(path, values);
		string output = key + "=[";
		for (int i = 0; i < values.Count(); i++)
		{
			if (i > 0)
				output = output + ",";
			output = output + values.Get(i).ToString();
		}
		return output + "]|";
	}

	private static string FingerprintText(string key, string path)
	{
		if (!g_Game || !g_Game.ConfigIsExisting(path))
			return key + "=<missing>|";

		string value;
		g_Game.ConfigGetText(path, value);
		return key + "=" + value + "|";
	}

	private static bool VerifyOptionalFloat(string path, float expected, out string error)
	{
		if (CVF_Constants.IsFallback(expected))
			return true;
		if (!g_Game || !g_Game.ConfigIsExisting(path))
		{
			error = "Loaded config value is missing: " + path;
			return false;
		}

		float actual = g_Game.ConfigGetFloat(path);
		if (!NearlyEqual(actual, expected))
		{
			error = "Loaded config value differs at " + path + ". Expected=" + expected.ToString() + " Actual=" + actual.ToString();
			return false;
		}
		return true;
	}

	private static bool VerifyOptionalArray(string path, array<float> expected, out string error)
	{
		if (CVF_Constants.IsFallbackArray(expected))
			return true;
		if (!g_Game || !g_Game.ConfigIsExisting(path))
		{
			error = "Loaded config array is missing: " + path;
			return false;
		}

		ref array<float> actual = new array<float>;
		g_Game.ConfigGetFloatArray(path, actual);
		if (actual.Count() != expected.Count())
		{
			error = "Loaded config array size differs at " + path + ". Expected=" + expected.Count().ToString() + " Actual=" + actual.Count().ToString();
			return false;
		}

		for (int i = 0; i < expected.Count(); i++)
		{
			if (!NearlyEqual(actual.Get(i), expected.Get(i)))
			{
				error = "Loaded config array differs at " + path + " index " + i.ToString();
				return false;
			}
		}
		return true;
	}

	private static bool VerifyOptionalText(string path, string expected, out string error)
	{
		if (expected == "")
			return true;
		if (!g_Game || !g_Game.ConfigIsExisting(path))
		{
			error = "Loaded config text is missing: " + path;
			return false;
		}

		string actual;
		g_Game.ConfigGetText(path, actual);
		if (actual != expected)
		{
			error = "Loaded config text differs at " + path + ". Expected=" + expected + " Actual=" + actual;
			return false;
		}
		return true;
	}

	private static bool NearlyEqual(float actual, float expected)
	{
		float tolerance = Math.Max(0.0001, Math.AbsFloat(expected) * 0.0001);
		return Math.AbsFloat(actual - expected) <= tolerance;
	}

	private static bool ValidatePairArray(array<float> values, int maximumCount, float minimum, float maximum)
	{
		if (CVF_Constants.IsFallbackArray(values))
			return true;
		if ((values.Count() % 2) != 0)
			return false;
		return ValidateArray(values, maximumCount, minimum, maximum);
	}

	private static bool ValidateArray(array<float> values, int maximumCount, float minimum, float maximum)
	{
		if (CVF_Constants.IsFallbackArray(values))
			return true;
		if (values.Count() > maximumCount)
			return false;

		for (int i = 0; i < values.Count(); i++)
		{
			if (!CVF_SharedUtils.IsFiniteInRange(values.Get(i), minimum, maximum))
				return false;
		}
		return true;
	}

	private static void WriteVehicle(FileHandle file, CVF_GeneratedVehicleConfig data)
	{
		FPrintln(file, "    class " + data.ClassName + " : " + data.ParentClass);
		FPrintln(file, "    {");
		FPrintln(file, "        class SimulationModule : SimulationModule");
		FPrintln(file, "        {");

		if (HasSteeringOverrides(data))
			WriteSteering(file, data);
		if (HasEngineOverrides(data))
			WriteEngine(file, data);
		if (HasGearboxOverrides(data))
			WriteGearbox(file, data);

		FPrintln(file, "        };");
		FPrintln(file, "    };");
		FPrintln(file, "");
	}

	private static void WriteRequiredAddons(FileHandle file, array<string> requiredAddons, bool clientTarget)
	{
		ref array<string> localAddons = new array<string>;
		if (clientTarget)
		{
			ref map<string, bool> known = new map<string, bool>;
			int addonCount = g_Game.ConfigGetChildrenCount("CfgPatches");
			for (int addonIndex = 0; addonIndex < addonCount; addonIndex++)
			{
				string localAddonName;
				g_Game.ConfigGetChildName("CfgPatches", addonIndex, localAddonName);
				if (!IsLoadedAddonClass(localAddonName) || !CVF_SharedUtils.IsSafeAddonName(localAddonName) || IsGeneratedAddon(localAddonName))
					continue;

				bool ignored;
				if (!known.Find(localAddonName, ignored))
				{
					known.Set(localAddonName, true);
					localAddons.Insert(localAddonName);
				}
			}
			localAddons.Sort();
		}
		else
		{
			for (int sourceIndex = 0; sourceIndex < requiredAddons.Count(); sourceIndex++)
			{
				string addonName = requiredAddons.Get(sourceIndex);
				if (IsLoadedAddonClass(addonName))
					localAddons.Insert(addonName);
			}
		}

		FPrintln(file, "        requiredAddons[] =");
		FPrintln(file, "        {");
		for (int i = 0; i < localAddons.Count(); i++)
		{
			string suffix = ",";
			if (i == localAddons.Count() - 1)
				suffix = "";
			FPrintln(file, "            \"" + localAddons.Get(i) + "\"" + suffix);
		}
		FPrintln(file, "        };");
	}

	private static bool IsGeneratedAddon(string addonName)
	{
		return addonName == "CustomVehicleFramework_GeneratedOverrides" || addonName == "CustomVehicleFramework_ClientGeneratedOverrides" || addonName == "CustomVehicleFramework_Overrides" || addonName == "CustomVehicleFramework_Overrides_Deprecated";
	}

	private static void WriteParentDeclarations(FileHandle file, array<ref CVF_GeneratedVehicleConfig> vehicles)
	{
		ref map<string, bool> declared = new map<string, bool>;
		for (int i = 0; i < vehicles.Count(); i++)
		{
			CVF_GeneratedVehicleConfig vehicle = vehicles.Get(i);
			bool ignored;
			if (!declared.Find(vehicle.ParentClass, ignored))
			{
				declared.Set(vehicle.ParentClass, true);
				FPrintln(file, "    class " + vehicle.ParentClass + ";");
			}
		}
		if (vehicles.Count() > 0)
			FPrintln(file, "    class SimulationModule;");
		if (declared.Count() > 0 || vehicles.Count() > 0)
			FPrintln(file, "");
	}

	private static bool HasSteeringOverrides(CVF_GeneratedVehicleConfig data)
	{
		return !CVF_Constants.IsFallback(data.MaxSteeringAngle) || !CVF_Constants.IsFallbackArray(data.SteeringIncreaseSpeed) || !CVF_Constants.IsFallbackArray(data.SteeringDecreaseSpeed) || !CVF_Constants.IsFallbackArray(data.SteeringCenteringSpeed);
	}

	private static bool HasEngineOverrides(CVF_GeneratedVehicleConfig data)
	{
		return !CVF_Constants.IsFallback(data.EngineRPMIdle) || !CVF_Constants.IsFallback(data.EngineRPMMin) || !CVF_Constants.IsFallback(data.EngineRPMClutch) || !CVF_Constants.IsFallback(data.EngineRPMRedline) || !CVF_Constants.IsFallbackArray(data.EngineTorqueCurve);
	}

	private static bool HasGearboxOverrides(CVF_GeneratedVehicleConfig data)
	{
		return data.GearboxType != "" || !CVF_Constants.IsFallback(data.GearboxReverse) || !CVF_Constants.IsFallbackArray(data.GearboxRatios);
	}

	private static void WriteSteering(FileHandle file, CVF_GeneratedVehicleConfig data)
	{
		FPrintln(file, BuildSimulationChildDeclaration(data, "Steering"));
		FPrintln(file, "            {");
		if (!CVF_Constants.IsFallback(data.MaxSteeringAngle)) FPrintln(file, "                maxSteeringAngle = " + FloatToConfig(data.MaxSteeringAngle) + ";");
		if (!CVF_Constants.IsFallbackArray(data.SteeringIncreaseSpeed)) FPrintln(file, "                increaseSpeed[] = {" + ArrayToConfig(data.SteeringIncreaseSpeed) + "};");
		if (!CVF_Constants.IsFallbackArray(data.SteeringDecreaseSpeed)) FPrintln(file, "                decreaseSpeed[] = {" + ArrayToConfig(data.SteeringDecreaseSpeed) + "};");
		if (!CVF_Constants.IsFallbackArray(data.SteeringCenteringSpeed)) FPrintln(file, "                centeringSpeed[] = {" + ArrayToConfig(data.SteeringCenteringSpeed) + "};");
		FPrintln(file, "            };");
	}

	private static void WriteEngine(FileHandle file, CVF_GeneratedVehicleConfig data)
	{
		FPrintln(file, BuildSimulationChildDeclaration(data, "Engine"));
		FPrintln(file, "            {");
		if (!CVF_Constants.IsFallbackArray(data.EngineTorqueCurve)) FPrintln(file, "                torqueCurve[] = {" + ArrayToConfig(data.EngineTorqueCurve) + "};");
		if (!CVF_Constants.IsFallback(data.EngineRPMIdle)) FPrintln(file, "                rpmIdle = " + FloatToConfig(data.EngineRPMIdle) + ";");
		if (!CVF_Constants.IsFallback(data.EngineRPMMin)) FPrintln(file, "                rpmMin = " + FloatToConfig(data.EngineRPMMin) + ";");
		if (!CVF_Constants.IsFallback(data.EngineRPMClutch)) FPrintln(file, "                rpmClutch = " + FloatToConfig(data.EngineRPMClutch) + ";");
		if (!CVF_Constants.IsFallback(data.EngineRPMRedline)) FPrintln(file, "                rpmRedline = " + FloatToConfig(data.EngineRPMRedline) + ";");
		FPrintln(file, "            };");
	}

	private static void WriteGearbox(FileHandle file, CVF_GeneratedVehicleConfig data)
	{
		FPrintln(file, BuildSimulationChildDeclaration(data, "Gearbox"));
		FPrintln(file, "            {");
		if (data.GearboxType != "") FPrintln(file, "                type = \"" + data.GearboxType + "\";");
		if (!CVF_Constants.IsFallback(data.GearboxReverse)) FPrintln(file, "                reverse = " + FloatToConfig(data.GearboxReverse) + ";");
		if (!CVF_Constants.IsFallbackArray(data.GearboxRatios)) FPrintln(file, "                ratios[] = {" + ArrayToConfig(data.GearboxRatios) + "};");
		FPrintln(file, "            };");
	}

	private static string BuildSimulationChildDeclaration(CVF_GeneratedVehicleConfig data, string childClass)
	{
		string declaration = "            class " + childClass;
		if (g_Game && data && g_Game.ConfigIsExisting("CfgVehicles " + data.ParentClass + " SimulationModule " + childClass))
			declaration = declaration + " : " + childClass;
		return declaration;
	}

	private static string ArrayToConfig(array<float> values)
	{
		string output = "";
		for (int i = 0; i < values.Count(); i++)
		{
			if (i > 0) output = output + ", ";
			output = output + FloatToConfig(values.Get(i));
		}
		return output;
	}

	private static string FloatToConfig(float value)
	{
		return value.ToString();
	}
}

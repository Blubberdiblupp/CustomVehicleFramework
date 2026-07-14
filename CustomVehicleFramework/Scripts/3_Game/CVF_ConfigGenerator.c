class CVF_ConfigGenerator
{
	private static ref CVF_GeneratedPackage s_CurrentPackage;
	private static int s_CurrentPayloadHash = 0;
	private static int s_CurrentPayloadChars = 0;
	private static bool s_HasGeneratedPackage = false;
	private static bool s_LoadedPackageVerified = false;
	private static string s_LoadedVerificationError = "";

	static bool GenerateNextBootConfig(CVF_ConfigManager configMgr)
	{
		if (!g_Game || !g_Game.IsServer() || !configMgr || !configMgr.m_Config)
			return false;
		s_LoadedPackageVerified = false;
		s_LoadedVerificationError = "";

		bool activeFileExisted = FileExist(CVF_Constants.GENERATED_CONFIG_FILE);
		if (!CVF_FileIO.EnsureServerDirectories())
		{
			CVF_Logger.Error("Could not create server generation directories.");
			return false;
		}

		CVF_GeneratedPackage package = BuildPackage(configMgr);
		if (!package)
		{
			CVF_Logger.Error("Could not build the native override package. The previous active files were kept.");
			return false;
		}
		string validationError;
		if (!CVF_ConfigRenderer.ValidatePackage(package, true, validationError))
		{
			CVF_Logger.Error("Generated package validation failed: " + validationError);
			return false;
		}

		string packageJson;
		string jsonError;
		if (!JsonFileLoader<CVF_GeneratedPackage>.MakeData(package, packageJson, jsonError, false))
		{
			CVF_Logger.Error("Could not serialize generated package: " + jsonError);
			return false;
		}

		int payloadHash = CVF_SharedUtils.HashText(packageJson);
		int payloadChars = packageJson.Length();
		if (payloadChars <= 0 || payloadChars > CVF_Constants.MAX_GENERATED_PACKAGE_CHARS)
		{
			CVF_Logger.Error("Generated package is empty or exceeds the supported size limit.");
			return false;
		}

		string renderError;
		if (!CVF_ConfigRenderer.WritePackage(package, CVF_Constants.STAGING_CONFIG_FILE, payloadHash, payloadChars, renderError))
		{
			CVF_Logger.Error(renderError);
			return false;
		}

		if (!CVF_FileIO.CopyTextFile(CVF_Constants.STAGING_CONFIG_FILE, CVF_Constants.GENERATED_CONFIG_FILE))
		{
			CVF_Logger.Error("Could not activate the generated server config.");
			return false;
		}
		if (!CVF_FileIO.WriteAllTextIfChanged(CVF_Constants.GENERATED_PREFIX_FILE, CVF_Constants.GENERATED_PBO_PREFIX))
		{
			CVF_Logger.Error("Could not write the generated addon prefix file.");
			return false;
		}
		string generatedInfo = "Custom Vehicle Framework generated native override.\n\nDo not edit config.cpp directly. Edit vehicles.json instead.\nThe static @CustomVehicleFrameworkServer bootstrap PBO maps this directory through -filePatching.\nLoad only @CustomVehicleFrameworkServer through -serverMod; do not add this generated directory to -serverMod.\n\nAfter restart, check the CVF script log. CVF reports success only after verifying every loaded native value.\n";
		if (!CVF_FileIO.WriteAllTextIfChanged(CVF_Constants.GENERATED_INFO_FILE, generatedInfo))
		{
			CVF_Logger.Error("Could not write the generated addon information file.");
			return false;
		}
		s_CurrentPackage = package;
		s_CurrentPayloadHash = payloadHash;
		s_CurrentPayloadChars = payloadChars;
		s_HasGeneratedPackage = true;
		RefreshLoadedVerification();

		CVF_Logger.Log("Generated native override config for the server bootstrap. Vehicles=" + package.Vehicles.Count().ToString() + " Hash=" + payloadHash.ToString() + " Path=" + CVF_Constants.GENERATED_CONFIG_FILE);
		LogLoadedConfigState(activeFileExisted);
		return true;
	}

	static int GetCurrentPayloadHash()
	{
		return s_CurrentPayloadHash;
	}

	static int GetCurrentPayloadChars()
	{
		return s_CurrentPayloadChars;
	}

	static bool HasGeneratedPackage()
	{
		return s_HasGeneratedPackage;
	}

	static int GetLoadedProtocol()
	{
		if (!g_Game || !g_Game.ConfigIsExisting(CVF_Constants.CONFIG_PROBE_PATH + " cvfProtocol"))
			return 0;
		return g_Game.ConfigGetInt(CVF_Constants.CONFIG_PROBE_PATH + " cvfProtocol");
	}

	static int GetLoadedPayloadHash()
	{
		if (!g_Game || !g_Game.ConfigIsExisting(CVF_Constants.CONFIG_PROBE_PATH + " cvfPayloadHash"))
			return 0;
		return g_Game.ConfigGetInt(CVF_Constants.CONFIG_PROBE_PATH + " cvfPayloadHash");
	}

	static int GetLoadedPayloadChars()
	{
		if (!g_Game || !g_Game.ConfigIsExisting(CVF_Constants.CONFIG_PROBE_PATH + " cvfPayloadChars"))
			return 0;
		return g_Game.ConfigGetInt(CVF_Constants.CONFIG_PROBE_PATH + " cvfPayloadChars");
	}

	static string GetLoadedServerId()
	{
		string serverId = "";
		if (g_Game)
			g_Game.ConfigGetText(CVF_Constants.CONFIG_PROBE_PATH + " cvfServerId", serverId);
		return serverId;
	}

	static bool IsLoadedConfigCurrent()
	{
		return IsLoadedProbeCurrent() && s_LoadedPackageVerified;
	}

	private static bool IsLoadedProbeCurrent()
	{
		if (!s_HasGeneratedPackage || !s_CurrentPackage)
			return false;
		return GetLoadedProtocol() == CVF_Constants.SYNC_PROTOCOL_VERSION && GetLoadedPayloadHash() == s_CurrentPayloadHash && GetLoadedPayloadChars() == s_CurrentPayloadChars && GetLoadedServerId() == s_CurrentPackage.ServerId;
	}

	private static void RefreshLoadedVerification()
	{
		s_LoadedPackageVerified = false;
		s_LoadedVerificationError = "";
		if (!IsLoadedProbeCurrent())
			return;

		s_LoadedPackageVerified = CVF_ConfigRenderer.VerifyLoadedPackage(s_CurrentPackage, s_LoadedVerificationError);
	}

	private static void LogLoadedConfigState(bool activeFileExisted)
	{
		if (IsLoadedConfigCurrent())
		{
			CVF_Logger.Log("The generated override config is active through the server bootstrap and all native vehicle values match the server JSON.");
			return;
		}
		if (IsLoadedProbeCurrent() && !s_LoadedPackageVerified)
		{
			CVF_Logger.Error("The CVF probe loaded, but native vehicle value verification failed: " + s_LoadedVerificationError);
			return;
		}

		if (activeFileExisted && GetLoadedProtocol() == 0)
		{
			CVF_Logger.Error("The generated config.cpp existed before startup, but the server bootstrap did not file-patch it. Verify -filePatching, -serverMod=@CustomVehicleFrameworkServer, the unbinarized bootstrap PBO, and the profiles\\CustomVehicleFramework_Generated addon prefix.");
			return;
		}

		CVF_Logger.Warning("Native overrides were generated for the next boot. Restart the complete server process so @CustomVehicleFrameworkServer can file-patch the generated config before scripts start.");
	}

	private static CVF_GeneratedPackage BuildPackage(CVF_ConfigManager configMgr)
	{
		CVF_GeneratedPackage package = new CVF_GeneratedPackage();
		package.ServerId = configMgr.m_Config.ServerId;
		if (!CaptureRequiredAddons(package))
			return null;

		if (!configMgr.m_Config.EnableVehicleOverrides)
			return package;

		ref map<string, bool> processedClasses = new map<string, bool>;
		if (configMgr.m_Config.GlobalSettings && configMgr.m_Config.GlobalSettings.HasNativeOverrides())
		{
			int totalClasses = g_Game.ConfigGetChildrenCount("CfgVehicles");
			for (int classIndex = 0; classIndex < totalClasses; classIndex++)
			{
				string globalClassName;
				g_Game.ConfigGetChildName("CfgVehicles", classIndex, globalClassName);
				if (!CVF_ConfigRenderer.IsSupportedVehicleClass(globalClassName))
					continue;

				string globalClassKey = globalClassName;
				globalClassKey.ToLower();
				processedClasses.Set(globalClassKey, true);
				if (!AppendNativeVehicle(package, configMgr, globalClassName))
					return null;
			}
		}

		for (int i = 0; i < configMgr.m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = configMgr.m_Config.VehicleOverrides.Get(i);
			if (!overrideData || overrideData.ClassName == "")
				continue;

			string classKey = overrideData.ClassName;
			classKey.ToLower();
			bool alreadyProcessed;
			if (processedClasses.Find(classKey, alreadyProcessed))
				continue;
			processedClasses.Set(classKey, true);

			if (!CVF_ConfigRenderer.IsSupportedVehicleClass(overrideData.ClassName))
			{
				CVF_Logger.Warning("Skipping unavailable or unsupported vehicle class: " + overrideData.ClassName);
				continue;
			}

			if (!AppendNativeVehicle(package, configMgr, overrideData.ClassName))
				return null;
		}

		return package;
	}

	private static bool AppendNativeVehicle(CVF_GeneratedPackage package, CVF_ConfigManager configMgr, string className)
	{
		CVF_ResolvedVehicleConfig resolved;
		if (!configMgr.GetResolvedConfigFor(className, resolved) || !HasNativeConfigOverrides(resolved))
			return true;

		CVF_GeneratedVehicleConfig generatedVehicle = BuildGeneratedVehicle(className, resolved);
		if (!generatedVehicle)
			return false;
		if (HasConfigOverrides(generatedVehicle))
			package.Vehicles.Insert(generatedVehicle);
		return true;
	}

	private static bool HasNativeConfigOverrides(CVF_ResolvedVehicleConfig data)
	{
		if (!data) return false;
		if (!CVF_Constants.IsFallback(data.MaxSteeringAngle)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMIdle)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMMin)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMClutch)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMRedline)) return true;
		if (data.GearboxType != "") return true;
		if (!CVF_Constants.IsFallback(data.GearboxReverse)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringIncreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringDecreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringCenteringSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.EngineTorqueCurve)) return true;
		if (!CVF_Constants.IsFallbackArray(data.GearboxRatios)) return true;
		if (!CVF_Constants.IsFallbackArray(data.GearSpeedTargetsKmh)) return true;
		return false;
	}

	private static CVF_GeneratedVehicleConfig BuildGeneratedVehicle(string className, CVF_ResolvedVehicleConfig resolved)
	{
		CVF_GeneratedVehicleConfig data = new CVF_GeneratedVehicleConfig();
		data.ClassName = className;
		if (!g_Game.ConfigGetBaseName("CfgVehicles " + className, data.ParentClass) || !CVF_SharedUtils.IsSafeIdentifier(data.ParentClass) || data.ParentClass == className)
		{
			CVF_Logger.Error("Could not determine a safe parent class for " + className);
			return null;
		}
		data.MaxSteeringAngle = resolved.MaxSteeringAngle;
		data.EngineRPMIdle = resolved.EngineRPMIdle;
		data.EngineRPMMin = resolved.EngineRPMMin;
		data.EngineRPMClutch = resolved.EngineRPMClutch;
		data.EngineRPMRedline = resolved.EngineRPMRedline;
		data.GearboxType = resolved.GearboxType;
		data.GearboxReverse = resolved.GearboxReverse;
		CVF_SharedUtils.CopyFloatArray(resolved.SteeringIncreaseSpeed, data.SteeringIncreaseSpeed);
		CVF_SharedUtils.CopyFloatArray(resolved.SteeringDecreaseSpeed, data.SteeringDecreaseSpeed);
		CVF_SharedUtils.CopyFloatArray(resolved.SteeringCenteringSpeed, data.SteeringCenteringSpeed);
		CVF_SharedUtils.CopyFloatArray(resolved.EngineTorqueCurve, data.EngineTorqueCurve);

		ref array<float> effectiveRatios = BuildEffectiveGearRatios(className, resolved);
		if (!effectiveRatios)
			return null;
		CVF_SharedUtils.CopyFloatArray(effectiveRatios, data.GearboxRatios);
		return data;
	}

	private static bool CaptureRequiredAddons(CVF_GeneratedPackage package)
	{
		if (!g_Game || !package || !package.RequiredAddons)
			return false;

		ref map<string, bool> known = new map<string, bool>;
		int count = g_Game.ConfigGetChildrenCount("CfgPatches");
		for (int i = 0; i < count; i++)
		{
			string addonName;
			g_Game.ConfigGetChildName("CfgPatches", i, addonName);
			if (!CVF_ConfigRenderer.IsLoadedAddonClass(addonName))
				continue;
			if (addonName == "CustomVehicleFramework_GeneratedOverrides" || addonName == "CustomVehicleFramework_Overrides" || addonName == "CustomVehicleFramework_Overrides_Deprecated")
				continue;
			if (!CVF_SharedUtils.IsSafeAddonName(addonName))
			{
				CVF_Logger.Error("Unsafe CfgPatches class name prevents deterministic override ordering: " + addonName);
				return false;
			}

			bool ignored;
			if (!known.Find(addonName, ignored))
			{
				known.Set(addonName, true);
				package.RequiredAddons.Insert(addonName);
			}
		}

		package.RequiredAddons.Sort();
		return package.RequiredAddons.Count() > 0;
	}

	private static bool HasConfigOverrides(CVF_GeneratedVehicleConfig data)
	{
		if (!data) return false;
		if (!CVF_Constants.IsFallback(data.MaxSteeringAngle)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMIdle)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMMin)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMClutch)) return true;
		if (!CVF_Constants.IsFallback(data.EngineRPMRedline)) return true;
		if (data.GearboxType != "") return true;
		if (!CVF_Constants.IsFallback(data.GearboxReverse)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringIncreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringDecreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.SteeringCenteringSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(data.EngineTorqueCurve)) return true;
		if (!CVF_Constants.IsFallbackArray(data.GearboxRatios)) return true;
		return false;
	}

	private static ref array<float> BuildEffectiveGearRatios(string className, CVF_ResolvedVehicleConfig data)
	{
		ref array<float> ratios = new array<float>;
		bool hasExplicitRatios = !CVF_Constants.IsFallbackArray(data.GearboxRatios);
		bool hasSpeedTargets = !CVF_Constants.IsFallbackArray(data.GearSpeedTargetsKmh);

		if (!hasSpeedTargets)
		{
			if (hasExplicitRatios)
				CVF_SharedUtils.CopyFloatArray(data.GearboxRatios, ratios);
			return ratios;
		}

		if (hasExplicitRatios)
			CVF_SharedUtils.CopyFloatArray(data.GearboxRatios, ratios);
		else
		{
			CVF_SharedUtils.CopyFloatArray(data.GearSpeedBaseRatios, ratios);
			if (ratios.Count() == 0 && g_Game)
			{
				string ratiosPath = "CfgVehicles " + className + " SimulationModule Gearbox ratios";
				if (g_Game.ConfigIsExisting(ratiosPath))
					g_Game.ConfigGetFloatArray(ratiosPath, ratios);
			}
		}

		if (ratios.Count() == 0 || !data.GearSpeedReferenceKmh || data.GearSpeedReferenceKmh.Count() < data.GearSpeedTargetsKmh.Count() || ratios.Count() < data.GearSpeedTargetsKmh.Count())
		{
			CVF_Logger.Error("Gear speed targets for " + className + " need matching GearSpeedReferenceKmh values and enough GearSpeedBaseRatios or GearboxRatios.");
			return null;
		}

		ref array<float> scaled = new array<float>;
		CVF_SharedUtils.CopyFloatArray(ratios, scaled);
		for (int gearIndex = 0; gearIndex < data.GearSpeedTargetsKmh.Count(); gearIndex++)
		{
			float targetKmh = data.GearSpeedTargetsKmh.Get(gearIndex);
			float referenceKmh = data.GearSpeedReferenceKmh.Get(gearIndex);
			if (!CVF_SharedUtils.IsFiniteInRange(targetKmh, 0.01, 10000.0) || !CVF_SharedUtils.IsFiniteInRange(referenceKmh, 0.01, 10000.0))
			{
				CVF_Logger.Error("Invalid gear speed reference or target for " + className + " at index " + gearIndex.ToString());
				return null;
			}

			float baseRatio = ratios.Get(gearIndex);
			scaled.Set(gearIndex, baseRatio * (referenceKmh / targetKmh));
		}

		return scaled;
	}
}

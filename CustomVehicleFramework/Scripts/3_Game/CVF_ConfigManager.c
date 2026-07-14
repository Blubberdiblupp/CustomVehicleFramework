class CVF_ConfigManager
{
	private bool m_IsLoaded = false;
	private bool m_HasLoadError = false;
	private bool m_LastGenerationSucceeded = false;
	private bool m_HasSourceSnapshot = false;
	private int m_SourceHash = 0;
	private int m_SourceChars = 0;
	ref CVF_ConfigRoot m_Config;
	private ref map<string, ref CVF_VehicleOverride> m_OverrideMap;
	private ref map<string, ref CVF_ResolvedVehicleConfig> m_ResolvedMap;

	void CVF_ConfigManager()
	{
		m_Config = new CVF_ConfigRoot();
		m_OverrideMap = new map<string, ref CVF_VehicleOverride>;
		m_ResolvedMap = new map<string, ref CVF_ResolvedVehicleConfig>;
	}

	void LoadConfig()
	{
		if (m_IsLoaded)
			return;

		if (!g_Game || !g_Game.IsServer())
			return;

		if (!CVF_FileIO.EnsureDirectory(CVF_Constants.CONFIG_DIR))
		{
			m_HasLoadError = true;
			m_IsLoaded = true;
			CVF_Logger.Error("Could not create the CVF profile directory.");
			return;
		}

		bool created = false;
		if (FileExist(CVF_Constants.CONFIG_FILE))
		{
			CVF_ConfigRoot loadedConfig = new CVF_ConfigRoot();
			string loadError;
			if (!JsonFileLoader<CVF_ConfigRoot>.LoadFile(CVF_Constants.CONFIG_FILE, loadedConfig, loadError))
			{
				RefreshSourceSnapshot();
				m_HasLoadError = true;
				m_IsLoaded = true;
				CVF_Logger.Error("Could not read vehicles.json. The file was not changed and the previous in-memory config remains active: " + loadError);
				return;
			}

			if (loadedConfig.Version > CVF_Constants.CONFIG_VERSION)
			{
				RefreshSourceSnapshot();
				m_HasLoadError = true;
				m_IsLoaded = true;
				CVF_Logger.Error("vehicles.json was written for a newer CVF version. The file was not changed.");
				return;
			}
			m_Config = loadedConfig;
		}
		else
		{
			created = true;
			m_Config = new CVF_ConfigRoot();
			if (!SaveConfigFile())
			{
				m_HasLoadError = true;
				m_IsLoaded = true;
				return;
			}
		}

		int overridesBeforeValidation = 0;
		if (m_Config && m_Config.VehicleOverrides)
			overridesBeforeValidation = m_Config.VehicleOverrides.Count();

		bool changed = EnsureConfigValid();
		int cleanupRemoved = 0;
		if (m_Config && m_Config.VehicleOverrides && overridesBeforeValidation > m_Config.VehicleOverrides.Count())
			cleanupRemoved = overridesBeforeValidation - m_Config.VehicleOverrides.Count();
		BuildOverrideMap();

		if (m_Config.AutoExtendVehicleOverrides)
		{
			if (ScanVehiclesAndExtendConfig())
				changed = true;
		}

		BuildResolvedConfigCache();

		if (changed && !SaveConfigFile())
		{
			m_HasLoadError = true;
			m_IsLoaded = true;
			return;
		}
		if (cleanupRemoved > 0)
			CVF_Logger.Log("Config cleanup removed " + cleanupRemoved.ToString() + " unchanged, invalid, or duplicate vehicle entries. Remaining=" + m_Config.VehicleOverrides.Count().ToString());

		m_HasLoadError = false;
		m_IsLoaded = true;
		RefreshSourceSnapshot();
		m_LastGenerationSucceeded = CVF_ConfigGenerator.GenerateNextBootConfig(this);

		if (created)
			CVF_Logger.Log("Created default config: " + CVF_Constants.CONFIG_FILE);
		else
			CVF_Logger.Log("Loaded config: " + CVF_Constants.CONFIG_FILE);

		CVF_Logger.Log("VehicleOverrides=" + m_Config.VehicleOverrides.Count().ToString() + " AutoExtend=" + m_Config.AutoExtendVehicleOverrides.ToString());
	}

	private bool EnsureConfigValid()
	{
		bool changed = false;

		if (!m_Config)
		{
			m_Config = new CVF_ConfigRoot();
			return true;
		}

		if (m_Config.Version < CVF_Constants.CONFIG_VERSION)
		{
			m_Config.Version = CVF_Constants.CONFIG_VERSION;
			changed = true;
		}

		if (!CVF_SharedUtils.IsSafeToken(m_Config.ServerId))
		{
			m_Config.ServerId = GenerateServerId();
			changed = true;
			CVF_Logger.Warning("Generated a new persistent ServerId. Do not copy this value to another server.");
		}

		if (!m_Config.GlobalSettings)
		{
			m_Config.GlobalSettings = new CVF_GlobalSettings();
			changed = true;
		}

		EnsureGlobalArrays(m_Config.GlobalSettings);

		if (!m_Config.VehicleOverrides)
		{
			m_Config.VehicleOverrides = new array<ref CVF_VehicleOverride>;
			changed = true;
		}

		for (int i = 0; i < m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = m_Config.VehicleOverrides.Get(i);
			if (!overrideData)
			{
				m_Config.VehicleOverrides.Remove(i);
				i--;
				changed = true;
				continue;
			}

			EnsureOverrideArrays(overrideData);
		}

		if (CompactVehicleOverrides())
			changed = true;

		return changed;
	}

	private string GenerateServerId()
	{
		int year, month, day;
		int hour, minute, second;
		GetYearMonthDay(year, month, day);
		GetHourMinuteSecond(hour, minute, second);
		return "cvf-" + year.ToString() + month.ToString() + day.ToString() + "-" + hour.ToString() + minute.ToString() + second.ToString() + "-" + Math.RandomInt(100000, 999999).ToString();
	}

	private void EnsureGlobalArrays(CVF_GlobalSettings data)
	{
		if (!data.SteeringIncreaseSpeed)
			data.SteeringIncreaseSpeed = new array<float>;
		if (!data.SteeringDecreaseSpeed)
			data.SteeringDecreaseSpeed = new array<float>;
		if (!data.SteeringCenteringSpeed)
			data.SteeringCenteringSpeed = new array<float>;
		if (!data.EngineTorqueCurve)
			data.EngineTorqueCurve = new array<float>;
		if (!data.GearboxRatios)
			data.GearboxRatios = new array<float>;
		if (!data.GearSpeedReferenceKmh)
			data.GearSpeedReferenceKmh = new array<float>;
		if (!data.GearSpeedTargetsKmh)
			data.GearSpeedTargetsKmh = new array<float>;
	}

	private void EnsureOverrideArrays(CVF_VehicleOverride data)
	{
		if (!data.SteeringIncreaseSpeed)
			data.SteeringIncreaseSpeed = new array<float>;
		if (!data.SteeringDecreaseSpeed)
			data.SteeringDecreaseSpeed = new array<float>;
		if (!data.SteeringCenteringSpeed)
			data.SteeringCenteringSpeed = new array<float>;
		if (!data.EngineTorqueCurve)
			data.EngineTorqueCurve = new array<float>;
		if (!data.GearboxRatios)
			data.GearboxRatios = new array<float>;
		if (!data.GearSpeedBaseRatios)
			data.GearSpeedBaseRatios = new array<float>;
		if (!data.GearSpeedReferenceKmh)
			data.GearSpeedReferenceKmh = new array<float>;
		if (!data.GearSpeedTargetsKmh)
			data.GearSpeedTargetsKmh = new array<float>;
	}

	private bool CompactVehicleOverrides()
	{
		if (!m_Config || !m_Config.VehicleOverrides)
			return false;

		bool dropNoOps = !m_Config.AutoExtendVehicleOverrides;
		bool changed = false;
		ref array<ref CVF_VehicleOverride> compacted = new array<ref CVF_VehicleOverride>;
		ref map<string, int> known = new map<string, int>;

		for (int i = 0; i < m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = m_Config.VehicleOverrides.Get(i);
			if (!overrideData || overrideData.ClassName == "")
			{
				changed = true;
				continue;
			}
			if (dropNoOps && !overrideData.HasAnyOverrides())
			{
				changed = true;
				continue;
			}

			string classKey = NormalizeClassKey(overrideData.ClassName);
			int existingIndex = -1;
			if (known.Find(classKey, existingIndex))
			{
				CVF_VehicleOverride existing = compacted.Get(existingIndex);
				if (!existing.HasAnyOverrides() && overrideData.HasAnyOverrides())
					compacted.Set(existingIndex, overrideData);

				changed = true;
				continue;
			}

			known.Set(classKey, compacted.Count());
			compacted.Insert(overrideData);
		}

		if (!changed && compacted.Count() == m_Config.VehicleOverrides.Count())
			return false;

		m_Config.VehicleOverrides = compacted;
		return true;
	}

	private bool ScanVehiclesAndExtendConfig()
	{
		if (!g_Game || !g_Game.IsServer())
			return false;

		int total = g_Game.ConfigGetChildrenCount("CfgVehicles");
		int added = 0;
		int capturedBaselines = 0;
		int canonicalized = 0;

		for (int i = 0; i < total; i++)
		{
			string className;
			g_Game.ConfigGetChildName("CfgVehicles", i, className);

			if (!CVF_ConfigRenderer.IsSupportedVehicleClass(className))
				continue;

			CVF_VehicleOverride existing;
			string classKey = NormalizeClassKey(className);
			if (m_OverrideMap.Find(classKey, existing))
			{
				if (existing.ClassName != className)
				{
					existing.ClassName = className;
					canonicalized++;
				}
				if (CaptureGearboxBaseline(className, existing))
					capturedBaselines++;
				continue;
			}

			CVF_VehicleOverride newOverride = new CVF_VehicleOverride(className);
			if (CaptureGearboxBaseline(className, newOverride))
				capturedBaselines++;
			m_Config.VehicleOverrides.Insert(newOverride);
			m_OverrideMap.Set(classKey, newOverride);
			added++;
		}

		if (added > 0 || capturedBaselines > 0 || canonicalized > 0)
		{
			CVF_Logger.Log("Auto-extend added " + added.ToString() + " vehicle classes, captured " + capturedBaselines.ToString() + " gearbox baselines, and corrected " + canonicalized.ToString() + " class names.");
			return true;
		}

		CVF_Logger.Log("Auto-extend found no new vehicle classes.");
		return false;
	}

	private bool CaptureGearboxBaseline(string className, CVF_VehicleOverride data)
	{
		if (!g_Game || !data || !CVF_Constants.IsFallbackArray(data.GearSpeedBaseRatios))
			return false;

		ref array<float> detected = new array<float>;
		string ratiosPath = "CfgVehicles " + className + " SimulationModule Gearbox ratios";
		if (!g_Game.ConfigIsExisting(ratiosPath))
			return false;

		g_Game.ConfigGetFloatArray(ratiosPath, detected);
		if (!detected || detected.Count() == 0)
			return false;

		CVF_SharedUtils.CopyFloatArray(detected, data.GearSpeedBaseRatios);
		return true;
	}

	private void BuildOverrideMap()
	{
		m_OverrideMap.Clear();

		if (!m_Config || !m_Config.VehicleOverrides)
			return;

		for (int i = 0; i < m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = m_Config.VehicleOverrides.Get(i);
			if (overrideData && overrideData.ClassName != "")
				m_OverrideMap.Set(NormalizeClassKey(overrideData.ClassName), overrideData);
		}
	}

	private void BuildResolvedConfigCache()
	{
		m_ResolvedMap.Clear();

		if (!m_Config || !m_Config.VehicleOverrides)
			return;

		for (int i = 0; i < m_Config.VehicleOverrides.Count(); i++)
		{
			CVF_VehicleOverride overrideData = m_Config.VehicleOverrides.Get(i);
			if (!overrideData || overrideData.ClassName == "")
				continue;

			CVF_ResolvedVehicleConfig resolved = ResolveConfig(overrideData);
			m_ResolvedMap.Set(NormalizeClassKey(overrideData.ClassName), resolved);
		}
	}

	private CVF_ResolvedVehicleConfig ResolveConfig(CVF_VehicleOverride overrideData)
	{
		CVF_GlobalSettings global = m_Config.GlobalSettings;
		CVF_ResolvedVehicleConfig resolved = new CVF_ResolvedVehicleConfig();

		resolved.MaxSpeedKmh = ResolveFloat(overrideData.MaxSpeedKmh, global.MaxSpeedKmh);
		resolved.ThrottleMultiplier = ResolveFloat(overrideData.ThrottleMultiplier, global.ThrottleMultiplier);
		resolved.ExtraDriveForce = ResolveFloat(overrideData.ExtraDriveForce, global.ExtraDriveForce);
		resolved.SteeringMultiplier = ResolveFloat(overrideData.SteeringMultiplier, global.SteeringMultiplier);
		resolved.SteeringYawAssist = ResolveFloat(overrideData.SteeringYawAssist, global.SteeringYawAssist);
		resolved.BrakeMultiplier = ResolveFloat(overrideData.BrakeMultiplier, global.BrakeMultiplier);
		resolved.ExtraBrakeForce = ResolveFloat(overrideData.ExtraBrakeForce, global.ExtraBrakeForce);
		resolved.DragResistance = ResolveFloat(overrideData.DragResistance, global.DragResistance);
		resolved.StabilityAssist = ResolveFloat(overrideData.StabilityAssist, global.StabilityAssist);
		resolved.MaxSteeringAngle = ResolveFloat(overrideData.MaxSteeringAngle, global.MaxSteeringAngle);
		resolved.EngineRPMIdle = ResolveFloat(overrideData.EngineRPMIdle, global.EngineRPMIdle);
		resolved.EngineRPMMin = ResolveFloat(overrideData.EngineRPMMin, global.EngineRPMMin);
		resolved.EngineRPMClutch = ResolveFloat(overrideData.EngineRPMClutch, global.EngineRPMClutch);
		resolved.EngineRPMRedline = ResolveFloat(overrideData.EngineRPMRedline, global.EngineRPMRedline);
		resolved.GearboxType = ResolveString(overrideData.GearboxType, global.GearboxType);
		resolved.GearboxReverse = ResolveFloat(overrideData.GearboxReverse, global.GearboxReverse);

		CopyResolvedArray(resolved.SteeringIncreaseSpeed, overrideData.SteeringIncreaseSpeed, global.SteeringIncreaseSpeed);
		CopyResolvedArray(resolved.SteeringDecreaseSpeed, overrideData.SteeringDecreaseSpeed, global.SteeringDecreaseSpeed);
		CopyResolvedArray(resolved.SteeringCenteringSpeed, overrideData.SteeringCenteringSpeed, global.SteeringCenteringSpeed);
		CopyResolvedArray(resolved.EngineTorqueCurve, overrideData.EngineTorqueCurve, global.EngineTorqueCurve);
		CopyResolvedArray(resolved.GearboxRatios, overrideData.GearboxRatios, global.GearboxRatios);
		CVF_SharedUtils.CopyFloatArray(overrideData.GearSpeedBaseRatios, resolved.GearSpeedBaseRatios);
		CopyResolvedArray(resolved.GearSpeedReferenceKmh, overrideData.GearSpeedReferenceKmh, global.GearSpeedReferenceKmh);
		CopyResolvedArray(resolved.GearSpeedTargetsKmh, overrideData.GearSpeedTargetsKmh, global.GearSpeedTargetsKmh);

		return resolved;
	}

	private float ResolveFloat(float overrideValue, float globalValue)
	{
		if (CVF_Constants.IsFallback(overrideValue))
			return globalValue;

		return overrideValue;
	}

	private string ResolveString(string overrideValue, string globalValue)
	{
		if (overrideValue == "")
			return globalValue;

		return overrideValue;
	}

	private void CopyResolvedArray(array<float> target, array<float> overrideValues, array<float> globalValues)
	{
		target.Clear();

		array<float> source = overrideValues;
		if (CVF_Constants.IsFallbackArray(source))
			source = globalValues;

		if (!source)
			return;

		for (int i = 0; i < source.Count(); i++)
			target.Insert(source.Get(i));
	}

	bool GetResolvedConfigFor(string className, out CVF_ResolvedVehicleConfig resolved)
	{
		LoadConfig();

		if (!m_Config || !m_Config.EnableVehicleOverrides)
		{
			resolved = null;
			return false;
		}

		string classKey = NormalizeClassKey(className);
		if (m_ResolvedMap.Find(classKey, resolved))
			return true;

		if (!m_Config.GlobalSettings || !m_Config.GlobalSettings.HasAnyOverrides() || !CVF_ConfigRenderer.IsSupportedVehicleClass(className))
		{
			resolved = null;
			return false;
		}

		CVF_VehicleOverride globalFallback = new CVF_VehicleOverride(className);
		resolved = ResolveConfig(globalFallback);
		m_ResolvedMap.Set(classKey, resolved);
		return true;

	}

	bool IsFrameworkEnabled()
	{
		LoadConfig();

		if (!m_Config)
			return false;

		return m_Config.EnableVehicleOverrides;
	}

	bool HasLoadError()
	{
		return m_HasLoadError;
	}

	bool WasLastGenerationSuccessful()
	{
		return m_LastGenerationSucceeded;
	}

	void ReloadConfigIfChanged()
	{
		if (!g_Game || !g_Game.IsServer() || !FileExist(CVF_Constants.CONFIG_FILE))
			return;

		string source;
		if (!CVF_FileIO.ReadAllText(CVF_Constants.CONFIG_FILE, source))
			return;

		int sourceHash = CVF_SharedUtils.HashText(source);
		int sourceChars = source.Length();
		if (!m_HasSourceSnapshot)
		{
			m_SourceHash = sourceHash;
			m_SourceChars = sourceChars;
			m_HasSourceSnapshot = true;
			return;
		}

		if (sourceHash == m_SourceHash && sourceChars == m_SourceChars)
			return;

		// Remember invalid edits too, so the watcher retries only after the admin saves again.
		m_SourceHash = sourceHash;
		m_SourceChars = sourceChars;
		CVF_Logger.Log("Detected a vehicles.json change. Preparing the bootstrap override config for the next server start.");

		m_IsLoaded = false;
		m_HasLoadError = false;
		m_LastGenerationSucceeded = false;
		LoadConfig();
		if (m_HasLoadError)
		{
			CVF_Logger.Error("The changed vehicles.json is invalid. The previous in-memory settings and generated config.cpp were kept.");
			return;
		}
		if (!m_LastGenerationSucceeded)
		{
			CVF_Logger.Error("The changed vehicles.json loaded, but config.cpp generation failed. The previous generated config.cpp was kept.");
			return;
		}

		CVF_Logger.Warning("The changed vehicles.json was converted to the generated bootstrap config.cpp. A complete server restart is required before native values change.");
	}

	void ReloadConfigForNewSpawns()
	{
		if (!g_Game || !g_Game.IsServer())
			return;

		m_IsLoaded = false;
		m_HasLoadError = false;
		m_LastGenerationSucceeded = false;
		LoadConfig();
		if (m_HasLoadError)
		{
			CVF_Logger.Error("Config reload failed. No next-boot override was generated.");
			return;
		}
		if (!m_LastGenerationSucceeded)
		{
			CVF_Logger.Error("Config reload succeeded, but the next-boot override could not be generated.");
			return;
		}
		CVF_Logger.Warning("Config reloaded and native overrides generated for the next server boot. Existing native vehicle config remains active until restart.");
	}

	void SaveConfig()
	{
		if (!m_Config || m_HasLoadError)
		{
			CVF_Logger.Error("Config save refused because the current vehicles.json did not load cleanly.");
			return;
		}

		if (!SaveConfigFile())
			return;
		BuildOverrideMap();
		BuildResolvedConfigCache();
		m_LastGenerationSucceeded = CVF_ConfigGenerator.GenerateNextBootConfig(this);
		if (m_LastGenerationSucceeded)
			CVF_Logger.Log("Config saved and next-boot override generated.");
		else
			CVF_Logger.Error("Config was saved, but the next-boot override could not be generated.");
	}

	private bool SaveConfigFile()
	{
		string saveError;
		if (JsonFileLoader<CVF_ConfigRoot>.SaveFile(CVF_Constants.CONFIG_FILE, m_Config, saveError))
		{
			RefreshSourceSnapshot();
			return true;
		}

		CVF_Logger.Error("Could not save vehicles.json: " + saveError);
		return false;
	}

	private void RefreshSourceSnapshot()
	{
		string source;
		if (!CVF_FileIO.ReadAllText(CVF_Constants.CONFIG_FILE, source))
			return;

		m_SourceHash = CVF_SharedUtils.HashText(source);
		m_SourceChars = source.Length();
		m_HasSourceSnapshot = true;
	}

	private string NormalizeClassKey(string className)
	{
		string key = className;
		key.ToLower();
		return key;
	}
}

ref CVF_ConfigManager g_CVF_ConfigManager;

CVF_ConfigManager GetCVFConfigManager()
{
	if (!g_CVF_ConfigManager)
		g_CVF_ConfigManager = new CVF_ConfigManager();

	return g_CVF_ConfigManager;
}

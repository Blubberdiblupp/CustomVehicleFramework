class CVF_Constants
{
	static const int CONFIG_VERSION = 3;
	static const int SYNC_PROTOCOL_VERSION = 5;
	static const string CONFIG_DIR = "$profile:CustomVehicleFramework";
	static const string CONFIG_FILE = "$profile:CustomVehicleFramework\\vehicles.json";

	static const string STAGING_CONFIG_FILE = "$profile:CustomVehicleFramework\\generated.next.cpp";
	static const string GENERATED_MOD_DIR = "$profile:CustomVehicleFramework_Generated";
	static const string GENERATED_CONFIG_FILE = "$profile:CustomVehicleFramework_Generated\\config.cpp";
	static const string GENERATED_PREFIX_FILE = "$profile:CustomVehicleFramework_Generated\\$PBOPREFIX$";
	static const string GENERATED_INFO_FILE = "$profile:CustomVehicleFramework_Generated\\README.txt";
	static const string GENERATED_PBO_PREFIX = "profiles\\CustomVehicleFramework_Generated";

	static const int RPC_SYNC_HELLO = 4406601;
	static const int RPC_CLIENT_STATUS = 4406603;
	static const int RPC_SYNC_NOTICE = 4406604;
	static const int SYNC_TIMEOUT_MS = 120000;
	static const int CONFIG_WATCH_INTERVAL_MS = 15000;
	static const int MAX_GENERATED_PACKAGE_CHARS = 12000000;

	static const string CONFIG_PROBE_PATH = "CfgCustomVehicleFramework GeneratedOverrideProbe";
	static const float FALLBACK_FLOAT = -1.0;

	static bool IsFallback(float value)
	{
		return value < 0.0;
	}

	static bool IsFallbackArray(array<float> values)
	{
		if (!values || values.Count() == 0)
			return true;

		for (int i = 0; i < values.Count(); i++)
		{
			if (!IsFallback(values.Get(i)))
				return false;
		}

		return true;
	}
}

enum CVF_ClientSyncStatus
{
	CVF_SYNC_NONE = 0,
	CVF_SYNC_READY = 1,
	CVF_SYNC_ERROR = 2
}

enum CVF_SyncNoticeType
{
	CVF_NOTICE_SERVER_RESTART_REQUIRED = 1,
	CVF_NOTICE_SYNC_ERROR = 2
}

class CVF_SharedUtils
{
	static int HashText(string value)
	{
		int hash = value.Hash();
		if (hash == 0)
			return 1;
		return hash;
	}

	static bool IsSafeIdentifier(string value)
	{
		if (value == "" || value.Length() > 128)
			return false;

		string first = value.Substring(0, 1);
		if ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_".IndexOf(first) == -1)
			return false;

		for (int i = 1; i < value.Length(); i++)
		{
			string character = value.Substring(i, 1);
			if ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789".IndexOf(character) == -1)
				return false;
		}

		return true;
	}

	static bool IsSafeAddonName(string value)
	{
		if (value == "" || value.Length() > 128)
			return false;

		for (int i = 0; i < value.Length(); i++)
		{
			string character = value.Substring(i, 1);
			if ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789".IndexOf(character) == -1)
				return false;
		}

		return true;
	}

	static bool IsSafeToken(string value)
	{
		if (value == "" || value.Length() > 96)
			return false;

		for (int i = 0; i < value.Length(); i++)
		{
			string character = value.Substring(i, 1);
			if ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_".IndexOf(character) == -1)
				return false;
		}

		return true;
	}

	static bool IsFiniteInRange(float value, float minimum, float maximum)
	{
		if (value != value)
			return false;
		return value >= minimum && value <= maximum;
	}

	static void CopyFloatArray(array<float> source, array<float> target)
	{
		if (!target)
			return;

		target.Clear();
		if (!source)
			return;

		for (int i = 0; i < source.Count(); i++)
			target.Insert(source.Get(i));
	}
}

class CVF_Logger
{
	static void Log(string message)
	{
		Print("[CVF] " + message);
	}

	static void Warning(string message)
	{
		Print("[CVF WARNING] " + message);
	}

	static void Error(string message)
	{
		Print("[CVF ERROR] " + message);
	}
}

class CVF_GlobalSettings : Managed
{
	float MaxSpeedKmh;
	float ThrottleMultiplier;
	float ExtraDriveForce;
	float SteeringMultiplier;
	float SteeringYawAssist;
	float BrakeMultiplier;
	float ExtraBrakeForce;
	float DragResistance;
	float StabilityAssist;
	float MaxSteeringAngle;
	ref array<float> SteeringIncreaseSpeed;
	ref array<float> SteeringDecreaseSpeed;
	ref array<float> SteeringCenteringSpeed;
	float EngineRPMIdle;
	float EngineRPMMin;
	float EngineRPMClutch;
	float EngineRPMRedline;
	ref array<float> EngineTorqueCurve;
	string GearboxType;
	float GearboxReverse;
	ref array<float> GearboxRatios;
	ref array<float> GearSpeedReferenceKmh;
	ref array<float> GearSpeedTargetsKmh;

	void CVF_GlobalSettings()
	{
		SetDefaults();
	}

	void SetDefaults()
	{
		MaxSpeedKmh = 0.0;
		ThrottleMultiplier = 1.0;
		ExtraDriveForce = 0.0;
		SteeringMultiplier = 1.0;
		SteeringYawAssist = 0.0;
		BrakeMultiplier = 1.0;
		ExtraBrakeForce = 0.0;
		DragResistance = 0.0;
		StabilityAssist = 0.0;

		MaxSteeringAngle = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMIdle = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMMin = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMClutch = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMRedline = CVF_Constants.FALLBACK_FLOAT;

		SteeringIncreaseSpeed = new array<float>;
		SteeringDecreaseSpeed = new array<float>;
		SteeringCenteringSpeed = new array<float>;
		EngineTorqueCurve = new array<float>;
		GearboxType = "";
		GearboxReverse = CVF_Constants.FALLBACK_FLOAT;
		GearboxRatios = new array<float>;
		GearSpeedReferenceKmh = new array<float>;
		GearSpeedTargetsKmh = new array<float>;
	}

	bool HasRuntimeOverrides()
	{
		if (MaxSpeedKmh != 0.0) return true;
		if (ThrottleMultiplier != 1.0) return true;
		if (ExtraDriveForce != 0.0) return true;
		if (SteeringMultiplier != 1.0) return true;
		if (SteeringYawAssist != 0.0) return true;
		if (BrakeMultiplier != 1.0) return true;
		if (ExtraBrakeForce != 0.0) return true;
		if (DragResistance != 0.0) return true;
		if (StabilityAssist != 0.0) return true;
		return false;
	}

	bool HasNativeOverrides()
	{
		if (!CVF_Constants.IsFallback(MaxSteeringAngle)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMIdle)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMMin)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMClutch)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMRedline)) return true;
		if (GearboxType != "") return true;
		if (!CVF_Constants.IsFallback(GearboxReverse)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringIncreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringDecreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringCenteringSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(EngineTorqueCurve)) return true;
		if (!CVF_Constants.IsFallbackArray(GearboxRatios)) return true;
		if (!CVF_Constants.IsFallbackArray(GearSpeedTargetsKmh)) return true;
		return false;
	}

	bool HasAnyOverrides()
	{
		return HasRuntimeOverrides() || HasNativeOverrides();
	}
}

class CVF_VehicleOverride : Managed
{
	string ClassName;

	float MaxSpeedKmh;
	float ThrottleMultiplier;
	float ExtraDriveForce;
	float SteeringMultiplier;
	float SteeringYawAssist;
	float BrakeMultiplier;
	float ExtraBrakeForce;
	float DragResistance;
	float StabilityAssist;
	float MaxSteeringAngle;
	ref array<float> SteeringIncreaseSpeed;
	ref array<float> SteeringDecreaseSpeed;
	ref array<float> SteeringCenteringSpeed;
	float EngineRPMIdle;
	float EngineRPMMin;
	float EngineRPMClutch;
	float EngineRPMRedline;
	ref array<float> EngineTorqueCurve;
	string GearboxType;
	float GearboxReverse;
	ref array<float> GearboxRatios;
	ref array<float> GearSpeedBaseRatios;
	ref array<float> GearSpeedReferenceKmh;
	ref array<float> GearSpeedTargetsKmh;

	void CVF_VehicleOverride(string name = "")
	{
		ClassName = name;
		SetToFallback();
	}

	void SetToFallback()
	{
		MaxSpeedKmh = CVF_Constants.FALLBACK_FLOAT;
		ThrottleMultiplier = CVF_Constants.FALLBACK_FLOAT;
		ExtraDriveForce = CVF_Constants.FALLBACK_FLOAT;
		SteeringMultiplier = CVF_Constants.FALLBACK_FLOAT;
		SteeringYawAssist = CVF_Constants.FALLBACK_FLOAT;
		BrakeMultiplier = CVF_Constants.FALLBACK_FLOAT;
		ExtraBrakeForce = CVF_Constants.FALLBACK_FLOAT;
		DragResistance = CVF_Constants.FALLBACK_FLOAT;
		StabilityAssist = CVF_Constants.FALLBACK_FLOAT;
		MaxSteeringAngle = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMIdle = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMMin = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMClutch = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMRedline = CVF_Constants.FALLBACK_FLOAT;
		GearboxType = "";
		GearboxReverse = CVF_Constants.FALLBACK_FLOAT;

		SteeringIncreaseSpeed = new array<float>;
		SteeringDecreaseSpeed = new array<float>;
		SteeringCenteringSpeed = new array<float>;
		EngineTorqueCurve = new array<float>;
		GearboxRatios = new array<float>;
		GearSpeedBaseRatios = new array<float>;
		GearSpeedReferenceKmh = new array<float>;
		GearSpeedTargetsKmh = new array<float>;
	}

	bool HasAnyOverrides()
	{
		if (!CVF_Constants.IsFallback(MaxSpeedKmh)) return true;
		if (!CVF_Constants.IsFallback(ThrottleMultiplier)) return true;
		if (!CVF_Constants.IsFallback(ExtraDriveForce)) return true;
		if (!CVF_Constants.IsFallback(SteeringMultiplier)) return true;
		if (!CVF_Constants.IsFallback(SteeringYawAssist)) return true;
		if (!CVF_Constants.IsFallback(BrakeMultiplier)) return true;
		if (!CVF_Constants.IsFallback(ExtraBrakeForce)) return true;
		if (!CVF_Constants.IsFallback(DragResistance)) return true;
		if (!CVF_Constants.IsFallback(StabilityAssist)) return true;
		if (!CVF_Constants.IsFallback(MaxSteeringAngle)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMIdle)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMMin)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMClutch)) return true;
		if (!CVF_Constants.IsFallback(EngineRPMRedline)) return true;
		if (GearboxType != "") return true;
		if (!CVF_Constants.IsFallback(GearboxReverse)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringIncreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringDecreaseSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(SteeringCenteringSpeed)) return true;
		if (!CVF_Constants.IsFallbackArray(EngineTorqueCurve)) return true;
		if (!CVF_Constants.IsFallbackArray(GearboxRatios)) return true;
		if (!CVF_Constants.IsFallbackArray(GearSpeedReferenceKmh)) return true;
		if (!CVF_Constants.IsFallbackArray(GearSpeedTargetsKmh)) return true;
		return false;
	}
}

class CVF_ConfigRoot : Managed
{
	int Version;
	string ServerId;
	bool EnableVehicleOverrides;
	bool AutoExtendVehicleOverrides;
	ref CVF_GlobalSettings GlobalSettings;
	ref array<ref CVF_VehicleOverride> VehicleOverrides;

	void CVF_ConfigRoot()
	{
		Version = CVF_Constants.CONFIG_VERSION;
		ServerId = "";
		EnableVehicleOverrides = true;
		AutoExtendVehicleOverrides = false;
		GlobalSettings = new CVF_GlobalSettings();
		VehicleOverrides = new array<ref CVF_VehicleOverride>;
	}
}

class CVF_ResolvedVehicleConfig : Managed
{
	float MaxSpeedKmh;
	float ThrottleMultiplier;
	float ExtraDriveForce;
	float SteeringMultiplier;
	float SteeringYawAssist;
	float BrakeMultiplier;
	float ExtraBrakeForce;
	float DragResistance;
	float StabilityAssist;
	float MaxSteeringAngle;
	ref array<float> SteeringIncreaseSpeed;
	ref array<float> SteeringDecreaseSpeed;
	ref array<float> SteeringCenteringSpeed;
	float EngineRPMIdle;
	float EngineRPMMin;
	float EngineRPMClutch;
	float EngineRPMRedline;
	ref array<float> EngineTorqueCurve;
	string GearboxType;
	float GearboxReverse;
	ref array<float> GearboxRatios;
	ref array<float> GearSpeedBaseRatios;
	ref array<float> GearSpeedReferenceKmh;
	ref array<float> GearSpeedTargetsKmh;

	void CVF_ResolvedVehicleConfig()
	{
		SteeringIncreaseSpeed = new array<float>;
		SteeringDecreaseSpeed = new array<float>;
		SteeringCenteringSpeed = new array<float>;
		EngineTorqueCurve = new array<float>;
		GearboxRatios = new array<float>;
		GearSpeedBaseRatios = new array<float>;
		GearSpeedReferenceKmh = new array<float>;
		GearSpeedTargetsKmh = new array<float>;
	}

	bool HasRuntimeOverrides()
	{
		if (MaxSpeedKmh != 0.0) return true;
		if (ThrottleMultiplier != 1.0) return true;
		if (ExtraDriveForce != 0.0) return true;
		if (SteeringMultiplier != 1.0) return true;
		if (SteeringYawAssist != 0.0) return true;
		if (BrakeMultiplier != 1.0) return true;
		if (ExtraBrakeForce != 0.0) return true;
		if (DragResistance != 0.0) return true;
		if (StabilityAssist != 0.0) return true;
		return false;
	}
}

class CVF_GeneratedVehicleConfig : Managed
{
	string ClassName;
	string ParentClass;
	float MaxSteeringAngle;
	ref array<float> SteeringIncreaseSpeed;
	ref array<float> SteeringDecreaseSpeed;
	ref array<float> SteeringCenteringSpeed;
	float EngineRPMIdle;
	float EngineRPMMin;
	float EngineRPMClutch;
	float EngineRPMRedline;
	ref array<float> EngineTorqueCurve;
	string GearboxType;
	float GearboxReverse;
	ref array<float> GearboxRatios;

	void CVF_GeneratedVehicleConfig()
	{
		ClassName = "";
		ParentClass = "";
		MaxSteeringAngle = CVF_Constants.FALLBACK_FLOAT;
		SteeringIncreaseSpeed = new array<float>;
		SteeringDecreaseSpeed = new array<float>;
		SteeringCenteringSpeed = new array<float>;
		EngineRPMIdle = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMMin = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMClutch = CVF_Constants.FALLBACK_FLOAT;
		EngineRPMRedline = CVF_Constants.FALLBACK_FLOAT;
		EngineTorqueCurve = new array<float>;
		GearboxType = "";
		GearboxReverse = CVF_Constants.FALLBACK_FLOAT;
		GearboxRatios = new array<float>;
	}
}

class CVF_GeneratedPackage : Managed
{
	int ProtocolVersion;
	int ConfigVersion;
	string ServerId;
	ref array<string> RequiredAddons;
	ref array<ref CVF_GeneratedVehicleConfig> Vehicles;

	void CVF_GeneratedPackage()
	{
		ProtocolVersion = CVF_Constants.SYNC_PROTOCOL_VERSION;
		ConfigVersion = CVF_Constants.CONFIG_VERSION;
		ServerId = "";
		RequiredAddons = new array<string>;
		Vehicles = new array<ref CVF_GeneratedVehicleConfig>;
	}
}

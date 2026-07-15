class CVF_ClientCache
{
	private static string s_SessionId = "";
	private static string s_LastEndpointKey = "";

	static string GetSessionId()
	{
		if (s_SessionId != "")
			return s_SessionId;

		int year, month, day;
		int hour, minute, second;
		GetYearMonthDay(year, month, day);
		GetHourMinuteSecond(hour, minute, second);
		s_SessionId = year.ToString() + month.ToString() + day.ToString() + hour.ToString() + minute.ToString() + second.ToString() + "-" + Math.RandomInt(100000, 999999).ToString();
		return s_SessionId;
	}

	static void RememberEndpoint(string address, int port)
	{
		s_LastEndpointKey = CVF_SharedUtils.NormalizeEndpoint(address, port);
	}

	static string ResolveCurrentEndpoint()
	{
		if (s_LastEndpointKey != "")
			return s_LastEndpointKey;

		string address;
		int port;
		if (g_Game && g_Game.GetHostAddress(address, port))
			return CVF_SharedUtils.NormalizeEndpoint(address, port);
		return "";
	}

	static int PrepareCachedServer(string serverId, int payloadHash, int payloadChars, int nativeFingerprint, int nativeFingerprintChars, string endpointKey, bool serverConfirmed, out string error)
	{
		error = "";
		if (!CVF_SharedUtils.IsSafeToken(serverId) || payloadHash == 0 || payloadChars <= 0 || payloadChars > CVF_Constants.MAX_GENERATED_PACKAGE_CHARS)
		{
			if (!serverConfirmed)
				return CVF_CachePreparationResult.CVF_CACHE_MISSING;
			error = "Invalid cached server metadata.";
			return CVF_CachePreparationResult.CVF_CACHE_ERROR;
		}

		bool cacheMissing;
		if (VerifyLoadedCachedPackage(serverId, payloadHash, payloadChars, nativeFingerprint, nativeFingerprintChars, cacheMissing, error))
			return CVF_CachePreparationResult.CVF_CACHE_ALREADY_LOADED;
		if (cacheMissing)
			return CVF_CachePreparationResult.CVF_CACHE_MISSING;
		if (!cacheMissing && IsClientProbeFor(serverId, payloadHash, payloadChars))
			return CVF_CachePreparationResult.CVF_CACHE_BOOTSTRAP_FAILED;

		CVF_CacheMetadata cacheMetadata;
		string metadataError;
		if (!LoadServerMetadata(serverId, cacheMetadata, metadataError))
			return CVF_CachePreparationResult.CVF_CACHE_MISSING;
		if (cacheMetadata.ProtocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION || cacheMetadata.ServerId != serverId || cacheMetadata.PayloadHash != payloadHash || cacheMetadata.PayloadChars != payloadChars || !FileExist(GetServerConfigPath(serverId)) || !FileExist(GetServerPackagePath(serverId)))
			return CVF_CachePreparationResult.CVF_CACHE_MISSING;

		CVF_CacheMetadata activeMetadata;
		string activeError;
		if (LoadMetadata(CVF_Constants.CLIENT_ACTIVE_METADATA_FILE, activeMetadata, activeError) && activeMetadata.ServerId == serverId && activeMetadata.PayloadHash == payloadHash && activeMetadata.PayloadChars == payloadChars)
		{
			if (activeMetadata.ActivationSessionId == GetSessionId())
				return CVF_CachePreparationResult.CVF_CACHE_RESTART_REQUIRED;

			error = "The client config was active before DayZ started, but its native probe or vehicle values were not loaded. Start DayZ with -profiles=profiles -filePatching and verify that the client bootstrap PBO is installed.";
			return CVF_CachePreparationResult.CVF_CACHE_BOOTSTRAP_FAILED;
		}

		if (!ActivateServer(serverId, endpointKey, error))
			return CVF_CachePreparationResult.CVF_CACHE_ERROR;
		return CVF_CachePreparationResult.CVF_CACHE_RESTART_REQUIRED;
	}

	static bool StoreAndActivatePackage(CVF_GeneratedPackage package, string packageJson, int payloadHash, int payloadChars, string endpointKey, out string error)
	{
		error = "";
		if (!CVF_ConfigRenderer.ValidatePackage(package, true, false, error))
			return false;
		if (!CVF_FileIO.EnsureClientDirectories())
		{
			error = "Could not create the client cache directories.";
			return false;
		}

		string serverDir = GetServerDir(package.ServerId);
		if (!CVF_FileIO.EnsureDirectory(serverDir))
		{
			error = "Could not create the per-server client cache directory.";
			return false;
		}

		if (!CVF_ConfigRenderer.WritePackage(package, GetServerConfigPath(package.ServerId), payloadHash, payloadChars, true, error))
			return false;
		if (!CVF_FileIO.WriteAllText(GetServerPackagePath(package.ServerId), packageJson))
		{
			error = "Could not write the cached structured vehicle package.";
			return false;
		}

		CVF_CacheMetadata metadata = new CVF_CacheMetadata();
		metadata.ServerId = package.ServerId;
		metadata.PayloadHash = payloadHash;
		metadata.PayloadChars = payloadChars;
		metadata.EndpointKey = endpointKey;
		if (!SaveMetadata(GetServerMetadataPath(package.ServerId), metadata, error))
			return false;

		UpdateEndpoint(endpointKey, package.ServerId, payloadHash, payloadChars);
		return ActivateServer(package.ServerId, endpointKey, error);
	}

	static bool TryPrepareCachedEndpoint(string address, int port)
	{
		CVF_RuntimeConfigStore.ClearVerifiedPackage();
		RememberEndpoint(address, port);
		CVF_ClientEndpointEntry entry;
		if (!FindEndpoint(s_LastEndpointKey, entry))
			return false;

		string error;
		int result = PrepareCachedServer(entry.ServerId, entry.PayloadHash, entry.PayloadChars, 0, 0, s_LastEndpointKey, false, error);
		if (result == CVF_CachePreparationResult.CVF_CACHE_ALREADY_LOADED || result == CVF_CachePreparationResult.CVF_CACHE_MISSING)
			return false;

		if (result == CVF_CachePreparationResult.CVF_CACHE_RESTART_REQUIRED)
			CVF_ClientUI.ShowRestartRequired("The cached vehicle settings for this server are now active. Close DayZ completely, start it again, and connect once more.");
		else
			CVF_ClientUI.ShowError(error);
		return true;
	}

	static void UpdateEndpoint(string endpointKey, string serverId, int payloadHash, int payloadChars)
	{
		if (endpointKey == "" || !CVF_FileIO.EnsureClientDirectories())
			return;

		CVF_ClientEndpointIndex index;
		LoadEndpointIndex(index);
		CVF_ClientEndpointEntry found;
		for (int i = 0; i < index.Entries.Count(); i++)
		{
			CVF_ClientEndpointEntry current = index.Entries.Get(i);
			if (current && current.EndpointKey == endpointKey)
			{
				found = current;
				break;
			}
		}

		if (!found)
		{
			found = new CVF_ClientEndpointEntry();
			found.EndpointKey = endpointKey;
			index.Entries.Insert(found);
		}
		found.ServerId = serverId;
		found.PayloadHash = payloadHash;
		found.PayloadChars = payloadChars;

		string saveError;
		if (!JsonFileLoader<CVF_ClientEndpointIndex>.SaveFile(CVF_Constants.CLIENT_ENDPOINT_INDEX_FILE, index, saveError))
			CVF_Logger.Warning("Could not update the client endpoint index: " + saveError);
	}

	private static bool ActivateServer(string serverId, string endpointKey, out string error)
	{
		error = "";
		if (!CVF_FileIO.EnsureClientDirectories())
		{
			error = "Could not create the active client config directory.";
			return false;
		}

		CVF_CacheMetadata metadata;
		if (!LoadServerMetadata(serverId, metadata, error))
			return false;

		string packageJson;
		if (!CVF_FileIO.ReadAllText(GetServerPackagePath(serverId), packageJson) || packageJson.Length() != metadata.PayloadChars || CVF_SharedUtils.HashText(packageJson) != metadata.PayloadHash)
		{
			error = "The cached structured package is missing or damaged.";
			return false;
		}
		CVF_GeneratedPackage package = new CVF_GeneratedPackage();
		JsonSerializer serializer = new JsonSerializer();
		if (!serializer.ReadFromString(package, packageJson, error) || package.ServerId != serverId)
		{
			error = "The cached structured package could not be rendered.";
			return false;
		}
		if (!CVF_ConfigRenderer.WritePackage(package, GetServerConfigPath(serverId), metadata.PayloadHash, metadata.PayloadChars, true, error))
			return false;
		if (!CVF_FileIO.CopyTextFile(GetServerConfigPath(serverId), CVF_Constants.CLIENT_ACTIVE_CONFIG_FILE))
		{
			error = "Could not activate the cached client vehicle config.";
			return false;
		}
		if (!CVF_FileIO.WriteAllTextIfChanged(CVF_Constants.CLIENT_ACTIVE_PREFIX_FILE, CVF_Constants.CLIENT_GENERATED_PBO_PREFIX))
		{
			error = "Could not write the client addon prefix marker.";
			return false;
		}

		string info = "Custom Vehicle Framework active client override.\n\nDo not edit config.cpp. It was generated from a validated server package.\nDayZ must be started with -profiles=profiles -filePatching.\nThe server must allow clients using file patching.\n";
		if (!CVF_FileIO.WriteAllTextIfChanged(CVF_Constants.CLIENT_ACTIVE_INFO_FILE, info))
		{
			error = "Could not write the active client config information file.";
			return false;
		}

		metadata.EndpointKey = endpointKey;
		metadata.ActivationSessionId = GetSessionId();
		return SaveMetadata(CVF_Constants.CLIENT_ACTIVE_METADATA_FILE, metadata, error);
	}

	private static bool VerifyLoadedCachedPackage(string serverId, int payloadHash, int payloadChars, int expectedNativeFingerprint, int expectedNativeFingerprintChars, out bool cacheMissing, out string error)
	{
		cacheMissing = false;
		error = "";
		CVF_CacheMetadata metadata;
		if (!LoadServerMetadata(serverId, metadata, error) || metadata.ProtocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION || metadata.ServerId != serverId || metadata.PayloadHash != payloadHash || metadata.PayloadChars != payloadChars)
		{
			cacheMissing = true;
			return false;
		}

		string packageJson;
		if (!CVF_FileIO.ReadAllText(GetServerPackagePath(serverId), packageJson) || packageJson.Length() != payloadChars || CVF_SharedUtils.HashText(packageJson) != payloadHash)
		{
			cacheMissing = true;
			error = "The cached structured vehicle package is missing or damaged.";
			return false;
		}

		CVF_GeneratedPackage package = new CVF_GeneratedPackage();
		JsonSerializer serializer = new JsonSerializer();
		if (!serializer.ReadFromString(package, packageJson, error) || package.ServerId != serverId)
		{
			cacheMissing = true;
			error = "The cached structured vehicle package is invalid.";
			return false;
		}
		if (!CVF_ConfigRenderer.ValidatePackage(package, true, false, error))
			return false;
		if (!CVF_ConfigRenderer.VerifyLoadedProbe(package, payloadHash, payloadChars, true, error))
			return false;
		if (!CVF_ConfigRenderer.VerifyLoadedPackage(package, error))
			return false;
		int localNativeFingerprint = 0;
		int localNativeFingerprintChars = 0;
		if (expectedNativeFingerprint != 0 || expectedNativeFingerprintChars != 0)
		{
			if (!CVF_ConfigRenderer.BuildLoadedNativeFingerprint(package, localNativeFingerprint, localNativeFingerprintChars, error))
				return false;
			if (localNativeFingerprint != expectedNativeFingerprint || localNativeFingerprintChars != expectedNativeFingerprintChars)
			{
				error = "The loaded client vehicle simulation fingerprint differs from the server. Server=" + expectedNativeFingerprint.ToString() + " Client=" + localNativeFingerprint.ToString();
				return false;
			}
		}

		CVF_RuntimeConfigStore.InstallVerifiedPackage(package, payloadHash);
		CVF_Logger.Log("Client native vehicle config verified. ServerId=" + serverId + " Hash=" + payloadHash.ToString() + " NativeFingerprint=" + localNativeFingerprint.ToString());
		return true;
	}

	private static bool IsClientProbeFor(string serverId, int payloadHash, int payloadChars)
	{
		if (!g_Game || !g_Game.ConfigIsExisting(CVF_Constants.CLIENT_CONFIG_PROBE_PATH + " cvfProtocol"))
			return false;

		string loadedServerId;
		g_Game.ConfigGetText(CVF_Constants.CLIENT_CONFIG_PROBE_PATH + " cvfServerId", loadedServerId);
		return g_Game.ConfigGetInt(CVF_Constants.CLIENT_CONFIG_PROBE_PATH + " cvfProtocol") == CVF_Constants.SYNC_PROTOCOL_VERSION && loadedServerId == serverId && g_Game.ConfigGetInt(CVF_Constants.CLIENT_CONFIG_PROBE_PATH + " cvfPayloadHash") == payloadHash && g_Game.ConfigGetInt(CVF_Constants.CLIENT_CONFIG_PROBE_PATH + " cvfPayloadChars") == payloadChars;
	}

	private static bool FindEndpoint(string endpointKey, out CVF_ClientEndpointEntry entry)
	{
		entry = null;
		CVF_ClientEndpointIndex index;
		LoadEndpointIndex(index);
		for (int i = 0; i < index.Entries.Count(); i++)
		{
			CVF_ClientEndpointEntry current = index.Entries.Get(i);
			if (current && current.EndpointKey == endpointKey)
			{
				entry = current;
				return true;
			}
		}
		return false;
	}

	private static void LoadEndpointIndex(out CVF_ClientEndpointIndex index)
	{
		index = new CVF_ClientEndpointIndex();
		if (!FileExist(CVF_Constants.CLIENT_ENDPOINT_INDEX_FILE))
			return;

		string error;
		JsonFileLoader<CVF_ClientEndpointIndex>.LoadFile(CVF_Constants.CLIENT_ENDPOINT_INDEX_FILE, index, error);
		if (!index.Entries)
			index.Entries = new array<ref CVF_ClientEndpointEntry>;
	}

	private static bool LoadServerMetadata(string serverId, out CVF_CacheMetadata metadata, out string error)
	{
		return LoadMetadata(GetServerMetadataPath(serverId), metadata, error);
	}

	private static bool LoadMetadata(string path, out CVF_CacheMetadata metadata, out string error)
	{
		metadata = new CVF_CacheMetadata();
		if (!FileExist(path))
		{
			error = "Metadata file is missing: " + path;
			return false;
		}
		return JsonFileLoader<CVF_CacheMetadata>.LoadFile(path, metadata, error);
	}

	private static bool SaveMetadata(string path, CVF_CacheMetadata metadata, out string error)
	{
		return JsonFileLoader<CVF_CacheMetadata>.SaveFile(path, metadata, error);
	}

	private static string GetServerDir(string serverId)
	{
		return CVF_Constants.CLIENT_SERVERS_DIR + "\\" + serverId;
	}

	private static string GetServerConfigPath(string serverId)
	{
		return GetServerDir(serverId) + "\\" + CVF_Constants.CACHE_CONFIG_FILE;
	}

	private static string GetServerPackagePath(string serverId)
	{
		return GetServerDir(serverId) + "\\" + CVF_Constants.CACHE_PACKAGE_FILE;
	}

	private static string GetServerMetadataPath(string serverId)
	{
		return GetServerDir(serverId) + "\\" + CVF_Constants.CACHE_METADATA_FILE;
	}
}

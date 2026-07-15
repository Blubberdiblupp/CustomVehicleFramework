class CVF_ClientNetworkSync
{
	private static string s_ServerId = "";
	private static string s_EndpointKey = "";
	private static int s_ExpectedHash = 0;
	private static int s_ExpectedChars = 0;
	private static int s_ExpectedProtocol = 0;
	private static int s_CurrentBatchId = -1;
	private static int s_ExpectedChunks = 0;
	private static ref array<string> s_Chunks = new array<string>;
	private static ref array<bool> s_ReceivedChunks = new array<bool>;

	static void ReceiveHello(string serverId, int payloadHash, int payloadChars, int protocolVersion, int loadedHash, bool serverReady, int nativeFingerprint, int nativeFingerprintChars)
	{
		ResetTransfer();
		CVF_RuntimeConfigStore.ClearVerifiedPackage();
		s_ServerId = serverId;
		s_ExpectedHash = payloadHash;
		s_ExpectedChars = payloadChars;
		s_ExpectedProtocol = protocolVersion;
		s_EndpointKey = CVF_ClientCache.ResolveCurrentEndpoint();

		if (!CVF_SharedUtils.IsSafeToken(serverId) || payloadHash == 0 || payloadChars <= 0 || payloadChars > CVF_Constants.MAX_GENERATED_PACKAGE_CHARS)
		{
			Fail("The server sent invalid vehicle package metadata.");
			return;
		}
		if (protocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION)
		{
			Fail("The server uses an incompatible Custom Vehicle Framework protocol.");
			return;
		}
		if (!serverReady || loadedHash != payloadHash)
		{
			Fail("This server generated new vehicle settings but has not loaded them yet. The server administrator must restart the complete server process again.");
			return;
		}
		if (nativeFingerprint == 0 || nativeFingerprintChars <= 0)
		{
			Fail("The server sent an invalid native vehicle fingerprint.");
			return;
		}

		CVF_ClientCache.UpdateEndpoint(s_EndpointKey, serverId, payloadHash, payloadChars);
		string error;
		int preparation = CVF_ClientCache.PrepareCachedServer(serverId, payloadHash, payloadChars, nativeFingerprint, nativeFingerprintChars, s_EndpointKey, true, error);
		switch (preparation)
		{
			case CVF_CachePreparationResult.CVF_CACHE_ALREADY_LOADED:
				SendStatus(CVF_ClientSyncStatus.CVF_SYNC_READY, "Native vehicle config and runtime package verified.");
				break;

			case CVF_CachePreparationResult.CVF_CACHE_MISSING:
				SendStatus(CVF_ClientSyncStatus.CVF_SYNC_REQUEST_PACKAGE, "Structured package is not cached.");
				break;

			case CVF_CachePreparationResult.CVF_CACHE_RESTART_REQUIRED:
				CVF_ClientUI.ShowRestartRequired("The cached vehicle settings for this server are now active. Close DayZ completely, start it again, and reconnect.");
				SendStatus(CVF_ClientSyncStatus.CVF_SYNC_RESTART_FROM_CACHE, "Cached package activated for the next DayZ start.");
				break;

			case CVF_CachePreparationResult.CVF_CACHE_BOOTSTRAP_FAILED:
				Fail(error);
				break;

			default:
				if (error == "")
					error = "The client vehicle package cache could not be prepared.";
				Fail(error);
				break;
		}
	}

	static void ReceivePackageChunk(int batchId, int chunkIndex, int totalChunks, string chunk)
	{
		if (s_ServerId == "" || s_ExpectedHash == 0)
		{
			Fail("Received a vehicle package before the server handshake.");
			return;
		}

		int calculatedChunks = (s_ExpectedChars + CVF_Constants.RPC_CONFIG_CHUNK_SIZE - 1) / CVF_Constants.RPC_CONFIG_CHUNK_SIZE;
		if (batchId <= 0 || totalChunks <= 0 || totalChunks > CVF_Constants.RPC_MAX_CHUNKS || totalChunks != calculatedChunks || chunkIndex < 0 || chunkIndex >= totalChunks || chunk.Length() > CVF_Constants.RPC_CONFIG_CHUNK_SIZE)
		{
			Fail("The server sent an invalid vehicle package chunk.");
			return;
		}

		if (s_CurrentBatchId != batchId)
			BeginTransfer(batchId, totalChunks);
		else if (s_ExpectedChunks != totalChunks)
		{
			Fail("The vehicle package chunk count changed during transfer.");
			return;
		}

		s_Chunks.Set(chunkIndex, chunk);
		s_ReceivedChunks.Set(chunkIndex, true);
		for (int i = 0; i < s_ExpectedChunks; i++)
		{
			if (!s_ReceivedChunks.Get(i))
				return;
		}

		string assembled = "";
		for (int assembleIndex = 0; assembleIndex < s_ExpectedChunks; assembleIndex++)
			assembled = assembled + s_Chunks.Get(assembleIndex);
		if (assembled.Length() != s_ExpectedChars || CVF_SharedUtils.HashText(assembled) != s_ExpectedHash)
		{
			Fail("The downloaded vehicle package failed its integrity check.");
			return;
		}

		CVF_GeneratedPackage package = new CVF_GeneratedPackage();
		JsonSerializer serializer = new JsonSerializer();
		string jsonError;
		if (!serializer.ReadFromString(package, assembled, jsonError))
		{
			Fail("The downloaded structured vehicle package is invalid: " + jsonError);
			return;
		}
		if (package.ServerId != s_ServerId || package.ProtocolVersion != s_ExpectedProtocol || package.ConfigVersion != CVF_Constants.CONFIG_VERSION)
		{
			Fail("The downloaded vehicle package does not match the server handshake.");
			return;
		}

		string cacheError;
		if (!CVF_ClientCache.StoreAndActivatePackage(package, assembled, s_ExpectedHash, s_ExpectedChars, s_EndpointKey, cacheError))
		{
			Fail(cacheError);
			return;
		}

		ResetTransfer();
		CVF_ClientUI.ShowRestartRequired("The vehicle settings for this server were downloaded and activated. Close DayZ completely, start it again, and reconnect.");
		SendStatus(CVF_ClientSyncStatus.CVF_SYNC_RESTART_AFTER_DOWNLOAD, "Package downloaded and activated for the next DayZ start.");
	}

	static void ReceiveNotice(int noticeType, string message)
	{
		if (noticeType == CVF_SyncNoticeType.CVF_NOTICE_RESTART_REQUIRED)
			CVF_ClientUI.ShowRestartRequired(message);
		else
			CVF_ClientUI.ShowError(message);
	}

	private static void BeginTransfer(int batchId, int totalChunks)
	{
		s_CurrentBatchId = batchId;
		s_ExpectedChunks = totalChunks;
		s_Chunks.Clear();
		s_ReceivedChunks.Clear();
		for (int i = 0; i < totalChunks; i++)
		{
			s_Chunks.Insert("");
			s_ReceivedChunks.Insert(false);
		}
	}

	private static void ResetTransfer()
	{
		s_CurrentBatchId = -1;
		s_ExpectedChunks = 0;
		s_Chunks.Clear();
		s_ReceivedChunks.Clear();
	}

	private static void Fail(string detail)
	{
		if (detail == "")
			detail = "Unknown client vehicle synchronization error.";
		CVF_ClientUI.ShowError(detail);
		SendStatus(CVF_ClientSyncStatus.CVF_SYNC_ERROR, detail);
		ResetTransfer();
	}

	private static void SendStatus(int status, string detail)
	{
		if (!g_Game || g_Game.IsServer() || s_ExpectedHash == 0)
			return;

		Param4<int, int, int, string> payload = new Param4<int, int, int, string>(status, s_ExpectedHash, CVF_Constants.SYNC_PROTOCOL_VERSION, detail);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_CLIENT_STATUS, payload, true, null);
	}
}

class CVF_ServerClientSyncState
{
	int Status;
	int ExpectedHash;

	void CVF_ServerClientSyncState(int expectedHash)
	{
		Status = CVF_ClientSyncStatus.CVF_SYNC_NONE;
		ExpectedHash = expectedHash;
	}
}

class CVF_NetworkSync
{
	private static ref map<string, ref CVF_ServerClientSyncState> s_ClientStates = new map<string, ref CVF_ServerClientSyncState>;

	static void SendHelloToClient(PlayerIdentity identity)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		GetCVFConfigManager().LoadConfig();
		if (GetCVFConfigManager().HasLoadError() || !GetCVFConfigManager().WasLastGenerationSuccessful() || !CVF_ConfigGenerator.HasGeneratedPackage())
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "The server vehicle configuration could not be generated. The administrator must check the CVF server log and vehicles.json.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 3000, false, identity);
			return;
		}

		int payloadHash = CVF_ConfigGenerator.GetCurrentPayloadHash();
		int payloadChars = CVF_ConfigGenerator.GetCurrentPayloadChars();
		string serverId = GetCVFConfigManager().m_Config.ServerId;
		int loadedHash = CVF_ConfigGenerator.GetLoadedPayloadHash();
		bool ready = CVF_ConfigGenerator.IsLoadedConfigCurrent();
		int nativeFingerprintHash = CVF_ConfigGenerator.GetLoadedNativeFingerprint();
		int nativeFingerprintChars = CVF_ConfigGenerator.GetLoadedNativeFingerprintChars();

		s_ClientStates.Set(identity.GetId(), new CVF_ServerClientSyncState(payloadHash));
		Param8<string, int, int, int, int, bool, int, int> hello = new Param8<string, int, int, int, int, bool, int, int>(serverId, payloadHash, payloadChars, CVF_Constants.SYNC_PROTOCOL_VERSION, loadedHash, ready, nativeFingerprintHash, nativeFingerprintChars);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_SYNC_HELLO, hello, true, identity);

		if (!ready)
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SERVER_RESTART_REQUIRED, "The server generated new vehicle overrides but has not loaded them yet. The administrator must restart the complete server process.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 3000, false, identity);
			return;
		}

		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckSyncTimeout, CVF_Constants.SYNC_TIMEOUT_MS, false, identity, payloadHash);
		CVF_Logger.Log("Sent client-native config handshake to " + identity.GetName() + " Hash=" + payloadHash.ToString() + " NativeFingerprint=" + nativeFingerprintHash.ToString());
	}

	static void HandleClientStatus(PlayerIdentity identity, int status, int payloadHash, int protocolVersion, string detail)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		CVF_ServerClientSyncState state;
		if (!s_ClientStates.Find(identity.GetId(), state))
		{
			CVF_Logger.Warning("Ignored CVF status without a server hello from " + identity.GetName());
			return;
		}
		if (protocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION || payloadHash != state.ExpectedHash || payloadHash != CVF_ConfigGenerator.GetCurrentPayloadHash())
		{
			state.Status = CVF_ClientSyncStatus.CVF_SYNC_ERROR;
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration handshake mismatch.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}
		if (!IsAllowedTransition(state.Status, status))
		{
			state.Status = CVF_ClientSyncStatus.CVF_SYNC_ERROR;
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Invalid vehicle configuration handshake state.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}

		state.Status = status;
		switch (status)
		{
			case CVF_ClientSyncStatus.CVF_SYNC_READY:
				CVF_Logger.Log("Client native vehicle config and runtime package verified: " + identity.GetName() + " Hash=" + payloadHash.ToString());
				break;

			case CVF_ClientSyncStatus.CVF_SYNC_REQUEST_PACKAGE:
				SendPackage(identity);
				break;

			case CVF_ClientSyncStatus.CVF_SYNC_RESTART_FROM_CACHE:
				SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_RESTART_REQUIRED, "Cached vehicle settings were activated. Close DayZ completely, start it again, and reconnect.");
				g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 2500, false, identity);
				break;

			case CVF_ClientSyncStatus.CVF_SYNC_RESTART_AFTER_DOWNLOAD:
				SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_RESTART_REQUIRED, "Vehicle settings were downloaded and activated. Close DayZ completely, start it again, and reconnect.");
				g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 2500, false, identity);
				break;

			default:
				CVF_Logger.Warning("Client vehicle synchronization failed for " + identity.GetName() + ": " + detail);
				SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration could not be prepared. Check the client script log.");
				g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 2000, false, identity);
				break;
		}
	}

	static void RemoveClient(PlayerIdentity identity)
	{
		if (identity)
			s_ClientStates.Remove(identity.GetId());
	}

	private static bool IsAllowedTransition(int previousStatus, int nextStatus)
	{
		if (previousStatus == CVF_ClientSyncStatus.CVF_SYNC_NONE)
			return nextStatus == CVF_ClientSyncStatus.CVF_SYNC_READY || nextStatus == CVF_ClientSyncStatus.CVF_SYNC_REQUEST_PACKAGE || nextStatus == CVF_ClientSyncStatus.CVF_SYNC_RESTART_FROM_CACHE || nextStatus == CVF_ClientSyncStatus.CVF_SYNC_ERROR;
		if (previousStatus == CVF_ClientSyncStatus.CVF_SYNC_REQUEST_PACKAGE)
			return nextStatus == CVF_ClientSyncStatus.CVF_SYNC_RESTART_AFTER_DOWNLOAD || nextStatus == CVF_ClientSyncStatus.CVF_SYNC_ERROR;
		return false;
	}

	private static void SendPackage(PlayerIdentity identity)
	{
		string packageJson = CVF_ConfigGenerator.GetCurrentPackageJson();
		if (packageJson == "" || packageJson.Length() > CVF_Constants.MAX_GENERATED_PACKAGE_CHARS)
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "The server vehicle package is empty or too large.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}

		int chunkSize = CVF_Constants.RPC_CONFIG_CHUNK_SIZE;
		int totalChunks = (packageJson.Length() + chunkSize - 1) / chunkSize;
		if (totalChunks <= 0 || totalChunks > CVF_Constants.RPC_MAX_CHUNKS)
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "The server vehicle package has an invalid chunk count.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}

		int batchId = Math.RandomInt(1, 2147483646);
		for (int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++)
		{
			int start = chunkIndex * chunkSize;
			int length = chunkSize;
			if (packageJson.Length() - start < length)
				length = packageJson.Length() - start;
			string chunk = packageJson.Substring(start, length);
			Param4<int, int, int, string> payload = new Param4<int, int, int, string>(batchId, chunkIndex, totalChunks, chunk);
			g_Game.RPCSingleParam(null, CVF_Constants.RPC_PACKAGE_CHUNK, payload, true, identity);
		}
		CVF_Logger.Log("Sent structured vehicle package to " + identity.GetName() + " Chunks=" + totalChunks.ToString());
	}

	private static void SendNotice(PlayerIdentity identity, int noticeType, string message)
	{
		Param2<int, string> notice = new Param2<int, string>(noticeType, message);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_SYNC_NOTICE, notice, true, identity);
	}

	private static void CheckSyncTimeout(PlayerIdentity identity, int expectedHash)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		CVF_ServerClientSyncState state;
		if (!s_ClientStates.Find(identity.GetId(), state))
			return;
		if (state.ExpectedHash != expectedHash || state.Status == CVF_ClientSyncStatus.CVF_SYNC_READY)
			return;

		SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration verification timed out.");
		DisconnectClient(identity);
	}

	private static void DisconnectClient(PlayerIdentity identity)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;
		s_ClientStates.Remove(identity.GetId());
		g_Game.DisconnectPlayer(identity);
	}
}
